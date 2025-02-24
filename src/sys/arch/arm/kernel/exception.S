	.file	"exception.S"
/*
 * Copyright (C) 2006-2008, 2010, Strawberry Development Group.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */
	
#include <eros/arch/arm/asm.h>
#include <arch-kerninc/kern-target-asm.h>
#include <eros/arch/arm/SWI.h>
#include <eros/arch/arm/mach-ep93xx/ep9315.h>
#include <kerninc/Process.h>
#include "arm.h"
#include "process_asm_offsets.h"
#include "PTEarm.h"

#ifndef NDEBUG
// Optional debugging checks:
#define STACK_CHECK
//#define EXCEPTION_TRACE
//#define CHECKREGS
#endif

#ifdef EXCEPTION_TRACE
#define traceBufEntries 128
#define traceBufSize (traceBufEntries * 12)
	.data
	.balign 8
	ENTRY(traceBuf)
	. = .+traceBufSize
	
	ENTRY(traceCursor)	// this was once at 0xfe101600
	.long traceBuf

	.text
// Log the following in the trace buffer:
// r0, proc_curProcess, caller's pc.
traceLog:
	stmfd sp!,{r1-r3}
	ldr r2,=traceCursor
	ldr r1,[r2]
	ldr r3,=proc_curProcess
	ldr r3,[r3]
	stmia r1!,{r0,r3,lr}
	ldr r3,=traceBuf+traceBufSize
	cmp r1,r3
	ldrhs r1,=traceBuf	// wrap around
	str r1,[r2]		// update traceCursor
#if 0
	stmfd sp!,{r0,r12,lr}
	ldr r0,=traceMsg
	bl printf
	ldmfd sp!,{r0,r12,lr}
	b 1f
traceMsg: .asciz "."
	.balign 4
1:
#endif
	ldmfd sp!,{r1-r3}	// restore and return
	mov pc,lr
#endif	// EXCEPTION_TRACE

	.text

// InitExceptionHandlers is entered and exits in Supervisor mode
// with IRQ and FIQ disabled.
	ENTRY(InitExceptionHandlers)
/* Set up all the exception stack pointers. */
#define IRQFIQDisable (MASK_CPSR_IRQDisable + MASK_CPSR_FIQDisable)
	msr cpsr_c,#(IRQFIQDisable + CPSRMode_Abort)
	ldr sp,=AbtStackBot
	msr cpsr_c,#(IRQFIQDisable + CPSRMode_Undefined)
	ldr sp,=UndStackBot
	msr cpsr_c,#(IRQFIQDisable + CPSRMode_IRQ)
	ldr sp,=IRQStackBot
	msr cpsr_c,#(IRQFIQDisable + CPSRMode_FIQ)
	ldr sp,=FIQStackBot
	msr cpsr_c,#(IRQFIQDisable + CPSRMode_Supervisor)
	mov pc,r14	// return

#ifndef NDEBUG
	ENTRY(CheckExceptionHandlerStacks)
	mrs r3,cpsr	// save

	msr cpsr_c,#(IRQFIQDisable + CPSRMode_Abort)
	ldr r2,=AbtStackBot
	cmp sp,r2
	bne abtStkErr
	msr cpsr_c,#(IRQFIQDisable + CPSRMode_Undefined)
	ldr r2,=UndStackBot
	cmp sp,r2
	bne undStkErr
	msr cpsr_c,#(IRQFIQDisable + CPSRMode_IRQ)
	ldr r2,=IRQStackBot
	cmp sp,r2
	bne irqStkErr
	msr cpsr_c,#(IRQFIQDisable + CPSRMode_FIQ)
	ldr r2,=FIQStackBot
	cmp sp,r2
	bne fiqStkErr
	msr cpsr_c,#(IRQFIQDisable + CPSRMode_Supervisor)
	ldr r2,=kstackBot
	cmp sp,r2
	bne supStkErr

	msr cpsr,r3	// restore
	mov pc,r14	// return

