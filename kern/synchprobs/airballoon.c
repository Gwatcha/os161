/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define NROPES 16

/*
 *     The structure I used for the  problem is a 2D array used as a map.  The
 *     number of rows correspond to NROPES, Col 0 is for stakes, Col. 1 is for
 *     hooks, the mapping represents a rope.
 * 
 *     To remove a rope, acquire the lock, search for the row associated with
 *     a particular stake or rope, write -1 in both columns, decrement ropes_left,
 *     and release the lock.
 * 
 *     To attach a rope to a different stake, acquire the lock, find the row
 *     indices of the two stakes and switch the entries in Col 0, and release the
 *     lock.
 */
struct map {
    struct lock *lock;
    volatile int ropes_left;
    volatile int ropes[NROPES][2];  /* Needs to be initialized */
    volatile int done; /* This is my subtitute for a thread_join() function, if
                          it equals 4, it means all threads have written that
                          they are done (by adding 1). */
};



/*
 * Describe your design and any invariants or locking protocols 
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?  
 */

static
void
dandelion(void *p, unsigned long n)
{
    struct map *map =  p;
    (void) n;

    kprintf("Dandelion thread starting\n");

    while (1) {
        lock_acquire(map->lock);

        /* Termination case  */
        if (map->ropes_left == 0)
            break;

        /* Find the next available hook (rope) and sever it. */
        for (int i = 0; i < NROPES; i++) {
            if (map->ropes[i][1] != -1) {
                /* Dandelion severes a rope */
                kprintf("Dandelion severed rope %d\n", map->ropes[i][1]);
                map->ropes[i][1] = -1;
                map->ropes[i][0] = -1;
                map->ropes_left--;
                break;
            }
        }

        if (map->ropes_left == 0)
            break;

        lock_release(map->lock);

        /* wait a bit */
        for (int w = 0; w < 500; w++);
    }

    map->done++;
    lock_release(map->lock);

    /* Dandelion exits */
    kprintf("Dandelion thread done\n");
}

static
void
marigold(void *p, unsigned long n)
{
	
    kprintf("Marigold thread starting\n");

    struct map *map =  p;
    (void) n;

    while (1) {
        lock_acquire(map->lock);

        /* Termination case  */
        if (map->ropes_left == 0)
            break;

        /* Find the next available stake and unhook the rope. */
        for (int i = 0; i < NROPES; i++) {
            if (map->ropes[i][0] != -1) {
                /* Marigold severes a rope */
                kprintf("Marigold severed rope %d from stake %d\n", map->ropes[i][1], map->ropes[i][0]);
                map->ropes[i][1] = -1;
                map->ropes[i][0] = -1;
                map->ropes_left--;
                break;
            }
        }

        if (map->ropes_left == 0)
            break;

        lock_release(map->lock);

        /* wait a bit */
        for (int w = 0; w < 500; w++);
    }

    map->done++;
    lock_release(map->lock);

    /* Marigold exits */
    kprintf("Marigold thread done\n");
}

static
void
flowerkiller(void *p, unsigned long n)
{
    struct map *map =  p;
    (void) n;

    kprintf("Lord FlowerKiller thread starting\n");

    while (1) {
        lock_acquire(map->lock);

        /* Termination case  */
        if (map->ropes_left < 2)
            break;

        /* Randomly find two available stakes. */
        int i1 = random() % NROPES;
        while (map->ropes[i1][0] == -1)
            i1 = random() % NROPES;

        int i2 = random() % NROPES;
        while (map->ropes[i2][0] == -1 || i2 == i1)
            i2 = random() % NROPES;

        /* Switch them around */

        kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", map->ropes[i1][1], map->ropes[i1][0], map->ropes[i2][0]);
        int rope_1 = map->ropes[i1][1];
        map->ropes[i1][1] = map->ropes[i2][1];
        map->ropes[i2][1] = rope_1;

        lock_release(map->lock);

        /* wait a bit */
        for (int w = 0; w < 500; w++);
    }

    map->done++;
    lock_release(map->lock);

    /* Flower killer leaves */
    kprintf("Lord FlowerKiller thread done\n");
}

static
void
balloon(void *p, unsigned long n)
{
        struct map *map =  p;
        (void) n;
	kprintf("Balloon thread starting\n");
        
retry: 
        /* Check if all ropes severed */
        lock_acquire(map->lock);
        if (map->ropes_left != 0) {
            lock_release(map->lock);
            goto retry;
        }

        kprintf("Balloon freed and Prince Dandelion escapes\n");
        map->done++;

        /* balloon exits */
        kprintf("Balloon thread done\n");
        lock_release(map->lock);
}


// Change this function as necessary
int
airballoon(int nargs, char **args)
{
	int err = 0;

	(void)nargs;
	(void)args;
	
        /* Initialize the map struct */
        struct map *map;
        map = kmalloc(sizeof(struct map));
        map->lock = lock_create("balloon lock");
        map->done = 0;
        map->ropes_left = NROPES;

        for (int i = 0; i < NROPES; i++) {
            /* Hooks and stakes start off 1:1. */
            map->ropes[i][0] = i;   
            map->ropes[i][1] = i;   
        }

	err = thread_fork("Marigold Thread",
			  NULL, marigold, map, 0);
	if(err)
		goto panic;
	
	err = thread_fork("Dandelion Thread",
			  NULL, dandelion, map, 0);
	if(err)
		goto panic;
	
	err = thread_fork("Lord FlowerKiller Thread",
			  NULL, flowerkiller, map, 0);
	if(err)
		goto panic;

	err = thread_fork("Air Balloon",
			  NULL, balloon, map, 0);
	if(err)
		goto panic;

	goto done;
panic:
	panic("airballoon: thread_fork failed: %s)\n",
	      strerror(err));
	
done:
        /* Wait for all threads */
        lock_acquire(map->lock);
        if (map->done != 4) {
            lock_release(map->lock);
            goto done;
        } 

        /* All theads done at this point */
        lock_release(map->lock);

        /* Free all memory allocated */
        lock_destroy(map->lock);
        kfree(map);
        kprintf("Main thread done\n");
	return 0;
}
