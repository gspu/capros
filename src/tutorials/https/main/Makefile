#
# Copyright (C) 2010, Strawberry Development Group
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

CROSS_BUILD=yes

EROS_SRC=../../..
include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/test
OBJECTS=$(BUILDDIR)/test.o
INC=-I$(EROS_ROOT)/include

NPRANGESIZE=1200
PRANGESIZE=2000

include $(EROS_SRC)/build/make/makerules.mk

install all: $(TARGETS)

include $(EROS_SRC)/base/tests/test.mk

$(BUILDDIR)/test: $(OBJECTS) $(LIBDEP) $(DYNCMMEDEPS)
	$(DYNCMMELINK) $(OBJECTS) $(CMMELIBS) -o $@
	$(TARGET_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled

-include $(BUILDDIR)/.*.m
