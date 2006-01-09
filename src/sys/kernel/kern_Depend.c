/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, Strawberry Development Group
 *
 * This file is part of the EROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <kerninc/kernel.h>
#include <arch-kerninc/KernTune.h>
#include <kerninc/Activity.h>
#include <kerninc/Depend.h>
#include <kerninc/Check.h>
#include <kerninc/Node.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Machine.h>
#include <kerninc/util.h>
#include <kerninc/PhysMem.h>
#include <kerninc/KernStats.h>
#include <arch-kerninc/PTE.h>

/* KeyDependEntry_Invalidate is in the architecture-specific file Mapping.c. */

#define dbg_alloc	0x1u
#define dbg_reuse	0x2u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* #define DEPEND_DEBUG */
const uint32_t nhash = 256;

/* These point to the entire cache: */
static KeyDependEntry* KeyDependTable;

/* LRU counters for each cache set: */
uint32_t *KeyDependLRU;
static uint64_t *KeyDependStats;

/* The ideal bucket size is one that allows for heavy sharing of
 * objects, so shoot for MaxThreads/4 as roughly the right bucket
 * size.  If you are actually sharing the same object over 1/4 of your
 * threads, something is probably very wrong.
 */

#if 0
const uint32_t KeyBucketSize = KTUNE_NTHREAD / 4;
#else
const uint32_t KeyBucketSize = 33;
#endif

uint32_t KeyBuckets;

#define keybucket_ndx(pk) ((((uint32_t) pk) / sizeof(Key)) % KeyBuckets)

/* Approximately half of the Nodes in the system will be used in
 * memory contexts.  Of these, perhaps 25% will be shared:
 */
void
Depend_InitKeyDependTable(uint32_t nNodes)
{
  kpsize_t len;
  uint32_t nEntries = nNodes * 8;
  nEntries += nEntries / 4;	/* extra 25% */
  
  if (nEntries % KeyBucketSize) {
    nEntries += KeyBucketSize;
    nEntries -= nEntries % KeyBucketSize;
  }

  KeyBuckets = nEntries / KeyBucketSize;

  len = KeyBuckets*sizeof(uint32_t);
  KeyDependLRU = (uint32_t *)KPAtoP(void *, physMem_Alloc(len, &physMem_any));
  bzero(KeyDependLRU, len);

  len = KeyBuckets*sizeof(uint64_t);
  KeyDependStats = (uint64_t *)KPAtoP(void *, physMem_Alloc(len, &physMem_any));
  bzero(KeyDependStats, len);

  DEBUG(alloc)
    printf("Allocated KeyDependLRU: 0x%x at 0x%08x\n",
		   sizeof(uint32_t[KeyBuckets]), KeyDependLRU);

  len = nEntries*sizeof(KeyDependEntry);
  KeyDependTable = (KeyDependEntry *)KPAtoP(void *, physMem_Alloc(len, &physMem_any));
  bzero(KeyDependTable, len);

  DEBUG(alloc)
    printf("Allocated KeyDependTable: 0x%x at 0x%08x\n",
		   sizeof(KeyDependEntry[nEntries]), KeyDependTable);
}

