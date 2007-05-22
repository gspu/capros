#ifndef __MACHINE_PROCESS_INLINE_H__
#define __MACHINE_PROCESS_INLINE_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/Invoke.h>
#include <kerninc/Invocation.h>
#include <kerninc/Activity.h>
#include <arch-kerninc/Process.h>

/* Machine-specific inline helper functions for process operations: */

#ifdef ASM_VALIDATE_STRINGS
#error This is not the case.
/* The reason to do this is that until OPTION_PURE_ENTRY_STRINGS is removed
   there are a whole lot of places where this gets called.
   The net effect is that ASM_VALIDATE_STRINGS implies
   OPTION_PURE_ENTRY_STRINGS whether OPTION_PURE_ENTRY_STRINGS is set or not. */

INLINE void 
proc_SetupEntryString(void)
{
#ifndef OPTION_SMALL_SPACES
  const uint32_t bias = 0;
#endif
      
  uint32_t addr = (uint32_t) trapFrame.sndPtr + bias + KUVA;
  
  inv.entry.data = (uint8_t *) addr;
}

#endif

INLINE void 
proc_SetPC(Process* thisPtr, uint32_t oc)
{
  thisPtr->trapFrame.EIP = oc;
}

/* Called in the IPC path to back up the PC to point to the invocation
 * trap instruction. */
INLINE void 
proc_AdjustInvocationPC(Process* thisPtr)
{
  thisPtr->trapFrame.EIP -= 2;
}

/* and this advances it: */
INLINE void 
proc_AdvancePostInvocationPC(Process * thisPtr)
{
  thisPtr->trapFrame.EIP += 2;
}

INLINE uint32_t 
proc_GetPC(Process* thisPtr)
{
  return thisPtr->trapFrame.EIP;
}

INLINE void 
proc_SetInstrSingleStep(Process* thisPtr)
{
  thisPtr->hazards |= hz_SingleStep;
  thisPtr->saveArea = 0;
}


#endif /* __MACHINE_PROCESS_INLINE_H__ */
