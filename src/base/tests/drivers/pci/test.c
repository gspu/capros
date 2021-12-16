/*
 * Copyright (C) 2009, Strawberry Development Group.
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

/* Test PCI.
*/

#include <eros/target.h>
#include <eros/Invoke.h>
//#include <idl/capros/PCIBus.h>
#include <idl/capros/PCIDev.h>
#include <idl/capros/Sleep.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM	KR_APP(0)
#define KR_SLEEP        KR_APP(1)

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

int
main(void)
{
  //result_t result;

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}
