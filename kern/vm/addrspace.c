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

// #include <types.h>
// #include <kern/errno.h>
// #include <current.h>
// #include <lib.h>
// #include <proc.h>
// #include <addrspace.h>
// #include <vm.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */


// static
// UNUSED
// void
// as_zero_region(paddr_t paddr, unsigned npages)
// {
//         bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
// }

// int
// as_prepare_load(struct addrspace *as)
// {
//         (void)as;
//         return 0;
// }

// int
// as_complete_load(struct addrspace *as)
// {
//         (void)as;
//         return 0;
// }

// int
// as_define_stack(struct addrspace *as, vaddr_t *stackptr)
// {
//         DEBUG(DB_VM, "vm: as_define_stack()\n");

//         page_table* pt = &as->as_page_table;

//         const vpage_t stack_top = addr_to_page(USERSTACK);
//         const vpage_t stack_bottom = stack_top - DUMBVM_STACKPAGES;

//         // /* Reserve virtual pages for the stack */
//         for (vpage_t vpage = stack_top; vpage > stack_bottom; --vpage) {

//                 reserve_vpage(pt, vpage);
//         }

//         *stackptr = USERSTACK;

//         DEBUG(DB_VM, "vm: as_define_stack() done\n");
//         return 0;
// }
// struct addrspace *
// as_create(void)
// {
//         struct addrspace *as;

//         as = kmalloc(sizeof(struct addrspace));
//         if (as == NULL) {
//                 return NULL;
//         }

	/*
	 * Initialize as needed.
	 */

//         return as;
// }

// int
// as_copy(struct addrspace* old, struct addrspace** ret)
// {
//         DEBUG(DB_VM, "vm: as_copy()\n");

//         *ret = as_create();
//         if (*ret == NULL) {
//                 return ENOMEM;
//         }
//         page_table* new_pt = &(*ret)->as_page_table;

//         const page_table* old_pt = &old->as_page_table;
//         const page_mapping* old_mappings = old_pt->pt_mappings;
//         const unsigned old_capacity = old_pt->pt_capacity;

//         for (unsigned i = 0; i < old_capacity; ++i) {

//                 const page_mapping* old_mapping = old_mappings + i;

//                 if (!page_mapping_is_valid(old_mapping)) {
//                         continue;
//                 }

//                 const vpage_t old_vpage = old_mapping->pm_vpage;
//                 const ppage_t old_ppage = old_mapping->pm_ppage;

//                 const ppage_t new_ppage = copy_to_new_page(old_ppage);

                /*
                 * TODO: if the old page is valid and the new page is invalid
                 * then we have run out of pages
                 */

//                 page_table_write(new_pt, old_vpage, new_ppage);
//         }

//         DEBUG(DB_VM, "vm: as_copy() done\n");

//         return 0;
// }

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
// int
// as_define_region(struct addrspace *as, vaddr_t vaddr, size_t size,
//                  int readable, int writeable, int executable)
// {
//         KASSERT(as != NULL);

//         DEBUG(DB_VM,
//               "vm: as_define_region(vaddr:      0x%08x\n"
//               "                     size:       0x%08x\n"
//               "                     readable:   %d\n"
//               "                     writeable:  %d\n"
//               "                     executable: %d\n"
//               ")\n",
//               vaddr, size, readable, writeable, executable);

//         // /* Not using these yet - all pages are read-write */
//         (void)readable;
//         (void)writeable;
//         (void)executable;

//         page_table* pt = &as->as_page_table;

//         const vaddr_t vaddr_max = vaddr + size;

//         const vpage_t vpage_min = addr_to_page(vaddr);
//         const vpage_t vpage_max = addr_to_page(vaddr_max);

//         // /* Reserve virtual pages for the region */
//         for (vpage_t vpage = vpage_min; vpage <= vpage_max; ++vpage) {

//                 reserve_vpage(pt, vpage);
//         }

//         DEBUG(DB_VM, "vm: as_define_region() done\n");

//         return 0;
// }
// struct addrspace *
// as_create(void)
// {
//         DEBUG(DB_VM, "vm: as_create()\n");

//         struct addrspace *as = kmalloc(sizeof(struct addrspace));
//         if (as==NULL) {
//                 return NULL;
//         }


//         // /* My best guess for now of a good initial capacity */
//         page_table_init_with_capacity(&as->as_page_table, 32);

//         DEBUG(DB_VM, "vm: as_create() done\n");

//         return as;
// }

// void
// as_destroy(struct addrspace *as)
// {
//         // /* TODO: free the used page frames! */
//         kfree(as);
// }

// void
// as_activate(void)
// {
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

//         const int spl = splhigh();

//         // /* Disable interrupts on this CPU while clearing the TLB. */
//         for (int i=0; i<NUM_TLB; i++) {
//                 tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
//         }

//         splx(spl);
// }

// void
// as_deactivate(void)
// {
//         // /* nothing */
// }

