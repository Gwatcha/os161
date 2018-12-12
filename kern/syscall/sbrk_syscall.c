#include <syscall.h>
#include <types.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <limits.h>
#include <kern/errno.h>
#include <page_table.h>

/*
* Retval is a pointer to the new ending of the user heap region.
* amount indicates the size to extend or shrink the heap by, and must be page aligned.
* On error (void *) - 1 is returned and errno is set.
*     Preconditions: as->as_heap_start and as->as_heap_end have already been defined
*     Errors: ENOMEM, not enough virtual memory to satisfy.
*             EINVAL, request would move the break below it's intial value.
*     NOTE: Sbrk should is expected to be atomic, so multiple threads in a process may call it.
*/
int sys_sbrk(int* retval, intptr_t amount) {
#if OPT_DUMBVM
        (void) retval;
        (void) amount;
        return ENOSYS;
#else 
        *retval =  -1;

	struct addrspace* as = proc_getas();


        /* Check proper amount given  */
        if ( amount % PAGE_SIZE != 0 || as->as_heap_end + amount < as->as_heap_start ) {
                return EINVAL;
        }

        /* Check if would result in too much heap */
        /* Currently, the stack is a fixed size, so too much heap would means
         * extending into the stack */
        if ( as->as_heap_end + amount >= USERSTACK - 18*4096 ) {
                return ENOMEM;
        }

        intptr_t npages = amount / PAGE_SIZE;
        page_table * pt = &as->as_page_table;
        // destroy pages such that the user can not fault on them anymore
        if ( amount < 0 ) {
                for ( intptr_t i = 1; i <= npages ; i++ ) { 
                        vpage_t nxt_vpage = addr_to_page( as->as_heap_end - i*4096 );
                        ppage_t nxt_ppage = page_table_read(pt, nxt_vpage);

                        /* this page is in use, so free it and invalidate it */
                        if (nxt_ppage != PPAGE_INVALID) {
                                // TODO free nxt_ppage ?  how does one do
                        }

                        page_table_remove(pt, nxt_vpage);
                }
        }

        // write new INVALID pages above heap so that the user threads may fault on their addresses
        else if ( amount > 0 ) {
                for ( intptr_t i = 0; i < npages ; i++ ) { 
                        page_table_write(pt, addr_to_page( as->as_heap_end + i*4096 ), PPAGE_INVALID );
                }
        }

        /* return old end, and adjust it */
        *retval =  as->as_heap_end;
        as->as_heap_end = as->as_heap_end + amount;
        return 0;
#endif
}
