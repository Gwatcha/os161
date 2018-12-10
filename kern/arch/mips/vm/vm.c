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
