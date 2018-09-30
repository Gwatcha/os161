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

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
        struct semaphore *sem;

        sem = kmalloc(sizeof(struct semaphore));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.


/*
 * lock_create::
 *   creates a new lock with the specified name and returns it.
 *     Inputs:         name - name for the lock
 *     Output:         struct lock if success, else, NULL
 *     Preconditions:  (Advised) no lock with same name
 */
struct lock *
lock_create(const char *name)
{

    /*
     * A lock struct has a wchan * lk_wchan, char * name, spinlock lk_spinlock,
     * thread *lk_holder, and volatile unsigned lk_free to initialize. 
     */

    struct lock *lock;

    lock = kmalloc(sizeof(struct lock));
    if (lock == NULL) {
        return NULL;
    }

    lock->lk_name = kstrdup(name);
    if (lock->lk_name == NULL) {
        kfree(lock);
        return NULL;
    }

    lock->lk_wchan = wchan_create(lock->lk_name);
    if (lock->lk_wchan == NULL) {
        kfree(lock->lk_name);
        kfree(lock);
        return NULL;
    }

    spinlock_init(&lock->lk_spinlock);

    //Lock holder is set to NULL since no thread holds the lock yet.
    lock->lk_holder = NULL;

    //Lock starts free.
    lock->lk_free = 1;

    return lock;
}

/*
 * lock_destroy::
 *   destroys specified lock, freeing all memory.
 *     Inputs:         lock - lock to destroy
 *     Preconditions:  lock != NULL, lock is not held, and no thread is waiting to
 *                     acquire lock.
 *     Postconditions: the lock is destroyed.
 */
void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&lock->lk_spinlock);
	wchan_destroy(lock->lk_wchan);

        kfree(lock->lk_name);
        kfree(lock);
}

/*
 * lock_acquire::
 *   acquires the lock specified by sleeping until the lock is free then taking the lock.
 *     Inputs:         lock - the lock to acquire, not ==NULL,
 *     Preconditions:  If the lock is not free, the lock must eventually be
 *                     released for this function to ever return. The lock must
 *                     have been created by a previous lock_create() function as
 *                     well.
 *     Postconditions: The lock is acquired and no other thread has it.
 */
void
lock_acquire(struct lock *lock)
{
        /*
         * May not block in an interrupt handler.
         * Semaphores do this so locks do it as well.
         */
        KASSERT(curthread->t_in_interrupt == false);

	/* Use the lock spinlock to protect the wchan. */
	spinlock_acquire(&lock->lk_spinlock);
        while (lock->lk_free == 0) {
            wchan_sleep(lock->lk_wchan, &lock->lk_spinlock);
        }
        lock->lk_free = 0; //Take lock
        KASSERT(lock->lk_free == 0);

        //Set new lock holder to this thread.
        lock->lk_holder = curthread; //Both these vars are pointers to thread structs.
	spinlock_release(&lock->lk_spinlock);
}

/*
 * lock_release::
 *   Atomically releases the lock 'lock', making it available for one random
 *   thread which is waiting on it in 'thread_acquire()'
 *     Inputs:         lock - the lock to release
 *     Preconditions:  The lock must be held by the this thread 
 *     Postconditions: The lock is no longer in posession by the this thread
 */
void
lock_release(struct lock *lock)
{
        //Acquire spinlock, set lock to free, wakeone, release spinlock.
        KASSERT(lock_do_i_hold(lock));
	spinlock_acquire(&lock->lk_spinlock);
        lock->lk_free = 1;
        KASSERT(lock->lk_free == 1);
	wchan_wakeone(lock->lk_wchan, &lock->lk_spinlock);
        KASSERT(lock->lk_free == 1);
	spinlock_release(&lock->lk_spinlock);
}

/*
 * lock_do_i_hold::
 *   checks whether this thread currently holds the lock specified,
 *     Inputs:         lock - the lock 
 *     Output:         true if this thread has the lock, false otherwise
 *     Preconditions:  lock != NULL
 */
bool
lock_do_i_hold(struct lock *lock)
{
        //Check if it is this thread holding this lock.
        if (lock->lk_free == 0 && lock->lk_holder == curthread)
            return true;
        
        return false;
}

