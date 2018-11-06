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
 *     indicating failure. The actual return value (if existent) is placed in
 *     the first parameter.
 */

#include <types.h>
#include <syscall.h>
#include <copyinout.h>
#include <vfs.h>

#include <mips/trapframe.h>

#include <proc.h>
#include <current.h>

#include <limits.h>
#include <kern/limits.h>

#include <kern/errno.h>
#include <kern/fcntl.h>

#include <synch.h>
#include <addrspace.h>

/* ------------------------------------------------------------------------- */
/* Private Utility Functions */

/* TODO: May be better to have a larger function which copies in and out
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
static
int
copyinstr_array(char ** user_ptr, char ** kbuff, int maxcopy) {

        int err = 0;
        int addr_bytes_copied = 0;
        int string_bytes_copied = 0;
        int initial_buf_size = 128;

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
                 * copy in string address at user_ptr + i to kbuff[i]
                 */

                char* temp = NULL;
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

        struct vnode * v = NULL;
        struct addrspace * new_as = NULL;
        vaddr_t entrypoint, stackptr;
        struct addrspace * old_as = proc_getas();

        /* deactivate address space since it will be in a volatile state, in
         * DUMBVIM, this does nothing */
        as_deactivate(); 

        /* ---------------------------------------------------------------- */

        /*
         * 1. Copy arguments (args) into a kernel buffer, kargs, and program
         * name into kprogram
         */

        err = copyinstr_array(args, kargs, ARG_MAX);
        if (err) {
                goto err;
        }

        size_t got;
        err = copyinstr((const_userptr_t)program, kprogram, NAME_MAX, &got);
        if (err) {
                goto err;
        }


        /*
         * 2. Create new address space
         */

        new_as = as_create(); 
	if (new_as == NULL) {
                err = ENOMEM;
                goto err;
	}

        /*
         * 3. Switch to new address space
         */
        
        proc_setas(new_as);

        /*
         * 4. Load new executable
         */

	/* Open the file. */
	err = vfs_open(kprogram, O_RDONLY, 0, &v);
	if (err) {
	        goto err;
	}
	err = load_elf(v, &entrypoint);
	if (err) {
                goto err;
	}

        /*
         * 5. Define new stack region
         */

	/* Define the user stack in the new address space */
	err = as_define_stack(new_as, &stackptr);
	if (err) {
		return err;
	}

        /*
         * 6. Copy arguments to new address space, properly arranging them
         *     Arguments are pointers in registers into user space
         *       - a0: null-terminated char* pointer to program name in user space
         *       - a1: null-terminated char** pointer to argument vector in user space
         *     Need to copy this information somewhere into the new address space
         */

        /* TODO: */

        /*
         * 7. Clean up old address space
         */

        as_destroy(old_as); /* the point of no return...  */

        /*
         * 8. Warp to user mode
         */

        /* clean up before doing so */
        kfree(*kargs);
        kfree(kargs);
	vfs_close(v);

        as_activate();
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
			  stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
        return EINVAL;

err:
        kfree(*kargs);
        kfree(kargs);
	vfs_close(v); /* if never opened? */

        /* clean up address space */
        proc_setas(old_as);
        as_destroy(new_as); /* DUMBVM handles the case where new_as is NULL */
        as_activate();
        return err;
}

static
void
enter_forked_process_wrapper(void* data1, unsigned long data2) {
        (void)data2;
        enter_forked_process((struct trapframe*)data1);
}

/* enum process_status { */
/*         is_alive, */
/*         has_exited, */
/* } */

struct process_table_entry {
        struct cv* pte_waitpid_cv;
        struct array pte_child_pids;
        pid_t pte_parent_pid;
        bool pte_has_exited;
        int pte_exit_status;
        /* int pte_refcount; */
};

static
struct process_table_entry*
process_table_entry_create(pid_t pid, pid_t parent_pid) {

        struct process_table_entry* pte = kmalloc(sizeof(struct process_table_entry));

        /* TODO: Better name for the CV */
        (void)pid;
        pte->pte_waitpid_cv = cv_create("");

        array_init(&pte->pte_child_pids);

        pte->pte_parent_pid = parent_pid;
        pte->pte_has_exited = false;

        return pte;
}

static
void
process_table_entry_destroy(struct process_table_entry* pte) {
        cv_destroy(pte->pte_waitpid_cv);
        kfree(pte);
}

typedef struct process_table_entry* process_table[__PID_MAX];

static process_table proc_table = { NULL };

/* TEMP HACK */
static struct lock* big_lock;

void
create_first_proc_table_entry() {
        proc_table[1] = process_table_entry_create(1, 0);

        /* TEMP HACK */
        big_lock = lock_create("asdf");
}

static
bool
process_has_child(struct process_table_entry* parent, pid_t child_pid) {
        /* TODO: Maybe lock the parent */

        /* kprintf("process_has_child(%p, ...)\n", parent); */

        struct array* child_pids = &parent->pte_child_pids;

        for (unsigned i = 0; i < child_pids->num; ++i) {
                if ((pid_t)array_get(child_pids, i) == child_pid) {
                        return true;
                }
        }
        return false;
}


