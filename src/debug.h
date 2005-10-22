/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Debugging output support. This was originally written for
 *
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002  Charles Kerr <charles@rebelbase.com>
 *
 * Liferea specific adaptions
 * Copyright (C) 2004-2005  Lars Lindner <lars.lindner@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

typedef enum
{
	DEBUG_CACHE		= (1<<0),
	DEBUG_CONF		= (1<<1),
	DEBUG_UPDATE		= (1<<2),
	DEBUG_PARSING		= (1<<3),
	DEBUG_GUI		= (1<<4),
	DEBUG_TRACE		= (1<<5),
	DEBUG_PLUGINS		= (1<<6),
	DEBUG_VERBOSE		= (1<<7)
}
DebugFlags;

extern void set_debug_level (unsigned long flags);

extern unsigned long debug_level;

extern void debug_printf (const char * strloc, const char * function, unsigned long level, const char* fmt, ...);

#ifdef __GNUC__
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define PRETTY_FUNCTION ""
#endif

#define debug0(level, fmt) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt)
#define debug1(level, fmt, A) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A)
#define debug2(level, fmt, A, B) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A, B)
#define debug3(level, fmt, A, B, C) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A, B, C)
#define debug4(level, fmt, A, B, C, D) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A, B, C, D)
#define debug5(level, fmt, A, B, C, D, E) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A, B, C, D, E)
#define debug6(level, fmt, A, B, C, D, E, F) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A, B, C, D, E, F)

#define debug_enter(A)  debug0 (DEBUG_TRACE, "+ "A)
#define debug_exit(A)   debug0 (DEBUG_TRACE, "- "A)

#define odebug0(fmt) debug_printf (G_STRLOC, PRETTY_FUNCTION,  0, fmt)
#define odebug1(fmt, A) debug_printf (G_STRLOC, PRETTY_FUNCTION,  0, fmt, A)
#define odebug2(fmt, A, B) debug_printf (G_STRLOC, PRETTY_FUNCTION,  0, fmt, A, B)
#define odebug3(fmt, A, B, C) debug_printf (G_STRLOC, PRETTY_FUNCTION,  0, fmt, A, B, C)
#define odebug4(fmt, A, B, C, D) debug_printf (G_STRLOC, PRETTY_FUNCTION,  0, fmt, A, B, C, D)
#define odebug5(fmt, A, B, C, D, E) debug_printf (G_STRLOC, PRETTY_FUNCTION,  0, fmt, A, B, C, D, E)
#define odebug6(fmt, A, B, C, D, E, F) debug_printf (G_STRLOC, PRETTY_FUNCTION,  0, fmt, A, B, C, D, E, F)

#endif
