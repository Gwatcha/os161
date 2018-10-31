/*
 * Assignment 5 process related syscalls, contains implementation of syscalls:
 *     read, sys_write, sys_lseek, sys_close, sys_dup2, and sys_chdir.
 *
 *     written by the infamous dream team - recognized world wide as the
 *     'thetraphandlers',
 *
 *     syscall specs may be found at http://ece.ubc.ca/~os161/man/syscall/, the
 *     only difference in the interface between the man page and these is that
 *     the return value is either 0, indicating success, or an error code,
 *     indicating failure. The actual return value is placed in the first
 *     parameter.
 */

#include <types.h>
#include <syscall.h>
#include <copyinout.h>

#include <proc.h>
#include <current.h>

#include <limits.h>

#include <kern/errno.h>

#include <addrspace.h>

/* ------------------------------------------------------------------------- */
/* Private Utility Functions */

/*
 * copyinstr_array:
 *     given a char** userptr and a pointer to a char** kbuff this function safely
 *     copies it into kbuff, if it is not possible, it returns an error code.
 *     Inputs:
 *         char **userptr, an array of pointers to string addresses all in user
 *         memory, the array is also in user memory. To be proper, it has to be
 *         terminated by 0. All strings are null terminated, if not EFAULT is
 *         returned. If the total size of userptr exceeds maxcopy, E2BIG is
 *         returned.
 *
 *         char** kbuff, will be overwritten with the address of the memory
 *         associated with kbuff, may have size larger than maxcopy
 *
 *         int maxcopy, the max number of bytes to copy before returning E2BIG.
 *
 *     Returns: 0 on success, error code on failure. possible error codes: E2BIG, EFAULT
 *     Postconditions: if return was successful, caller must call kfree(*kbuff)
 *                     and kfree(kbuff) when finished with kbuff. If
 *                     unsucessful, caller is discard kbuff.
 */
int
copyinstr_array(char ** user_ptr, char ** kbuff, int maxcopy) {

        int err = 0;
        int addr_bytes_copied = 0;
        int string_bytes_copied = 0;

        /*
         * kernel buffer memory is dynamically allocated throughout the copyin of
         * user_ptr, it starts at 128 bytes (2^7) and is doubled if required,
         *
         * this is done for two kernel buffers, one which holds the addresses
         * of strings (kbuff), and one which holds all the strings (kstrings)
         *
         * kbuff and kstrings are connected so that kbuff[i] points to
         * an address inside kstrings which indicates the beginning of a string.
         * so kbuff[0] = &kstrings[0]
         */

        int initial_buf_size = 128;
        kbuff = kmalloc(initial_buf_size);
        char * kstrings = kmalloc(initial_buf_size);
        int kbuff_size = initial_buf_size;
        int kstrings_size = initial_buf_size;

        int i = 0; /* used as index for user_ptr[] and kbuff[] */
        int s = 0; /* keeps track of the next available spot in kstrings */

        /* when this loop exits, we have either encountered an error or have
         * finished copying user memory. on each iteration, we copyin the next
         * string's address, and copyin the string for that address. In doing
         * so, we make sure to break on error or termination, as well as resize
         * kbuff and kstring if needed */
        while( true ) {

                /*
                 * copy in string address at user_ptr[i] to kbuff[i]
                 */

                char * temp;
                err = copyin( (const_userptr_t) (user_ptr + i), temp, sizeof(char*) );
                addr_bytes_copied += sizeof(char*);
                if (err) {
                        break;
                }
                if (maxcopy < addr_bytes_copied + string_bytes_copied) {
                        err = E2BIG;
                        break;
                }

                /* termination case */
                if (temp == 0) {
                        break;
                }

                kbuff[i] = temp;

                /* resize kbuff case */
                if (addr_bytes_copied == kbuff_size) {
                        char ** koldbuff = kbuff;
                        kbuff = kmalloc(kbuff_size*2);
                        memcpy(kbuff, koldbuff, kbuff_size);
                        kbuff_size *= 2;
                }

                /*
                 * copy in the string at address kbuff[i] into kstrings[s]
                 */

                /* len for copyinstr is just the space left in kstrings*/
                size_t	len = kstrings_size - string_bytes_copied;
                size_t got;
                err = copyinstr( (const_userptr_t) kbuff[i], (kstrings + s), len, &got);
                /* resize kstrings case (loop because we may do succesive resizes for this string) */
                while (err == ENAMETOOLONG) {
                        err = 0;
                        /* check if too big */
                        if ( maxcopy < kstrings_size + addr_bytes_copied ) {
                                err = E2BIG;
                                break;
                        }
                        /* resize */
                        char * koldstrings = kstrings;
                        kstrings = kmalloc(kstrings_size*2);
                        memcpy(koldstrings, kstrings, kstrings_size);
                        kstrings_size *= 2;

                        /* retry copyinstr */
                        len = kstrings_size - string_bytes_copied;
                        err = copyinstr( (const_userptr_t) user_ptr[i], (kstrings + s), len, &got);
                        if (err == EFAULT) {
                                break;
                        }
                }

                if (err == EFAULT) {
                        break;
                }

                /* we have successively copied the next string in */
                string_bytes_copied += got;
                s += got;

                /* store this strings length in kbuff[i], this is to make it easier to
                   set up all the pointers from kbuff to kstrings afterwards. */
                kbuff[i] = (char*) got;

                if (maxcopy < addr_bytes_copied + string_bytes_copied) {
                        err = E2BIG;
                        break;
                }

                i += 1;
        }

        /* clean up code in case of error */
        if (err) {
                kfree(kbuff);
                kfree(kstrings);
                return err;
        }

        /* lastly, set up kbuff to point to the strings in kstrings
           note that i is equal to the end of kbuff + 1 */
        s = 0; /* used as index into kstrings */
        for (int j = 0; j < i; j++) {
                s += (int) kbuff[j]; /* both 32 bits */
                kbuff[j] = &kstrings[s];
        }

        /* if we are here, it is an invariant that kbuff[i] = user_ptr[i] for i up to
           and including the index with 0 */

        /* DEBUG ONLY: check the invariant */
        for (unsigned int i = 0; kbuff[i] != 0; i++) {
                char * temp = NULL;
                copyin( (const_userptr_t) user_ptr[i], temp, sizeof(char*) );
                KASSERT(kbuff[i] == temp);
        }

        return 0;
}

