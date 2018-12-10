/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <mainbus.h>
#include <lib.h>
#include <thread.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <page_table.h>
#include <vm.h>
#include <coremap.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 *
 * NOTE: it's been found over the years that students often begin on
 * the VM assignment by copying dumbvm.c and trying to improve it.
 * This is not recommended. dumbvm is (more or less intentionally) not
 * a good design reference. The first recommendation would be: do not
 * look at dumbvm at all. The second recommendation would be: if you
 * do, be sure to review it from the perspective of comparing it to
 * what a VM system is supposed to do, and understanding what corners
 * it's cutting (there are many) and why, and more importantly, how.
 */


// ~~~~~~~~~~~~~~~~~~~Utility Functions ~~~~~~~~~~~~~~~~~~~~~~

page_t
addr_to_page(unsigned addr)
{
        return addr >> PAGE_SIZE_LOG_2;
}

unsigned
page_to_addr(page_t page)
{
        return page << PAGE_SIZE_LOG_2;
}

page_t
size_to_page_count(size_t size)
{
	return addr_to_page(size + PAGE_SIZE - 1);
}

/*
 * Called in boot sequence.
 */
void
vm_bootstrap(void)
{
        /* TODO ? initialize page table */
}

/*
 * Called by interrupt handler in the case of an interprocessor interrupt of
 * type IPI_TLBSHOOTDOWN, where all processors are specified.
 */
void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

/*
 * Called by interrupt handler in the case of an interprocessor interrupt of
 * type IPI_TLBSHOOTDOWN, where all processors are specified.
 */
void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

/*
 * Called in the case of a TLB fault,
 *        Possible faulttypes:
 *        VM_FAULT_READ        A read was attempted
 *        VM_FAULT_WRITE       A write was attempted
 *        VM_FAULT_READONLY    A write to a readonly page was attempted
 */
