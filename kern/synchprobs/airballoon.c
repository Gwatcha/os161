#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define NROPES 16


/*
 * I use two structures for this problem, one is a map of stakes to ropes,
 * and another is simply an array of ropes which indicates whether a rope
 * is severed or not. Both structures are locked before read or write.
 * Dandelion and balloon only utilize the rope struct whereas Marigold and
 * FlowerKill utilize both the rope struct and the map struct.
 *
 *  For Marigold and LordFK, it is an invariant that first the map lock is
 *  acquired and then the ropes lock is acquires.
 */


/*
 * Passed to threads which require both the map and ropes struct
 */
struct access {
    struct map *map;
    struct ropes *ropes;
};

/*
 * This structure contains the status of all ropes (severed or not), it is
 * shared to all threads. To change or read, it is an invariant that lock is
 * acquired.
 *
 * To remove a rope write -1 into the rope index in the ropes array. 
 *
 * This is also used to synchronize finishing for the main function. This is
 * done through the 'int done' variable.
 */
struct ropes {
    struct lock *lock;
    volatile int ropes_left;
    volatile int ropes[NROPES];  /* The rope array is indexed by room number
                                    and has value 1:Not Severed or 0:Severed */

    volatile int done; /* This is used as a subtitute for a thread_join() for
                          main. It starts at 0 and if it equals 4, it means all
                          threads have written that they are done (by adding
                          1). Threads write 1 right before releasing the lock
                          and exitting */
};

/*
 * This data structure is shared only among Marigold and Lord FlowerKiller.  It is
 * an invariant that is locked before read or updated.  This structure contains an
 * array which is indexed by stake number and contains the rope number the stake
 * is associated with. 

 * To attach a rope to a different stake, acquire the lock for map, and
 * swap the values of two indices. 
 */
struct map {
    struct lock *lock;
    volatile int stakes[NROPES];  
};


/*
 * Dandelion:
     * 1. searches through her own hook:rope_num array and finds the rope_num
     *    of the next rope still attached 
     * 2. writes -1 into her own hook:rope_num array at rope_num
     * 3. acquires the lock on struct ropes
     * 4. if the rope at ropes[rope_num] is not cut, cut it by writing -1, and decrementing ropes_left
     *    if the rope at ropes[rope_num] is cut, 
     * 5. if (ropes_left == 0), add 1 to done and exit
     * 6. repeat
 */
static
void
dandelion(void *p, unsigned long n)
{
    struct ropes *ropes =  p;
    (void) n;

    kprintf("Dandelion thread starting\n");

    /* populate hooks:rope_num array */
    int hooks[NROPES];
    for (int i = 0; i < NROPES; i++)
        hooks[i] = i;

    /* terminates when ropes_left == 0 */
    while (1) {
        /* find rope */
        int chosen_rope = -1;
        for (int i = 0; i < NROPES; i++) {
            if ( hooks[i] != -1 ) {
                chosen_rope = hooks[i];
                hooks[i] = -1; /* dandelion either severs the rope or finds it
                                  severed so set this to -1, indicating the
                                  rope in this hook is severed */
                break;
            }
        }

        lock_acquire(ropes->lock);
        /* termination case */
        if (chosen_rope == -1 || ropes->ropes_left == 0)
            goto dandelion_done;

        /*  sever our chosen rope (if it is still not cut). */
        if (ropes->ropes[chosen_rope] != -1) {
            /* Dandelion severes a rope */
            kprintf("Dandelion severed rope %d\n", chosen_rope);
            ropes->ropes[chosen_rope] = -1;
            ropes->ropes_left--;
        }

        /* check termination case again */
        if (ropes->ropes_left == 0)
            goto dandelion_done;

        lock_release(ropes->lock);
    }

dandelion_done:
    /* indicate we are done and leave */
    ropes->done++;
    lock_release(ropes->lock);
    kprintf("Dandelion thread done\n");
}

/*
 * Marigold:
 *     1. chooses random stake x
 *     2. acquires lock for map and checks which rope it is connected to
 *     3. acquires lock for ropes and severs rope x if it is not already severed and decrements ropes_left
 *     4. checks if ropes_left == 0
 *         if so, break out of loop, increment done, release locks and exit
 *         else, repeat after releasing locks
 */
static
void
marigold(void *p, unsigned long n)
{
	
    kprintf("Marigold thread starting\n");

    struct access *access =  p;
    (void) n;

    /* Loop until ropes_left == 0 */
    while (1) {
        lock_acquire(access->map->lock);
        lock_acquire(access->ropes->lock);

        /* Find the next stake with a rope attached to it and sever the rope. */
        for (int i = 0; i < NROPES; i++) {
            if (access->map->stakes[i] != -1) {
                /* We found a rope! Now just consult the ropes struct*/
                if ( access->ropes->ropes[access->map->stakes[i]] == 1 ) { /* if rope stakes[i] exists, cut it */
                    /* Marigold severes a rope */
                    kprintf("Marigold severed rope %d from stake %d\n", access->map->stakes[i], i);
                    /* Remove it from the map as well as the list of ropes */
                    access->ropes->ropes[access->map->stakes[i]] = -1;
                    access->ropes->ropes_left--;
                }

                access->map->stakes[i] = -1;
                break;
            }
        }

        /* Check termination case */
        if (access->ropes->ropes_left == 0)
            break;

        lock_release(access->ropes->lock);
        lock_release(access->map->lock);
    }

    access->ropes->done++;
    lock_release(access->ropes->lock);
    lock_release(access->map->lock);

    /* Marigold exits */
    kprintf("Marigold thread done\n");
}

