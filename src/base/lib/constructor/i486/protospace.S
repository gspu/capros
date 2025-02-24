	.file "protospace.S"
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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

/* protospace -- trampoline code that lets a process initialize its
 * own address space so that the constructor isn't tied up by
 * initialization time.  This code is trusted by the constructor,
 * and runs in an immutable space.
 */

#include <eros/machine/asm.h>
	
#include <eros/target-asm.h>
#include <eros/StdKeyType.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/Number.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>
#include <idl/capros/ProcCre.h>

#include <idl/capros/Constructor.h>
#include <domain/ProtoSpace.h>
#include <domain/Runtime.h>
	
#define KEYS(k0, k1, k2, k3) (k0 + k1 << 8 + k2 << 16 + k3 << 24)
	
#define NO_KEYS KEYS(KR_VOID, KR_VOID, KR_VOID, KR_VOID)
	
	.section .rodata
fab_space:
	.long	IT_Call
	.long	PSKR_SPACE
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long   KEYS(KR_BANK,KR_SCHED,KR_VOID,KR_VOID)	/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	KEYS(PSKR_SPACE,KR_VOID,KR_VOID,KR_VOID)	/* rcv keys */

pc_value:
	.long	IT_Call
	.long	PSKR_PROC_PC
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	NO_KEYS		/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	NO_KEYS		/* rcv keys */
	
swap_space_and_pc:
	.long	IT_Call
	.long	KR_SELF
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	KEYS(PSKR_SPACE,KR_VOID,KR_VOID,KR_VOID)	/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	/* rcv_key1 is returned as void. Accept it in PSKR_PROC_PC
	to clear that slot that we used temporarily. */
	.long	KEYS(PSKR_SPACE,PSKR_PROC_PC,KR_VOID,KR_VOID)	/* rcv keys */
	
destroy_caller:	
	.long	IT_Call
	.long	KR_CREATOR
	.long	0			/* snd len */
	.long	0			/* snd ptr */
	.long	KEYS(KR_BANK,KR_RETURN,KR_VOID,KR_VOID)	/* snd keys */
	.long	0			/* rcv len */
	.long	0			/* rcv data */
	.long	NO_KEYS			/* rcv keys */
	
	.text
#ifdef __ELF__
ENTRY(_start)
#else
ENTRY(start)
#endif
	/* On entry, if we came from the constructor, EBX has zero. */
	
	/* Run the constructor for the address space */
	
	movl	$fab_space,%ebp	/* invocation descriptor */
	
	movl	$OC_capros_Constructor_request,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

	/* check return value */
	testl	$0,%eax
	jnz	EXT(sepuku)

	/* Fetch the program counter of this process out
	   of the startup key that holds it.  If we had just
	   one more register, we wouldn't have to get the #$)*^%
	   thing from a key */
	
	movl	$pc_value,%ebp
	
	movl	$OC_capros_Number_get,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

	/* New program PC is now in %ebx.  Switch to the real
	   address space and proceed... */
	
	movl	$swap_space_and_pc,%ebp
	movl	$OC_capros_Process_swapAddrSpaceAndPC32,%eax	/* r0 */
	/* inherit %ebx from return from above, which is new PC */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

	/* THIS POINT SHOULD NOT BE REACHED -- WE ARE NOW RUNNING THE
	   APPLICATION'S CODE */
	hlt
	
LEXT(sepuku)
	/* construction of the address space failed.  Ask our process
	   creator to blow us away and report failure to the user */
	
	movl	$destroy_caller,%ebp
	
	movl	$OC_capros_ProcCre_destroyCallerAndReturn,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */
	
	/* THIS POINT SHOULD NOT BE REACHED -- APPLICATION NOW DEAD */
	
	hlt
