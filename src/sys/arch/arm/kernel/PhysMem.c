/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/PhysMem.h>
#include <kerninc/multiboot.h>

#define dbg_init	0x1u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)


kpa_t physMem_PhysicalPageBound = 0;

static void checkBounds(kpa_t base, kpa_t bound)
{
  if (bound > physMem_PhysicalPageBound) {        /* take max */
    physMem_PhysicalPageBound = align_up(bound, EROS_PAGE_SIZE);
  }
}

void
physMem_Init()
{
  int32_t mmapLength;
  struct grub_mmap * mp;
  PmemConstraint constraint;
  kpa_t constraintSize;
  kpsize_t size;

  DEBUG (init) printf("MultibootInfoPtr = %x\n", MultibootInfoPtr);

  for (mmapLength = MultibootInfoPtr->mmap_length,
         mp = (struct grub_mmap *) MultibootInfoPtr->mmap_addr;
       mmapLength > 0;
       ) {
    DEBUG (init) printf("0x%08lx 0x%08lx 0x%08lx 0x%08lx 0x%08lx 0x%08lx 0x%08lx\n",
       mp,
       mp->size,
       mp->base_addr_low,
       mp->base_addr_high,
       mp->length_low,
       mp->length_high,
       mp->type);

    /* This machine has only 32 bits of phys mem. */
    assert(mp->base_addr_high == 0 && mp->length_high == 0);
    kpa_t base = (kpa_t)mp->base_addr_low;
    kpsize_t bound;
    size = (kpsize_t)mp->length_low;
    bound = base + size;

    if (mp->type == 1) {	/* available RAM */
      (void) physMem_AddRegion(base, bound, MI_MEMORY, false);
      checkBounds(base, bound);
    }
    /* On to the next. */
    /* mp->size does not include the size of the size field itself,
       so add 4. */
    mmapLength -= (mp->size + 4);
    mp = (struct grub_mmap *) (((char *)mp) + (mp->size + 4));
  }

  /* Preloaded modules are contained in mmap memory,
     so no need to add regions for them. */

  /* Reserve regions of physical memory that are already used. */

  /* Reserve kernel code and rodata. */
  constraint.base = (kpa_t)KTextPA;
  constraintSize = (&_etext - &_start);
  constraint.align = 1;
  constraint.bound = constraint.base + constraintSize;
  physMem_Alloc(constraintSize, &constraint);

  /* Reserve kernel data and bss.
     The kernel stack is within the data section. */
  constraint.base = align_up(constraint.bound, 0x100000); /* 1MB boundary */
  constraintSize = (&_end - &__data_start);
  constraint.align = 1;
  constraint.bound = constraint.base + constraintSize;
  physMem_Alloc(constraint.bound - constraint.base, &constraint);

  /* Reserve kernel mapping tables. */
  constraint.base = align_up(constraint.bound, 0x4000);	/* 16KB boundary */
  constraintSize = 0x4000 + 0x1000;
  constraint.align = 0x1000;
  constraint.bound = constraint.base + constraintSize;
  physMem_Alloc(constraint.bound - constraint.base, &constraint);
                                                                                
  /* Modules are in flash memory; no need to reserve RAM. */

  /* Multiboot information is in data/bss, no need to reserve. */

  /* physMem_ReservePhysicalMemory();
     Map was set up by lostart.S. */
}

kpsize_t
physMem_TotalPhysicalPages()
{
  return physMem_PhysicalPageBound / EROS_PAGE_SIZE;
}
