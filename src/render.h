/**
 * @file render.h  generic XSLT rendering handling
 * 
 * Copyright (C) 2006-2008 Lars Windolf <lars.lindner@gmail.com>
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

#include <gtk/gtk.h>

/** render parameter type */
typedef struct renderParam {
	gchar	**params;
	guint	len;
} *renderParamPtr;

/**
 * Applies the stylesheet xslt to the given XML document with the given parameters.
 *
 * @param doc		XML source document
 * @param xsltName	name of a stylesheet
 * @param params	parameter/value string array (will be free'd)
 */
gchar * render_xml (xmlDocPtr doc, const gchar *xsltName, renderParamPtr paramSet);

/**
 * Creates a new rendering parameter set.
 *
 * @returns new parameter set
 */
renderParamPtr render_parameter_new (void);

/**
 * Helper function to add a rendering parameter to the given parameter set.
 *
 * @param paramSet	a parameter set to extend
 * @param fmt		format code
 *
 * @returns a new parameter/value string array
 */
void render_parameter_add (renderParamPtr paramSet, const gchar *fmt, ...);

/**
 * Returns CSS definitions for inclusion in XHTML output.
 *
 * @param externalCss	TRUE if CSS can be served as file reference
 */
const gchar * render_get_css (gboolean externalCss);

#endif
