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

typedef struct {
        pid_t cme_pid;
} core_map_entry;


static core_map_entry* core_map = NULL;

static ppage_t coremap_first_page = 0; /* Index of the first page frame available */
static ppage_t coremap_last_page = 0;  /* One past the last page frame available */

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

/* under dumbvm, always have 72k of user stack */
/* (this must be > 64K so argument blocks of size ARG_MAX will fit) */
#define DUMBVM_STACKPAGES    18

#define PAGE_SIZE_LOG_2 12

/*
 * Wrap ram_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;


/* The number of hardware pages available to the vm system */
static
size_t
hardware_pages_available()
{
        return coremap_last_page - coremap_first_page;
}

static
UNUSED
paddr_t
physical_memory_available()
{
        return hardware_pages_available() * PAGE_SIZE;
}


static
UNUSED
page_t
addr_to_page(unsigned addr)
{
        return addr >> PAGE_SIZE_LOG_2;
}

static
UNUSED
unsigned
page_to_addr(page_t page)
{
        return page << PAGE_SIZE_LOG_2;
}

static
UNUSED
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
        /* On entry, there is no VM yet, so we cannot call kmalloc. */
        /* Instead, we use ram_stealmem. */
        KASSERT(core_map == NULL);

        const paddr_t last_paddr = ram_getsize();
        const paddr_t first_paddr = ram_getfirstfree();
	DEBUG(DB_VM, "first_paddr:  %x\n", first_paddr);
	DEBUG(DB_VM, "last_paddr:   %x\n", last_paddr);

        coremap_first_page = addr_to_page(first_paddr);
        coremap_last_page = addr_to_page(last_paddr);
	DEBUG(DB_VM, "coremap_first_page:  %d\n", coremap_first_page);
	DEBUG(DB_VM, "coremap_last_page:   %d\n", coremap_last_page);

        const page_t num_hardware_pages = hardware_pages_available();
	DEBUG(DB_VM, "Hardware pages available:  %zu\n", num_hardware_pages);

        const size_t coremap_bytes_required = sizeof(core_map_entry) * num_hardware_pages;
	DEBUG(DB_VM, "Coremap size (bytes): %zu\n", coremap_bytes_required);

        const page_t coremap_pages_required = size_to_page_count(coremap_bytes_required);
	DEBUG(DB_VM, "Coremap size (pages): %zu\n", coremap_pages_required);

        const paddr_t core_map_paddr = first_paddr;

        core_map = (core_map_entry*)PADDR_TO_KVADDR(core_map_paddr);

        /* It may be possible to use excess bytes in the coremap pages for other kernel memory, */
        /* but for now let's assume the coremap occupies the entirety of its pages */

        DEBUG(DB_VM, "coremap:  %p\n", core_map);
        DEBUG(DB_VM, "&coremap: %p\n", &core_map);

        for (ppage_t i = 0; i < coremap_pages_required; ++i) {
                core_map[i].cme_pid = PID_KERN;
        }
        for (ppage_t i = coremap_pages_required; i < num_hardware_pages; ++i) {
                core_map[i].cme_pid = PID_INVALID;
        }

        /* TODO Initialize page table */
}

static
UNUSED
page_t
find_free_coremap_entries(unsigned npages)
{
        const ppage_t imax = hardware_pages_available() - npages + 1;
        for (ppage_t i = 0; i < imax; ++i) {

                const ppage_t jmax = i + npages;
                for (ppage_t j = i; j < jmax; ++j) {

                        if (core_map[j].cme_pid != PID_INVALID) {
                                goto try_next;
                        }
                }
                return i;

        try_next:
                continue;
        }
        return PPAGE_INVALID;
}

static
UNUSED
ppage_t
claim_free_pages(unsigned npages)
{
	spinlock_acquire(&stealmem_lock);

        const page_t first_free_index = find_free_coremap_entries(npages);

        if (first_free_index == PPAGE_INVALID) {
                spinlock_release(&stealmem_lock);
                return PPAGE_INVALID;
        }

        const page_t imax = first_free_index + npages;
        for (page_t i = first_free_index; i < imax; ++i) {
                core_map[i].cme_pid = PID_KERN;
        }

	spinlock_release(&stealmem_lock);

	return first_free_index + coremap_first_page;
}

static
paddr_t
getppages(unsigned long npages)
{
        const ppage_t ppage = claim_free_pages(npages);

        if (ppage == PPAGE_INVALID) {
                kprintf("could not get a page\n");
                return 0;
        }
        return page_to_addr(ppage);
}

/*
 *  Allocates some kernel-space virtual pages.  Kernel TLB mapped VA's must be
 *  in MIPS_KSEG2 [0xc000 0000 - 0xffff ffff].
 *  Called by kmalloc.
 */
vaddr_t
alloc_kpages(unsigned npages)
{
        /* TODO Find n free pages in the coremap structure */
        /* TODO set them to be used my kernel */
        /* TODO update TLB? or just wait for the fault?  */

        const paddr_t pa = getppages(npages);

	return PADDR_TO_KVADDR(pa);
}

/*
 * Called by kfree. Frees some kernel-space virtual pages. Kernel TLB mapped
 * VA's must be in MIPS_KSEG2 [0xc000 0000 - 0xffff ffff].
 */
void
free_kpages(vaddr_t vaddr)
{
        const ppage_t page = addr_to_page(KVADDR_TO_PADDR(vaddr));

        spinlock_acquire(&stealmem_lock);
        core_map[page - coremap_first_page].cme_pid = PID_INVALID;
        spinlock_release(&stealmem_lock);
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
                kprintf("vm: hard fault! 0x%x\n", faultaddress);
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
		DEBUG(DB_VM, "vm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	kprintf("vm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
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

        DEBUG(DB_VM, "vm: as_create() done\n");

	return as;
}

void
as_destroy(struct addrspace *as)
{
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
        /*
         * TODO: Consider writing TLB entries using the combined
         * hash of the virtual page and process id
         */
        /* TODO: free the used page frames! */
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

        const vaddr_t vaddr_max = vaddr + size;

        const vpage_t vpage_min = addr_to_page(vaddr);
        const vpage_t vpage_max = addr_to_page(vaddr_max);

        /* Reserve virtual pages for the region */
        for (vpage_t vpage = vpage_min; vpage <= vpage_max; ++vpage) {

                reserve_vpage(pt, vpage);
        }

        DEBUG(DB_VM, "vm: as_define_region() done\n");

        return 0;
}

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
        const vpage_t stack_bottom = stack_top - DUMBVM_STACKPAGES;

        /* Reserve virtual pages for the stack */
        for (vpage_t vpage = stack_top; vpage > stack_bottom; --vpage) {

                reserve_vpage(pt, vpage);
        }

	*stackptr = USERSTACK;

        DEBUG(DB_VM, "vm: as_define_stack() done\n");
	return 0;
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

        DEBUG(DB_VM, "vm: copying 0x%08x -> 0x%08x\n", old_page, new_page);

        memcpy((void*)new_address, (const void*)old_address, PAGE_SIZE);

        return new_page;
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
