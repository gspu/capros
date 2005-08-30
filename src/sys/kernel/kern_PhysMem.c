/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group
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

#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/PhysMem.h>
/*#include <disk/DiskKey.hxx>*/
#include <kerninc/BootInfo.h>
     
#define dbg_init	0x1u
#define dbg_avail	0x2u
#define dbg_alloc	0x4u
#define dbg_new		0x8u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Implementation of Memory Constraints */
/* physMem_pages: must be page aligned */
PmemConstraint physMem_pages = { (kpa_t) 0u, ~((kpa_t)0u), EROS_PAGE_SIZE };
/* physMem_pages: must be word aligned */
PmemConstraint physMem_any = { (kpa_t) 0u, ~((kpa_t)0u), sizeof(uint32_t) };

extern void end();


kpa_t physMem_PhysicalPageBound = 0;

static PmemInfo PhysicalMemoryInfo[MAX_MEMINFO];
PmemInfo *physMem_pmemInfo = &PhysicalMemoryInfo[0];
unsigned long physMem_nPmemInfo;

/* The area we will reserve as ROM: */
kpa_t ROMBase  =  0xc0000;
kpa_t ROMBound = 0x100000;

static void checkBounds(kpa_t base, kpa_t bound)
{
  if (bound > physMem_PhysicalPageBound) {        /* take max */
    physMem_PhysicalPageBound = align_up(bound, EROS_PAGE_SIZE);
  }

  /* Check if it overlaps ROM area. */
  if (   base < ROMBound
      && bound > ROMBase ) {
    /* We don't allow memory at 0xFFFFF, only ROM. */
    if (bound >= ROMBound) {
      printf("base=0x%x, bound=0x%x\n",
             (unsigned long)base, (unsigned long)bound);
      fatal("ROM conflict\n");
    } else {
      /* Truncate ROM area so as not to overlap. */
      ROMBase = bound;
    }
  }
}

void
physMem_Init()
{
  unsigned i;

  for (i = 0; i < BootInfoPtr->nMemInfo; i++) {
    MemInfo *pmi = &BootInfoPtr->memInfo[i];

    if (pmi->type == MI_MEMORY) {
      (void) physMem_AddRegion(pmi->base, pmi->bound, MI_MEMORY, false);

      checkBounds(pmi->base, pmi->bound);
    }
  }

  /* Get upper bound of preloaded memory. */
  for (i = 0; i < BootInfoPtr->nDivInfo; i++) {
    DivisionInfo *di = &BootInfoPtr->divInfo[i];

    checkBounds(di->where, di->bound);
  }

  (void) physMem_AddRegion(ROMBase, ROMBound, MI_BOOTROM, true);

  DEBUG(init) physMem_PrintStatus();

  physMem_ReservePhysicalMemory();
}

PmemInfo *
physMem_AddRegion(kpa_t base, kpa_t bound, uint32_t type, bool readOnly)
{
  PmemInfo *kmi = &physMem_pmemInfo[physMem_nPmemInfo];
  unsigned int i = 0;

  if (type == MI_DEVICEMEM) {
    /* Do not do this check for the bootup cases, as some of those
     * actually do overlap. */
    for (i = 0; i < physMem_nPmemInfo; i++) {
      if (base >= physMem_pmemInfo[i].base && base < physMem_pmemInfo[i].bound)
	return 0;

      if (bound > physMem_pmemInfo[i].base && bound <= physMem_pmemInfo[i].bound)
	return 0;
    }
  }

  assert (physMem_nPmemInfo < MAX_MEMINFO);

  kmi->base = base;
  kmi->bound = bound;
  kmi->type = type;
  kmi->nPages = 0;
  kmi->basepa = 0;
  kmi->firstObHdr = 0;
  kmi->readOnly = readOnly;

  if (type == MI_MEMORY) {
    kmi->allocBase = base;
    kmi->allocBound = bound;
  }
  else {
    kmi->allocBase = 0;
    kmi->allocBound = 0;
  }

  physMem_nPmemInfo++;

  return kmi;
}

