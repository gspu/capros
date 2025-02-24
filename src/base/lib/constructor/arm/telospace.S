	.file "telospace.S"
/*
 * Copyright (C) 2006, 2007, 2009, Strawberry Development Group.
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

/* telospace -- trampoline code that lets a process destroy itself and
its own address space and return. 
This code runs in a read-only address space.
This code shares a page with protospace, because it can. */

#include <eros/machine/asm.h>
	
#include <eros/Invoke.h>

#include <idl/capros/SpaceBank.h>
#include <idl/capros/ProcCre.h>

#include <domain/ProtoSpaceDS.h>
#include <domain/Runtime.h>
#include <eros/arch/arm/SWI.h>

/* This file is compiled into its own sections so we can load it
at a known address. */
#define rodataSect .rodatatelo
#define textSect .texttelo
	
/* We must declare a Message structure for SWI_Invoke to use. */
	.section rodataSect
	.balign 4
messageStruct:
	.byte 0, 0, 0, 0
	.byte 0, 0, 0, 0
	.long 0, 0, 0	/* snd_len, snd_code, snd_w1 */
	/* the values above are not used,
	   as the values are passed in registers */
	.long 0,0	/* snd_w2 and _w3 */
	.long 0		/* snd_data */
	.byte 0, 0, 0, 0	/* snd_keys, not used */
	.long 0, 0	/* rcv_limit and rcv_data, not used */
	/* The remainder of the Message structure is fields for values */
	/* that are returned in registers, not in the structure. */
	.section textSect

#define InvConstants(label, snd_invKey, snd_code, snd_w1, \
                     snd_key0, snd_key1, snd_key2, rcv_key0, rcv_key1) \
	.section rodataSect ;\
	.balign 4 ;\
/* Just declare the values to be loaded into registers. */ \
label:	.byte snd_invKey, IT_Call, 0, 0 ;\
	.byte snd_key0,snd_key1,snd_key2,KR_VOID ;\
	.long 0		/* snd_len */ ;\
	.long snd_code ;\
	.long snd_w1 ;\
/* #if RESUME_SLOT == 3 */ \
	.long 0		/* rcv_limit */ ;\
	.byte rcv_key0,rcv_key1,KR_VOID,KR_VOID ;\
	.section textSect

	.section textSect
ENTRY(_start)
ENTRY(telospacestart)

/* We get here from protospace_destroy_small_2(), which calls:
  capros_Process_swapAddrSpaceAndPC32Proto(KR_SELF, PSKR_PROTO,
    0x400,      // well known telospace address
    finalResult,    // w2_in
    &w1_out,
    KR_TEMP2);

  KR_TEMP2 has the address space GPT, which we will free.
  KR_TEMP1 has the base page of the stack, which we will free.
  KR_TEMP0 has the first page of the data/bss area, which we will free.
 */

//  The passed finalResult is returned in w1, which is placed in r2.
	mov r7,r2	// save it

	/* Free the pages and GPT: */
	InvConstants(sb_reclaim_objects, KR_BANK, OC_capros_SpaceBank_free3,
        	0, KR_TEMP0, KR_TEMP1, KR_TEMP2, KR_VOID, KR_VOID)
	ldr r0,=sb_reclaim_objects
	ldmia r0,{r1-r5,r12,r14}
	ldr r0,=messageStruct	/* our constant Message structure */
	swi SWI_Invoke

	/* Ask our process to destroy this process and return to our caller. */
	InvConstants(destroy_caller, KR_CREATOR,
		     OC_capros_ProcCre_destroyCallerAndReturn, 0,
		     KR_BANK, KR_RETURN, KR_VOID, KR_VOID, KR_VOID)
	ldr r0,=destroy_caller
	ldmia r0,{r1-r5,r12,r14}
	mov r5,r7	// snd_w1 has return code for KR_RETURN
	ldr r0,=messageStruct	/* our constant Message structure */
	swi SWI_Invoke
	
	/* THIS POINT SHOULD NOT BE REACHED -- APPLICATION NOW DEAD */
	swi SWI_halt
