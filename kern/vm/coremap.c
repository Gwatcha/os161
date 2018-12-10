#include <types.h>
#include <thread.h>
#include <spl.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <page_table.h>

#include <vm.h>
#include <coremap.h>

#include <spinlock.h>

typedef struct {
        pid_t cme_pid;
} core_map_entry;


static core_map_entry* core_map = NULL;

static ppage_t coremap_first_page = 0; /* Index of the first page frame available */
static ppage_t coremap_last_page = 0;  /* One past the last page frame available */

/*
 * Wrap ram_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

void coremap_bootstrap(void) {
        /* On entry, there is no VM yet, so we cannot call kmalloc. */
        /* Instead, we use ram_stealmem. */
        KASSERT(core_map == NULL);

        const paddr_t last_paddr = ram_getsize();
        const paddr_t first_paddr = ram_getfirstfree();
	DEBUG(DB_VM, "first_paddr:  %x\n", first_paddr);
	DEBUG(DB_VM, "last_paddr:   %x\n", last_paddr);

        coremap_first_page = addr_to_page(first_paddr);
        coremap_last_page = addr_to_page(last_paddr);
	DEBUG(DB_VM, "coremap_first_page:  %d\n", coremap_first_page);
	DEBUG(DB_VM, "coremap_last_page:   %d\n", coremap_last_page);

        const page_t num_hardware_pages = hardware_pages_available();
	DEBUG(DB_VM, "Hardware pages available:  %zu\n", num_hardware_pages);

        const size_t coremap_bytes_required = sizeof(core_map_entry) * num_hardware_pages;
	DEBUG(DB_VM, "Coremap size (bytes): %zu\n", coremap_bytes_required);

        const page_t coremap_pages_required = size_to_page_count(coremap_bytes_required);
	DEBUG(DB_VM, "Coremap size (pages): %zu\n", coremap_pages_required);

        const paddr_t core_map_paddr = first_paddr;

        core_map = (core_map_entry*)PADDR_TO_KVADDR(core_map_paddr);

        /* It may be possible to use excess bytes in the coremap pages for other kernel memory, */
        /* but for now let's assume the coremap occupies the entirety of its pages */

        DEBUG(DB_VM, "coremap:  %p\n", core_map);
        DEBUG(DB_VM, "&coremap: %p\n", &core_map);

        for (ppage_t i = 0; i < coremap_pages_required; ++i) {
                core_map[i].cme_pid = PID_KERN;
        }
        for (ppage_t i = coremap_pages_required; i < num_hardware_pages; ++i) {
                core_map[i].cme_pid = PID_INVALID;
        }
}

/* The number of hardware pages available to the vm system */
size_t
hardware_pages_available()
{
        return coremap_last_page - coremap_first_page;
}

UNUSED
paddr_t
physical_memory_available()
{
        return hardware_pages_available() * PAGE_SIZE;
}

paddr_t
getppages(unsigned long npages)
{
        const ppage_t ppage = claim_free_pages(npages);

        if (ppage == PPAGE_INVALID) {
                kprintf("could not get a page\n");
                return 0;
        }
        return page_to_addr(ppage);
}

/*
 *  Allocates some kernel-space virtual pages.  Kernel TLB mapped VA's must be
 *  in MIPS_KSEG2 [0xc000 0000 - 0xffff ffff].
 *  Called by kmalloc.
 */
vaddr_t
alloc_kpages(unsigned npages)
{
        const paddr_t pa = getppages(npages);

	return PADDR_TO_KVADDR(pa);
}

/*
 * Called by kfree. Frees some kernel-space virtual pages. Kernel TLB mapped
 * VA's must be in MIPS_KSEG2 [0xc000 0000 - 0xffff ffff].
 */
void
free_kpages(vaddr_t vaddr)
{
        const ppage_t page = addr_to_page(KVADDR_TO_PADDR(vaddr));

        spinlock_acquire(&stealmem_lock);
        core_map[page - coremap_first_page].cme_pid = PID_INVALID;
        spinlock_release(&stealmem_lock);
}

UNUSED
page_t
find_free_coremap_entries(unsigned npages)
{
        const ppage_t imax = hardware_pages_available() - npages + 1;
        for (ppage_t i = 0; i < imax; ++i) {

                const ppage_t jmax = i + npages;
                for (ppage_t j = i; j < jmax; ++j) {

                        if (core_map[j].cme_pid != PID_INVALID) {
                                goto try_next;
                        }
                }
                return i;

        try_next:
                continue;
        }
        return PPAGE_INVALID;
}

UNUSED
ppage_t
claim_free_pages(unsigned npages)
{
	spinlock_acquire(&stealmem_lock);

        const page_t first_free_index = find_free_coremap_entries(npages);

        if (first_free_index == PPAGE_INVALID) {
                spinlock_release(&stealmem_lock);
                return PPAGE_INVALID;
        }

        const page_t imax = first_free_index + npages;
        for (page_t i = first_free_index; i < imax; ++i) {
                core_map[i].cme_pid = PID_KERN;
        }

	spinlock_release(&stealmem_lock);

	return first_free_index + coremap_first_page;
}
