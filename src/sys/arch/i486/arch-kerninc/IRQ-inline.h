/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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

#include <arch-kerninc/SaveArea.h>
#include <kerninc/StallQueue.h>

typedef void (*InterruptHandler)(savearea_t*);

struct UserIrq {
  bool       isPending;	/* only valid if isAlloc */
  bool       isAlloc;
  StallQueue sleepers;
};
extern struct UserIrq UserIrqEntries[NUM_HW_INTERRUPT];

extern uint32_t irq_DisableDepth;

INLINE uint32_t 
irq_DISABLE_DEPTH(void)
{
  return irq_DisableDepth;
}

#ifdef GNU_INLINE_ASM
INLINE void
irq_DISABLE(void)
{
  /* Disable BEFORE updating disabledepth */
  GNU_INLINE_ASM ("cli");
  irq_DisableDepth++;
}

#endif /* GNU_INLINE_ASM */

#ifdef GNU_INLINE_ASM
INLINE void
irq_ENABLE(void)
{
  /* Adjust disabledepth before re-enabling  */
  irq_DisableDepth--;
  if (irq_DisableDepth == 0)
    GNU_INLINE_ASM ("sti");
}

#endif /* GNU_INLINE_ASM */
