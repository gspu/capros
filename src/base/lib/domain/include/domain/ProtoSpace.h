#ifndef __PROTOSPACE_H__
#define __PROTOSPACE_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#define PSKR_SPACE    KR_APP(0)
#define PSKR_PROC_PC  KR_APP(1)
/* Following are used in the destroy logic: */
#define PSKR_PROTO    KR_APP(2)
#define PSKR_K0       KR_APP(3)
#define PSKR_K1       KR_APP(4)
#define PSKR_K2       KR_APP(5)

#ifndef __ASSEMBLER__
/* This is obsolete. Use protospace_destroy_small instead. */
void protospace_destroy(uint32_t krReturner, uint32_t krProto, uint32_t krMyDom,
			uint32_t krMyProcCre,
			uint32_t krBank, int smallSpace);
#endif

#endif /* __PROTOSPACE_H__ */

