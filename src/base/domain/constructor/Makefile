#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group.
#
# This file is part of the CapROS Operating System,
# and is derived from the EROS Operating System.
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

# This material is based upon work supported by the US Defense Advanced
# Research Projects Agency under Contract No. W31P4Q-07-C-0070.
# Approved for public release, distribution unlimited.

default: install

EROS_SRC=../../..
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk

GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=metacon.map
VPATH=$(EROS_TARGET)
TARGETS=$(BUILDDIR)/constructor $(BUILDDIR)/metaconstructor
OPTIM=-O
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/constructor: $(BUILDDIR)/constructor.o $(DOMCRT0) $(DOMLIB)
	$(CROSSLINK) $(BUILDDIR)/constructor.o $(SMALL_SPACE) $(CROSSLIBS) -o $@
# and for debugging:
	$(TARGET_OBJDUMP) --syms $@ | sort >$(BUILDDIR)/syms
	$(TARGET_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled

metaconstructor.c: $(BUILDDIR)/constituents.h
constructor.c: $(BUILDDIR)/constituents.h

$(BUILDDIR)/metaconstructor: $(BUILDDIR)/metaconstructor.o $(DOMCRT0) $(DOMLIB)
	$(CROSSLINK) $(BUILDDIR)/metaconstructor.o $(SMALL_SPACE) $(CROSSLIBS) -o $@
#	objdump --headers metaconstructor

IDL=$(wildcard *.idl)
interfaces: $(IDL)
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)
	$(INSTALL) -m 644 metacon.map $(CAPROS_DOMAIN)
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m
