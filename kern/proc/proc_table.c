
#include "proc_table.h"

#include "array.h"
#include "limits.h"
#include "synch.h"

struct proc_table_entry {
	struct cv* pte_waitpid_cv;
	struct array pte_child_pids;
	pid_t pte_parent_pid;
	bool pte_has_exited;
	int pte_exit_status;
};



typedef struct proc_table_entry* proc_table[__PID_MAX];

static proc_table p_table = { NULL };

static struct lock* pid_locks[__PID_MAX] = { NULL };

static
struct proc_table_entry*
proc_table_entry_create(pid_t pid, const pid_t parent_pid /* may be PID_INVALID */) {

	struct proc_table_entry* pte = kmalloc(sizeof(struct proc_table_entry));

        {
                char buf[64];
                snprintf(buf, sizeof(buf), "waitpid_cv_%d", pid);
                pte->pte_waitpid_cv = cv_create(buf);
        }


	array_init(&pte->pte_child_pids);

	pte->pte_parent_pid = parent_pid;
	pte->pte_has_exited = false;
	pte->pte_exit_status = 0;

	return pte;
}

static
void
proc_table_entry_destroy(struct proc_table_entry* pte) {
	cv_destroy(pte->pte_waitpid_cv);

	array_setsize(&pte->pte_child_pids, 0);
	array_cleanup(&pte->pte_child_pids);

	kfree(pte);
}

void
proc_table_init() {

	p_table[PID_KERN] = proc_table_entry_create(PID_KERN, PID_INVALID);

        for (pid_t i = 0; i < __PID_MAX; ++i) {
                char buf[64];
                snprintf(buf, sizeof(buf), "pid_lock_%d", i);
                pid_locks[i] = lock_create(buf);
        }
}

void
pid_lock_acquire(pid_t pid) {
        KASSERTM(!pid_lock_do_i_hold(pid), "pid %d", pid);

        DEBUG(DB_PROC_TABLE, "acquiring pid lock %d\n", pid);
        lock_acquire(pid_locks[pid]);
        DEBUG(DB_PROC_TABLE, "acquired pid lock %d\n", pid);
}

void
pid_lock_release(pid_t pid) {
        KASSERTM(pid_lock_do_i_hold(pid), "pid %d", pid);
        lock_release(pid_locks[pid]);
        DEBUG(DB_PROC_TABLE, "released pid lock %d\n", pid);
}

bool
pid_lock_do_i_hold(pid_t pid) {
        return lock_do_i_hold(pid_locks[pid]);
}

bool
proc_table_entry_exists(pid_t pid) {
        return pid > 0 && p_table[pid] != NULL;
}

void remove_proc_table_entry(pid_t pid) {
        KASSERTM(pid_lock_do_i_hold(pid), "pid %d", pid);
        KASSERTM(proc_table_entry_exists(pid), "pid %d", pid);
        proc_table_entry_destroy(p_table[pid]);
        p_table[pid] = NULL;
}

bool
proc_has_child(pid_t parent, pid_t child) {

        const struct array* child_pids = &p_table[parent]->pte_child_pids;

	for (unsigned i = 0; i < child_pids->num; ++i) {
		if ((pid_t)array_get(child_pids, i) == child) {
			return true;
		}
	}
	return false;
}

static
int
proc_add_child(pid_t parent, pid_t child) {
        return array_add(&p_table[parent]->pte_child_pids, (void*)child, NULL);
}

const struct array*
proc_get_children(pid_t pid) {
        return &p_table[pid]->pte_child_pids;
}

/* Returns PID_INVALID if the process does not have a parent */
pid_t proc_get_parent(pid_t pid) {
        return p_table[pid]->pte_parent_pid;
}

/* Returns the exit status of the process */
int proc_wait_on_pid(pid_t pid) {

        if (!proc_has_exited(pid)) {
                cv_wait(p_table[pid]->pte_waitpid_cv, pid_locks[pid]);
        }

        int exit_status = p_table[pid]->pte_exit_status;

        DEBUG(DB_PROC_TABLE,
              "remove_proc_table_entry(%d), parent has collected exit status\n", pid);

        remove_proc_table_entry(pid);
        return exit_status;
}

void proc_exit(pid_t pid, int status) {
        struct proc_table_entry* entry = p_table[pid];
        entry->pte_has_exited = true;
        entry->pte_exit_status = status;
        cv_broadcast(entry->pte_waitpid_cv, pid_locks[pid]);
}
bool proc_has_exited(pid_t pid) {
        return p_table[pid]->pte_has_exited;
}

/*
 * Reserves a new pid and adds it to parent_pid's children (if parent_pid is valid)
 *
 * Returns PID_INVALID if a pid cannot be reserved
 *
 * parent_pid may be invalid.
 */
pid_t
reserve_pid(pid_t parent_pid) {

        if (parent_pid != PID_INVALID) {

                /* We add the child pid to its children, so it must be locked */
                KASSERT(pid_lock_do_i_hold(parent_pid));
        }

	for (pid_t pid = PID_MIN; pid < __PID_MAX; ++pid) {

                if (pid == parent_pid) {
                        /* When called from sys_fork, we lock the parent before
                         * entering this function, so we must skip the parent to
                         * avoid potentially deadlocking on the parent's lock
                         */
                        continue;
                }

                if (p_table[pid] == NULL) {
                        pid_lock_acquire(pid);
                        if (p_table[pid] == NULL) {

                                p_table[pid] = proc_table_entry_create(pid, parent_pid);

                                if (parent_pid != PID_INVALID) {

                                        proc_add_child(parent_pid, pid);
                                }

                                pid_lock_release(pid);
                                return pid;
                        }
                        pid_lock_release(pid);
                }
        }
        return PID_INVALID;
}


