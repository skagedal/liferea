/*
   admin namespace support
   
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string.h>
#include "ns_admin.h"
#include "common.h"
#include "metadata.h"

/* you can find an admin namespace spec at:
   http://web.resource.org/rss/1.0/modules/admin/
 
  taglist for admin:
  --------------------------------
    errorReportsTo
    generatorAgent
  --------------------------------
  
  both tags usually contains URIs which we simply display in the
  feed info view footer
*/

static void parse_channel_tag(feedPtr fp, xmlNodePtr cur) {
	gchar	*value;
	
	value = utf8_fix(xmlGetProp(cur, "resource"));	
	
	if(!xmlStrcmp("errorReportsTo", cur->name))
		metadata_list_set(&(fp->metadata), "errorReportsTo", value);
	else if (!xmlStrcmp("generatorAgent", cur->name))
		metadata_list_set(&(fp->metadata), "feedgeneratorUri", value);
	
	g_free(value);
	return;
}

static void ns_admin_insert_ns_uris(NsHandler *nsh, GHashTable *hash) {
	g_hash_table_insert(hash, "http://webns.net/mvcb/", nsh);
}

NsHandler *ns_admin_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->insertNsUris		= ns_admin_insert_ns_uris;
	nsh->prefix			= "admin";
	nsh->parseChannelTag		= parse_channel_tag;;

	return nsh;
}
