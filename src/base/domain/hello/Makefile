#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, Strawberry Development Group
#
# This file is part of the EROS Operating System.
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
#


default: package

EROS_SRC=../../..
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk


GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=hello.map
TARGETS=$(BUILDDIR)/hello
OPTIM=-O
OBJECTS=$(BUILDDIR)/hello.o
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include
CFLAGS=

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/hello: $(OBJECTS) $(DOMCRT0) $(DOMLIB)
	$(CROSSLINK) $(OBJECTS) $(SMALL_SPACE) $(CROSSLIBS) -o $@

hello.c: $(BUILDDIR)/constituents.h

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)
	$(INSTALL) -m 644 hello.map $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m
