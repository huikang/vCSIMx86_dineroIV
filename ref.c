/* 
 * Memory reference handling functions for Dinero IV
 * Written by Jan Edler and Mark D. Hill
 *
 * Copyright (C) 1997 NEC Research Institute, Inc. and Mark D. Hill.
 * All rights reserved.
 * Copyright (C) 1985, 1989 Mark D. Hill.  All rights reserved.
 * 
 * Permission to use, copy, modify, and distribute this software and
 * its associated documentation for non-commercial purposes is hereby
 * granted (for commercial purposes see below), provided that the above
 * copyright notice appears in all copies, derivative works or modified
 * versions of the software and any portions thereof, and that both the
 * copyright notice and this permission notice appear in the documentation.
 * NEC Research Institute Inc. and Mark D. Hill shall be given a copy of
 * any such derivative work or modified version of the software and NEC
 * Research Institute Inc.  and any of its affiliated companies (collectively
 * referred to as NECI) and Mark D. Hill shall be granted permission to use,
 * copy, modify, and distribute the software for internal use and research.
 * The name of NEC Research Institute Inc. and its affiliated companies
 * shall not be used in advertising or publicity related to the distribution
 * of the software, without the prior written consent of NECI.  All copies,
 * derivative works, or modified versions of the software shall be exported
 * or reexported in accordance with applicable laws and regulations relating
 * to export control.  This software is experimental.  NECI and Mark D. Hill
 * make no representations regarding the suitability of this software for
 * any purpose and neither NECI nor Mark D. Hill will support the software.
 * 
 * Use of this software for commercial purposes is also possible, but only
 * if, in addition to the above requirements for non-commercial use, written
 * permission for such use is obtained by the commercial user from NECI or
 * Mark D. Hill prior to the fabrication and distribution of the software.
 * 
 * THE SOFTWARE IS PROVIDED AS IS.  NECI AND MARK D. HILL DO NOT MAKE
 * ANY WARRANTEES EITHER EXPRESS OR IMPLIED WITH REGARD TO THE SOFTWARE.
 * NECI AND MARK D. HILL ALSO DISCLAIM ANY WARRANTY THAT THE SOFTWARE IS
 * FREE OF INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS OF OTHERS.
 * NO OTHER LICENSE EXPRESS OR IMPLIED IS HEREBY GRANTED.  NECI AND MARK
 * D. HILL SHALL NOT BE LIABLE FOR ANY DAMAGES, INCLUDING GENERAL, SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, ARISING OUT OF THE USE OR INABILITY
 * TO USE THE SOFTWARE.
 *
 * $Header: /home/edler/dinero/d4/RCS/ref.c,v 1.8 1997/12/11 07:56:19 edler Exp $
 */



/*
 * This file contains the primary functions
 * for handling memory references in Dinero IV.
 *
 *	Replacement policy functions
 * 	Prefetch policy functions
 *	Write allocate and back/through policy functions
 *	Other support functions
 *	Primary user-callable function: d4ref
 *
 * These functions are grouped together to allow customization
 * on a per-cache basis to be performed,
 * with macros such as D4CUSTOM, D4_CACHE_*, and D4_OPTS_*.
 * In such a case, this file is #include'd into some other file
 * multiple times (see d4customize in misc.c).
 * The first time, we do only some stuff that must be done just one time.
 * Each additional time, we define all the customized functions for one cache.
 *
 * If you add new policy functions to this file, you also need to add
 * some code to d4customize in misc.c!
 */


/*
 * The first group of stuff in this file is only done once
 * if we are customizing
 */
#if !D4CUSTOM || D4_REF_ONCE==0
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "d4.h"


/* some systems don't provide a proper declaration for random() */
#ifdef D4_RANDOM_DEF
extern D4_RANDOM_DEF random(void);
#endif


/* some silly macros to cope with ## wierdness */
#define D4_OPT__(cacheid,spec)	D4_OPTS_ ## cacheid ## _ ## spec
#define D4_OPT_(cacheid,spec)	D4_OPT__(cacheid,spec)
#define D4_OPT(spec)		D4_OPT_(D4_CACHEID,spec)


#if !D4CUSTOM /* these are just dummies to avoid unresolved references */
int d4_ncustom = 0;
long *d4_cust_vals[1] = { NULL };
#endif

/* This is a general mechanism by which you can tell if we are customized */
const int d4custom = D4CUSTOM;

/*
 * We need some non-customized policy functions even in the customized
 * case.  These dummy functions aren't for actually calling, but
 * are needed to resolve references, e.g., when the user sets
 * the function pointers in d4cache before calling d4setup.
 * They also allow comparison to be done in obscure cases, e.g., to
 * see if a particular policy is in effect.
 */
#if D4CUSTOM
static void d4dummy_crash (char *name)
	{ fprintf (stderr, "Dinero IV: someone called dummy function %s!\n", name);
	  exit(9); }
d4stacknode *d4rep_lru (d4cache *c, int stacknum, d4memref m, d4stacknode *ptr)
	{ d4dummy_crash("d4rep_lru"); return NULL; }
d4stacknode *d4rep_fifo (d4cache *c, int stacknum, d4memref m, d4stacknode *ptr)
	{ d4dummy_crash("d4rep_fifo"); return NULL; }
d4stacknode *d4rep_random (d4cache *c, int stacknum, d4memref m, d4stacknode *ptr)
	{ d4dummy_crash("d4rep_random"); return NULL; }