void
Depend_AddKey(Key *pKey, PTE* pte, bool allowMerge)
{
#ifndef NDEBUG
  kva_t mapping_page_kva;
  ObjectHeader *pMappingPage = 0;
  bool reallyAllowMerge;
#endif
  uint32_t whichBucket;
  KeyDependEntry* entry = 0;
  uint32_t i;

  assert(KeyBuckets);
  assert(KeyBucketSize);

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Depend_AddKey(): top");
#endif

  keyBits_SetWrHazard(pKey);
  
#ifndef NDEBUG
  mapping_page_kva = ((kva_t)pte & ~EROS_PAGE_MASK);
  pMappingPage = objC_PhysPageToObHdr(VTOP(mapping_page_kva));
  reallyAllowMerge = pMappingPage ? true : false;

  if (reallyAllowMerge != allowMerge)
    fatal("pPTE=0x%08x pObj=0x%08x -- merge bug!\n", pte, pMappingPage);
#endif

  assert(reallyAllowMerge == allowMerge);

#ifdef DEPEND_DEBUG
  printf("Add slot depend entry for slot=0x%08x pte=0x%08x: ",
	       pKey, pte);
#endif

  whichBucket = keybucket_ndx(pKey);
  entry = &KeyDependTable[whichBucket * KeyBucketSize];

  for (i = 0; i < KeyBucketSize; i++) {
    if (entry[i].slotTag == SLOT_TAG(pKey)) {
      PTE *curStart = entry[i].start;
      PTE *curEnd =   entry[i].start + entry[i].pteCount;

      /* If the current start matches, we are done in all cases: */
      if (curStart == pte) {
#ifdef DBG_WILD_PTR
	if (dbg_wild_ptr)
	  check_Consistency("Depend_AddKey(): start matches PTE");
#endif
	return;			/* already got it */
      }

      if ( allowMerge && pte_CanMergeWith(entry[i].start, pte) ) {
	KernStats.nDepMerge++;
	if ( (kva_t) pte < (kva_t) curStart ) {
	  entry[i].start = pte;
	  entry[i].pteCount = curEnd - pte;
	}
	else if ( (kva_t) pte >= (kva_t) curEnd ) {
	  entry[i].pteCount = ( (pte - curStart) + 1);
	}
        
	/* otherwise falls within existing range -- do nothing */
#ifdef DEPEND_DEBUG
	printf("merged\n");
#endif
#ifdef DBG_WILD_PTR
	if (dbg_wild_ptr)
	  check_Consistency("Depend_AddKey(): post-merge");
#endif
	return;
      }
    }
  }
#ifdef DEPEND_DEBUG
  printf("new\n");
#endif

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Depend_AddKey(): allocate new entry");
#endif
    
  /* Allocate new entry. */
  KernStats.nDepend++;
  entry += KeyDependLRU[whichBucket];
  KeyDependLRU[whichBucket]++;
  KeyDependLRU[whichBucket] %= KeyBucketSize;

  /* We only want to measure the distributions of unmerged
   * allocations.  Mergeable buckets are a good thing, and would only
   * serve to bias the histogram BADLY.
   */
  KeyDependStats[whichBucket]++;

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Depend_AddKey(): post select LRU");
#endif
    
  DEBUG(reuse)
    if (entry->start)
      dprintf(true, "Reusing depend entry at 0x%08x\n"
		      "start = 0x%08x count= 0x%08x\n",
		      entry, entry->start, entry->pteCount);
  
  if (KeyDependEntry_InUse(entry))
    KeyDependEntry_Invalidate(entry);

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Depend_AddKey(): after invalidate");
#endif
    
  entry->start = pte;
  entry->pteCount = 1;
  entry->slotTag = SLOT_TAG(pKey);

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Depend_AddKey(): after new entry");
#endif
}

void
Depend_InvalidateKey(Key* pKey)
{
  bool didZap = false;
  uint32_t i;
  
#ifdef DEPEND_DEBUG
  printf("Invalidating depend entries for key=0x%08x\n", pKey);
#endif

  uint32_t whichBucket = keybucket_ndx(pKey);
  KeyDependEntry* bucket
    = &KeyDependTable[whichBucket * KeyBucketSize];

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    if (check_Contexts("Before really zapping") == false)
      halt('a');
#endif

  for (i = 0; i < KeyBucketSize; i++) {
    if (bucket[i].slotTag == SLOT_TAG(pKey)) {
      didZap = true;

      KeyDependEntry_Invalidate(&bucket[i]);
#ifdef DBG_WILD_PTR
      if (dbg_wild_ptr) {
	if (check_Contexts("after zap") == false) {
	  printf("Depend start 0x%08x, count 0x%x\n",
			 bucket[i].start, bucket[i].pteCount);
	  halt('b');
	}
      }
#endif
    }
  }

  if (didZap) {
    KernStats.nDepZap++;
    UpdateTLB();
  }
}

#ifdef OPTION_DDB
void
Depend_ddb_dump_hist()
{
  uint32_t i;
  extern void db_printf(const char *fmt, ...);

  printf("Usage counts for depend buckets:\n");
  for (i = 0; i < KeyBuckets; i++)
    printf("Bucket %d: uses 0x%08x%08x\n", i,
	   (uint32_t) (KeyDependStats[i]>>32),
	   (uint32_t) (KeyDependStats[i]));
}

void
Depend_ddb_dump_bucket(uint32_t ndx)
{
  extern void db_printf(const char *fmt, ...);

  if (ndx >= KeyBuckets) {
    printf("No such bucket.\n");
  }
  else {
    KeyDependEntry* bucket
      = &KeyDependTable[ndx * KeyBucketSize];
    uint32_t i;

    for (i = 0; i < KeyBucketSize; i++) {
      if (bucket[i].start)
	printf("%3d: PTE=0x%08x count=%4d tag=0x%x\n",
	       i,
	       bucket[i].start,
	       bucket[i].pteCount,
	       bucket[i].slotTag);
    }
  }
}
#endif
