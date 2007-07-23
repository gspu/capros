#ifndef __KERN_TARGET_ASM_ARM_H__
#define __KERN_TARGET_ASM_ARM_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

/*
 * Basic type declarations for the target platform, used pervasively
 * within the kernel code.
 *
 * This file is included from Assembler, C and C++ code, so it needs
 * to be handled very carefully.  In particular, parentheses mean
 * something in assembler and should be avoided.
 */

#if !(defined(__arm__))
#error "Inappropriate target file"
#endif

#define CACHE_LINE_SIZE 32

/*
Physical memory layout:
At 0x00000000 (KTextPA), exception vectors and kernel code (text and rodata)
followed by pad to page boundary
followed by available memory to 1MB boundary (so we can map kernel code
  as an entire section read-only)
followed by kernel data, bss
followed by pad to 16KB boundary
followed by First-Level Page Table for FCSE
followed by Coarse Page Table for exception vectors
 */

/*
 */
#define KTextPA    0x00000000	/* start of kernel text in phys mem */
#define FlashMemPA 0x60000000	/* start of flash memory in phys mem */

/*
Virtual Memory map

by Modified Virtual Address (that is, the address as modified by the
Fast Context Switch Extension (FCSE))

If a memory space is loaded that does not entirely fit below 0x0200....,
then it is mapped from zero to UserEndVA and FCSE is disabled.
Otherwise, the FCSE memory space is mapped:
0x0000.... unused
           Note, this range of modified virtual addresses is inaccessible
           when FCSE enabled and PID != 0, but this range of
           *unmodified* virtual addresses *is* accessible. 
           Thus, if the kernel uses a NULL pointer, it will be caught
           only if it would have been caught in the current small space. 
0x0200.... 87 (63 temporarily) recently-used small memory spaces

(UserEndVA) All the following addresses are mapped in every
           address space and accessible only to the kernel.
0x8000.... "temporarily" mapped to device registers, to avoid hassle of
           PA != VA.
0xa000.... (HeapVA) Kernel heap.
0xb000.... (FlashMemVA) mapped to flash memory.
0xc000.... (PhysMapVA) mapped to all physical memory. Beware: don't access user
           memory via this map, or you will get cache aliases. 
           This is only for page tables and other kernel-only memory
           that is not mapped anywhere else.
           TODO: Need work to support memory above physical address 0x2fffffff.
0xf000.... node table, object table
0xf800.... windows to map other processes
0xfc00.... (DeviceRegsVA) mapped to physical 0x8000.... 
           memory-mapped device registers
0xfe00.... (KTextVA) exception vectors and kernel code, read-only
           (_start to _etext)
           (exception vector is mapped here and at 0xffff0000.
           If it's ever modified, adjust the cache.)
followed by padding to 1MB boundary (so we can map code ro and data rw
           and still map using section descriptors)
0xfe10.... (Link command puts data section here because kernel code
           is assumed to be less than 1MB in size.)
followed by kernel data, bss (read-write)
           Kernel stack is the first thing in the data section
           (despite that it needs no initialization,
           so it will be covered by the same TLB entry as the data and bss
           and so stack overflow will cause a fault.)
           (Mapped read-write to next 1MB boundary)
           (Boot code assumes data and bss total size is less than 1MB)
followed by padding to 1MB boundary (so we can map code ro and data rw
           and still map using section descriptors)
followed by unused
0xffff0000 exception vectors, read-only
0xffff1000 unused, to guard against small negative integers used as pointers
           by accident.
 */
#define NumSmallSpaces  0x40	/* including 0, which isn't used */
#define UserEndVA	0x80000000
/* UserEndVA == NumSmallSpaces * 0x02000000 */
#define HeapVA		0xa0000000
#define HeapEndVA	0xb0000000
#define FlashMemVA	0xb0000000
#define PhysMapVA	0xc0000000
#define DeviceRegsVA	0xfc000000
#define KTextVA		0xfe000000

/* N.B.: the value of KDataPackedAddr must match that in the kernel makefiles
   conf/Makefile.* and sys.arm.mk. */
/* The kernel data section begins in the flash memory at address
   KDataPackedAddr - KTextVA + FlashMemPA */
#define KDataPackedAddr 0xfe030000
#define KTextPackedSize (KDataPackedAddr - KTextVA)

#endif /* __KERN_TARGET_ASM_ARM_H__ */
