/*
 * Copyright (c) 2018
 *	The Trap Handlers
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

#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <types.h>

/*
 * called by main 
 */
void coremap_bootstrap(void);

/* The number of hardware pages available to the vm system */
size_t
hardware_pages_available(void);

UNUSED
paddr_t
physical_memory_available(void);

paddr_t
getppages(unsigned long npages);

/*
 *  Allocates some kernel-space virtual pages.  Kernel TLB mapped VA's must be
 *  in MIPS_KSEG2 [0xc000 0000 - 0xffff ffff].
 *  Called by kmalloc.
 */
vaddr_t
alloc_kpages(unsigned npages);

/*
 * Called by kfree. Frees some kernel-space virtual pages. Kernel TLB mapped
 * VA's must be in MIPS_KSEG2 [0xc000 0000 - 0xffff ffff].
 */
void
free_kpages(vaddr_t vaddr);

UNUSED
page_t
find_free_coremap_entries(unsigned npages);

UNUSED
ppage_t
claim_free_pages(unsigned npages);

#endif /* _COREMAP_H_ */