d4pendstack *d4prefetch_none (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
	{ d4dummy_crash("d4prefetch_none"); return NULL; }
d4pendstack *d4prefetch_always (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
	{ d4dummy_crash("d4prefetch_always"); return NULL; }
d4pendstack *d4prefetch_loadforw (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
	{ d4dummy_crash("d4prefetch_loadforw"); return NULL; }
d4pendstack *d4prefetch_subblock (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
	{ d4dummy_crash("d4prefetch_subblock"); return NULL; }
d4pendstack *d4prefetch_miss (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
	{ d4dummy_crash("d4prefetch_miss"); return NULL; }
d4pendstack *d4prefetch_tagged (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
	{ d4dummy_crash("d4prefetch_tagged"); return NULL; }
int d4walloc_always (d4cache *c, d4memref m)
	{ d4dummy_crash("d4walloc_always"); return 0; }
int d4walloc_never (d4cache *c, d4memref m)
	{ d4dummy_crash("d4walloc_never"); return 0; }
int d4walloc_nofetch (d4cache *c, d4memref m)
	{ d4dummy_crash("d4walloc_nofetch"); return 0; }
int d4wback_always (d4cache *c, d4memref m, int setnumber, d4stacknode *ptr, int walloc)
	{ d4dummy_crash("d4wback_always"); return 0; }
int d4wback_never (d4cache *c, d4memref m, int setnumber, d4stacknode *ptr, int walloc)
	{ d4dummy_crash("d4wback_never"); return 0; }
int d4wback_nofetch (d4cache *c, d4memref m, int setnumber, d4stacknode *ptr, int walloc)
	{ d4dummy_crash("d4wback_nofetch"); return 0; }
#endif


#if D4CUSTOM
/*
 * Dynamic stub for d4ref in customized case.
 * This routine will normally be called at most 1 time for each cache.
 * Remember, if D4CUSTOM is 1, the "real" d4ref, later in this file,
 * will appear multiple times, renamed to something else each time.
 */
#undef d4ref /* defeat the macro from d4.h, which users normally invoke */
extern void d4ref (d4cache *, d4memref); /* prototype to avoid compiler warnings */

void d4ref (d4cache *c, d4memref m)
{
	extern int d4_ncustom;
	extern void (*d4_custom[]) (d4cache *, d4memref);

	if (c->cacheid <= 0 || c->cacheid > d4_ncustom) {
		/* "can't happen" */
		fprintf (stderr, "Dinero IV: d4_custom is messed up "
			 "(d4_ncustom %d, cacheid %d)\n", d4_ncustom, c->cacheid);
		exit (9);
	}
	c->ref = d4_custom[c->cacheid-1];
	c->ref (c, m);
}
#endif

#define D4_REF_ONCE 1	/* don't do it again */
#endif	/* !D4CUSTOM || D4_REF_ONCE==0 */


/*
 * Everything else in this file may be done multiple times
 * if we're customizing.
 */
#if !D4CUSTOM || D4_REF_ONCE>1	/* applies everything else except the last line */


#if !D4CUSTOM || D4_OPT (rep_lru)
/*
 * LRU replacement policy
 * With D4CUSTOM!=0 and inlining, this is also good for direct-mapped caches
 */
D4_INLINE d4stacknode * d4rep_lru (d4cache *c, int stacknum, d4memref m, d4stacknode *ptr)
{
	if (ptr != NULL) {	/* hits */
		if ((!D4CUSTOM || D4VAL (c, assoc) > 1 || (D4VAL (c, flags) & D4F_CCC) != 0) &&
		    ptr != c->stack[stacknum].top)
			d4movetotop (c, stacknum, ptr);
	}
	else {				/* misses */
		ptr = c->stack[stacknum].top->up;
		assert (ptr->valid == 0);
		ptr->blockaddr = D4ADDR2BLOCK (c, m.address);
		if ((!D4CUSTOM || D4VAL(c,assoc) >= D4HASH_THRESH || (D4VAL(c,flags)&D4F_CCC)!=0) &&
		    c->stack[stacknum].n > D4HASH_THRESH)
			d4hash (c, stacknum, ptr);
		c->stack[stacknum].top = ptr;	/* quicker than d4movetotop */
	}
	return ptr;
}
#endif	/* !D4CUSTOM || D4_OPT (rep_lru) */


#if !D4CUSTOM || D4_OPT (rep_fifo)
/*
 * FIFO replacement policy
 */
D4_INLINE d4stacknode * d4rep_fifo (d4cache *c, int stacknum, d4memref m, d4stacknode *ptr)
{
	if (ptr == NULL) {	/* misses */
		ptr = c->stack[stacknum].top->up;
		assert (ptr->valid == 0);
		ptr->blockaddr = D4ADDR2BLOCK (c, m.address);
		if (c->stack[stacknum].n > D4HASH_THRESH)
			d4hash (c, stacknum, ptr);
		c->stack[stacknum].top = ptr;	/* quicker than d4movetotop */
	}
	return ptr;
}
#endif	/* !D4CUSTOM || D4_OPT (rep_fifo) */


#if !D4CUSTOM || D4_OPT (rep_random)
/*
 * Random replacement policy.
 */
D4_INLINE d4stacknode * d4rep_random (d4cache *c, int stacknum, d4memref m, d4stacknode *ptr)
{
	if (ptr == NULL) {	/* misses */
		int setsize = c->stack[stacknum].n - 1;
		ptr = c->stack[stacknum].top->up;
		assert (ptr->valid == 0);
		ptr->blockaddr = D4ADDR2BLOCK (c, m.address);
		if (setsize >= D4HASH_THRESH)
			d4hash (c, stacknum, ptr);
		c->stack[stacknum].top = ptr;	/* quicker than d4movetotop */
		if (ptr->up->valid != 0)	/* set is full */
			d4movetobot (c, stacknum, d4findnth (c, stacknum, 2 + (random() % setsize)));
	}
	return ptr;
}
#endif	/* !D4CUSTOM || D4_OPT (rep_random) */


#if !D4CUSTOM || D4_OPT (prefetch_none)
/*
 * Demand fetch only policy, no prefetching
 */
D4_INLINE d4pendstack * d4prefetch_none (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
{
	/* no prefetch, nothing to do */
	return NULL;
}
#endif	/* !D4CUSTOM || D4_OPT (prefetch_none) */


#if !D4CUSTOM || D4_OPT (prefetch_always)
/*
 * Always prefetch policy
 */
D4_INLINE d4pendstack * d4prefetch_always (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
{
	d4pendstack *pf;

	pf = d4get_mref();
	pf->m.address = D4ADDR2SUBBLOCK (c, m.address + c->prefetch_distance);
	pf->m.accesstype = m.accesstype | D4PREFETCH;
	pf->m.size = 1<<D4VAL(c,lg2subblocksize);
	return pf;
}
#endif	/* !D4CUSTOM || D4_OPT (prefetch_always) */


#if !D4CUSTOM || D4_OPT (prefetch_loadforw)
/*
 * Load forward prefetch policy
 * Don't prefetch into next block.
 */
D4_INLINE d4pendstack * d4prefetch_loadforw (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
{
	d4pendstack *pf;

	if (D4ADDR2BLOCK(c,m.address+c->prefetch_distance) != D4ADDR2BLOCK(c,m.address))
		return NULL;

	pf = d4get_mref();
	pf->m.address = D4ADDR2SUBBLOCK (c, m.address + c->prefetch_distance);
	pf->m.accesstype = m.accesstype | D4PREFETCH;
	pf->m.size = 1<<D4VAL(c,lg2subblocksize);
	return pf;
}
#endif	/* !D4CUSTOM || D4_OPT (prefetch_loadforw) */


#if !D4CUSTOM || D4_OPT (prefetch_subblock)
/*
 * Subblock prefetch policy
 * Don't prefetch into next block; wrap around within block instead.
 */
D4_INLINE d4pendstack * d4prefetch_subblock (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
{
	d4pendstack *pf;

	pf = d4get_mref();
	pf->m.address = D4ADDR2SUBBLOCK (c, m.address + c->prefetch_distance);
	pf->m.accesstype = m.accesstype | D4PREFETCH;
	pf->m.size = 1<<D4VAL(c,lg2subblocksize);

	if (D4ADDR2BLOCK(c,pf->m.address) != D4ADDR2BLOCK(c,m.address))
		pf->m.address -= 1<<D4VAL(c,lg2blocksize);
	return pf;
}
#endif	/* !D4CUSTOM || D4_OPT (prefetch_subblock) */


#if !D4CUSTOM || D4_OPT (prefetch_miss)
/*
 * Miss prefetch policy
 * Prefetch only on misses
 */
D4_INLINE d4pendstack * d4prefetch_miss (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
{
	d4pendstack *pf;

	if (!miss)
		return NULL;

	pf = d4get_mref();
	pf->m.address = D4ADDR2SUBBLOCK (c, m.address + c->prefetch_distance);
	pf->m.accesstype = m.accesstype | D4PREFETCH;
	pf->m.size = 1<<D4VAL(c,lg2subblocksize);
	return pf;
}
#endif	/* !D4CUSTOM || D4_OPT (prefetch_miss) */


#if !D4CUSTOM || D4_OPT (prefetch_tagged)
/* 
 * Tagged prefetch (see Smith, "cache Memories," ~p.20) initiates 
 * a prefetch on the first demand reference to a (sub)-block.  Thus, 
 * a prefetch is initiated on a demand miss or the first demand 
 * reference to a (sub)-block that was brought into the cache by a 
 * prefetch. 
 *
 * Tagged prefetching is implemented using demand reference bits
 * in the cache entry.  A prefetch is started on a demand miss and
 * on a refernce to a (sub)-block whose reference bit was not previously set.
 */
D4_INLINE d4pendstack * d4prefetch_tagged (d4cache *c, d4memref m, int miss, d4stacknode *stackptr)
{
	d4pendstack *pf;
	int sbbits;

	sbbits = D4ADDR2SBMASK(c,m);
	if (!miss && (sbbits & stackptr->referenced) != 0)
		return NULL;

	pf = d4get_mref();
	pf->m.address = D4ADDR2SUBBLOCK (c, m.address + c->prefetch_distance);
	pf->m.accesstype = m.accesstype | D4PREFETCH;
	pf->m.size = 1<<D4VAL(c,lg2subblocksize);
	return pf;
}
#endif	/* !D4CUSTOM || D4_OPT (prefetch_tagged) */


#if !D4CUSTOM || D4_OPT (walloc_always)
/*
 * Always write allocate
 */
D4_INLINE int d4walloc_always (d4cache *c, d4memref m)
{
	return 1;
}
#endif	/* !D4CUSTOM || D4_OPT (walloc_always) */


#if !D4CUSTOM || D4_OPT (walloc_never)
/*
 * Never write allocate
 */
D4_INLINE int d4walloc_never (d4cache *c, d4memref m)
{
	return 0;
}
#endif	/* !D4CUSTOM || D4_OPT (walloc_never) */


#if !D4CUSTOM || D4_OPT (walloc_nofetch)
/*
 * Write allocate if no fetch is required
 * (write exactly fills an integral number of subblocks)
 */
D4_INLINE int d4walloc_nofetch (d4cache *c, d4memref m)
{
	return m.size == D4REFNSB(c,m) << D4VAL (c, lg2subblocksize);
}
#endif	/* !D4CUSTOM || D4_OPT (walloc_nofetch) */


#if !D4CUSTOM || D4_OPT (wback_always)
/*
 * Always write back
 */
D4_INLINE int d4wback_always (d4cache *c, d4memref m, int setnumber, d4stacknode *ptr, int walloc)
{
	return 1;
}
#endif	/* !D4CUSTOM || D4_OPT (wback_always) */


#if !D4CUSTOM || D4_OPT (wback_never)
/*
 * Never write back (i.e., always write through)
 */
D4_INLINE int d4wback_never (d4cache *c, d4memref m, int setnumber, d4stacknode *ptr, int walloc)
{
	return 0;
}
#endif	/* !D4CUSTOM || D4_OPT (wback_never) */


#if !D4CUSTOM || D4_OPT (wback_nofetch)
/*
 * Write back if no fetch is required
 * The actual test is for every affected subblock to be valid or
 * for the write to completely cover all affected subblocks.
 */
D4_INLINE int d4wback_nofetch (d4cache *c, d4memref m, int setnumber, d4stacknode *ptr, int walloc)
{
	return (D4ADDR2SBMASK(c,m) & ~ptr->valid) == 0 ||
	       m.size == (D4REFNSB(c,m) << D4VAL (c, lg2subblocksize));
}
#endif	/* !D4CUSTOM || D4_OPT (wback_nofetch) */



#if !D4CUSTOM || D4_OPT (ccc)
/*
 * This function implements an infinite-sized cache, used
 * when classifying cache misses into compulsory, capacity,
 * and conflict misses.
 *
 * Return value:
 *	-1 if at least 1 affected subblock (but not the whole block)
 *		misses in the infinite cache
 *	0 if all affected subblocks hit in the infinite cache
 *	1 if the whole block misses in the infinite cache
 * Note we require that the number of subblocks per block be a
 *	divisor of D4_BITMAP_RSIZE, so blocks are not split across bitmaps
 */

static int d4infcache (d4cache *c, d4memref m)
{
	const unsigned int sbsize = 1 << D4VAL (c, lg2subblocksize);
	const d4addr sbaddr = D4ADDR2SUBBLOCK (c, m.address);
	const int nsb = D4REFNSB (c, m);
	unsigned int bitoff; /* offset of bit in bitmap */
	int hi, lo, i, b;
	static int totranges = 0, totbitmaps = 0;

	bitoff = (sbaddr & (D4_BITMAP_RSIZE-1)) / sbsize;
    
	/* binary search for range containing our address */
	hi = c->nranges-1;
	lo = 0;
    
	while (lo <= hi) {
		i = lo + (hi-lo)/2;
		if (c->ranges[i].addr + D4_BITMAP_RSIZE <= sbaddr)
			lo = i + 1;		/* need to look higher */
		else if (c->ranges[i].addr > sbaddr)
			hi = i - 1;		/* need to look lower */
		else { /* found the right range */
			const int sbpb = 1 << (D4VAL (c, lg2blocksize) - D4VAL (c, lg2subblocksize));
			int nb;
			/* count affected bits we've seen */
			for (nb = 0, b = 0;  b < nsb;  b++)
				nb += ((c->ranges[i].bitmap[(bitoff+b)/CHAR_BIT] &
					(1<<((bitoff+b)%CHAR_BIT))) != 0);
			if (nb == nsb)
				return 0;	/* we've seen it all before */
			/* consider the whole block */
			if (sbpb != 1 && nsb != sbpb) {
				unsigned int bbitoff = (D4ADDR2BLOCK (c, m.address) &
							(D4_BITMAP_RSIZE-1)) / sbsize;
				for (nb = 0, b = 0;  b < sbpb;  b++)
					nb += ((c->ranges[i].bitmap[(bbitoff+b)/CHAR_BIT] &
						(1<<((bbitoff+b)%CHAR_BIT))) != 0);
			}
			/* set the bits */
			for (b = 0;  b < nsb;  b++) {
				c->ranges[i].bitmap[(bitoff+b)/CHAR_BIT] |=
					(1<<((bitoff+b)%CHAR_BIT));
				// printf("\t\tExist range %d bitoff %d %d\n", i, (bitoff+b)/CHAR_BIT,
				//   c->ranges[i].bitmap[(bitoff+b)/CHAR_BIT]);
            }
			return nb==0 ? 1 : -1;
		}
	}
	/* lo > hi: range not found; find position and insert new range */
	if (c->nranges >= c->maxranges-1) {
	  //printf("\t\t[%s] Add range cacheid=%d\n", __func__, c->cacheid);
		/* ran out of range pointers; allocate some more */
		int oldmaxranges = c->maxranges;
		c->maxranges = (c->maxranges + 10) * 2;
		if (c->ranges == NULL) /* don't trust realloc(NULL,...) */
			c->ranges = malloc (c->maxranges * sizeof(*c->ranges));
		else
			c->ranges = realloc (c->ranges, c->maxranges * sizeof(*c->ranges));
		if (c->ranges == NULL) {
			fprintf (stderr, "DineroIV: can't allocate more "
				 "bitmap pointers for cache %s (%d so far, total %d)\n",
				 c->name, oldmaxranges, totranges);
			exit(1);
		}
		totranges++;
	}
	for (i = c->nranges++ - 1;  i >= 0;  i--) {
		if (c->ranges[i].addr < sbaddr)
			break;
		c->ranges[i+1] = c->ranges[i];
		//printf("\t\tcopy old range %d to %d\n", i, i+1);
	}
	c->ranges[i+1].addr = sbaddr & ~(D4_BITMAP_RSIZE-1);
	c->ranges[i+1].bitmap = calloc ((((D4_BITMAP_RSIZE + sbsize - 1)
					/ sbsize) + CHAR_BIT - 1) / CHAR_BIT, 1);
    /* printf("%s range=%dth, addr=%0lx, bitmap size=%d (nr of subblock)\n",
           __func__, i+1, c->ranges[i+1].addr,
           (((D4_BITMAP_RSIZE + sbsize - 1) / sbsize) + CHAR_BIT - 1) / CHAR_BIT);
     */
	if (c->ranges[i+1].bitmap == NULL) {
		fprintf (stderr, "DineroIV: can't allocate another bitmap "
			 "(currently %d, total %d, each mapping 0x%x bytes)\n",
			 c->nranges-1, totbitmaps, D4_BITMAP_RSIZE);
		exit(1);
	}
	totbitmaps++;
	for (b = 0;  b < nsb;  b++, bitoff++) {
		c->ranges[i+1].bitmap[bitoff/CHAR_BIT] |= (1<<(bitoff%CHAR_BIT));
		//printf("\t\tInserted range %d bitoff %d %d\n",
		//i+1, bitoff/CHAR_BIT,
		//c->ranges[i+1].bitmap[bitoff/CHAR_BIT]);
    }
	// printf("\t\tInserted at range %d begining %0lx\n", i+1, c->ranges[i+1].addr);
	return 1; /* we've not seen it before */
}

static int d4updatemmap (d4cache *c, d4memref m)
{
	const unsigned int sbsize = 1 << D4VAL (c, lg2subblocksize);
	const d4addr sbaddr = D4ADDR2SUBBLOCK (c, m.address);
	const int nsb = D4REFNSB (c, m);
	unsigned int bitoff; /* offset of bit in bitmap */
	int hi, lo, i, b;
	static int totranges = 0, totbitmaps = 0;
    
	bitoff = (sbaddr & (D4_BITMAP_RSIZE-1)) / sbsize;
    printf("\t\t[%s] sbsize=%u, sbaddr=%0lx, bitoff=%0lx\n",
           __func__, sbsize, sbaddr, bitoff);

	/* binary search for range containing our address */
	hi = c->nranges_mtrack-1;
    printf("\t\t[%s] nranges=%d maxranges=%d nsb=%d\n",
           __func__, hi, c->maxranges_mtrack, nsb);
	lo = 0;
	while (lo <= hi) {
		i = lo + (hi-lo)/2;
		if (c->ranges_mtrack[i].addr + D4_BITMAP_RSIZE <= sbaddr)
			lo = i + 1;		/* need to look higher */
		else if (c->ranges_mtrack[i].addr > sbaddr)
			hi = i - 1;		/* need to look lower */
		else { /* found the right range */
			const int sbpb = 1 << (D4VAL (c, lg2blocksize) - D4VAL (c, lg2subblocksize));
			int nb;
			/* count affected bits we've seen */
			for (nb = 0, b = 0;  b < nsb;  b++)
				nb += ((c->ranges_mtrack[i].bitmap[(bitoff+b)/CHAR_BIT] &
                        (1<<((bitoff+b)%CHAR_BIT))) != 0);
			if (nb == nsb)
				return 0;	/* we've seen it all before */
			/* consider the whole block */
			if (sbpb != 1 && nsb != sbpb) {
				unsigned int bbitoff = (D4ADDR2BLOCK (c, m.address) &
                                        (D4_BITMAP_RSIZE-1)) / sbsize;
				for (nb = 0, b = 0;  b < sbpb;  b++)
					nb += ((c->ranges_mtrack[i].bitmap[(bbitoff+b)/CHAR_BIT] &
                            (1<<((bbitoff+b)%CHAR_BIT))) != 0);
			}
			/* set the bits */
			for (b = 0;  b < nsb;  b++) {
				c->ranges_mtrack[i].bitmap[(bitoff+b)/CHAR_BIT] |=
                (1<<((bitoff+b)%CHAR_BIT));
                printf("\t\trange %d bitoff %d %d\n", i, bitoff+b,
                       c->ranges_mtrack[i].bitmap[(bitoff+b)/CHAR_BIT]);
            }
			return nb==0 ? 1 : -1;
		}
	}
	/* lo > hi: range not found; find position and insert new range */
	if (c->nranges_mtrack >= c->maxranges_mtrack-1) {
		/* ran out of range pointers; allocate some more */
		int oldmaxranges = c->maxranges_mtrack;
		c->maxranges_mtrack = (c->maxranges_mtrack + 10) * 2;
		if (c->ranges_mtrack == NULL) /* don't trust realloc(NULL,...) */
			c->ranges_mtrack = malloc (c->maxranges_mtrack * sizeof(*c->ranges_mtrack));
		else
			c->ranges_mtrack = realloc (c->ranges_mtrack,
                                        c->maxranges_mtrack * sizeof(*c->ranges_mtrack));
		if (c->ranges_mtrack == NULL) {
			fprintf (stderr, "DineroIV: can't allocate more "
                     "bitmap pointers for cache %s (%d so far, total %d)\n",
                     c->name, oldmaxranges, totranges);
			exit(1);
		}
		totranges++;
	}
	for (i = c->nranges_mtrack++ - 1;  i >= 0;  i--) {
		if (c->ranges_mtrack[i].addr < sbaddr)
			break;
		c->ranges_mtrack[i+1] = c->ranges_mtrack[i];
	}
	c->ranges_mtrack[i+1].addr = sbaddr & ~(D4_BITMAP_RSIZE-1);
	c->ranges_mtrack[i+1].bitmap = calloc ((((D4_BITMAP_RSIZE + sbsize - 1)
                                      / sbsize) + CHAR_BIT - 1) / CHAR_BIT, 1);
    printf("\t\t[%s] range=%dth, addr=%0lx, bitmap size=%d (nr of subblock)\n",
           __func__, i+1, c->ranges_mtrack[i+1].addr,
           (((D4_BITMAP_RSIZE + sbsize - 1) / sbsize) + CHAR_BIT - 1) / CHAR_BIT);
    
	if (c->ranges_mtrack[i+1].bitmap == NULL) {
		fprintf (stderr, "DineroIV: can't allocate another bitmap "
                 "(currently %d, total %d, each mapping 0x%x bytes)\n",
                 c->nranges_mtrack-1, totbitmaps, D4_BITMAP_RSIZE);
		exit(1);
	}
	totbitmaps++;
	for (b = 0;  b < nsb;  b++, bitoff++) {
		c->ranges_mtrack[i+1].bitmap[bitoff/CHAR_BIT] |= (1<<(bitoff%CHAR_BIT));
        printf("\t\trange %d bitoff %d %d\n", i+1, bitoff,
               c->ranges_mtrack[i+1].bitmap[(bitoff/CHAR_BIT)]);
    }
    printf("\t\t[%s] Setbit range %d bitoff %d\n", __func__, i+1, bitoff-1);
	return 1; /* we've not seen it before */
}

static int d4replacemmap (d4cache *c, d4memref m)
{
	const unsigned int sbsize = 1 << D4VAL (c, lg2subblocksize);
	const d4addr sbaddr = D4ADDR2SUBBLOCK (c, m.address);
	const int nsb = D4REFNSB (c, m);
	unsigned int bitoff; /* offset of bit in bitmap */
	int hi, lo, i, b;
    
	bitoff = (sbaddr & (D4_BITMAP_RSIZE-1)) / sbsize;
    printf("\t\t[%s] sbsize=%u, sbaddr=%0lx, bitoff=%0lx\n",
           __func__, sbsize, sbaddr, bitoff);
    
	/* binary search for range containing our address */
	hi = c->nranges_mtrack-1;
    printf("\t\t[%s] nranges=%d maxranges=%d nsb=%d\n",
           __func__, hi, c->maxranges_mtrack, nsb);
	lo = 0;
	while (lo <= hi) {
		i = lo + (hi-lo)/2;
		if (c->ranges_mtrack[i].addr + D4_BITMAP_RSIZE <= sbaddr)
			lo = i + 1;		/* need to look higher */
		else if (c->ranges_mtrack[i].addr > sbaddr)
			hi = i - 1;		/* need to look lower */
		else { /* found the right range */
			const int sbpb = 1 << (D4VAL (c, lg2blocksize) - D4VAL (c, lg2subblocksize));
			int nb;
			/* count affected bits we've seen */
			for (nb = 0, b = 0;  b < nsb;  b++)
				nb += ((c->ranges_mtrack[i].bitmap[(bitoff+b)/CHAR_BIT] &
                        (1<<((bitoff+b)%CHAR_BIT))) != 0);
			if (nb == nsb) {
                printf("\t\tSee the whole block nb=%d\n", nb);
                for (b = 0;  b < nsb;  b++) {
                    c->ranges_mtrack[i].bitmap[(bitoff+b)/CHAR_BIT] &=
                    ~(1<<((bitoff+b)%CHAR_BIT));
                    printf("\t\t[%s]range %d bitoff %d %d\n", __func__, i,
                           bitoff+b, c->ranges_mtrack[i].bitmap[(bitoff+b)/CHAR_BIT]);
                }
				return 0;	/* we've seen it all before */
            }
			/* consider the whole block */
			if (sbpb != 1 && nsb != sbpb) {
				unsigned int bbitoff = (D4ADDR2BLOCK (c, m.address) &
                                        (D4_BITMAP_RSIZE-1)) / sbsize;
				for (nb = 0, b = 0;  b < sbpb;  b++)
					nb += ((c->ranges_mtrack[i].bitmap[(bbitoff+b)/CHAR_BIT] &
                            (1<<((bbitoff+b)%CHAR_BIT))) != 0);
			}
			/* clear the bits */
			for (b = 0;  b < nsb;  b++) {
				c->ranges_mtrack[i].bitmap[(bitoff+b)/CHAR_BIT] &=
                    ~(1<<((bitoff+b)%CHAR_BIT));
                printf("\t\t[%s] range %d bitoff %d %d\n", __func__, i, bitoff+b,
                       c->ranges_mtrack[i].bitmap[(bitoff+b)/CHAR_BIT]);
            }
			return nb==0 ? 1 : -1;
		}
	}
    
    assert(0);
}

/*
 * Update the state of an blockaddress in the memory
 * Parameters: (1) c must be the LLC
 *             (2) blockaddr is the key in the BST
 *             (3) access = 1: the blockaddr is accessed, e.g., brough to the LLC;
 *                 access = 0: means it is being replaced.
 *             (4) vmid, requesting the insert, or replace
 * Return:     0: not miss
 *             1: inter_vm miss, 2: intra_vm miss, 3: compulsory miss
 *
 */
static int update_llc_mem_list(d4cache *c, d4addr blockaddr, int access, int vmid) {
    d4memllc *p, *x;
    int found;
    int vm_miss;
    int index;
    
    vm_miss = 0;
    if ((c->root) == NULL) {
        p = malloc(sizeof(d4memllc));
        p->blockaddr = blockaddr;
        p->state = access;
        p->vmid = vmid;
        p->infvalid = 1;
        p->left = p->right = NULL;
        c->root = p;
        c->vm_comp_miss++;
        /*printf("\t\t[%s] Create the first element %0lx vmid=%u\n", __func__,
               ((c->root))->blockaddr, c->root->vmid);*/
        vm_miss = 3;
    }
    
    //printf("\t\t[%s] Root element %0lx\n", __func__,
    //       ((c->root))->blockaddr);
    p = (c->root);
    x = p;
    found = 0;
    
    index = (blockaddr) & (MEM_TABLE_SIZE - 1);
    if (mem_entry_table[index] != NULL) {
      if (unlikely(mem_entry_table[index]->blockaddr != blockaddr)) {
        while (x != NULL) {
	  p = x;
	  if (blockaddr == x->blockaddr) {
	    //printf("\t\t[%s] Found %0lx vmid=%u, access=%d\n",
	    //	   __func__, x->blockaddr, x->vmid, access);
	    found = 1;
	    break;
	  }
	  x = (blockaddr < x->blockaddr) ? x->left : x->right;
        }
        if (found) { /* insert to hash */
	  mem_entry_table[index] = x;
        }
      } else { /* this one is hashed */
        found = 1;
        x = mem_entry_table[index];
      }
    } else {
      while (x != NULL) {
	  p = x;
	  if (blockaddr == x->blockaddr) {
	    //printf("\t\t[%s] Found %0lx vmid=%u, access=%d\n",
	    //	   __func__, x->blockaddr, x->vmid, access);
	    found = 1;
	    break;
	  }
	  x = (blockaddr < x->blockaddr) ? x->left : x->right;
      }
    }

    if (!access)    /* For replacing, we must found the address */
        assert(found == 1);
    
    if (found) {
        if (access) {   /* bring the vm into LLC */
            if (x->state == in_mem) {
                
                if (x->infvalid == 0) {
                    c->vm_comp_miss++;
                    vm_miss = 3;
                    // printf("\t\tcompulsory miss due to infvalid\n");
                } else {
                
                    if (x->vmid != vmid) {
		      //printf("\t\t[%s] inter miss %u replace by %u\n",
		      //       __func__, x->vmid, vmid);
                        c->inter_vm_miss++;
                        vm_miss = 1;
                    } else {
		      //printf("\t\t[%s] intra miss vmid=%u, state=%u\n",
		      //       __func__, vmid, x->infvalid);
                        c->intra_vm_miss++;
                        vm_miss = 2;
                    }
                }
                x->state = in_llc;
                x->vmid = vmid;
                x->infvalid = 1;
            } else if (x->infvalid == 0) {
                assert(x->state == in_llc);
                
                x->infvalid = 1;
                c->vm_comp_miss++;
                vm_miss = 3;
                // printf("\t\t[%s] validate %0lx by vmid %u\n", __func__, blockaddr, x->vmid);
            }
        } else {        /* replace this element to memory, vmid causes the replacement */
            assert(x->state == in_llc);
            x->state = in_mem;
            x->vmid = vmid;
            //x->infvalid = 1;
            //printf("\t\t[%s] replacing %0lx to mem by vmid=%u\n",
	    //     __func__, x->blockaddr, x->vmid);
        }
    } else {
        /* Bring a new element to LLC */
        x = malloc(sizeof(d4memllc));
        x->blockaddr = blockaddr;
        x->vmid = vmid;
        x->state = in_llc;
        x->left = x->right = NULL;
        x->infvalid = 1;
        if (blockaddr < p->blockaddr)
            p->left = x;
        else
            p->right = x;
        c->vm_comp_miss++;
        vm_miss = 3;
        //printf("\t\t[%s] Comp miss insert %0lx by vmid %u\n", __func__, blockaddr, x->vmid);
        
    }
    
    double demand_comp_alltype;
    demand_comp_alltype = c->comp_miss[D4XMISC]
                          + c->comp_miss[D4XREAD]
                          + c->comp_miss[D4XWRITE]
                          + c->comp_miss[D4XINSTRN];
    //printf("\t\t[%s] vm comp_miss=%d\n", __func__, c->vm_comp_miss);
    //printf("\t\t[%s] comp_miss=%f\n", __func__, demand_comp_alltype);
    return vm_miss;
}

#endif /* !D4CUSTOM || D4_OPT (ccc) */


/*
 * Split a memory reference if it crosses a block boundary.
 * The remainder, if any, is queued for processing later.
 */
D4_INLINE d4memref d4_splitm (d4cache *c, d4memref mr, d4addr ba)
{
	const int bsize = 1 << D4VAL (c, lg2blocksize);
	const int bmask = bsize - 1;
	int newsize;
	d4pendstack *pf;

	if (ba == D4ADDR2BLOCK (c, mr.address + mr.size - 1))
		return mr;
	pf = d4get_mref();
        pf->m.address = ba + bsize;
        pf->m.accesstype = mr.accesstype | D4_MULTIBLOCK;
	newsize = bsize - (mr.address&bmask);
        pf->m.size = mr.size - newsize;
	pf->next = c->pending;
	c->pending = pf;
	c->multiblock++;
	mr.size = newsize;
	return mr;
}


/*
 * Handle a memory reference for the given cache.
 * The user calls this function for the cache closest to
 * the processor; other caches are handled automatically.
 */
void d4ref (d4cache *c, d4memref mr)
{
#if (D4DEBUG)
	if(c->cacheid == 1)
		printf("\tinside d4ref, MEM\n");
	else
		printf("\tinside d4ref, cacheid[%d], level[%d] \n", c->cacheid, c->level);
	printf("\t\tmemref struct: address[%llu], accesstype[%d], size[%d]\n", mr.address, mr.accesstype, mr.size);
#endif
	/* special cases first */
    if ((D4VAL (c, flags) & D4F_MEM) != 0) { /* special case for simulated memory */
		c->fetch[(int)mr.accesstype]++;
		//printf("\t\tIn main memory\n");
    }
    else if (mr.accesstype == D4XCOPYB || mr.accesstype == D4XINVAL) {
		d4memref m = mr;	/* dumb compilers might de-optimize if we take addr of mr */
		if (m.accesstype == D4XCOPYB) {
			d4copyback (c, &m, 1);
        }
		else
			d4invalidate (c, &m, 1);
    }
    else {				 /* everything else */
		const d4addr blockaddr = D4ADDR2BLOCK (c, mr.address);
		const d4memref m = d4_splitm (c, mr, blockaddr);
		const int atype = D4BASIC_ATYPE (m.accesstype);
		const int setnumber = D4ADDR2SET(c, m.address);

		const int ronly = D4CUSTOM && (D4VAL (c, flags) & D4F_RO) != 0; /* conservative */
		const int walloc = !ronly && atype == D4XWRITE && D4VAL (c, wallocf) (c, m);
		const int sbbits = D4ADDR2SBMASK (c, m);
        unsigned char vmid = mr.vmid;
        //printf("\t\tvmid=%u\n", vmid);
		int miss, blockmiss, wback;
		d4stacknode *ptr;
        int vm_miss;

		if ((D4VAL (c, flags) & D4F_RO) != 0 && atype == D4XWRITE) {
			fprintf (stderr, "Dinero IV: write to read-only cacheid %d (%s)\n",
				 c->cacheid, c->name);
			exit (9);
		}

		/*
		 * Find address in the cache.
		 * Quickly check for top of stack.
		 */
		ptr = c->stack[setnumber].top;
		/*if (ptr->valid == 0) {
            printf("%llu Invalid entry\n", blockaddr);
	    }*/
		if (ptr->blockaddr == blockaddr && ptr->valid != 0)
			; /* found it */
		else if (!D4CUSTOM || D4VAL (c, assoc) > 1)
			ptr = d4_find (c, setnumber, blockaddr);
		else
			ptr = NULL;

		blockmiss = (ptr == NULL);
		if(blockmiss) {
			miss = blockmiss;
#if (D4DEBUG)
			printf("\t\tBLOCKMISS\n");
#endif
		}
		else
		{
			miss = (sbbits & ptr->valid) != sbbits;
#if (D4DEBUG)
			if(miss)
				printf("\t\tMISS\n");
			else
				printf("\t\tHIT\n");
#endif
		}

		/*
		 * Prefetch on reads and instruction fetches, but not on
		 * writes, misc, and prefetch references.
		 * Optionally, some percentage may be thrown away.
		 */
		if ((!D4CUSTOM || !D4_OPT (prefetch_none)) && (m.accesstype == D4XREAD || m.accesstype == D4XINSTRN)) {
			d4pendstack *pf = D4VAL (c, prefetchf) (c, m, miss, ptr);
			if (pf != NULL) {
				/* Note: 0 <= random() <= 2^31-1 and 0 <= random()/(INT_MAX/100) < 100. */
				if (D4VAL (c, prefetch_abortpercent) > 0 && random()/(INT_MAX/100) < D4VAL (c, prefetch_abortpercent))
					d4put_mref (pf);	/* throw it away */
				else {
					pf->next = c->pending;	/* add to pending list */
					c->pending = pf;
				}
			}
		}
        
		/*
		 * Update the cache
		 * Don't do it for non-write-allocate misses
		 */
		wback = 0;
		if (ronly || atype != D4XWRITE || !blockmiss || walloc) {
#if (D4DEBUG)
			printf("\t\tUPDATING CACHE\n");
#endif
			/*
			 * Adjust priority stack as necessary
			 */
			ptr = D4VAL (c, replacementf) (c, setnumber, m, ptr);
			/*
			 * Update state bits
			 */
			if (blockmiss) {
				assert (ptr->valid == 0);
				ptr->referenced = 0;
				ptr->dirty = 0;
			}
			ptr->valid |= sbbits;
            
            if (c->isllc) {
                /*
                 * Update the blockaddr in the tree, blockaddr is being accessed
                 * This must be doen when a replacement is happened
                 * i.e., for any read hit, read miss,
                 */
                /* d4updatemmap (c, m); */
                vm_miss = update_llc_mem_list(c, blockaddr, 1, vmid);
                /*printf("\t\t[%s] LLC raddr=%0lx baddr=%0lx sbbits=%d setnumber=%d, vmid=%u\n",
		  __func__, mr.address, blockaddr, sbbits, setnumber, mr.vmid);*/
            }
            
			if ((m.accesstype & D4PREFETCH) == 0) /* prefetching usage statistic */
				ptr->referenced |= sbbits;

			/*
			 * For writes, decide if write-back or write-through.
			 * Set the dirty bits if write-back is going to be used.
			 *
			 * The write-back flag in this conditional accounts for non-write-allocate,
			 * write-back caches.  Since the wback flag is in this conditional,
			 * it forces write-back misses to be written through to higher caches
			 * even if those caches are write-back too.  This is important
			 * because the write might hit on one of those caches; any cache
			 * that contains the address must be updated. We can't go directly
			 * to memory even if all the write-back caches are non-write-allocate.
			 */
			wback = !ronly && (atype == D4XWRITE) && D4VAL (c, wbackf) (c, m, setnumber, ptr, walloc);
			if (wback)
				ptr->dirty |= sbbits;

			/*
			 * Take care of replaced block
			 * including write-back if necessary
			 */
			if (blockmiss) {
				d4stacknode *rptr = c->stack[setnumber].top->up;
				if (rptr->valid != 0) {
                    
#if (D4DEBUG)
					printf("\t\tdisplacing blockaddr: %llu\n", rptr->blockaddr);
#endif
                    d4memref tmp_m;
                    tmp_m.address = rptr->blockaddr;
                    tmp_m.size = mr.size;
                    if (c->isllc)
                        update_llc_mem_list(c, rptr->blockaddr, 0, vmid);
                        //d4replacemmap (c, tmp_m);
					if (!ronly && (rptr->valid & rptr->dirty) != 0)
						d4_wbblock (c, rptr, D4VAL (c, lg2subblocksize));
					if (c->stack[setnumber].n > D4HASH_THRESH)
						d4_unhash (c, setnumber, rptr);
					rptr->valid = 0;
				}
			}
		}

		/*
		 * Prepare reference for downstream cache.
		 * We do this for write-throughs, read-type misses,
		 * and fetches for incompletely written subblocks
		 * when a write misses and write-allocate is being used.
		 * In some cases, a write can generate two downstream references:
		 * a fetch to load the complete subblock and a write-through store.
		 *
		 * MODIFIED:
		 * Removed the check in the second conditional, the write allocate
		 * with write sizes that are not in subblock chunks. The behavior
		 * with the previous code version creates results that are not
		 * intuitive; write misses that are not in subblock chunk sizes
		 * will show up as read misses followed by write hits.
		 * The fix modifies the size for downstream caches to the size in
		 * terms of subblocks it does not create two references to do this.
		 */
		if (!ronly && atype == D4XWRITE && !wback) {
#if (D4DEBUG)
			printf("\t\tWRITING THROUGH\n");
#endif
			d4pendstack *newm = d4get_mref();
			newm->m = m; 
			newm->m.address = D4ADDR2SUBBLOCK (c, m.address);
			newm->m.size = D4REFNSB (c, m) << D4VAL (c, lg2subblocksize);
			newm->next = c->pending;
            newm->m.vmid = vmid;
			c->pending = newm;
#if (D4DEBUG)
			printf("\t\tpending reference created size%d\n", newm->m.size);
#endif
		}
		if (miss && (ronly || atype != D4XWRITE))/*|| (walloc && m.size != D4REFNSB (c, m) << D4VAL (c, lg2subblocksize))))*/ {
#if (D4DEBUG)
			printf("\t\tinside miss && (read || atype != D4XWRITE)\n");
#endif
			d4pendstack *newm = d4get_mref();
			/* note, we drop prefetch attribute */
			newm->m.accesstype = atype;
			newm->m.address = D4ADDR2SUBBLOCK (c, m.address);
			newm->m.size = D4REFNSB (c, m) << D4VAL (c, lg2subblocksize);
			newm->next = c->pending;
            newm->m.vmid = vmid;
			c->pending = newm;
#if (D4DEBUG)
			printf("\t\tpending reference created\n");
#endif
		}
	
		/*
		 * Do fully associative and infinite sized caches too.
		 * This allows classifying misses into {compulsory,capacity,conflict}.
		 * An extra "set" is provided (==c->numsets) for the fully associative
		 * simulation.
		 */
		if ((D4CUSTOM && D4_OPT (ccc)) || (!D4CUSTOM && (c->flags & D4F_CCC) != 0)) {
						/* set to use for fully assoc cache */
			const int fullset = D4VAL(c,numsets);
						/* number of blocks in fully assoc cache */
            /*printf("ccc, fullset=%d setsize=%d\n", fullset, c->stack[fullset-2].n);*/
			int fullmiss, fullblockmiss;	/* like miss and blockmiss, but for fully assoc cache */
	
			ptr = c->stack[fullset].top;
			if (ptr->blockaddr != blockaddr)
				ptr = d4_find (c, fullset, blockaddr);
			else if (ptr->valid == 0)
				ptr = NULL;
	
			fullblockmiss = (ptr == NULL);
			fullmiss = fullblockmiss || (sbbits & ptr->valid) != sbbits;
	
			/* take care of stack update */
			if (ronly || atype != D4XWRITE || !fullblockmiss || walloc) {
				ptr = D4VAL (c, replacementf) (c, fullset, m, ptr);
				assert (!fullblockmiss || ptr->valid == 0);
				ptr->valid |= sbbits;
			}
	
			int infmiss = 0; /* assume hit in infinite cache */
			/* classify misses */
			if (miss) {
				if (!fullmiss) { /* hit in fully assoc: conflict miss */
				  c->conf_miss[(int)m.accesstype]++;
				  if (vm_miss == 3) {
				    c->vm_comp_miss--;
				  }
				}
				else {
					infmiss = d4infcache (c, m);
					if (infmiss != 0) {/* first miss: compulsory */
					  c->comp_miss[(int)m.accesstype]++;
					}
					else	/* hit in infinite cache: capacity miss */
					  c->cap_miss[(int)m.accesstype]++;
				}
				if (blockmiss) {
				  if (!fullblockmiss) /* block hit in full assoc */
				    c->conf_blockmiss[(int)m.accesstype]++;
				  else if (infmiss == 1) /* block miss in full and inf */
				    c->comp_blockmiss[(int)m.accesstype]++;
				  else /* part of block hit in infinite cache */
				    c->cap_blockmiss[(int)m.accesstype]++;
				}
			} else {
			  if (vm_miss == 3) {
			    c->vm_comp_miss--;
			  }
			}
            
            if (c->isllc) {
                double demand_comp_alltype;
                demand_comp_alltype = c->comp_miss[D4XMISC]
                + c->comp_miss[D4XREAD]
                + c->comp_miss[D4XWRITE]
                +c->comp_miss[D4XINSTRN];
#if (D4DEBUG)
		if ((demand_comp_alltype !=  c->vm_comp_miss ) && c->isllc) {
		  printf("\t\tinconsistent vm_miss=%d, infmiss=%d\n", vm_miss, infmiss);
                }
                printf("\t\tvm_miss=%d, infmiss=%d\n", vm_miss, infmiss);
                printf("\t\tvm_comp_miss=%d\n", c->vm_comp_miss);
                printf("\t\tcomp_miss=%f\n", demand_comp_alltype);
#endif
            }

            
            
			/* take care of replaced block */
			if (fullblockmiss) {
				d4stacknode *rptr = c->stack[fullset].top->up;
				if (rptr->valid != 0) {
					if (c->stack[fullset].n > D4HASH_THRESH)
						d4_unhash (c, fullset, rptr);
					rptr->valid = 0;
				}
			}
		}
        
		/*
		 * Update non-ccc metrics. 
		 */
		c->fetch[(int)m.accesstype]++;
	
		if (miss) {
			c->miss[(int)m.accesstype]++;
			if (blockmiss)
				c->blockmiss[(int)m.accesstype]++;
		}
    
		/*
		 * Now make recursive calls for pending references
		 */
		if (c->pending)
			d4_dopending (c, c->pending, 0);
    }
}

#endif /* !D4CUSTOM || D4_REF_ONCE>1 */
#undef D4_REF_ONCE
#define D4_REF_ONCE 2	/* from now on, skip the first stuff and do the rest */
