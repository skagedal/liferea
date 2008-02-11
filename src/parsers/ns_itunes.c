/**
 * @file ns_itunes.c itunes namespace support
 *
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include "ns_itunes.h"
#include "common.h"
#include "xml.h"

/* a namespace documentation can be found at 
   http://www.apple.com/itunes/store/podcaststechspecs.html
*/

static void
parse_item_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	gchar *tmp;
	
	if (!xmlStrcmp(cur->name, "author")) {
		tmp = common_utf8_fix (xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1));
		if (tmp) {
			ctxt->item->metadata = metadata_list_append (ctxt->item->metadata, "author", tmp);
			g_free (tmp);
		}
	}
	
	if (!xmlStrcmp (cur->name, "summary")) {
		tmp = common_utf8_fix (xhtml_extract (cur, 0, NULL));
		item_set_description (ctxt->item, tmp);
		g_free (tmp);
	}
	
	if (!xmlStrcmp(cur->name, "keywords")) {
		gchar *keyword = tmp = common_utf8_fix (xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1));
		/* parse comma separated list and strip leading spaces... */
		while (tmp) {
			tmp = strchr (tmp, ',');
			if (tmp) {
				*tmp = 0;
				tmp++;
			}
			while (g_unichar_isspace (*keyword)) {
				keyword = g_utf8_next_char (keyword);
			}
			ctxt->item->metadata = metadata_list_append (ctxt->item->metadata, "category", keyword);
			keyword = tmp;
		}
		g_free (tmp);
	}
}

static void
ns_itunes_register_ns (NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash)
{
	g_hash_table_insert (prefixhash, "itunes", nsh);
	g_hash_table_insert (urihash, "http://www.itunes.com/dtds/podcast-1.0.dtd", nsh);
}

NsHandler *
ns_itunes_get_handler (void)
{
	NsHandler 	*nsh;
	
	nsh = g_new0 (NsHandler, 1);
	nsh->prefix		= "itunes";
	nsh->registerNs		= ns_itunes_register_ns;
	nsh->parseItemTag	= parse_item_tag;

	return nsh;
}
