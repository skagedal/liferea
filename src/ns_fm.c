/*
   freshmeat namespace support
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

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
#include "htmlview.h"
#include "ns_fm.h"
#include "common.h"

#define FM_IMG_START	"<br><img style=\"margin-top:10px;\" src=\""
#define FM_IMG_END		" \">"

static gchar ns_fm_prefix[] = "fm";

/* you can find the fm DTD under http://freshmeat.net/backend/fm-releases-0.1.dtd

  it defines a lot of entities and one tag "screenshot_url", which we
  output as a HTML image in the item view footer
*/

gchar * ns_fm_getRSSNsPrefix(void) { return ns_fm_prefix; }

static void ns_fm_addInfoStruct(GHashTable *nslist, gchar *tagname, gchar *tagvalue) {
	GHashTable	*nsvalues;
	
	g_assert(nslist != NULL);
	
	if(tagvalue == NULL)
		return;
			
	if(NULL == (nsvalues = (GHashTable *)g_hash_table_lookup(nslist, ns_fm_prefix))) {
		nsvalues = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(nslist, (gpointer)ns_fm_prefix, (gpointer)nsvalues);
	}
	g_hash_table_insert(nsvalues, (gpointer)tagname, (gpointer)tagvalue);
}

static void ns_fm_parseItemTag(RSSItemPtr ip, xmlNodePtr cur) {
	xmlChar *string;
	gchar	*tmp;
	
	if(!xmlStrcmp("screenshot_url", cur->name)) {
 		string = xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
 		tmp = CONVERT(string);
		if(NULL != string) {
 			xmlFree(string);
 
			if(strlen(tmp) > 0) {
				/* maybe for just one tag this is overkill, but copy&paste is so easy! */
				ns_fm_addInfoStruct(ip->nsinfos, "screenshot_url", tmp);
			} else {
				g_free(tmp);
			}
		}
	}
}

static void ns_fm_output(gpointer key, gpointer value, gpointer userdata) {
	gchar 	**buffer = (gchar **)userdata;
	
	addToHTMLBuffer(buffer, FM_IMG_START);
	addToHTMLBuffer(buffer, (gchar *)value);
	addToHTMLBuffer(buffer, FM_IMG_END);	
}

static gchar * ns_fm_doOutput(GHashTable *nsinfos) {
	GHashTable	*nsvalues;
	gchar		*buffer = NULL;
	
	g_assert(NULL != nsinfos);
	/* we print all channel infos as a (key,value) table */
	if(NULL != (nsvalues = g_hash_table_lookup(nsinfos, (gpointer)ns_fm_prefix))) {
		g_hash_table_foreach(nsvalues, ns_fm_output, (gpointer)&buffer);
	}
	
	return buffer;
}

static gchar * ns_fm_doItemOutput(gpointer obj) {

	if(NULL != obj)
		return ns_fm_doOutput(((RSSItemPtr)obj)->nsinfos);
		
	return NULL;
}

RSSNsHandler *ns_fm_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	if(NULL != (nsh = (RSSNsHandler *)g_malloc(sizeof(RSSNsHandler)))) {
		nsh->parseChannelTag		= NULL;
		nsh->parseItemTag		= ns_fm_parseItemTag;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= NULL;
		nsh->doItemHeaderOutput		= NULL;
		nsh->doItemFooterOutput		= ns_fm_doItemOutput;
	}

	return nsh;
}
