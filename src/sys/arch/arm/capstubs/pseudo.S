/*
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/arch/arm/asm.h>
//#include <eros/Invoke.h>
#include <eros/arch/arm/SWI.h>

	.weak	EXT(COPY_KEYREG)
	.set	EXT(COPY_KEYREG),EXT(__rt_do_COPY_KEYREG)

/* On entry to COPY_KEYREG, r0 has the "from" slot number,
   and r1 has the "to" slot number. */
__rt_do_COPY_KEYREG:	.globl __rt_do_COPY_KEYREG
	swi SWI_CopyKeyReg
	mov pc,lr	// return

	.weak	EXT(XCHG_KEYREG)
	.set	EXT(XCHG_KEYREG),EXT(__rt_do_XCHG_KEYREG)

/* On entry to XCHG_KEYREG, r0 and r1 have the slot numbers to exchange. */
__rt_do_XCHG_KEYREG:	.globl __rt_do_XCHG_KEYREG
	swi SWI_XchgKeyReg
	mov pc,lr	// return