/*
 * Flowerkiller:
 *     1. acquires map lock
 *     2. checks termination case
 *         exits if end, updating done and relasing lock
 *         else:
 *     3. switches a value around between two indexes (first one must have rope)
 *     4. releases lock
 *     5. repeats
 */
static
void
flowerkiller(void *p, unsigned long n)
{
    struct access *access =  p;
    (void) n;

    kprintf("Lord FlowerKiller thread starting\n");

    /* Loop until ropes_left == 0 */
    while (1) {
        /* acquire map lock */
        lock_acquire(access->map->lock);
        /* check termination case through acquiring ropes lock */
        lock_acquire(access->ropes->lock);
        if (access->ropes->ropes_left == 0)
            break;

        /* Randomly find two available stakes, stake one must have a rope attached to it. */
        int i1 = random() % NROPES;
        while (access->map->stakes[i1] == -1)
            i1 = random() % NROPES;

        int i2 = random() % NROPES;
        while (i2 == i1)
            i2 = random() % NROPES;

        /* Switch them around */
        kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", access->map->stakes[i1], i1, i2);
        int rope_1 = access->map->stakes[i1];
        access->map->stakes[i1] = access->map->stakes[i2];
        access->map->stakes[i2] = rope_1;

        /* release both locks before looping again */
        lock_release(access->ropes->lock);
        lock_release(access->map->lock);
    }


    /* Indicate to main that we are done */
    access->ropes->done++;
    lock_release(access->ropes->lock);
    lock_release(access->map->lock);

    /* Flower killer leaves */
    kprintf("Lord FlowerKiller thread done\n");
}

/*
 * baloon simply waits until ropes_left is equal to 0, if it does, it prints it's
 * message and exits
 */
static
void
balloon(void *p, unsigned long n)
{
        struct ropes *ropes = p;
        (void) n;
	kprintf("Balloon thread starting\n");
        
retry: 
        /* Check if all ropes severed */
        lock_acquire(ropes->lock);
        if (ropes->ropes_left != 0) {
            lock_release(ropes->lock);
            goto retry;
        }

        kprintf("Balloon freed and Prince Dandelion escapes\n");
        ropes->done++;

        /* balloon exits */
        kprintf("Balloon thread done\n");
        lock_release(ropes->lock);
}


int
airballoon(int nargs, char **args)
{
	int err = 0;

	(void)nargs;
	(void)args;
	
        /* Initialize the ropes struct */
        struct ropes *ropes;
        ropes = kmalloc(sizeof(struct ropes));
        ropes->lock = lock_create("ropes lock");
        ropes->done = 0;
        ropes->ropes_left = NROPES;

        for (int i = 0; i < NROPES; i++)
            ropes->ropes[i] = 1; /* 1 indicates rope exists */
        
        /* Initialize the map struct */
        struct map *map;
        map = kmalloc(sizeof(struct map));
        map->lock = lock_create("map lock");

        for (int i = 0; i < NROPES; i++) {
            /* stakes and ropes start off 1:1. */
            map->stakes[i] = i;   
        }

        /* Initialze access struct */
        struct access *access;
        access = kmalloc(sizeof(struct access));
        access->map = map;
        access->ropes = ropes;


        /* Initialize threads */
	err = thread_fork("Marigold Thread",
			  NULL, marigold, access, 0);
	if(err)
		goto panic;
	
	err = thread_fork("Dandelion Thread",
			  NULL, dandelion, ropes, 0);
	if(err)
		goto panic;
	
	err = thread_fork("Lord FlowerKiller Thread",
			  NULL, flowerkiller, access, 0);
	if(err)
		goto panic;

	err = thread_fork("Air Balloon",
			  NULL, balloon, ropes, 0);
	if(err)
		goto panic;
	
        /* Wait for all threads */
        while(1) {
            lock_acquire(ropes->lock);
            if (ropes->done == 4) {
                lock_release(ropes->lock);
                goto done;
            } 
            lock_release(ropes->lock);
        }

panic:
	panic("airballoon: thread_fork failed: %s)\n",
	      strerror(err));
done:

        /* All theads done at this point */

        /* Free all memory allocated */
        lock_destroy(ropes->lock);
        lock_destroy(map->lock);
        kfree(map);
        kfree(ropes);
        kfree(access);
        kprintf("Main thread done\n");
	return 0;
}