kpsize_t
physMem_TotalPhysicalPages()
{
  return physMem_PhysicalPageBound / EROS_PAGE_SIZE;
}

kpsize_t
physMem_MemAvailable(PmemConstraint *mc, unsigned unitSize, bool contiguous)
{
  unsigned nUnits = 0;
  unsigned rgn = 0;

  for (rgn = 0; rgn < BootInfoPtr->nMemInfo; rgn++) {
    PmemInfo *kmi = &physMem_pmemInfo[rgn];
    uint32_t base = kmi->allocBase;
    uint32_t bound = kmi->allocBound;
    unsigned unitsHere;

    if (kmi->type != MI_MEMORY)
      continue;

    base = max(base, mc->base);
    bound = min(bound, mc->bound);

    if (base >= bound)
      continue;

    /* The region (partially) overlaps the requested region */
    base = align_up(base, mc->align);

    unitsHere = (bound - base) / unitSize;

    if (contiguous) {
      if (nUnits < unitsHere)
	nUnits = unitsHere;
    }
    else
      nUnits += unitsHere;
  }

  DEBUG(avail) {
    printf("%d units of %d %% %d %s bytes available\n",
		   nUnits, unitSize, mc->align, 
		   contiguous ? "contiguous" : "total");
  }

  return nUnits;
}

/* Preferentially allocate out of higher memory, because legacy DMA
 * controllers tend to have restricted addressable bounds in physical
 * memory. */
static PmemInfo *
preferred_region(PmemInfo *rgn1, PmemInfo *rgn2)
{
  if (rgn1 == 0)
    return rgn2;

  if (rgn2 && rgn1->base < rgn2->base)
    return rgn2;

  return rgn1;
}

PmemInfo *
physMem_ChooseRegion(kpsize_t nBytes, PmemConstraint *mc)
{
  PmemInfo *allocTarget = 0;
  unsigned rgn = 0;

  for (rgn = 0; rgn < BootInfoPtr->nMemInfo; rgn++) {
    PmemInfo *kmi = &physMem_pmemInfo[rgn];
    kpa_t base = kmi->allocBase;
    kpa_t bound = kmi->allocBound;
    kpa_t where;

    if (kmi->type != MI_MEMORY)
      continue;

    base = max(base, mc->base);
    bound = min(bound, mc->bound);

    if (base >= bound)
      continue;

    /* The region (partially) overlaps the requested region. See if it
     * has enough suitably aligned space: */
    
    where = bound - nBytes;
    where = align_down(where, mc->align);

    if (where >= base)
      allocTarget = preferred_region(allocTarget, kmi);
  }

  return allocTarget;
}

/* Allocate nBytes from available physical memory with constraint mc.
   nBytes must be a multiple of mc->align. */
