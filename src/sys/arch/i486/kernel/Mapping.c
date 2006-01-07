/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, Strawberry Development Group.
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
#include <kerninc/Process.h>
#include <kerninc/PhysMem.h>
#include <kerninc/Depend.h>
#include <kerninc/Machine.h>
#include <kerninc/util.h>
#include <arch-kerninc/PTE.h>
#include "CpuFeatures.h"
#include "lostart.h"

extern void etext();
extern void end();
extern void start();

PTE* KernPageDir /* = (PTE*) xKERNPAGEDIR */;
kpmap_t KernPageDir_pa /* = xKERNPAGEDIR */;

#ifdef OPTION_SMALL_SPACES
#include <kerninc/Invocation.h>
void 
proc_WriteDisableSmallSpaces()
{
  uint32_t nFrames = KTUNE_NCONTEXT / 32;
  PTE *pte = proc_smallSpaces;
  unsigned i = 0;
  
  for (i = 0; i < nFrames * NPTE_PER_PAGE; i++)
    pte_WriteProtect(&pte[i]);
}

static void
MakeSmallSpaces()
{
  uint32_t nFrames = 0;
  PTE *pageTab = 0;
  uint32_t vaddr = 0;
  uint32_t dirndx = 0;
  uint32_t i = 0;
  PTE *pageDir = 0;

  assert (KTUNE_NCONTEXT % 32 == 0);

  /* Allocate the page *tables* for the small spaces. */
  
  nFrames = KTUNE_NCONTEXT / 32;
  
  proc_smallSpaces = 
    KPAtoP(PTE *,physMem_Alloc(nFrames * EROS_PAGE_SIZE, &physMem_pages));

  bzero(proc_smallSpaces, nFrames * EROS_PAGE_SIZE);

  assert (((uint32_t)proc_smallSpaces & EROS_PAGE_MASK) == 0);

  pageTab = proc_smallSpaces;
  
  /* Insert those page tables into the master kernel map, from which
     they will be copied to all other spaces. */
  
  vaddr = UMSGTOP;
  dirndx = (vaddr >> 22);

  pageDir = KernPageDir;

  for (i = 0; i < nFrames; i++) {
    pte_set(&pageDir[dirndx], (VTOP(pageTab) & PTE_FRAMEBITS) );
    pte_set(&pageDir[dirndx], PTE_W|PTE_V|PTE_ACC|PTE_USER);

    pageTab += NPTE_PER_PAGE;
    dirndx++;
  }
}
#endif
  
#ifdef WRITE_THROUGH
#define RWMODE (PTE_W|PTE_WT)
#else
#define RWMODE PTE_W
#endif

/* FIX: This will need to turn into something more complex if Intel
   ever decides to build SMP machines in which only some processors
   support the global page feature. */
static unsigned GlobalPage = 0u; /* or PTE_GLBL if supported */

#ifdef NEW_KMAP
static inline kpa_t 
GetKernelPageTable(kva_t va)
{
  PTE *pageDir = KernPageDir; /* kernel page directory */
  uint32_t dirndx = (KVTOL(va) >> 22);
  return pte_PageFrame(&pageDir[dirndx]);
}
#endif

