
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
	/* int pte_refcount; */
};



typedef struct proc_table_entry* proc_table[__PID_MAX];

static proc_table p_table = { NULL };

static struct lock* pid_locks[__PID_MAX];

static
struct proc_table_entry*
proc_table_entry_create(pid_t pid, const pid_t* parent_pid) {

	struct proc_table_entry* pte = kmalloc(sizeof(struct proc_table_entry));

	/* TODO: Better name for the CV */
	(void)pid;
	pte->pte_waitpid_cv = cv_create("");

	array_init(&pte->pte_child_pids);

	pte->pte_parent_pid = parent_pid != NULL ? *parent_pid : -1;
	pte->pte_has_exited = false;

	return pte;
}

static
void
proc_table_entry_destroy(struct proc_table_entry* pte) {
	cv_destroy(pte->pte_waitpid_cv); /* FIXME: kpanic!  */

	array_setsize(&pte->pte_child_pids, 0);
	array_cleanup(&pte->pte_child_pids);

	kfree(pte);
}

void
proc_table_init() {
        /* TEMP HACK */
	p_table[1] = proc_table_entry_create(1, 0);

        for (pid_t i = 0; i < __PID_MAX; ++i) {
                char buf[64];
                snprintf(buf, sizeof(buf), "pid_lock_%d", i);
                pid_locks[i] = lock_create(buf);
        }
}

void
pid_lock_acquire(pid_t pid) {
        lock_acquire(pid_locks[pid]);
}

void
pid_lock_release(pid_t pid) {
        lock_release(pid_locks[pid]);
}

bool
proc_table_entry_exists(pid_t pid) {
        return p_table[pid] != NULL;
}

void remove_proc_table_entry(pid_t pid) {
        KASSERT(proc_table_entry_exists(pid));
        proc_table_entry_destroy(p_table[pid]);
        p_table[pid] = NULL;
}

bool
proc_has_child(pid_t parent_pid, pid_t child_pid) {
	/* TODO: Maybe lock the parent */

        const struct array* child_pids = &p_table[parent_pid]->pte_child_pids;

	for (unsigned i = 0; i < child_pids->num; ++i) {
		if ((pid_t)array_get(child_pids, i) == child_pid) {
			return true;
		}
	}
	return false;
}

/* Returns INVALID_PID if a pid cannot be reserved */
pid_t
reserve_pid(const pid_t* parent_pid /* may be NULL */) {
	for (pid_t pid = 1; pid < __PID_MAX; ++pid) {

                if (parent_pid != NULL && pid == *parent_pid) {
                        /* Avoid deadlocking on our own lock */
                        continue;
                }

                if (p_table[pid] == NULL) {
                        lock_acquire(pid_locks[pid]);
                        if (p_table[pid] == NULL) {

                                /* child_proc->p_pid = pid; */
                                p_table[pid] = proc_table_entry_create(pid, parent_pid);
                                lock_release(pid_locks[pid]);
                                return pid;
                        }
                        lock_release(pid_locks[pid]);
                }
        }
        return INVALID_PID;
}


