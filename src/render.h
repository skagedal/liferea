/**
 * @file render.h generic XSLT rendering handling
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. 
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

#ifndef _RENDER_H
#define _RENDER_H

#include <glib.h>

/**
 * Applies the stylesheet xslt to the given file with the given parameters.
 *
 * @param filename	valid absolut source filename
 * @param xsltName	name of a stylesheet
 * @param params	comma separated parameter/value list
 *
 * @returns rendered XHTML
 */
gchar * render_file(const gchar *filename, const gchar *xsltName, const gchar **params);

/**
 * Applies the stylesheet xslt to the given XML document with the given parameters.
 *
 * @param doc		XML source document
 * @param xsltName	name of a stylesheet
 * @param params	comma separated parameter/value list
 */
gchar * render_xml(xmlDocPtr doc, const gchar *xsltName, const gchar **params);

/**
 * Helper function to add a rendering parameter to the given parameter list.
 *
 * @param params	the old parameter list (will be freed)
 *
 * @returns a new parameter list
 */
gchar ** render_add_parameter(gchar **params, const gchar *fmt, ...);

// FIXME: remove me once ui_htmlview.c doesn't do rendering anymore
GString * render_get_css(gboolean twoPane);

#endif