/* Private Utility Functions */
/* ------------------------------------------------------------------------- */
/* System calls */


int
sys_execv(const char *program, char **args) {

        /*
         * May return these errors:
         * ENODEV	The device prefix of program did not exist.
         * ENOTDIR	A non-final component of program was not a directory.
         * ENOENT	program did not exist.
         * EISDIR	program is a directory.
         * ENOEXEC	program is not in a recognizable executable file format, was for the wrong platform, or contained invalid fields.
         * ENOMEM	Insufficient virtual memory is available.
         * E2BIG	The total size of the argument strings exceeeds ARG_MAX.
         * EIO		A hard I/O error occurred.
         * EFAULT	One of the arguments is an invalid pointer.
         */

        int err = 0;
        char ** kargs = NULL;
        char * kprogram = NULL;
        struct addrspace * new_as = NULL;
        struct vnode * v = NULL;
        vaddr_t entrypoint, stackptr;
        struct addrspace * old_as = proc_getas();

        /* ---------------------------------------------------------------- */

        /*
         * 1. Copy arguments (args) into a kernel buffer, kargs, and program
         * name into kprogram
         */

        err = copyinstr_array(args, kargs, ARG_MAX);
        if (err) {
                goto err;
        }

        err = copinstr(program, kprogram, NAME_MAX);
        if (err) {
                goto err;
        }

        /*
         * 2. Load new executable
         */

	/* Open the file. */
	err = vfs_open(kprogram, O_RDONLY, 0, &v);
	if (err) {
		return err;
	}
	err = load_elf(v, &entrypoint);
	if (err) {
            goto err;
	}

        /*
         * 3. Create new address space
         */

        new_as = as_create(); 
	if (new_as == NULL) {
                err = ENOMEM:
                goto err;
	}

        /*
         * 4. Switch to new address space
         */
        
        as_deactivate();
        proc_setas(new_as);
        as_activate();



        /*
         * 5. Define new stack region
         *     as_define_stack
         *     see run_program for example
         *
         * 6. Copy arguments to new address space, properly arranging them
         *     NOTE: Always use copy_in API to touch user memory
         *
         *     Arguments are pointers in registers into user space
         *       - a0: null-terminated char* pointer to program name in user space
         *       - a1: null-terminated char** pointer to argument vector in user space
         *     Need to copy this information somewhere into the new address space
         *
         *     There is a maximum number of arguments
         *     - Null terminator must occur after an appropriate number of arguments
         *     - It is an error if null terminator is not encountered within the appropriate number of args
         *       - If there is no null-terminator, the addresses received are probably garbage
         *
         * 7. Clean up old address space
         *     as_destroy
         *     NOT TOO SOON: must come back to the old address space if exec fails
         *
         * 8. Warp to user mode
         *     - enter_new_process
         */

        (void) program;
        (void) *args;

        /* clean up and return */
        kfree(*kargs);
        kfree(kargs);
	vfs_close(v);
        return 0;

err:
        kfree(*kargs);
        kfree(kargs);
	vfs_close(v); /* if never opened? */

        /* clean up address */
        as_deactivate();
        proc_setas(old_as);
        as_destroy(new_as); /* if never created? */
        as_activate();
        return err;
}

int
sys_fork(pid_t* retval) {
        (void)  retval;
        return 0;
}

int
sys_getpid(pid_t* retval) {
        (void) retval;
        return 0;
}

int
sys_waitpid(pid_t* retval, pid_t pid, int *status, int options) {
        (void) retval;
        (void) pid;
        (void) *status;
        (void) options;
        return 0;
}

int
sys__exit(int exitcode) {
        (void) exitcode;
        return 0;
}