kpa_t
physMem_Alloc(kpsize_t nBytes, PmemConstraint *mc)
{
  PmemInfo *allocTarget;

  assert(((unsigned)nBytes & (mc->align - 1)) == 0);  

  DEBUG(alloc)
     printf("PhysMem::Alloc: nBytes=0x%x, "
                    "mc->base=0x%x, mc->bound=0x%x, mc->align=0x%x\n",
                    (unsigned)nBytes, (unsigned)mc->base,
                    (unsigned)mc->bound, mc->align);

  allocTarget = physMem_ChooseRegion(nBytes, mc);

  if (allocTarget) {
    /* We are willing to waste space at either the beginning or the end
       if necessary for alignment. */
    kpa_t allocTargetBaseAligned = align_up(allocTarget->allocBase, mc->align);
    kpa_t allocTargetBoundAligned = align_down(allocTarget->allocBound,
                                               mc->align);

    /* Apply the constraint. */
    kpa_t base  = max(allocTargetBaseAligned, mc->base);
    kpa_t bound = min(allocTargetBoundAligned, mc->bound);

    /* We will only grab from the end or from the beginning -- not
     * from the middle. In fact, we will only grab from the beginning
     * if we absolutely have to... */

    kpa_t where
        /* The following value is used in the "grab from end" case.
           We assign it here to avoid a compiler warning. */
        = allocTargetBoundAligned - nBytes;
    if (bound == allocTargetBoundAligned) { /* grab from end */
      /* where = allocTargetBoundAligned - nBytes; */
      allocTarget->allocBound = where;
    } else if (base == allocTargetBaseAligned) { /* grab from beginning */
      where = allocTargetBaseAligned;
      allocTarget->allocBase = where + nBytes;
    } else {
      printf("0x%x 0x%x 0x%x 0x%x\n",
                     (unsigned)allocTarget->allocBase,
                     (unsigned)allocTarget->allocBound,
                     (unsigned)mc->base, (unsigned)mc->bound );
      fatal("Physical memory allocator will not split memory regions\n");
    }

    assert(where >= base);
    assert(where + nBytes <= bound);
      
    DEBUG(alloc)
      printf("Alloc %u %% %u at 0x%08x. "
		     "Rgn 0x%08x now [0x%08x,0x%08x)\n", 
		     (uint32_t) nBytes, mc->align, (uint32_t) where, 
		     allocTarget, 
		     (unsigned) allocTarget->allocBase, 
		     (unsigned) allocTarget->allocBound);

    return where;
  }

  return 0;
}

#ifdef OPTION_DDB
void
physMem_ddb_dump()
{
  unsigned rgn = 0;
  extern void db_printf(const char *fmt, ...);

  for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
    PmemInfo *kmi = &physMem_pmemInfo[rgn];

    kpa_t base = kmi->allocBase;
    kpa_t bound = kmi->allocBound;
    unsigned long unallocated = bound - base;
    unsigned long total = kmi->bound - kmi->base;

    printf("Rgn 0x%08x ty=%d: [0x%08x,0x%08x) %u/%u bytes available\n", 
	   kmi,
	   kmi->type,
	   (unsigned long) kmi->base,
	   (unsigned long) kmi->bound,
	   unallocated,
	   total);
    if (kmi->firstObHdr) {
      printf("  allocBase 0x%08x%08x allocBound 0x%08x%08x\n",
	     (unsigned long) (kmi->allocBase >> 32),
	     (unsigned long) (kmi->allocBase),
	     (unsigned long) (kmi->allocBound >> 32),
	     (unsigned long) (kmi->allocBound));
      printf("  nPages 0x%08x (%d) basepa 0x%08x firstObHdr 0x%08x\n",
	     kmi->nPages,
	     kmi->nPages,
	     kmi->basepa,
	     kmi->firstObHdr);
    }
  }
}
#endif

#define NEW_STOP false
extern void *malloc(size_t sz);

#ifndef __KERNEL__
void *
operator new[](size_t sz, void *place)
{
  void *v = 0;
  
  if (place == 0)
    v = KPAtoP(void *, physMem_Alloc(sz, &physMem_any));
  else if (place == (void *) 1)
    v = malloc(sz);

  DEBUG(new)
    dprintf(NEW_STOP, "placement vec new grabs "
		    "0x%x (%d) at 0x%08x\n", sz,
		    sz, v);

  return v;
}

void *
operator new(size_t sz, void * place)
{
  void *v = 0;
  
  if (place == 0)
    v = KPAtoP(void *, physMem_Alloc(sz, &physMem_any));
  else if (place == (void *) 1)
    v = malloc(sz);
    
  DEBUG(new)
    dprintf(NEW_STOP, "placement new grabs "
		    "0x%x (%d) at 0x%08x\n", sz,
		    sz, v);

  return v;
}

void *
operator new(size_t sz)
{
  fatal("Inappropriate call to non-placement operator new\n");

  return KPAtoP(void *, physMem_Alloc(sz, &physMem_any));
}

void *
operator new [](size_t sz)
{
  fatal("Inappropriate call to non-placement operator vector new\n");
  return KPAtoP(void *, physMem_Alloc(sz, &physMem_any));
}
#endif /*__KERNEL__*/
