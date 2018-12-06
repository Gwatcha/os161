#include <page_table.h>

#include <types.h>
#include <lib.h>

static
unsigned
hash(unsigned x)
{
        /* George Marsaglia's Xorshift hashing algorithm */
	x += 1;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
        return x;
}

static
void
page_mapping_set_empty(page_mapping* pm)
{
        pm->pm_ppage = PPAGE_INVALID;
}

static
bool
page_mapping_is_empty(const page_mapping* pm)
{
        return pm->pm_ppage == PPAGE_INVALID;
}

static
bool
page_mapping_is_occupied(const page_mapping* pm)
{
        return pm->pm_ppage != PPAGE_INVALID;
}

void
page_table_init_with_buffer(page_table* pt,
                            page_mapping* mappings,
                            unsigned capacity,
                            bool owns_mappings) {
        pt->pt_mappings = mappings;
        pt->pt_capacity = capacity;
        pt->pt_count = 0;
        pt->pt_owns_mappings = owns_mappings;
        pt->pt_resize_pending = false;
}

static
page_mapping*
page_mappings_create(unsigned capacity)
{
        page_mapping* mappings = (page_mapping*)kmalloc(capacity * sizeof(page_mapping));
        if (mappings == NULL) {
                return NULL;
        }

        for (unsigned i = 0; i < capacity; ++i) {
                page_mapping_set_empty(mappings + i);
        }
        return mappings;
}

void
page_table_init_with_capacity(page_table* pt, unsigned capacity)
{
        pt->pt_mappings = page_mappings_create(capacity);
        pt->pt_capacity = capacity;
        pt->pt_count = 0;
        pt->pt_owns_mappings = true;
        pt->pt_resize_pending = false;
}

void
page_table_init(page_table* pt)
{
        page_table_init_with_capacity(pt, PAGE_TABLE_CAPACITY_MIN);

}

page_table* page_table_create_with_capacity(unsigned capacity)
{
        page_table* pt = (page_table*)kmalloc(sizeof(page_table));
        if (pt == NULL) {
                return NULL;
        }
        page_table_init_with_capacity(pt, capacity);
        return pt;
}

page_table*
page_table_create()
{
        return page_table_create_with_capacity(PAGE_TABLE_CAPACITY_MIN);
}

void
page_table_cleanup(page_table* pt)
{
        if (pt->pt_owns_mappings) {
                kfree(pt->pt_mappings);
        }
        pt->pt_mappings = NULL;
        pt->pt_capacity = 0;
        pt->pt_count = 0;
}

void
page_table_destroy(page_table* pt)
{
        page_table_cleanup(pt);
        kfree(pt);
}

void
page_table_resize(page_table* pt, unsigned capacity)
{
        KASSERT(capacity > pt->pt_count);

        capacity = max(capacity, PAGE_TABLE_CAPACITY_MIN);

        if (capacity == pt->pt_capacity) {
                return;
        }

        if (pt->pt_resize_pending) {
                return;
        }
        pt->pt_resize_pending = true;

        page_mapping* old_mappings = pt->pt_mappings;
        const unsigned old_capacity = pt->pt_capacity;
        const unsigned old_count = pt->pt_capacity;
        const unsigned old_mappings_owned = pt->pt_owns_mappings;

        pt->pt_mappings = page_mappings_create(capacity);
        pt->pt_capacity = capacity;
        pt->pt_count = 0; // We'll be adding the elements back one by one
        pt->pt_owns_mappings = true;

        for (unsigned i = 0; i < old_capacity; ++i) {
                const page_mapping* mapping = old_mappings + i;
                if (page_mapping_is_empty(mapping)) {
                        continue;
                }
                page_table_write(pt, mapping->pm_vpage, mapping->pm_ppage);
        }

        if (old_mappings_owned) {
                kfree(old_mappings);
        }

        /* If our implementation works this will be true */
        KASSERT(pt->pt_count == old_count);

        pt->pt_resize_pending = false;
}

float
page_table_load_factor(const page_table* pt)
{
        return pt->pt_capacity <= 0 ? 1.f : (float)pt->pt_count / pt->pt_capacity;
}

static
unsigned
page_table_find_slot(const page_table* pt, vpage_t vpage)
{
        KASSERT(pt != NULL);

        const page_mapping* mappings = pt->pt_mappings;
        const unsigned capacity = pt->pt_capacity;

        unsigned i = hash(vpage) % capacity;

        while (page_mapping_is_occupied(mappings + i) && mappings[i].pm_vpage != vpage) {
                i = (i + 1) % capacity;
        }
        return i;
}

ppage_t
page_table_read(const page_table* pt, vpage_t vpage)
{
        KASSERT(pt != NULL);

        const page_mapping* mappings = pt->pt_mappings;

        const int i = page_table_find_slot(pt, vpage);

        if (page_mapping_is_occupied(mappings + i)) {
                /* Key found */
                return mappings[i].pm_ppage;
        }
        /* Key not found */
        return PPAGE_INVALID;
}

void
page_table_write(page_table* pt, const vpage_t vpage, ppage_t ppage)
{
        KASSERT(pt != NULL);

        page_mapping* mappings = pt->pt_mappings;

        const int i = page_table_find_slot(pt, vpage);

        if (page_mapping_is_occupied(mappings + i)) {
                /* Overwrite the old mapping */
                mappings[i].pm_ppage = ppage;
                return;
        }

        /* Insert a new mapping */
        mappings[i].pm_vpage = vpage;
        mappings[i].pm_ppage = ppage;
        pt->pt_count += 1;

        if (page_table_load_factor(pt) > PAGE_TABLE_LOAD_FACTOR_MAX) {
                page_table_resize(pt, pt->pt_capacity * PAGE_TABLE_GROWTH_FACTOR);
        }
}

void
page_table_remove(page_table* pt, int vpage)
{
        KASSERT(pt != NULL);

        page_mapping* mappings = pt->pt_mappings;
        const unsigned capacity = pt->pt_capacity;

        int i = page_table_find_slot(pt, vpage);

        if (page_mapping_is_empty(mappings + i)) {
                /* Key is not in table */
                return;
        }

        pt->pt_count -= 1;

        int j = i;

        while (true) {
                page_mapping_set_empty(mappings + i);
        r2:
                j = (j + 1) % capacity;

                if (page_mapping_is_empty(mappings + j)) {
                        break;
                }

                int k = hash(mappings[j].pm_vpage) % capacity;
                if (i <= j ? i < k && k <= j : i < k || k <= j) {
                        goto r2;
                }

                mappings[i] = mappings[j];
                i = j;
        }
        if (page_table_load_factor(pt) < PAGE_TABLE_LOAD_FACTOR_MIN) {
                page_table_resize(pt, pt->pt_capacity / PAGE_TABLE_GROWTH_FACTOR);
        }
}

