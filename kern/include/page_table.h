/*
 * Copyright (c) 2018
 *	The Trap Handlers!
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

#ifndef _PAGE_TABLE_H_
#define _PAGE_TABLE_H_

#define PAGE_TABLE_LOAD_FACTOR_MAX 0.7
#define PAGE_TABLE_LOAD_FACTOR_MIN 0.1
#define PAGE_TABLE_CAPACITY_MIN 8
#define PAGE_TABLE_GROWTH_FACTOR 2

#define VPAGE_INVALID -1
#define PPAGE_INVALID -1

#include <types.h>

typedef int page_t;
typedef page_t ppage_t;
typedef page_t vpage_t;

typedef struct page_mapping {
        vpage_t pm_vpage; /* The virtual page number */
        ppage_t pm_ppage; /* The physical page number */
} page_mapping;

void page_mapping_invalidate(page_mapping* pm);
bool page_mapping_is_valid(const page_mapping* pm);

typedef struct {
        page_mapping* pt_mappings;
        unsigned pt_capacity;
        unsigned pt_count; /* number of page mappings, not of buckets */

        /* Is the memory of the pt_mappings array owned? */
        /* If so it must be freed */
        bool pt_owns_mappings;

        /* Is a resize pending? Used to prevent recursive resize loop */
        bool pt_resize_pending;
} page_table;


void page_table_init_with_buffer(page_table*,
                                 page_mapping* mappings,
                                 unsigned capacity,
                                 bool owns_mappings);

void page_table_init_with_capacity(page_table*, unsigned capacity);

void page_table_init(page_table*);

page_table* page_table_create_with_capacity(unsigned capacity);

page_table* page_table_create(void);

void page_table_cleanup(page_table*);

void page_table_destroy(page_table*);

void page_table_resize(page_table*, const unsigned capacity);

float page_table_load_factor(const page_table*);

bool page_table_contains(const page_table* pt, vpage_t vpage);

ppage_t page_table_read(const page_table* pt, vpage_t vpage);

void page_table_write(page_table* pt, vpage_t vpage, ppage_t ppage);

void page_table_remove(page_table* pt, vpage_t vpage);

#endif /* _PAGE_TABLE_H_ */
