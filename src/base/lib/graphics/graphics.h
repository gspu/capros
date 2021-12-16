#ifndef __GRAPHICS_GRAPHICS_H__
#define __GRAPHICS_GRAPHICS_H__

/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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

/* Basic type definitions for the graphics library. */

/* Coordinates should be specified as signed integers so they can
   express negative relative coordinates. Coordinates should be
   specified as 32-bit quantities because Apple and MS got this wrong
   and it was really painful. */
typedef long coord_t;

#endif /* __GRAPHICS_GRAPHICS_H__ */