abtStkErr:
undStkErr:
irqStkErr:
fiqStkErr:
supStkErr:
	ldr r0,=ckStackMsg
	bl printf
0:	bl Debugger
	b 0b
ckStackMsg:	.asciz "Stack pointer error!\n"
	.balign 4
#endif

/***********************************************************************
 Reset handler
 ***********************************************************************/
	ENTRY(ResetExceptionHandler)
	ldr r0,=ResetMsg
	b handlerCommon

/***********************************************************************
 Undefined exception handler
 ***********************************************************************/
	ENTRY(UndefinedExceptionHandler)
	stmfd sp!, {r12,lr}	/* save some working registers */
	mrs r12,spsr
	stmfd sp!,{r0,r2,r3,r12}	// save registers
	ldr r0,=UndefinedMsg
	b toDebugger

/***********************************************************************
 Abort handlers
 ***********************************************************************/
	ENTRY(DataAbortExceptionHandler)
/* ARM920T uses the Base Restored Abort Model, that is,
   the base register is unchanged, so no fixup is needed. */
	sub lr,lr,#8		// addr of faulting instruction
	stmfd sp!, {r0-r1,r12,lr}	/* Save some working registers. */
	mov r0,#dataAbort	// parameter to PageFault()
	b CommonAbortHandler

	ENTRY(PrefetchAbortExceptionHandler)
	sub lr,lr,#4		// addr of faulting instruction
	stmfd sp!, {r0-r1,r12,lr}	/* Save some working registers. */
	mov r0,#prefetchAbort	// parameter to PageFault()
CommonAbortHandler:
	mrs r12,spsr

#ifdef EXCEPTION_TRACE
	stmfd sp!,{r0,r12,lr}
	mov r0,r12	// spsr
	bl traceLog
	ldmfd sp!,{r0,r12,lr}
#endif

