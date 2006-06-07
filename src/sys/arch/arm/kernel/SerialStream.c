/*
 * Copyright (C) 2006, Strawberry Development Group.
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

/* This implements a console outputing to UART1.
   It is assumed that the boot loader left UART1 in a good state. */

#include <kerninc/KernStream.h>
#include "ep93xx-uart.h"

void SerialStream_Put(uint8_t c);

void
SerialStream_Init(void)
{
#if 0
  /* It is assumed that the boot loader left UART1 in a good state. */
#else
#define BaudRate 57600
#define BaudRateDivisor (UARTCLK/(16*BaudRate)-1)
  UART1.LinCtrlLow = BaudRateDivisor & 0xff;
  UART1.LinCtrlMid = (BaudRateDivisor >> 8) & 0xff;
  /* Must write LineCtrlHigh last. */
  /* 8 bits No parity 1 stop bit */
  UART1.LinCtrlHigh = UARTLinCtrlHigh_WLEN_8 | UARTLinCtrlHigh_FEN;
  UART1.Ctrl = UARTCtrl_UARTE;
  { int i;
    for (i = 55; i > 0; i--) ;
  }
#endif
}

void
SerialStream_Put(uint8_t c)
{
  /* Wait until transmit buffer not full. */
  while (UART1.Flag & UARTFlag_TXFF) ;
  UART1.Data = c;
}

struct KernStream TheSerialStream = {
  SerialStream_Init,
  SerialStream_Put
#ifdef OPTION_DDB
  ,
  SerialStream_Get,
  SerialStream_SetDebugging,
  SerialStream_EnableDebuggerInput
#endif /*OPTION_DDB*/
};
KernStream* kstream_SerialStream = &TheSerialStream;
