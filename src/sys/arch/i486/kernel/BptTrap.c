/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

/* Drivers for 386 protection faults */

#include <eros/target.h>
#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Debug.h>
#include <kerninc/Machine.h>
#include <kerninc/util.h>
#include <kerninc/Process.h>
#include "IDT.h"

#ifdef OPTION_DDB
bool continue_user_bpt;
#endif

bool
BptTrap(savearea_t *sa)
{
  sa->EIP -= 0x1;		/* correct the PC! */

  if ( sa_IsKernel(sa)) {
#ifdef OPTION_DDB
    extern void kdb_trap(int, int, savearea_t *);
    kdb_trap(sa->ExceptNo, sa->Error, sa);
    return false;
#else
    halt('a');
#endif
  }

#ifdef OPTION_DDB
  continue_user_bpt = false;
  dprintf(true, "Domain takes BPT trap. error=%d eip=0x%08x\n",
		  sa->Error, sa->EIP);
  if (continue_user_bpt) {
    sa->EIP += 0x1;
    return false;
  }
#else
  printf("Domain takes BPT trap. error=%d eip=0x%08x\n",
	      sa->Error, sa->EIP);
  /* sa->Dump(); */
#endif
  proc_SetFault(((Process*) act_CurContext()), FC_BreakPointTrap,
		sa->EIP, false);

  return false;
}
