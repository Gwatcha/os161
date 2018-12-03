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
/* #include <spinlock.h> */
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

typedef struct {

} core_map_entry;

static core_map_entry* core_map = NULL;

/*
* Called in boot sequence.
*/
void
vm_bootstrap(void)
{
        /* TODO Initiliaze core map */
        /* On entry, there is no VM yet (duuuuh), so we can not call kmalloc. */
        /* Instead, we use ram_stealmem. */

        KASSERT(core_map == NULL);

        const size_t num_hardware_pages = mainbus_ramsize() / PAGE_SIZE;

        const size_t coremap_bytes_required = sizeof(core_map_entry) * num_hardware_pages;

        const size_t coremap_pages_required = 1 + coremap_bytes_required / PAGE_SIZE;

        core_map = (core_map_entry*)ram_stealmem(coremap_pages_required);


        /* TODO Initialize page table */
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

        (void)npages;
        return 0;
}

/*
 * Called by kfree. Frees some kernel-space virtual pages. Kernel TLB mapped
 * VA's must be in MIPS_KSEG2 [0xc000 0000 - 0xffff ffff].
 */
void
free_kpages(vaddr_t addr)
{
	(void)addr;
}

/*
 * Called by interrupt handler in the case of an interprocessor interrupt of
 * type IPI_TLBSHOOTDOWN, where all processors are specified.
 */
void
vm_tlbshootdown_all(void)
{
}

/*
 * Called by interrupt handler in the case of an interprocessor interrupt of
 * type IPI_TLBSHOOTDOWN, where all processors are specified.
 */
void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
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
	DEBUG(DB_VM, "smartvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
                    /* TODO */
	    case VM_FAULT_READ:
                    /* TODO */
	    case VM_FAULT_WRITE:
                    /* TODO */
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

	struct addrspace *as;
	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
        int spl;
	spl = splhigh();
        /* do tlb stuff */
	splx(spl);

	return EFAULT;
}