static void 
MapKernelPage(kva_t va, kpa_t pa, uint32_t mode)
{
  PTE *pageDir = KernPageDir; /* kernel page directory */
  PTE *pageTab;

  uint32_t tabndx;
  uint32_t dirndx = (KVTOL(va) >> 22);

  if (mode & PTE_PGSZ) {
    pte_Invalidate(&pageDir[dirndx]);
    pte_set(&pageDir[dirndx], (VTOP(pa) & PTE_FRAMEBITS) );
    pte_set(&pageDir[dirndx], mode);

    return;
  }

  tabndx = (KVTOL(va) >> 12) & 0x3ffu;

  if ( pte_is(&pageDir[dirndx], PTE_V) ) {
    pageTab = (PTE *) PTOV(pte_PageFrame(&pageDir[dirndx]));
  }
  else {
    /* Allocate a new page table. Allocating here also has the
     * property that all of the pages above those allocated for use
     * by the kernel will be contiguous in virtual space.
     */
    pageTab = KPAtoP(PTE *,physMem_Alloc(EROS_PAGE_SIZE, &physMem_pages));
    bzero(pageTab, EROS_PAGE_SIZE);

    assert (((uint32_t)pageTab & EROS_PAGE_MASK) == 0);
      
    /* pageTab = ::new PTE[NPTE_PER_PAGE]; */

#if 0
    printf("Allocated new page table at 0x%x, dirndx 0x%x\n",
		   pageTab, dirndx);
#endif
    
    pte_set(&pageDir[dirndx], (VTOP(pageTab) & PTE_FRAMEBITS) );
    pte_set(&pageDir[dirndx], PTE_W | PTE_V | mode);
  }

  pte_Invalidate(&pageTab[tabndx]);
  pte_set(&pageTab[tabndx], (pa & PTE_FRAMEBITS) );
  pte_set(&pageTab[tabndx], mode);
}

#ifdef NEW_KMAP
/* A "mapping window" is a region in the kernel virtual address map
   into which we will map and unmap pages at need. The "window"
   consists of two parts:

   1. A set of pre-allocated PTEs (that is, PTEs whose containing page
   tables are known to be present and valid). These effectively define
   the virtual addresses at which the mapped page frames will apper.

   2. A set of mappings **of these PTEs** at a known kernel virtual
   address. These mappings are used by the kernel to rewrite the PTEs
   as needed to map, unmap, or change the permissions of mapped
   frames. 

   The entire mapping window notion is new -- the kernel didn't need
   this until physical memories grew larger than the available kernel
   map. As this trend is expected to continue for the forseeable
   future, it seems regrettably likely that the mapping window idea is
   here to stay.

   FIX: the mapping window implementation itself is probably machine
   independent, and we should consider relocating the code accordingly
   when we do a second architecture port. The only real assumption
   made here is that the created mapping window and it's PTEs should
   appear at sequential virtual addresses. Hmm. It also makes the
   assumption that PTEs are organized into page frames, which is not
   true on (e.g.) the PowerPC (unless matters are so arrange by
   software), and is not necessarily true on Pentium family processors
   when PPE is enabled -- need to burn that bridge someday.

   MakeMappingWindow returns the base address of the next available
   kernel virtual page.

   The following structure simulates an array of mapping windows. We
   handle it this way because there is no clean way in C to statically
   declare an initialized array of structs when the size of the struct
   is known only at compile time. The /base/ and /lru/ vectors 
*/

typedef struct MappingWindow MappingWindow;
struct MappingWindow {
  unsigned lru;			/* index of next entry to (try to) allocate */
  unsigned nmap;		/* number of current mappings */
  kva_t addr;
  PTE *pte;
  unsigned wsz;			/* number of logical subwindows */
  unsigned lazy;		/* whether to do lazy invalidations */
  kpa_t *pa;
  uint8_t *isfree;
};

