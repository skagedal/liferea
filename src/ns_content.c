/**
 * @file ns_content.c content namespace support
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include "ns_content.h"
#include "common.h"

/* a namespace documentation can be found at 
   http://web.resource.org/rss/1.0/modules/content/

   This namespace handler is (for now) only used to handle
   <content:encoding> tags. If such a tag appears the originial
   description will be replaced by the encoded content.
   
*/

static void parse_item_tag(itemPtr ip, xmlNodePtr cur) {
	gchar *tmp;
  	if(!xmlStrcmp(cur->name, "encoded")) {
		//metadata_list_set(&(ip->metadata), "description", utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));
		tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
		if (tmp != NULL)
			item_set_description(ip,tmp);
		g_free(tmp);
	}
}

static void ns_content_insert_ns_uris(NsHandler *nsh, GHashTable *hash) {
	g_hash_table_insert(hash, "http://purl.org/rss/1.0/modules/content/", nsh);
}

NsHandler *ns_content_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->insertNsUris		= ns_content_insert_ns_uris;
	nsh->prefix			= "content";
	nsh->parseItemTag		= parse_item_tag;

	return nsh;
}