int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	faultaddress &= PAGE_FRAME;

	switch (faulttype) {
	    case VM_FAULT_READONLY:
                    /* TODO */
		/* We always create pages read-write, so we can't get this */
		panic("vm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

        const pid_t pid = curproc->p_pid;

	struct addrspace* as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

        page_table* pt = &as->as_page_table;

        const vpage_t vpage = addr_to_page(faultaddress);

        if (!page_table_contains(pt, vpage)) {
                kprintf("vm: hard fault! pid %d, vaddr 0x%x\n", pid, faultaddress);
                return EFAULT;
        }

        ppage_t ppage = page_table_read(pt, vpage);
        if (ppage == PPAGE_INVALID) {
                ppage = claim_free_pages(1);
                if (ppage == PPAGE_INVALID) {
                        kprintf("vm: Ran out of memory!\n");
                        return ENOMEM;
                }
                else {
                        page_table_write(pt, vpage, ppage);
                }
        }

        const paddr_t paddr = page_to_addr(ppage);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	int spl = splhigh();

        /*
         * TODO: Consider writing TLB entries using the combined
         * hash of the virtual page and process id
         */
	for (int i = 0; i < NUM_TLB; i++) {

                uint32_t ehi, elo;

		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
                /*
                 * TLB PID Note 1, see TLB PID Note 2
                 */
		ehi = faultaddress | (pid << 6);
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "vm: pid %d 0x%x -> 0x%x\n", pid, faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

        /*
         * WARNING, May not want to use krpintf in here after a tlb write, as
         * it may touch some of the TLB entries and make some weird bugs
         */

	// kprintf("vm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
}


static
void
reserve_vpage(page_table* pt, vpage_t vpage)
{
        KASSERTM(!page_table_contains(pt, vpage),
                 "Page table already contains an entry for %x", vpage);

        DEBUG(DB_VM, "vm: reserve vpage 0x%x\n", vpage);

        /*
         * Write an invalid ppage:
         * actual page frames are allocated when the memory is accessed
         */
        page_table_write(pt, vpage, PPAGE_INVALID);
}

static
ppage_t
copy_to_new_page(ppage_t old_page)
{
        if (old_page == PPAGE_INVALID) {
                return PPAGE_INVALID;
        }

        const ppage_t new_page = claim_free_pages(1);
        if (new_page == PPAGE_INVALID) {
                return PPAGE_INVALID;
        }

        const vaddr_t old_address = PADDR_TO_KVADDR(page_to_addr(old_page));
        const vaddr_t new_address = PADDR_TO_KVADDR(page_to_addr(new_page));

        DEBUG(DB_VM, "vm: copy page 0x%x -> 0x%x\n", old_page, new_page);

        memcpy((void*)new_address, (const void*)old_address, PAGE_SIZE);

        DEBUG(DB_VM, "vm: done copy page\n");

        return new_page;
}

// ~~~~~~~~~~~ Address Space Functionality ~~~~~~~~~~~

static
UNUSED
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int
as_prepare_load(struct addrspace *as)
{
        (void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
        DEBUG(DB_VM, "vm: as_define_stack()\n");

        page_table* pt = &as->as_page_table;

        const vpage_t stack_top = addr_to_page(USERSTACK);
        const vpage_t stack_bottom = stack_top - STACKPAGES;

        /* Reserve virtual pages for the stack */
        for (vpage_t vpage = stack_top; vpage > stack_bottom; --vpage) {

                reserve_vpage(pt, vpage);
        }

	*stackptr = USERSTACK;

        DEBUG(DB_VM, "vm: as_define_stack() done\n");
	return 0;
}

int
as_copy(struct addrspace* old, struct addrspace** ret)
{
        DEBUG(DB_VM, "vm: as_copy()\n");

        *ret = as_create();
        if (*ret == NULL) {
                return ENOMEM;
        }
        page_table* new_pt = &(*ret)->as_page_table;

        const page_table* old_pt = &old->as_page_table;
        const page_mapping* old_mappings = old_pt->pt_mappings;
        const unsigned old_capacity = old_pt->pt_capacity;

        for (unsigned i = 0; i < old_capacity; ++i) {

                const page_mapping* old_mapping = old_mappings + i;

                if (!page_mapping_is_valid(old_mapping)) {
                        continue;
                }

                const vpage_t old_vpage = old_mapping->pm_vpage;
                const ppage_t old_ppage = old_mapping->pm_ppage;

                const ppage_t new_ppage = copy_to_new_page(old_ppage);

                /*
                 * TODO: if the old page is valid and the new page is invalid
                 * then we have run out of pages
                 */

                page_table_write(new_pt, old_vpage, new_ppage);
        }

        DEBUG(DB_VM, "vm: as_copy() done\n");

	return 0;
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t size,
		 int readable, int writeable, int executable)
{
        KASSERT(as != NULL);

        DEBUG(DB_VM,
              "vm: as_define_region(vaddr:      0x%08x\n"
              "                     size:       0x%08x\n"
              "                     readable:   %d\n"
              "                     writeable:  %d\n"
              "                     executable: %d\n"
              ")\n",
              vaddr, size, readable, writeable, executable);

	/* Not using these yet - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

        page_table* pt = &as->as_page_table;

        const vaddr_t vaddr_max = vaddr + size - 1;

        const vpage_t vpage_min = addr_to_page(vaddr);
        const vpage_t vpage_max = addr_to_page(vaddr_max);

        /* Reserve virtual pages for the region */
        for (vpage_t vpage = vpage_min; vpage <= vpage_max; ++vpage) {
                reserve_vpage(pt, vpage);
        }

        /* check if this region extends past our current heap start, if so, move */
        /* the heap start further up, we can do this because regions are */
        /* never defined when the heap is in use */
        while ( vaddr_max >= as->as_heap_start  ) {
                as->as_heap_start += PAGE_SIZE;
                as->as_heap_end = as->as_heap_start;
        }


        DEBUG(DB_VM, "vm: as_define_region() done\n");

        return 0;
}
struct addrspace *
as_create(void)
{
        DEBUG(DB_VM, "vm: as_create()\n");

	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}


        /* My best guess for now of a good initial capacity */
        page_table_init_with_capacity(&as->as_page_table, 32);

        /* the heap starts at 0, and if any region is defined (excluding the */
        /* stack) the heap start is moved to the next free page */
        /* after that region */
        as->as_heap_start = 0;
        as->as_heap_end = as->as_heap_start;

        DEBUG(DB_VM, "vm: as_create() done\n");

	return as;
}

void
as_destroy(struct addrspace *as)
{
        /* TODO: free the used page frames! */
        page_table_cleanup(&as->as_page_table);
	kfree(as);
}

void
as_activate(void)
{
        /*
         * TLB PID Note 2
         * Although I am setting and would like to rely on the PID field
         * in the TLB EHI word, it does not appear to work. For this
         * reason I am falling back on clearing the TLB after every
         * process switch (as did DUMBVM).
         *
         * Documentation:
         *
         * Question on piazza (as to why it doesn't work):
         *     https://piazza.com/class/jlpfhk5mesk6s0?cid=1143
         *
         * See TLB PID Note 1.
         */

	const int spl = splhigh();

	/* Disable interrupts on this CPU while clearing the TLB. */
	for (int i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}