#define DEFWINDOW(nm,wsz,lazy) \
  static kpa_t nm##_pamap[wsz]; \
  static uint8_t nm##_isfree[wsz]; \
  MappingWindow nm = { 0, 0, 0, 0, wsz, lazy, nm##_pamap, nm##_isfree }

/* The PageDirWindow is a single window shared across all processors,
   and is used to map page directories. We do aggressive invalidations
   on this directory. */
DEFWINDOW(PageDirWindow_s, KTUNE_NCONTEXT, 0);
MappingWindow *PageDirWindow = &PageDirWindow_s;

/* The TempMapWindow will be used in per-CPU fashion. We will
   accomplish this by splitting the window into NCPU windows each of
   length 32 after the kernel heap has been initialized. We define it
   here as a single larger window because the required array cannot be
   statically initialized conveniently in C unless some upper limit on
   NCPU is fixed, and I don't want to do that.

   The choice of 32 slots per CPU is arbitrary, but here is a "back of
   envelope" rationale:

   IPC currently requires at most 2 page tables, as transferred
   string might conceivably span 2. Note we will reference these
   only when building mappings.

   bzero/bcopy require max 2 at any given time.

   depend zapper requires max 1 at any given time.

   32 is comfortably large enough so that we will not be whacking the
   TLB with excessive frequency.
*/

DEFWINDOW(TempMapWindow_s, NCPU *32, 1);
MappingWindow *TempMapWindow = &TempMapWindow_s;

kva_t
mach_winmap(MappingWindow* mw, kva_t lastva, kpa_t pa)
{
  unsigned slot;

  /* See if the previously mapped VA still maps this pa. If so, we
     may not need to grab a new slot. */
  if (mw->lazy && lastva % EROS_PAGE_SIZE == 0) {
    slot = (lastva - mw->addr) / EROS_PAGE_SIZE;
    if (mw->isfree[slot] && mw->pa[slot] == pa) {
      mw->isfree[slot] = 0;
      mw->nmap++;
      return lastva;
    }
  }

  /* Until our windows get significantly bigger, brute search over the
     freeMap is reasonable. 1024/32 directories == 32, which isn't at
     all bad. */
 try_again:
  for ( ; mw->lru < mw->wsz; mw->lru++)
    if (mw->isfree[mw->lru])
      break;

  if (mw->lru == mw->wsz) {
    assert(mw->nmap < mw->wsz);

    /* No slot was free within the remaining slots in the window, but
       there are free slots below the point where we started the
       current search. If this is a lazy window, we need to invalidate
       the TLB before we can safely use them, because there may be
       residual mappings left in the TLB for these entries. */
    if (mw->lazy)
      mach_FlushTLB();

    mw->lru = 0;
    goto try_again;
  }

  /* We have a slot. The mapping is always RW, even if that isn't
     strictly needed. This is so that subsequent remappings with
     possibly different permissions won't get us into trouble. */
  slot = mw->lru;
  mw->pa[slot] = pa;
  mw->isfree[slot] = 0;
  mw->nmap++;

  pte_set(mw->pte[slot], (pa & PTE_FRAMEBITS) );
  pte_set(mw->pte[slot], RWMODE | PTE_V);

  return (mw->addr + slot * EROS_PAGE_SIZE);
}

void
mach_winunmap(MappingWindow* mw, kva_t va)
{
  unsigned slot;

  slot = (va - mw->addr) / EROS_PAGE_SIZE;
  mw->isfree[slot] = -1;
  mw->nmap--;

  if (mw->lazy == 0)
    mach_FlushTLBWith(va);
}

static kva_t
InitMappingWindow(MappingWindow *mw, kva_t addr)
{
  unsigned i;
  kpa_t lastMapPage = 0;
  unsigned nFrames = mw->wsz;
  mw->addr = addr;

  for (i = 0; i < nFrames; i++, addr++) {
    mw->isfree[i] = 1;
    mw->pa[i] = -1;
  }

  for (i = 0; i < nFrames; i++, addr++)
    MapKernelPage(addr, 0, RWMODE|GlobalPage|PTE_V);

  mw->pte = KVAtoV(PTE *, addr);

  for (i = 0; i < nFrames; i++) {
    kpa_t mapPageAddr = 
      GetKernelPageTable(mw->addr + i * EROS_PAGE_SIZE);
    if (mapPageAddr != lastMapPage) {
      MapKernelPage(addr, mapPageAddr, RWMODE|GlobalPage|PTE_V);
      lastMapPage = mapPageAddr;
      addr += EROS_PAGE_SIZE;
    }
  }

  /* We now have the window PTEs mapped, but the value of winMap is
     incorrect. It points to the base of the correct table, but we
     want it to contain the virtual address of the first PTE. */
  mw->pte += 
    ((VtoKVA(mw->pte) / EROS_PAGE_SIZE) % NPTE_PER_PAGE);

  return addr;
}
#endif /* NEW_KMAP */

/* Reserve machine-specific memory. */
void
physMem_ReservePhysicalMemory()
{
  PmemConstraint constraint;

  /* Master kernel directory */
  constraint.base = KERNPBASE;	/* no particular reason for this value */
  constraint.bound = 0x01000000; /* no particular reason for this value */
  constraint.align = EROS_PAGE_SIZE;

  KernPageDir_pa = physMem_Alloc(EROS_PAGE_SIZE, &constraint);
  KernPageDir = KVAtoV(PTE *, KernPageDir_pa);
  printf("Grabbed kernel directory at 0x%08x\n", (uint32_t) KernPageDir_pa);
}


/* Build a kernel mapping table.  In the initial construction, we
   create TWO mappings for the kernel.  The first begins at VA=0x0,
   and is used until we get a chance to set up a new global descriptor
   table.  The second begins at KVA, and goes into effect after we set
   up a global descriptor table for the kernel.

   Since the page table entries for the two mappings are identical,
   the duplication of mapping has essentially no cost.  There is also
   no need to undo the duplication, as all of the mappings in question
   are supervisor-only.
   */
#ifdef NEW_KMAP
kpa_t DirectoryCache;
PTE * DirectoryPTECache;

void
i486_BuildKernelMap()
{

  /* On entry to MapKernel we are running unmapped */
  unsigned paddr = 0;

  PTE *pageDir = KernPageDir; /* kernel page directory */

#ifndef NO_GLOBAL_PAGES
  if (CpuIdHi >= 1 && CpuFeatures & CPUFEAT_PGE)
    GlobalPage = PTE_GLBL;
#endif

  bzero(pageDir, EROS_PAGE_SIZE);

  /* Note that the master kernel mapping page itself is mapped
     read-only. We would not map it at all, but we need to be able to
     copy entries from it when creating per-page directories. By the
     time we have exited from the BuildKernelMap() procedure, we will
     be done with all modifications to the master kernel mapping
     page.  */
  MapKernelPage(VtoKVA(KernPageDir), KernPageDir_pa, GlobalPage|PTE_V);

  /* Create valid mappings for kernel code, data, bss. */
  for (paddr=align_down(PtoKPA(start), EROS_PAGE_SIZE); 
       paddr < align_up(PtoKPA(end), EROS_PAGE_SIZE); paddr += EROS_PAGE_SIZE) {
    uint32_t mode = GlobalPage | PTE_V;

    if (paddr >= align_down(PtoKPA(etext), EROS_PAGE_SIZE))
      mode |= RWMODE;

    /* Hand-protect kernel page zero, which holds the kernel page
     * directory, but is never referenced by virtual addresses once
     * we load the new mapping table pointer:
     */
    if (paddr == 0)
      mode = 0;

    MapKernelPage(paddr, paddr, mode);
  }

  /* Create a mapping for the kernel page directory: */
  {
    paddr = align_up(paddr, EROS_PAGE_SIZE);
    MapKernelPage(paddr, paddr, RWMODE|GlobalPage|PTE_V);
    paddr += EROS_PAGE_SIZE;
  }

  /* Create mappings for the per-CPU stacks. 
     N.B.: CPU0 stack is the array kernStack in bss. */
  {
    uint32_t limit = NCPU * EROS_KSTACK_SIZE;
    paddr = align_up(paddr, EROS_KSTACK_SIZE);
    limit += paddr;

    for (; paddr < limit; paddr += EROS_PAGE_SIZE)
      MapKernelPage(paddr, paddr, RWMODE|GlobalPage|PTE_V);
  }

  /* Create mappings for the context caches. These were previously
     allocated from the heap, but with the new "virtual registers"
     extension we will need to be able to map this region as a
     user-accessable region, so we need to add do it here where we can
     map it as page frames rather than as bytes. 

     Making the context cache entries accessable from user mode
     certainly SEEMS like a security violation, but it isn't. While there
     is definitely security sensitive state in the context cache, the
     context cache region is rendered inaccessable by the segmentation
     mechanism. We will then separately enable access in a given
     process to a small, security-insensitive subregion that contains
     the pseudo-registers. */
  {
    extern Process *proc_ContextCache;
    uint32_t bound = align_up(KTUNE_NCONTEXT * sizeof(Process), EROS_PAGE_SIZE);
    bound += paddr;

    /* Create mappings for the context cache. */
    for ( ; paddr < bound; paddr += EROS_PAGE_SIZE) {
      MapKernelPage(paddr, paddr, RWMODE|GlobalPage|PTE_V|PTE_USER);
      printf("Mapped ctxt cache at 0x%08x\n", paddr);
    }

    /* Initialize the proc_ContextCache pointer here. If it is NOT
       initialized, then the context cache initialization logic would
       allocate it from the heap. */
    proc_ContextCache = KPAtoP(Process *, paddr);
  }

  /* Create the mapping windows that we will used for page directories
     and for ephemeral mappings. */
  paddr = InitMappingWindow(PageDirWindow, paddr);
  paddr = InitMappingWindow(TempMapWindow, paddr);

#ifdef OPTION_SMALL_SPACES
  MakeSmallSpaces();
#endif
}
#else	/* not NEW_KMAP (this is the case) */
void
i486_BuildKernelMap()
{
  PTE *pageDir = KernPageDir; /* kernel page directory */
  uint32_t physPages;
  uint32_t increment = 0;
  uint32_t i = 0;
  unsigned heap_first_page;
#ifndef NO_LARGE_PAGES
  bool supports_large_pages = false;
#endif
  extern kva_t heap_start;
  extern kva_t heap_end;
  extern kva_t heap_defined;
  extern kva_t heap_bound;
  unsigned nPages;
  unsigned va;
  
  bzero(pageDir, EROS_PAGE_SIZE);

  /* The kernel mapping table must include all of the physical pages,
   * including the ramdisk if any.
   *
   * FIX: This is broken, as the kernel will soon need to support
   * machines where there are more physical pages than there are
   * virtual pages.
   */
  physPages = physMem_TotalPhysicalPages();

  /*  printf("pageDir: 0x%x\n", pageDir); */

  /* Computation of the virtual address must user paddr + KVA
   * because we are using segmentation tricks to make kernel segment
   * offsets be the same be the same as physical addresses
   */
  
  increment = 1;		/* increment in pages by default */

#ifndef NO_GLOBAL_PAGES
  if (CpuIdHi >= 1 && CpuFeatures & CPUFEAT_PGE)
    GlobalPage = PTE_GLBL;
#endif
  

#ifndef NO_LARGE_PAGES
  if (CpuIdHi >= 1 && CpuFeatures & CPUFEAT_PSE) {
    supports_large_pages = true;
  }
#endif
  
#ifndef NO_LARGE_PAGES
  if (supports_large_pages) {
    /* Enable the page size extensions: */
    
    __asm__ __volatile__ ("\tmov %%cr4,%%eax\n"
			  "\torl $0x10,%%eax\n"
			  "\tmov %%eax,%%cr4\n"
			  : /* no outputs */
			  : /* no inputs */
			  : "ax" /* smash any convenient register */);

    increment = 1024;		/* increment in large pages */
  }
#endif

#ifndef NO_GLOBAL_PAGES
  if (GlobalPage) {
    /* Enable the page global extensions: */
    
    __asm__ __volatile__ ("\tmov %%cr4,%%eax\n"
			  "\torl $0x80,%%eax\n"
			  "\tmov %%eax,%%cr4\n"
			  : /* no outputs */
			  : /* no inputs */
			  : "ax" /* smash any convenient register */);

    increment = 1024;		/* increment in large pages */
  }
#endif

  /* FIX: This is no longer correct -- we should no longer be mapping
   * all of physical memory, just the kernel heap. I am temporarily
   * leaving this vestigial code here because when it is removed we
   * will need to deal with page tables in a different way
   * (preallocation), and I want to make one change at a time. As a
   * TEMPORARY measure I am simply placing the heap above the physical
   * page map and leaving the physical page map in place.
   */
  for (i = 0 ; i < physPages; i += increment) {
    uint32_t paddr = i * EROS_PAGE_SIZE;
    uint32_t vaddr = KVTOL(PTOV(paddr));
    uint32_t dirndx = (vaddr >> 22);
    uint32_t pdirndx = (paddr >> 22);
      
    uint32_t mode = GlobalPage | PTE_V;

#ifndef NO_LARGE_PAGES
    if (supports_large_pages) {
#ifndef NDEBUG
      uint32_t tabndx = (vaddr >> 12) & 0x3ffu;
      assert (tabndx == 0);
#endif

      mode |= PTE_W|PTE_PGSZ;
#ifdef WRITE_THROUGH
      mode |= PTE_WT;
#endif

      MapKernelPage(PTOV(paddr), paddr, mode);
      pageDir[pdirndx] = pageDir[dirndx];
      continue;
    }
#endif

    /* not supports_large_pages */
    assert((paddr & EROS_PAGE_MASK) == 0);

    /* Make writeable, except for kernel code. */
    if (paddr < (uint32_t) start ||
	paddr >= ((uint32_t)etext & PTE_FRAMEBITS)){
      mode |= PTE_W;
#ifdef WRITE_THROUGH
      mode |= PTE_WT;
#endif
    }

    /* Hand-protect kernel page zero, which holds the kernel page
     * directory, but is never referenced by virtual addresses once
     * we load the new mapping table pointer:
     */
    if (vaddr == 0)
      mode = 0;

    MapKernelPage(PTOV(paddr), paddr, mode);

    /* This is a sleazy trick to avoid redundant page table
       allocation: */
    pageDir[pdirndx] = pageDir[dirndx];
  }

  heap_first_page = align_up_uint32(physPages, 1024);

  printf("Last physpage at 0x%08x\n", EROS_PAGE_SIZE * physPages);

  heap_start = PTOV(EROS_PAGE_SIZE * heap_first_page);
  heap_end = heap_start;
  heap_defined = heap_start;

  /* BELOW THIS POINT we do not bother to duplicate mappings, since by
   * the time any of these are needed we have already loaded the
   * segment structures.
   */

  /* We do not need to allocate the PAGES for the heap content, but we
   * *do* need to allocate the page tables that will map those pages:
   * To do this, perform a conservative computation of the size of the
   * heap that will be needed. This relies on the fact that PhysMem
   * has already been initialized, so we know at this point how many
   * total physical pages are present.
   *
   * We compute below a generously conservative approximation to the
   * largest likely heap so that we can preallocate page tables for
   * the heap.
   */

  {
    kpsize_t usablePages = physPages; /* well, close */

    heap_bound = 0;

    /* We will allocate one node per page: */
    heap_bound += usablePages * sizeof(Node);
    
    /* We will allocate four depend entries per node: */
    heap_bound += usablePages * (4 * sizeof(KeyDependEntry));

    /* We will allocate one ObjectHeader structure per page: */
    heap_bound += usablePages * (4 * sizeof(ObjectHeader));

    /* Oink oink oink: */
    heap_bound += 1024 * 1024;

    /* Finally, allow for the object headers we will need for card
     * memory: */
    heap_bound += ((KTUNE_MAX_CARDMEM * 1024)/4096) * sizeof(ObjectHeader);

    /* Round up to nearest page: */
    heap_bound = align_up_uint32(heap_bound, EROS_PAGE_SIZE);

    assert((heap_bound % EROS_PAGE_SIZE) == 0);
    assert((heap_start % EROS_PAGE_SIZE) == 0);

    nPages = (heap_bound / EROS_PAGE_SIZE);
    printf("Heap contains %d pages\n", nPages);

    heap_bound += heap_start;

    /* Place the heap right at the end of the physical page map:

       We build this mapping for the side effect of ensuring that
       mapping entries for the heap are actually present. */
    for(va = heap_start; va < heap_bound; va += EROS_PAGE_SIZE)
      MapKernelPage(va, 0, PTE_W|GlobalPage);
  }

#if 1
  /* Allocate the context cache by hand from pages, since we need to
     map it specially. Mark this portion of memory user accessable as
     a special case. */
 {
   uint32_t i;
   uint32_t len = align_up(KTUNE_NCONTEXT * sizeof(Process), EROS_PAGE_SIZE);
   uint32_t nPage = len/EROS_PAGE_SIZE;
   kva_t cache_va = heap_bound;

   /* Align up to next page directory to guarantee that the user bits
      get set. This works around a bug in MapKernelPage in which
      PTE_USER permissions don't get propagated to the directory. I
      don't want to "fix" that until I can think about it a bit. */
   cache_va = align_up(cache_va, EROS_PAGE_SIZE * 1024);

   for (i = 0; i < nPage; i++) {
     kpa_t pa = physMem_Alloc(EROS_PAGE_SIZE, &physMem_pages);
     kva_t va = cache_va + (i * EROS_PAGE_SIZE);
     MapKernelPage(va, pa, PTE_W|PTE_V|PTE_USER|GlobalPage);
   }

#if 1
   {
     /* Cannot just store this to proc_ContextCache, since the
	checking logic tests that against NULL to see if it should run
	consistency checks. */
     extern kva_t proc_ContextCacheRegion;
     proc_ContextCacheRegion = cache_va;
   }
#endif
 }
#endif
  /* Note that the kernel space does NOT have a FROMSPACE recursive
     mapping! */

  /* BELOW THIS POINT we do not use the /GlobalPage/ bit, as
   * these mappings are per-IPC and MUST get flushed when a context
   * switch occurs.
   */
  
#ifdef KVA_PTEBUF
  {
    PTE *pageTab = 0;
    PTE *pte = 0;

    /* Set up mapping slots for the receive buffer page: */
    pageTab = KPAtoP(PTE *, physMem_Alloc(EROS_PAGE_SIZE, &physMem_pages));
    assert (((uint32_t)pageTab & EROS_PAGE_MASK) == 0);
    bzero(pageTab, EROS_PAGE_SIZE);

    /* I AM NO LONGER CONVINCED THAT THIS IS NECESSARY in the contiguous 
       string case. Copying the user PDEs should be sufficient in that
       situation, and if we really needed to copy the PTEs were weren't
       going to see any TLB locality in any case. */
    pte_set(&pageDir[KVTOL(KVA_PTEBUF) >> 22], (VTOP(pageTab) & PTE_FRAMEBITS) );
    pte_set(&pageDir[KVTOL(KVA_PTEBUF) >> 22], PTE_W|PTE_V );
  
    /* Following is harmless on pre-pentium: */
    pte_clr(&pageDir[KVTOL(KVA_PTEBUF) >> 22], PTE_PGSZ );

    pte = &pageTab[(KVTOL(KVA_PTEBUF) >> 12) & 0x3ffu];

    pte_kern_ptebuf = pte;

    /* 64K message limit = 16 pages + 1 for unaligned. */
    for (i = 0; i < 17; i++) {
      pte_set(pte, PTE_W|PTE_V|PTE_DRTY|PTE_ACC);

#ifdef WRITE_THROUGH
      if (cpuType >= 5)
	pte_set(*pte, PTE_WT);
#endif
  
      pte++;
    }
  }
#endif
  
#ifdef OPTION_SMALL_SPACES
  MakeSmallSpaces();
#endif
  
#if 0
  printf("Built Kernel Page Map!\n");
#endif
  printf("CpuFeatures=0x%08x\n",CpuFeatures);
}
#endif /* NEW_KMAP */

void 
mach_MapHeapPage(kva_t va, kpa_t paddr)
{
  uint32_t mode = 0;

  assert((paddr & EROS_PAGE_MASK) == 0);
    
  mode = PTE_V|PTE_W;

#ifndef NO_GLOBAL_PAGES
  if (CpuIdHi >= 1 && CpuFeatures & CPUFEAT_PGE)
    mode |= PTE_GLBL;
#endif
#ifdef WRITE_THROUGH
  mode |= PTE_WT;
#endif

  MapKernelPage(va, paddr, mode);
}

/* Note that the RETURN from this procedure flushes the I prefetch
 * cache, which is why it is not inline.
 */
void
mach_SetMappingTable(kpmap_t pAddr)
{
  __asm__ __volatile__("movl %0,%%cr3"
		       : /* no outputs */
		       : "r" (pAddr) );
}

kpmap_t
mach_GetMappingTable()
{
  kpmap_t result;
  
  __asm__ __volatile__("movl %%cr3,%0"
		       : "=r" (result));
  return result;
}

void
mach_EnableVirtualMapping()
{
  __asm__ __volatile__("movl %%cr0,%%eax\n\t"
		       "orl $0x80010000,%%eax\n\t"
		       "movl %%eax,%%cr0\n\t"	/* Turn on PG,WP bits. */
		       : /* no outputs */
		       : /* no inputs */
		       : "ax" /* eax smashed */);
}

/* Procedure used by Check: */

#ifdef USES_MAPPING_PAGES
/* This is x86 specific, and needs to go in an architecture file when
 * I get it working!
 */
#include <kerninc/ObjectCache.h>
bool
check_MappingPage(ObjectHeader *pPage)
{
  PTE* pte = 0;
  uint32_t ent = 0;
  PTE* thePTE = 0; /*@ not null @*/
  ObjectHeader* thePageHdr = 0;

  if (pPage->producerNdx == EROS_NODE_LGSIZE)
    return true;

  pte = (PTE*) objC_ObHdrToPage(pPage);

  for (ent = 0; ent < MAPPING_ENTRIES_PER_PAGE; ent++) {
    thePTE = &pte[ent];

    if ((thePTE->w_value & (PTE_V|PTE_W)) == (PTE_V|PTE_W)) { /* writeable */
      uint32_t pageFrame = pte_PageFrame(thePTE);
      kva_t thePage = PTOV(pageFrame);

      if (thePage >= KVTOL(KVA_FROMSPACE))
	continue;


      thePageHdr = objC_PhysPageToObHdr(pageFrame);


      if (objH_GetFlags(thePageHdr, OFLG_CKPT)) {
	printf("Writable PTE=0x%08x (map page 0x%08x), ckpt pg"
		       " 0x%08x%08x\n",
		       pte_AsWord(thePTE), pte,
		       (uint32_t) (thePageHdr->kt_u.ob.oid >> 32),
		       (uint32_t) thePageHdr->kt_u.ob.oid);

	return false;
      }
      if (!objH_IsDirty(thePageHdr)) {
	printf("Writable PTE=0x%08x (map page 0x%08x), clean pg"
		       " 0x%08x%08x\n",
		       pte_AsWord(thePTE), pte,
		       (uint32_t) (thePageHdr->kt_u.ob.oid >> 32),
		       (uint32_t) thePageHdr->kt_u.ob.oid);

	return false;
      }

    }
  }

  return true;
}
#endif
