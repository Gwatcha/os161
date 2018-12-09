#include <syscall.h>
#include <types.h>

/*
 * Retval is a pointer to the new ending of the user heap region.
 * amount indicates the size to extend or shrink the heap by, and must be page aligned.
 * On error (void *) - 1 is returned and errno is set.
 *     Errors: ENOMEM, not enough virtual memory to satisfy.
 *             EINVAL, request would move the break below it's intial value.
 *     NOTE: Sbrk should is expected to be atomic, so multiple threads in a process may call it.
 */
int sys_sbrk(void* retval, intptr_t amount) {
#ifdef OPT_DUMBVM
        (void) retval;
        (void) amount;
        return ENOSYS;
#else
        (void) retval;
        (void) amount;

        /*
         * TODO sbrk extends the heap.
         */

        return 1;
#endif
}
