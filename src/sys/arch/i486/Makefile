#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2005, 2006, Strawberry Development Group
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
include $(EROS_SRC)/build/make/makevars.mk

# currently not using NOKBD
KERNEL_CONFIGS=DEFAULT	# NOKBD

ifeq ($(findstring $(EROS_CONFIG),$(KERNEL_CONFIGS)),)
KERNEL_CONFIGS += $(EROS_CONFIG)
endif

BUILDDIR=.
KERNDIRS=$(patsubst %,BUILD/%,$(KERNEL_CONFIGS))

all install: configs

configs:
	for kc in $(KERNEL_CONFIGS); do \
		$(MAKE) -C conf KERNEL_CONFIG=$$kc install; \
	done

COMMON_DIRS=$(CONFDIR)

# builds KERNDIR even if it doesn't presently exist:
DIRS=$(COMMON_DIRS) $(KERNDIRS)
CLEANDIRS=$(COMMON_DIRS)

include $(EROS_SRC)/build/make/makerules.mk

# The following works because the recursion step happens first.
nonrecursiveClean:
	-rm -rf build BUILD
