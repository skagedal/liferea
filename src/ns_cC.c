/**
 * @file ns_cC.c creativeCommon RSS namespace support
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


#include "ns_cC.h"
#include "common.h"

#define RSS1_CC_PREFIX	"cc"
#define RSS2_CC_PREFIX	"creativeCommons"

/* you can find the RSS 2.0 creativeCommon namespace spec at:
   http://backend.userland.com/creativeCommonsRssModule
 
   there is only one tag which can appear inside
   channel and items:

   license
   
   --------------------------------------------------------
   you can find the RSS 1.0 cC namespace spec at:
   http://web.resource.org/rss/1.0/modules/cc/

   channels, images and items can have a license tag,
   for every license rdf:ressource a License tag must
   exist...

   license
   License
      permits
      requires
      
   for simplicity we only parse the license tags and
   provide a link to the license
*/

gchar * parse_tag(xmlNodePtr cur) {
	gchar	*buffer = NULL;
	gchar	*tmp;
	
 	if(!xmlStrcmp("license", cur->name)) {
 		if(NULL != (tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)))) {
			/* RSS 2.0 module handling */
			addToHTMLBuffer(&buffer, "<a href=\"");
			addToHTMLBuffer(&buffer, tmp);
			addToHTMLBuffer(&buffer, "\">");
			addToHTMLBuffer(&buffer, tmp);
			addToHTMLBuffer(&buffer, "</a>");	
			g_free(tmp);
 		} else {
			/* RSS 1.0 module handling */
			buffer = g_strdup("Creative Commons");
 		}
 	}	
	return buffer;
}

static void parse_channel_tag(feedPtr fp, xmlNodePtr cur) {

	metadata_list_set(&(fp->metadata), "license", parse_tag(cur));
}

static void parse_item_tag(itemPtr ip, xmlNodePtr cur) {

	metadata_list_set(&(ip->metadata), "license", parse_tag(cur));
}

static void ns_cC_insert_ns_uris(NsHandler *nsh, GHashTable *hash) {
	g_hash_table_insert(hash, "http://web.resource.org/cc/", nsh);
	g_hash_table_insert(hash, "http://backend.userland.com/creativeCommonsRssModule", nsh);
}

NsHandler *ns_cC1_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->prefix 			= RSS1_CC_PREFIX;
	nsh->insertNsUris		= ns_cC_insert_ns_uris;
	nsh->parseChannelTag		= parse_channel_tag;
	nsh->parseItemTag		= parse_item_tag;

	return nsh;
}

NsHandler *ns_cC2_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->prefix			= RSS2_CC_PREFIX;
	nsh->parseChannelTag		= parse_channel_tag;
	nsh->parseItemTag		= parse_item_tag;

	return nsh;
}
