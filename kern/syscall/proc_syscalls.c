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
#include <proc_table.h>
#include <current.h>

#include <limits.h>
#include <kern/limits.h>

#include <kern/errno.h>
#include <kern/fcntl.h>

#include <synch.h>
#include <addrspace.h>

static
void
enter_forked_process_wrapper(void* data1, unsigned long data2) {
	(void)data2;
	enter_forked_process((struct trapframe*)data1);
}

/* ------------------------------------------------------------------------- */
/* System calls */
int sys_execv(const char *program, char **argv) {

	/*
	 * May return these errors:
	 * ENODEV       The device prefix of program did not exist.
	 * ENOTDIR      A non-final component of program was not a directory.
	 * ENOENT       program did not exist.
	 * EISDIR       program is a directory.
	 * ENOEXEC      program is not in a recognizable executable file format, was for the wrong platform, or contained invalid fields.
	 * ENOMEM       Insufficient virtual memory is available.
	 * E2BIG        The total size of the argument strings exceeeds ARG_MAX.
	 * EIO          A hard I/O error occurred.
	 * EFAULT       One of the arguments is an invalid pointer.
	 */

	int err = 0;

	/* deactivate address space since it will be in a volatile state, in
	 * DUMBVIM, this does nothing */
	struct addrspace * old_as = proc_getas();
	struct addrspace * new_as = NULL; /* null for now */
	as_deactivate();

	/* ---------------------------------------------------------------- */

	/*
	 * 1. Copy arguments (argv) into a kernel buffer, kargv, and program
	 * name into kprogram
	 */

	int argc;
	size_t kargv_size; 
	char ** kargv;
	err = copyinstr_array(argv, &kargv, &argc, &kargv_size, ARG_MAX);
	if (err) {
		/* no need deallocate kargv on copyinstr_array err  */
		as_activate();
		return err;
	}

	size_t got;
	char * kprogram = kmalloc(NAME_MAX);
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
	 * Switch to new address space
	 */

	proc_setas(new_as);
	as_activate();

	/*
	 * Load new executable
	 */

	/* Open the file. */
	struct vnode * v;
	err = vfs_open(kprogram, O_RDONLY, 0, &v);
	if (err) {
		goto err;
	}

	vaddr_t entrypoint;
	err = load_elf(v, &entrypoint);
	if (err) {
                vfs_close(v);
		goto err;
	}

        /* done with file */
        vfs_close(v);


	/*
	 * Define new stack region
	 */

	/* Define the user stack in the new address space */
	vaddr_t stackptr;
	err = as_define_stack(new_as, &stackptr);
	if (err) {
		goto err;
	}

	/*
	 * Copy arguments to new address space, properly arranging them.
	 * Arguments are pointers in registers into user space.
	 *         - a0: argc
	 *         - a1: char** pointer that points to a string array of length argc
	 * we chose to store argc and the program name on the stack, so first
	 * we need to make space on it. Since the stackpointer is subtract then
	 * store, it is currently at the highest address + 1 in the stack
	 * pointer area, so we just subtract the number of bytes read from userspace.
	 */

	stackptr -= kargv_size;

        /* copy pointers */
	err = copyout( kargv, (userptr_t) stackptr, sizeof(char*)*argc);
	if (err) {
		goto err;
	}

        /* TODO FIXME */
        /* copy arguments */
        /* for (int i = 0; i < argc; i++) { */
        /*         err = copyout( kargv[i], (userptr_t) stackptr + , ); */
        /*         if (err) { */
        /*                 goto err; */
        /*         } */
        /* } */


	/*
	 * Clean up old address space
	 */

	as_destroy(old_as); /* the point of no return...  */

	/*
	 * 8. Warp to user mode
	 */


	/* clean up before doing so */
	kfree(*kargv);
	kfree(kargv);
	kfree(kprogram);

	enter_new_process(argc, (userptr_t) stackptr /*userspace addr of argv*/,
			NULL /*userspace addr of environment*/,
			stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;

err:
	kfree(*kargv); 
	kfree(kargv);
	kfree(kprogram);

	/* clean up address space */
	proc_setas(old_as);
	as_destroy(new_as); /* DUMBVM handles the case where new_as is NULL */
	as_activate();
	return err;
}

int
sys_fork(pid_t* retval, struct trapframe* trapframe) {

	/* Errors:
	 * EMPROC  The current user already has too many processes.
	 * ENPROC  There are already too many processes on the system.
	 * ENOMEM  Sufficient virtual memory for the new process was not available.
	 */

        const pid_t curpid = curproc->p_pid;

        /* SYNCHRONIZATION SCHEME
         *
         * Must acquire the parent's lock to ensure that its proc_table
         * entry is not concurrently read (by wait or exit) as it is modified
         * here.
         *
         * It is not necessary to lock the child pid, because it is not yet
         * running concurrently (we have the only reference to it).
         */

        pid_lock_acquire(curpid);

        KASSERTM(proc_table_entry_exists(curpid), "pid %d", curpid);

	/* Find an unused pid for the child */
        const pid_t child_pid = reserve_pid(curpid);
        if (child_pid == INVALID_PID) {
                pid_lock_release(curpid);
                return ENPROC;
        }

        DEBUG(DB_PROC_TABLE, "fork %d -> %d\n", curpid, child_pid);

        /* Create child process with proc_create */
        struct proc* child_proc = proc_create(curproc->p_name, child_pid);

        /* Add the child's pid to the parent's list of children */
        int error = proc_add_child(curpid, child_pid);
        if (error) {
                pid_lock_release(curpid);
                return error;
        }

        /* Copy the address space */
        as_copy(curproc->p_addrspace, &child_proc->p_addrspace);

        /* Copy the file table */
        file_table_copy(&curproc->p_file_table, &child_proc->p_file_table);

	/* TODO: Copy threads */

	/* TODO: Create kernel thread */

	/* Create a copy of the trapframe for the child */
	struct trapframe* tf_copy = kmalloc(sizeof(struct trapframe));
	memcpy(tf_copy, trapframe, sizeof(struct trapframe));

        /* Fork the child process */
        thread_fork("child", child_proc, &enter_forked_process_wrapper, tf_copy, 0);

        *retval = child_proc->p_pid;

        pid_lock_release(curpid);

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

        if (retval != NULL) {
                *retval = pid;
        }
        if (status != NULL) {
                *status = 0;
        }

        /* TODO: use copy_in/copy_out */
        /* if (status is invalid) { */
        /*         return EFAULT; */
        /* } */

        if (options != 0) {
                return EINVAL;
        }

        if (!proc_table_entry_exists(pid)) {
                return ESRCH;
        }

        pid_t curpid = curproc->p_pid;


        if (!proc_has_child(curpid, pid)) {
                pid_lock_release(pid);
                return ECHILD;
        }

        DEBUG(DB_PROC_TABLE, "wait %d on %d\n", curpid, pid);

        /* SYNCHRONIZATION SCHEME
         *
         * Must acquire the lock of the pid we are waiting on, to prevent
         * it from exiting concurrently while we are running wait on it.
         * The lock is released when we wait on its CV, and acquired again
         * when it broadcasts on the CV that is has exited.
         *
         * Must NOT acquire the lock of curpid, because doing so prevents the
         * child process from exiting, because the child process acquires its
         * parent's lock in exit. The child process acquires its parent's lock
         * in exit to ensure that its parent's proc_table entry is not modified
         * (by fork or exit) while the child is reading from it.
         */

        pid_lock_acquire(pid);

        const int exit_status = proc_wait_on_pid(pid);
        if (status != NULL) {
                *status = exit_status;
        }

        pid_lock_release(pid);

        DEBUG(DB_PROC_TABLE, "done wait %d on %d\n", curpid, pid);

	return 0;
}

void
sys__exit(int exitcode) {

	const pid_t curpid = curproc->p_pid;

        DEBUG(DB_PROC_TABLE, "exit %d\n", curpid);

	KASSERTM(proc_table_entry_exists(curpid), "pid %d", curpid);

        const pid_t parent_pid = proc_get_parent(curpid);

        /* SYNCHRONIZATION SCHEME
         *
         * Must acquire the parent's lock to ensure that its proc_table
         * entry is not modified (by fork or exit) as the current exiting
         * process reads from it.
         *
         * Must acquire the current process's lock to prevent other processes
         * from reading from it (in wait or exit) while we are writing to it.
         *
         * Must acquire the lock of each of the current process's children
         * within a narrow scope while removing entries of children that have
         * already exited, again to prevent concurrent access.
         *
         * By acquiring parent locks before child locks, we can use the
         * parent-child relationships to avoid deadlocks, because
         * the parent-child relationships from a tree (a graph without cycles)
         */

        if (parent_pid != INVALID_PID) {
                pid_lock_acquire(parent_pid);
        }
        pid_lock_acquire(curpid);

        struct array* child_pids = proc_get_children(curpid);

	/* Clean up proc_table_entry of each child that has already exited */
	unsigned num_children = child_pids->num;
	for (unsigned i = 0; i < num_children; ++i) {

		pid_t child_pid = (pid_t)array_get(child_pids, i);

                DEBUG(DB_PROC_TABLE, "%d examining child %d\n", curpid, child_pid);

                pid_lock_acquire(child_pid);

		/* child's proc table entry should exist until its parent exits */
                KASSERTM(proc_table_entry_exists(child_pid), "pid %d", child_pid);

		/* this process should be the parent of its children */
                KASSERTM(proc_get_parent(child_pid) == curpid, "pid %d", child_pid);

                if (proc_has_exited(child_pid)) {

                        DEBUG(DB_PROC_TABLE, "remove_proc_table_entry(%d) - child\n", child_pid);
                        remove_proc_table_entry(child_pid);
                }

                pid_lock_release(child_pid);
        }

        proc_exit(curpid, exitcode);

        enum parent_status {
                ps_invalid,
                ps_no_entry,
                ps_has_exited,
                ps_pid_recycled,
                ps_alive,
                ps_count
        };

        enum parent_status pstatus =
                parent_pid == INVALID_PID               ? ps_invalid :
                !proc_table_entry_exists(parent_pid)    ? ps_no_entry :
                proc_has_exited(parent_pid)             ? ps_has_exited :
                !proc_has_child(parent_pid, curpid)     ? ps_pid_recycled :
                                                          ps_alive;

        if (pstatus != ps_alive) {

                /* Destroy this entry; there is no living parent that may call waitpid */
		remove_proc_table_entry(curpid);

                const char* db_format[ps_count] = {
                        "remove_proc_table_entry(%d), parent %d is invalid\n",
                        "remove_proc_table_entry(%d), parent %d has no entry\n",
                        "remove_proc_table_entry(%d), parent %d has exited\n",
                        "remove_proc_table_entry(%d), parent %d has been recycled\n",
                        NULL
                };

                DEBUG(DB_PROC_TABLE, db_format[pstatus], curpid, parent_pid);
        }

        pid_lock_release(curpid);
        if (parent_pid != INVALID_PID) {
                pid_lock_release(parent_pid);
        }

        /* Print in reverse (easy way to tell where the call began and ended) */
        DEBUG(DB_PROC_TABLE, "%d exit\n", curpid);

        thread_exit_destroy_proc();
}