int
sys_fork(pid_t* retval, struct trapframe* trapframe) {

        /* Errors:
         * EMPROC  The current user already has too many processes.
         * ENPROC  There are already too many processes on the system.
         * ENOMEM  Sufficient virtual memory for the new process was not available.
         */

        lock_acquire(big_lock);

        /* Create child process with proc_create */
        struct proc* newproc = proc_create_runprogram(curproc->p_name);

        const pid_t parent_pid = curproc->p_pid;

        /* Find an unused pid for the child */
        for (pid_t pid = 1; ; ++pid) {

                if (pid >= __PID_MAX) {
                        lock_release(big_lock);
                        return ENPROC;
                }

                if (proc_table[pid] == NULL) {
                        /* TODO: Lock */
                        newproc->p_pid = pid;
                        proc_table[pid] = process_table_entry_create(pid, parent_pid);
                        break;
                }
        }

        /* Set the child's parent pid */
        proc_table[newproc->p_pid]->pte_parent_pid = parent_pid;

        /* kprintf("Fork %d -> %d\n", parent_pid, newproc->p_pid); */
        /* kprintf("   proc_table[%d]                 = %p\n", parent_pid, proc_table[parent_pid]); */
        /* kprintf("  &proc_table[%d]->pte_child_pids = %p\n", parent_pid, &proc_table[parent_pid]->pte_child_pids); */

        /* kprintf("Proc table entry of id %d is %p\n", curproc->pid, proc_table[curproc->pid]); */

        /* Add the child's pid to the parent's list of children */
        int error = array_add(&proc_table[curproc->p_pid]->pte_child_pids,
                              (void*)newproc->p_pid, NULL);
        if (error) {
                lock_release(big_lock);
                return error;
        }

        /* Copy the address space */
        as_copy(curproc->p_addrspace, &newproc->p_addrspace);

        /* Copy the file table */
        file_table_copy(&curproc->p_file_table, &newproc->p_file_table);

        /* TODO: Copy threads */

        /* TODO: Create kernel thread */

        /* Create a copy of the trapframe for the child */
        struct trapframe* tf_copy = kmalloc(sizeof(struct trapframe));
        memcpy(tf_copy, trapframe, sizeof(struct trapframe));

        /* Fork the child process */
        thread_fork("child", newproc, &enter_forked_process_wrapper, tf_copy, 0);

        *retval = newproc->p_pid;
        /* kprintf("   Woohoot!\n"); */

        lock_release(big_lock);

        return 0;
}

int
sys_getpid(pid_t* retval) {
        *retval = curproc->p_pid;
        return 0;
}

int
sys_waitpid(pid_t* retval, pid_t pid, int *status, int options) {

       /* Errors:
        * EINVAL  The options argument requested invalid or unsupported options.
        * ECHILD  The pid argument named a process that was not a child of the current process.
        * ESRCH   The pid argument named a nonexistent process.
        * EFAULT  The status argument was an invalid pointer.
        */

        (void) retval;
        (void) pid;
        (void) *status;
        (void) options;
        *status = 0;

        return 0;
}

void
sys__exit(int exitcode) {

        lock_acquire(big_lock);

        /* kprintf("begin exit\n"); */

        const pid_t curpid = curproc->p_pid;

        struct process_table_entry* p_table_entry = proc_table[curproc->p_pid];

        KASSERT(p_table_entry != NULL);

        struct array* child_pids = &p_table_entry->pte_child_pids;

        /* Clean up proc_table_entry of each child that has already exited */
        unsigned num_children = child_pids->num;
        for (unsigned i = 0; i < num_children; ++i) {

                pid_t child_pid = (pid_t)array_get(child_pids, i);

                struct process_table_entry* child_p_table_entry = proc_table[child_pid];

                /* child's proc table entry should exist until its parent exits */
                KASSERT(child_p_table_entry != NULL);

                /* this process should be the parent of its children */
                KASSERT(child_p_table_entry->pte_parent_pid == curpid);

                if (child_p_table_entry->pte_has_exited) {
                        process_table_entry_destroy(child_p_table_entry);
                        proc_table[child_pid] = NULL;
                }
        }

        const pid_t parent_pid = p_table_entry->pte_parent_pid;

        /* const struct proc_table_enry* parent_proc_table_entry =  */

        /* const bool parent_still_exists = process_has_child(proc_table[parent_pid], curpid); */

        /* const bool parent_has_exited = proc_table[parent_pid]->pte_has_exited; */

        if (proc_table[parent_pid] == NULL ||
            proc_table[parent_pid]->pte_has_exited ||
            !process_has_child(proc_table[parent_pid], curpid)) {


            /* !parent_still_exists || parent_has_exited) { */
                /* If parent has already exited, can destroy this entry */

                process_table_entry_destroy(p_table_entry);
                proc_table[curpid] = NULL;
        }
        else {
                /* If parent has not exited, cannot destroy this entry */

                p_table_entry->pte_has_exited = true;
                p_table_entry->pte_exit_status = exitcode;

                /* Signal that we have exited in case parent is presently waiting */
                /* cv_broadcast(p_table_entry->pte_waitpid_cv, p_table_entry->pte_lock); */
        }

        /* kprintf("   end exit\n"); */

        lock_release(big_lock);

        (void)exitcode;
        thread_exit();
}
