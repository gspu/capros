#
# Copyright (C) 2001, Jonathan S. Shapiro.
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
default: all

CROSS_BUILD=yes

EROS_SRC=../../../..
EROS_CONFIG=DEFAULT
include $(EROS_SRC)/build/make/makevars.mk

GENERATED=$(BUILDDIR)/constituents.h
TARGETS=$(BUILDDIR)/soundtest
SOUNDTEST=$(BUILDDIR)/soundtest.o
OBJECTS=$(SOUNDTEST)
DOMAIN=$(EROS_ROOT)/domain/drivers/sound

include $(EROS_SRC)/build/make/makerules.mk
include $(EROS_SRC)/base/tests/drivers/drivers.mk

OPTIM=-O

all: $(TARGETS)

soundtest.c:$(BUILDDIR)/constituents.h

$(BUILDDIR)/soundtest:$(OBJECTS) $(DOMLIB) $(DOMCRT0) $(DOMAIN)
	$(CROSSLINK) -o $@ $(OBJECTS) $(SMALL_SPACE) $(CROSSLIBS) 

-include $(BUILDDIR)/.*.m