////////////////////////////////////////////////////////////
// CV

/*
* cv_create::
*   Calling cv_create creates the condition variable cv, allocating memory whilst doing so.
*       Inputs:         name - the name of the new condition variable
*       Returns:        new cv, if an error occured, returns NULL.
*       Precondition:   (Advised) there is no such cv with the same name.
*/
struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(struct cv));
        if (cv == NULL) {
            return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
            kfree(cv);
            return NULL;
        }

        //Create wait channel.
        cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

        return cv;
}

/*
* cv_destroy::
*   Calling cv_destroy destroys the condition variable cv, freeing all allocated
*   memory in the process.
*       Inputs:         cv - the condition variable to destroy, != NULL.
*       Precondition:   No threads are waiting on the cv or depend or will
*                       depend  on it's existence.
*       Postcondition:  cv is free'd.
*/
void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

	wchan_destroy(cv->cv_wchan); // Will assert if thread is waiting on cv.
        kfree(cv->cv_name);
        kfree(cv);
}

/*
* cv_wait::
*   Calling cv_wait enqueues the current thread on c (suspending its
*   execution), and unlocks m, as a single atomic action. Before returning, 
*   it acquires the lock passed again.
*       Inputs:         cv - the condition variable to wait on
*                       lock - the associated lock for cv.
*       Precondition:   lock must be held before calling.
*       Postcondition:  The thread has been signaled to wake up by another
*                       thread and is running again. It also holds lock.
*/
void 
cv_wait(struct cv *cv, struct lock *lock)
{

    //Assert lock is held already.
    KASSERT(lock_do_i_hold(lock));

    //Pre-emptively set the lock to be free so that when we go to sleep on the
    //cv_wchan, the spinlock for the lock is released and it becomes free.
    spinlock_acquire(&lock->lk_spinlock);
    lock->lk_free = 1;
    wchan_wakeone(lock->lk_wchan, &lock->lk_spinlock);

    //Enqueue (put to sleep) the current thread on cv's wait channel,
    wchan_sleep(cv->cv_wchan, &lock->lk_spinlock);

    //zzzzzz...

    /* At this point, we have been awaken by either cv_signal or cv_broadcast
    and the spinlock for the lock is held once again. */
    spinlock_release(&lock->lk_spinlock); //We don't need to hold the spinlock.
    lock_acquire(lock); 
    KASSERT(lock_do_i_hold(lock));
}

/*
* cv_signal::
*   Examines cv and if there are any threads enqueued on c then *one* thread
*   is allowed to resume execution. This entire operation is a single atomic
*   action.
*       Inputs:        cv - the condition variable to signal on.
*                      lock - the associated lock for cv.
*       Precondition:  lock must be held before calling.
*       Postcondition: If there was at least on thread waiting on cv, one
*                      thread is awoken
*/
void
cv_signal(struct cv *cv, struct lock *lock)
{

    KASSERT(lock_do_i_hold(lock));

    /* Wake one person waiting on the cv wait channel.  Note that the lock spin
    lock protects the wait channel for the cv. */
    spinlock_acquire(&lock->lk_spinlock);
    wchan_wakeone(cv->cv_wchan, &lock->lk_spinlock);
    spinlock_release(&lock->lk_spinlock);
}


/*
* cv_broadcast::
*   Examines cv and if there are any threads enqueued on c then all such threads
*   are allowed to resume execution. This entire operation is a single atomic
*   action.
*       Inputs: cv - the condition variable to broadcast on.
*               lock - the associated lock for cv.
*       Precondition: lock must be held before calling.
*       Postcondition: all threads waiting in cv are woken up.
*/
void
cv_broadcast(struct cv *cv, struct lock *lock)
{
    KASSERT(lock_do_i_hold(lock));

    /* Wake all waiting on the cv wait channel. Just as for cv_signal, the
    spinlock associated with lock is used to protect the wait channel for
    cv.  */
    spinlock_acquire(&lock->lk_spinlock);
    wchan_wakeall(cv->cv_wchan, &lock->lk_spinlock);
    spinlock_release(&lock->lk_spinlock);
}
