#
# Copyright (C) 2009, 2010, Strawberry Development Group.
#
# This file is part of the CapROS Operating System.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

default: interfaces

EROS_SRC=../../../..
CROSS_BUILD=yes

include $(EROS_SRC)/build/make/makevars.mk

ASM_HEADERS=$(wildcard include/asm/*.h)

include $(EROS_SRC)/build/make/makerules.mk

interfaces:
	$(INSTALL) -d $(EROS_ROOT)/include/linux-arch/$(LINUX2624_TARGET)/asm
	$(INSTALL) -m 644 $(ASM_HEADERS) $(EROS_ROOT)/include/linux-arch/$(LINUX2624_TARGET)/asm
