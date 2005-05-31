/**
 * @file common.h common routines
 *
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004,2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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
 
#ifndef _COMMON_H
#define _COMMON_H

#include <config.h>
#include <time.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <glib.h>

#define htmlToCDATA(buffer) g_strdup_printf("<![CDATA[%s]]>", buffer)

extern gboolean lifereaStarted;

struct folder;

typedef struct node {
	gint type;
	gpointer ui_data;
} *nodePtr;

/** Conversion function which should be applied to all read XML strings, 
   to ensure proper UTF8. doc points to the xml document and its encoding and
   string is a xmlChar pointer to the read string. The result gchar
   string is returned, the original XML string is freed. */
gchar * utf8_fix(xmlChar * string);

/* converts a UTF-8 string to HTML (resolves XML entities) */
gchar * convertToHTML(gchar * string);

/* converts a UTF-8 string containing HTML tags to plain text */
gchar * unhtmlize(gchar *string);

/* parses a XML node and returns its contents as a string */
/* gchar * parseHTML(htmlNodePtr cur); */

/** to extract not escaped XHTML from a node
 * @param only extract the children of the passed node */
gchar * extractHTMLNode(xmlNodePtr cur, gboolean children);

void	addToHTMLBufferFast(gchar **buffer, const gchar *string);
void	addToHTMLBuffer(gchar **buffer, const gchar *string);

/** Common function to create a XML DOM object from a given
   XML buffer. This function sets up a parser context,
   enables recovery mode and sets up the error handler.
   
   The function returns a XML document and (if errors)
   occur sets the errormsg to the last error message. */
xmlDocPtr parseBuffer(gchar *data, size_t dataLength, gchar **errormsg);

time_t 	parseISO8601Date(gchar *date);
/**
 * Parses a RFC822 format date. This FAILS if a timezone string is
 * specified such as EDT or EST and that timezone is in daylight
 * savings time.
 *
 * @returns UNIX time (GMT, no daylight savings time)
 */
time_t 	parseRFC822Date(gchar *date);
gchar *createRFC822Date(const time_t *time);
/* FIXME: formatDate used by several functions not only
   to format date column, don't use always date column format!!!
   maybe gchar * formatDate(time_t, gchar *format) */
gchar * formatDate(time_t t);

gchar *	common_get_cache_path(void);
gchar * common_create_cache_filename( const gchar *folder, const gchar *key, const gchar *extension);

/**
 * Encodes all non URI conformant characters in the passed
 * string to be included in a HTTP URI. The original string
 * is freed.
 *
 * @param string	string to be URI-escaped
 * @returns new string that can be inserted into a HTTP URI
 */
gchar * encode_uri_string(gchar *string);

/**
 * Encodes all non URI conformant characters in the passed
 * string and returns a valid HTTP URI. The original string
 * is freed.
 *
 * @param uri_string	string to be URI-escaped
 * @returns valid HTTP URI
 */
gchar * encode_uri(gchar *uri_string);

/**
 * Takes an URL and returns a new string containing
 * the escaped URL.
 *
 * @param url		the URL to escape
 */
xmlChar * common_uri_escape(const xmlChar *url);

/** 
 * To correctly escape and expand URLs, does not touch the
 * passed strings.
 *
 * @param url		relative URL
 * @param baseURL	base URL
 * @returns resulting absolute URL
 */
xmlChar * common_build_url(const gchar *url, const gchar *baseURL);

#ifndef HAVE_STRSEP
char *strsep (char **stringp, const char *delim);
#endif

gchar *strreplace(const char *string, const char *delimiter,
			   const char *replacement);

#endif
