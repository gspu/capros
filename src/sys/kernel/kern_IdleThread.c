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

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Machine.h>

#define StackSize 256

/* I wonder
 * if I shouldn't hand-code this activity in assembler for the sake of
 * reducing it's stack, though.
 */

void IdleActivity_Start();

Activity *
StartIdleActivity()
{
  fixreg_t *stack = MALLOC(fixreg_t, StackSize);

  Activity *idleActivity = 
    kact_InitKernActivity("Idler", pr_Idle, dispatchQueues[pr_Idle], 
			&IdleActivity_Start, stack, &stack[StackSize]);
  idleActivity->readyQ = dispatchQueues[pr_Idle];

  act_Wakeup(idleActivity);	/* let it initialize itself... */

  
  printf("Initialized IdleActivity (activity 0x%x,context 0x%x)\n",
	 idleActivity, idleActivity->context );

  return idleActivity;
}

void
IdleActivity_Start(void)
{
  int stack;
  printf("Start IdleActivity (activity 0x%x,context 0x%x,stack 0x%x)\n",
	 act_curActivity, act_curActivity->context, &stack);

  for(;;) {
    /* On machines with high privilege-crossing latency, it is NOT a
     * good idea for the idle activity to enter and leave the kernel
     * aggressively.  On the x86, for example, this can easily render
     * the processor non-interruptable for 300-400 cycles PER YIELD,
     * which (among other problems) has the effect of quantizing
     * interrupt response time. 
     *    Machine::SpinWaitUs(50);
     */

    /* Note: on i386, we cannot execut a hlt instruction here, because
       that requires privilege level 0. */

  }
}