HandlePageFault:
	/* Was fault from user mode? */
	add r1,r12,#1		// add 1 to mode bits of SPSR
	tst r1,#0b01110		// magic calculation
	bne AbortBadMode	// Neither user nor system mode
	/* Save registers in current context. */
	ldr r1,=(proc_curProcess)
	ldr r1,[r1]		// current Process
	str r12,[r1,#PR_OFF_CPSR]!	// save his CPSR
	add r1,r1,#8		// r1 points to location for saved r0 etc
	ldmfd sp!,{r12,lr}	// pop saved r0-r1
	stmia r1!,{r12,lr}	// save r0-r1
	ldmfd sp!,{r12,lr}	// pop saved r12 and user pc
	str lr,[r1,#-12]	// save user pc
	stmia r1,{r2-r14}^	// save user r2-r14
	/* Caution: must not follow this with an instruction that
        uses a banked register. */

	/* The kernel runs in Supervisor mode. Switch to it. */
	msr cpsr_c,#(MASK_CPSR_IRQDisable + CPSRMode_Supervisor)

	/* Get abort info to pass to C code. */
	mrc p15,0,r1,c5,c0	// get fault status register
	mrc p15,0,r2,c6,c0	// get fault address register

#ifndef NDEBUG
	add r11,r0,#3	// for stack traceback
#endif
	bl PageFault
	b .		// should never return

AbortBadMode:
	and r1,r12,#0b11111	// get just mode bits
	cmp r1,#0b10011		// supervisor mode?
	cmpeq r0,#dataAbort	// and data abort?
	bne AbortError		// no, something seriously wrong
	// A data abort from supervisor mode.
	// Look up the PC in a table.
	add r0,pc,#(TransferTable - (.+8))	// table addr to r0
1:	ldr r1,[r0],#8		// first word of a pair
	cmp r1,#0
	beq AbortError		// end of table
	cmp r1,lr		// does the PC match?
	bne 1b			// no, try next entry in table
// Found a match.
	ldr lr,[r0,#-4]	// second word of the pair - where to transfer to
	str lr,[sp,#12]
	ldmfd sp!,{r0-r1,r12,pc}^

TransferTable:
	.long LoadWordFromUserSpaceInstr,LoadWordFromUserSpaceReturn
	.long StoreByteToUserSpaceInstr,StoreByteToUserSpaceReturn
	.long SafeLoadByteInstr,SafeLoadByteReturn
	.long SafeStoreByteInstr,SafeStoreByteReturn
	.long SWI_CSwap32_Ref1,SWI_CSwap32_Fault1
	.long SWI_CSwap32_Ref2,SWI_CSwap32_Fault2
	.long 0		// end of table

AbortError:
	stmfd sp!,{r2,r3,r12}	// save registers clobbered by procedure call

	cmp r0,#prefetchAbort
	ldreq r0,=PrefetchAbortMsg
	ldrne r0,=DataAbortMsg
	mov r1,r14	// exception pc
	mov r2,r12	// SPSR
	orr r12,r12,#MASK_CPSR_IRQDisable	// mode we will go to
	msr cpsr_c,r12	// go to the previous mode
	mov r3,sp	// pick up that sp
	msr cpsr_c,#(MASK_CPSR_IRQDisable + CPSRMode_Abort)	// and back
	bl printf

	ldmfd sp!,{r2,r3,r12}	// restore registers
	orr lr,r12,#MASK_CPSR_IRQDisable	// mode we will go to
	ldmfd sp!,{r0-r1,r12}	// restore registers
	add sp,sp,#4
	// Abort stack is now empty.

	// Go to the previous mode, so we can examine r13 and r14.
	// The previous mode is most likely Supervisor.
	msr cpsr_c,lr

	/* If the abort is due to a bad stack pointer, the following
	will crash in a loop. If that happens, try:
		ldr sp,=(kstackBot-0x800)
	*/
	stmfd sp!,{r0-r3}
	ldr r0,=AbortMsgR14
	mov r1,r14
	bl printf
	ldmfd sp!,{r0-r3}

	// Set up the stack like a procedure prolog.
	/* If the abort was from supervisor mode, this might allow
	tracing the stack. */
	mov ip,sp		// note ip === r12
	stmfd sp!,{fp,ip,lr,pc}
	sub fp,ip,#4

0:
#ifdef OPTION_DDB
	bl Debugger
#endif
	b 0b

/***********************************************************************
 Unused exception handler
 ***********************************************************************/
	ENTRY(UnusedExceptionHandler)
	ldr r0,=UnusedMsg
handlerCommon:
	mov r1,r14	// exception pc
	bl printf
	b .

/***********************************************************************
 IRQ handler
 ***********************************************************************/
	ENTRY(IRQExceptionHandler)
	sub r14,r14,#4	// address to return to
	stmfd sp!,{r0-r3,r10-r12,r14}	// save some registers
	/* C procedures preserve r4-r11. */
	mrs r11,spsr	// save spsr in r11

#ifdef EXCEPTION_TRACE
	mov r0,r11	// spsr
	bl traceLog
	tst r11,#0x80	// had IRQ disabled??
	blne Debugger
#endif

/* 
Change to Abort mode.
This allows nested interrupts, 
and also allows IRQ handlers to have Undefined exceptions handled.

When in Undefined mode, IRQ is disabled or r14_und and spsr_und
have no information. Thus we don't need to save those registers. 

When in Abort mode, IRQ is disabled or spsr_abt has no information.
Thus we don't need to save that register. 
However we DO need to save r14_abt.
*/
#ifdef OPTION_DDB
/* For stack trace, we need the interrupted fp and pc, which are on
the IRQ stack.
Get a pointer to the saved fp (r11) into r0.
Saved pc is 8 beyond that.
Coordinate any changes with db_stack_trace_cmd. */
	add r0,sp,#20
#endif
	msr cpsr_c,#(MASK_CPSR_IRQDisable + CPSRMode_Abort)
	mov r10,r14	// save r14_abt

#ifdef OPTION_DDB
	stmfd sp!,{r0,r11}
	mov r11,#6	// for stack traceback
#endif
#ifdef CHECKREGS
	stmfd sp!,{r4-r11}	// we will check that C saves these
#endif

	bl DoIRQException	// go to C code

// handler returns here.
#ifdef CHECKREGS
	ldmfd sp!,{r0-r3}
	cmp r0,r4
	cmpeq r1,r5
	cmpeq r2,r6
	cmpeq r3,r7
	ldmfd sp!,{r0-r3}
	cmpeq r0,r8
	cmpeq r1,r9
	cmpeq r2,r10
	cmpeq r3,r11
	beq 1f
	ldr r0,=regMsg
	bl printf
	b halt
regMsg:	.asciz "ISR not saving regs!\n"
	.balign 4
1:
#endif

#ifdef EXCEPTION_TRACE
	mov r0,sp
	bl traceLog
#endif

#ifdef OPTION_DDB
	ldmfd sp!,{r0, r11}
#endif
// r10 still has saved r14_abt
// r11 has interrupted cpsr
	mov r14,r10	// restore r14_abt
	msr cpsr_c,#(MASK_CPSR_IRQDisable + CPSRMode_IRQ)
			// Go to IRQ mode. IRQ still disabled.
	ldr r0,=(AHB_VA+VIC1_AHB_OFS)	// VIC1 base
	str r0,[r0,#0x30]	// write to VIC1VectAddr

	/* Did we interrupt a process? */
	add r0,r11,#1		// add 1 to mode bits of SPSR
	tst r0,#0b01110		// magic calculation
	/* If ne, it is neither user nor system mode, so we just
	return to the kernel. */
	msrne spsr_cxsf,r11	// return to kernel
	ldmnefd sp!,{r0-r3,r10-r12,pc}^
#ifdef STACK_CHECK
	/* Interrupted user process, should be at bottom of stack. */
	ldr r0,=(IRQStackBot-32)
	cmp sp,r0
	beq 1f
	ldr r0,=stackMsg
	bl printf
	b halt
stackMsg:	.asciz "IRQ Stack is off"
	.balign 4
1:
#endif

	// What follows is an optimized version of ExitTheKernel().

	// Interrupt handlers should never invalidate mappings,
	// so it's not necessary to call UpdateTLB().

	// Do we need to run a different process?
	// This is the same test as in ExitTheKernel(), except that
	// interrupts can only wake up Processes, not suspend them;
	// the current Process cannot become unrunnable.
	ldr r0,=deferredWork
	ldr r0,[r0]
	msr spsr_cxsf,r11	// used if quickly returning
	cmp r0,#0
	/* If eq, we can quickly return to the process we interrupted. */
	ldmeqfd sp!,{r0-r3,r10-r12,pc}^

	/* Save the registers in the current process's context. */
	ldr lr,=proc_curProcess
	ldr lr,[lr]		// current Process
	str r11,[lr,#PR_OFF_CPSR]!	// save his CPSR
	ldmfd sp!,{r0-r3,r10-r11}	// get most saved regs
	str r0,[lr,#8]!		// save r0
	mov r0,lr		// ptr to r0 in savearea
	ldmfd sp!,{r12,lr}	// pop saved r12 and user pc
	str lr,[r0,#-4]		// save user pc
	stmib r0,{r1-r14}^	// save user r1-r14
	/* Caution: must not follow this with an instruction that
        uses a banked register. */

	/* The IRQ stack is empty now. Because we interrupted
	user (or system) mode, there can be no other nested interrupt. */

	msr cpsr_c,#(MASK_CPSR_IRQDisable + CPSRMode_Supervisor)
			// Go to Supervisor mode. IRQ still disabled.

#ifdef STACK_CHECK
	/* Should be at top of stack. */
	ldr r2,=kstackBot
	cmp sp,r2
	beq 1f
	ldr r0,=supStackMsg
	mov r1,sp
	bl printf
	b halt
supStackMsg:	.asciz "Supervisor Stack is 0x%x should be 0x%x\n"
	.balign 4
1:
#endif

#ifdef STATEDEBUG
	ldr r0,=proc_curProcess
	ldr r0,[r0]
	mov r1,#Trap_FromInt
	bl proc_LogState
#endif

	bl HandleDeferredWork

#ifdef STATEDEBUG
	ldr r0,=proc_curProcess
	ldr r0,[r0]
	mov r1,#Disp_FromInt
	bl proc_LogState 
#endif

	ldr r0,=proc_curProcess
	ldr r0,[r0]		// current Process
	b resume_process

/***********************************************************************
 FIQ handler
 ***********************************************************************/
	ENTRY(FIQExceptionHandler)
	ldr r0,=FIQMsg
	b handlerCommon

/***********************************************************************
 SWI handler
 ***********************************************************************/
	ENTRY(SWIExceptionHandler)
	stmfd sp!, {r12,lr}	/* save some working registers */
	mrs r12,spsr
	tst r12,#0x20		// get Thumb bit
	ldrneh lr,[lr,#-2]	// get Thumb SWI instruction
	ldreq lr,[lr,#-4]	// get ARM SWI instruction
	and lr,lr,#0xff	// Thumb can only specify 8 bits

#ifdef EXCEPTION_TRACE
	stmfd sp!,{r0,r12,lr}
	mov r0,r12	// SPSR
	bl traceLog
	ldr r0,[sp,#8]	// SWI # in saved lr
	bl traceLog
	ldmfd sp!,{r0,r12,lr}
#endif
	cmp lr,#SWI_MaxSWI
	ldrls pc, [pc, lr, lsl #2]	// dispatch
	b SWIOutOfRange
// SWI table must follow immediately.
#define Handler(swinum) .long swinum##_Handler
	Handler(SWI_Invoke)	// 0
	Handler(SWI_CSwap32)	// 1
	Handler(SWI_CopyKeyReg)	// 2
	Handler(SWI_XchgKeyReg)	// 3
	Handler(SWI_Bpt)	// 4
	Handler(SWI_PutIRQ)	// 5
	Handler(SWI_DisableIRQ)	// 6
	Handler(SWI_EnableIRQ)	// 7

SWI_Invoke_Handler:
	/* r12 still has spsr */
#ifndef NDEBUG
	/* Was fault from user mode? */
	add lr,r12,#1		// add 1 to mode bits of SPSR
	tst lr,#0b01110		// magic calculation
	bne SwiBadMode	// Neither user nor system mode
#endif
	/* Save registers in current context. */
	ldr lr,=proc_curProcess
	ldr lr,[lr]		// current Process
	str r12,[lr,#PR_OFF_CPSR]!	// save his CPSR
	str r0,[lr,#8]!		// save r0
	mov r0,lr		// ptr to r0 in savearea
	ldmfd sp!,{r12,lr}	// pop saved r12 and user pc
	str lr,[r0,#-4]		// save user pc
	stmib r0,{r1-r14}^	// save user r1-r14
	/* Caution: must not follow this with an instruction that
        uses a banked register. */
/* r1-r12 have user's registers:
   r1 - low byte has snd_invKey
        next byte has invType
        high bytes may have garbage
   r2 - keys to send
   r3 - snd_len
   r4 - snd_code
   r5 - snd_w1
   r12 - rcv_limit
   user's r14 in Process block - rcv_keys
 */
#ifdef OPTION_DDB
	mov r11,#2	// for stack tracebank.
#endif
/* Note: snd_w1 must be passed to the kernel in a register,
   because it is used by code running in read-only memory. */
	sub r0,r0,#(PR_OFF_CPSR + 8)	// point to Process structure
	bl InvokeArm		// continue in C
	// does not return
0:	b 0b		// in case it does

SWI_Bpt_Handler:
/* The bkpt instruction is designed for use with debug hardware.
   Without such hardware, it causes a Prefetch Abort, which is
   awkward to handle.
   That is why we use a SWI instead. */
/* Note, SWI is not allowed in Supervisor mode, i.e. in the kernel.
   Call Debugger() instead. */
/* Save registers in a savearea on the stack.
   Stack has:
     user r12
     user pc
   Savearea has:
     CPSR
     r15
     r0-r14
     three more words
 */
#ifndef NDEBUG
	/* Was fault from user mode? */
	add lr,r12,#1		// add 1 to mode bits of SPSR
	tst lr,#0b01110		// magic calculation
	bne SwiBadMode	// Neither user nor system mode
#endif
	mov lr,sp
	sub sp,sp,#80
	str r12,[sp]		// save his CPSR
	str r0,[sp,#8]		// save r0
	add r0,sp,#8		// ptr to r0 in savearea
	ldmfd lr,{r12,lr}	// get saved r12 and user pc
	str lr,[r0,#-4]		// save user pc
	stmib r0,{r1-r14}^	// save user r1-r14
	/* Caution: must not follow this with an instruction that
        uses a banked register. */

	mov r0,#3	// T_BPTFLT
	mov r1,#0	// not used
	mov r2,sp		// ptr to savearea
#ifndef NDEBUG
	mov r11,#0	// for stack tracebank.
	bl kdb_trap
#endif

	/* Return using the savearea on the stack. */
	ldmia sp!,{r12,r14}	// CPSR and pc
	msr spsr_fsxc,r12	// load SPSR
	ldmia sp,{r0-r14}^	// load r0-r12, r13_usr, r14_usr
	nop		// Cannot use banked register immediately
			// after User mode ldm
	add sp,sp,#80
	movs pc,r14	// restore pc and transfer SPSR -> CPSR


/***********************  IRQ-related SWIs *****************************/

SWI_PutIRQ_Handler:
	tst r0,#MASK_CPSR_IRQDisable
	bne SWI_DisableIRQ_Handler	// disabling
SWI_EnableIRQ_Handler:
	// r12 has spsr
	and r12,r12,#(~MASK_CPSR_IRQDisable)
	msr spsr,r12
#ifndef NDEBUG
	// Turn off the red LED on the EP9315.
	ldr r12,=(APB_VA+GPIO_APB_OFS+0x20)	// Port E data register
	ldr lr,[r12]
	bic lr,lr,#0x2
	str lr,[r12]
#endif
	/* Never need to switch contexts here. Just quickly return. */
	ldmfd sp!,{r12,pc}^     // restore regs, and SPSR to CPSR

// Disable IRQ and return the old CPSR in r0
SWI_DisableIRQ_Handler:
	// r12 has spsr
	// Check that he has I/O privilege
	ldr lr,=proc_curProcess
	ldr lr,[lr]
	mov r0,r12	// return old psr in r0
	/* kernelFlags is a byte. Here we rely on it being on a word boundary,
        and load the entire word, because not zero-extending the byte
        saves one cycle. */
	ldr lr,[lr,#PR_OFF_kernelFlags]
	orr r12,r12,#MASK_CPSR_IRQDisable
	tst lr,#KF_IoPriv
	msrne spsr,r12
#ifndef NDEBUG
	beq SWIOutOfRange
	// Turn on the red LED on the EP9315. This will give us a clue
	// if a user forgets to reenable IRQ. 
	ldr r12,=(APB_VA+GPIO_APB_OFS+0x20)	// Port E data register
	ldr lr,[r12]
	orr lr,lr,#0x2
	str lr,[r12]
#endif
	/* Never need to switch contexts here. Just quickly return. */
	ldmnefd sp!,{r12,pc}^     // restore regs, and SPSR to CPSR
	// If he didn't have I/O privilege:
SWIOutOfRange:
#ifndef NDEBUG
	/* Was fault from user mode? */
	add lr,r12,#1		// add 1 to mode bits of SPSR
	tst lr,#0b01110		// magic calculation
	bne SwiBadMode	// Neither user nor system mode
#endif
	/* Save registers in current context. */
	ldr lr,=proc_curProcess
	ldr lr,[lr]		// current Process
	str r12,[lr,#PR_OFF_CPSR]!	// save his CPSR
	str r0,[lr,#8]!		// save r0
	mov r0,lr		// ptr to r0 in savearea
	ldmfd sp!,{r12,lr}	// pop saved r12 and user pc
	str lr,[r0,#-4]		// save user pc
	stmib r0,{r1-r14}^	// save user r1-r14
	/* Caution: must not follow this with an instruction that
        uses a banked register. */
#ifdef OPTION_DDB
	mov r11,#2	// for stack tracebank.
#endif
	sub r0,r0,#(PR_OFF_CPSR + 8)	// point to Process structure
	bl InvalidSWI		// continue in C
	// does not return
0:	b 0b		// in case it does


/***********************  atomic-related SWIs *****************************/

SWI_CSwap32_Handler:
/*
 * The effect of SWI SWI_CSwap32 is to execute the following block of code as
 * an atomic instruction:
 *
 * if (*p_word==old_val)
 *   *p_word=new_val;
 *   return old_val;
 * else
 *   return *p_word;

r0 has p_word
r1 has old_val
r2 has new_val
On return, r0 has the original value of *p_word.

IRQ is disabled here, but *NOT* FIQ.
 */
#ifndef NDEBUG
	/* Was fault from user mode? */
	add lr,r12,#1		// add 1 to mode bits of SPSR
	tst lr,#0b01110		// magic calculation
	bne SwiBadMode	// Neither user nor system mode
#endif
/* The following instruction will fault if the user does not have access.
   The fault handler recognizes this instruction address. */
	ENTRY(SWI_CSwap32_Ref1)
	ldrt lr,[r0]
	cmp lr,r1
/* The following instruction will fault if the user does not have write access.
   The fault handler recognizes this instruction address. */
	ENTRY(SWI_CSwap32_Ref2)
	streqt r2,[r0]
	mov r0,lr
	/* Never need to switch contexts here. Just quickly return. */
	ldmfd sp!,{r12,pc}^     // restore regs, and SPSR to CPSR

// Get here if the above ldrt faulted.
ENTRY(SWI_CSwap32_Fault1)
	stmfd sp!, {r0,r1}	/* save some working registers */
	mov r0,#CSwapLoad	// parameter to PageFault()
	b 1f
	
// Get here if the above strt faulted.
ENTRY(SWI_CSwap32_Fault2)
	stmfd sp!, {r0,r1}	/* save some working registers */
	mov r0,#CSwapStore	// parameter to PageFault()
1:
// Calc addr of fault instr = SWI instr
	ldr lr,[sp,#12]		// get PC at exception
// r12 still has spsr
	tst r12,#0x20		// get Thumb bit
	subne lr,lr,#2		// get addr of Thumb SWI instruction
	subeq lr,lr,#4		// get addr of ARM SWI instruction
	str lr,[sp,#12]

	b HandlePageFault
/* HandlePageFault will unnecessarily switch to Supervisor mode
and read the FSR and FAR, but this is not a performance path. */


// r0 has "from" slot number.
// r1 has "to" slot number.
SWI_CopyKeyReg_Handler:
#ifndef NDEBUG
	/* Was fault from user mode? */
	add lr,r12,#1		// add 1 to mode bits of SPSR
	tst lr,#0b01110		// magic calculation
	bne SwiBadMode	// Neither user nor system mode
#endif

	// Ignore extraneous bits in the slot numbers.
	and r0,r0,#(EROS_NODE_SIZE-1)	// from
	ands r1,r1,#(EROS_NODE_SIZE-1)	// to
	/* It's not important to optimize the case of r1 == KR_VOID,
	but the "ne" below costs nothing. */
	blne copy_key
	/* Never need to switch contexts here. Just quickly return. */
	ldmfd sp!,{r12,pc}^     // restore regs, and SPSR to CPSR

// r0 and r1 have the two slot numbers.
SWI_XchgKeyReg_Handler:
#ifndef NDEBUG
	/* Was fault from user mode? */
	add lr,r12,#1		// add 1 to mode bits of SPSR
	tst lr,#0b01110		// magic calculation
	bne SwiBadMode	// Neither user nor system mode
#endif

	// Ignore extraneous bits in the slot numbers.
	and r0,r0,#(EROS_NODE_SIZE-1)	// from
	and r1,r1,#(EROS_NODE_SIZE-1)	// to
	bl xchg_key 
	/* Never need to switch contexts here. Just quickly return. */
	ldmfd sp!,{r12,pc}^     // restore regs, and SPSR to CPSR

SwiBadMode:
	stmfd sp!,{r0,r2,r3,r12}	// save registers
	ldr r0,=SWIMsg

toDebugger:
	// Set up the stack like a procedure prolog.
	// If the exception was from supervisor mode, this will allow
	// tracing the stack.
	mov ip,sp		// note ip === r12
	stmfd sp!,{fp,ip,lr,pc}
	sub fp,ip,#4

	ldr r1,[ip,#20]	// exception pc
	bl printf
0:
#ifndef NDEBUG
	bl Debugger
#endif
	b 0b

	ENTRY(resume_process)
/* r0 has a pointer to the Process structure. */

#ifdef EXCEPTION_TRACE
	stmfd sp!,{r0}
	ldr r0,[r0,#PR_OFF_CPSR]
	bl traceLog
	ldr r0,[sp]	// get process pointer again
	ldr r0,[r0,#PR_OFF_r15]
	bl traceLog
	ldmfd sp!,{r0}
#endif

	ldr sp,=(kstackBot)	// empty the kernel stack!
#if 0	// if trying to find a bug
	mov r4,r0
	bl mach_FlushTLBsCaches
	mov r0,r4
#endif

	add r4,r0,#PR_OFF_firstLevelMappingTable
/* Process structure has, in order:
firstLevelMappingTable
pid
dacr
CPSR
r15
r0-r14
 */
	ldmia r4!,{r5-r7}	// get flmt, pid, dacr
	mrc p15,0,r0,c2,c0	// read translation table base register
	mcr p15,0,r5,c2,c0	// load translation table base register
	cmp r5,r0		// if changing the TTBR,
	blne mach_FlushTLBsCaches	// flush TLB and cache
	ldmia r4!,{r12,r14}	// get CPSR, r15
	mcr p15,0,r6,c13,c0	// load Process ID register
	mcr p15,0,r7,c3,c0	// load domain access control register, DACR
	msr spsr_fsxc,r12	// load SPSR
	ldmia r4,{r0-r14}^	// load r0-r12, r13_usr, r14_usr
	nop		// Cannot use banked register immediately
			// after User mode ldm
	movs pc,r14	// restore pc and transfer SPSR -> CPSR

ResetMsg:	.asciz "Reset Exception! at 0x%08x"
UndefinedMsg:	.asciz "Undefined Exception! at 0x%08x\n"
SWIMsg:	.asciz "SWI Exception! at 0x%08x\n"
DataAbortMsg:	.asciz "Data Abort Exception! at %#x, SPSR=%#x\n sp=%#x, "
PrefetchAbortMsg:	.asciz "Prefetch Abort Exception! at %#x, SPSR=%#x\n sp=%#x, "
AbortMsgR14:	.asciz "R14=%#x\n"
UnusedMsg:	.asciz "Unused Exception! at 0x%08x"
FIQMsg:	.asciz "FIQ Exception! at 0x%08x"
	.balign 4

	.bss
#define MaxInterruptDepth 32	// no enforcement of this

#define AbtStackSize (500 * MaxInterruptDepth)
	. = . + AbtStackSize
	.balign CACHE_LINE_SIZE	// align on cache line for performance
AbtStackBot:

#define UndStackSize (500 * MaxInterruptDepth)
	. = . + UndStackSize
	.balign CACHE_LINE_SIZE	// align on cache line for performance
UndStackBot:

#define IRQStackSize (32 * MaxInterruptDepth)
	. = . + IRQStackSize
	.balign CACHE_LINE_SIZE	// align on cache line for performance
IRQStackBot:

#define FIQStackSize 500
	. = . + FIQStackSize
	.balign CACHE_LINE_SIZE	// align on cache line for performance
FIQStackBot:
