/*
   slash namespace support
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include "htmlview.h"
#include "ns_slash.h"
#include "common.h"

#define SLASH_START	"<table style=\"width:100%\" cellpadding=\"0\" cellspacing=\"0\"><tr><td style=\"margin-bottom:5px;border-width:1px;border-style:solid;border-color:black;background:#408060;width:100%;font-size:x-small;color:black;padding-left:5px;padding-right:5px;\">"
#define KEY_START	""
#define KEY_END		" "
#define VALUE_START	"<span style=\"color:white;\">"
#define VALUE_END	"</span> "
#define SLASH_END	"</td></tr></table>"

static gchar ns_slash_prefix[] = "slash";

/* a tag list from http://f3.grp.yahoofs.com/v1/YP40P2oiXvP5CAx4TM6aQw8mDrCtNDwF9_BkMwcvulZHdlhYmCk5cS66_06t9OaIVsubWpwtMUTxYNG7/Modules/Proposed/mod_slash.html

   hmm... maybe you can find a somewhat short URL!

-------------------------------------------------------

 <item> Elements:

    * <slash:section> ( #PCDATA )
    * <slash:department> ( #PCDATA )
    * <slash:comments> ( positive integer )
    * <slash:hit_parade> ( comma-separated integers )

-------------------------------------------------------

*/

static gchar * taglist[] = {	"section",
				"department",				
				/* "comments",   disabled to avoid unread status after each feed update */
				/* "hitparade",*/
				NULL
			   };

gchar * ns_slash_getRSSNsPrefix(void) { return ns_slash_prefix; }

static void ns_slash_addInfoStruct(GHashTable *nslist, gchar *tagname, gchar *tagvalue) {
	GHashTable	*nsvalues;
	
	g_assert(nslist != NULL);
	
	if(tagvalue == NULL)
		return;
			
	if(NULL == (nsvalues = (GHashTable *)g_hash_table_lookup(nslist, ns_slash_prefix))) {
		nsvalues = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(nslist, (gpointer)ns_slash_prefix, (gpointer)nsvalues);
	}
	g_hash_table_insert(nsvalues, (gpointer)tagname, (gpointer)tagvalue);
}

static void ns_slash_parseItemTag(RSSItemPtr ip,xmlDocPtr doc, xmlNodePtr cur) {
	int 		i;
	
	/* compare with each possible tag name */
	for(i = 0; taglist[i] != NULL; i++) {
		if(!xmlStrcmp((const xmlChar *)taglist[i], cur->name)) {
			ns_slash_addInfoStruct(ip->nsinfos, taglist[i], CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));
		}
	}
}

/* maybe I should overthink method names :-) */
static void ns_slash_output(gpointer key, gpointer value, gpointer userdata) {
	gchar 	**buffer = (gchar **)userdata;
	
	addToHTMLBuffer(buffer, KEY_START);
	addToHTMLBuffer(buffer, (gchar *)key);
	addToHTMLBuffer(buffer, KEY_END);
	addToHTMLBuffer(buffer, VALUE_START);	
	addToHTMLBuffer(buffer, (gchar *)value);
	addToHTMLBuffer(buffer, VALUE_END);
}

static gchar * ns_slash_doOutput(GHashTable *nsinfos) {
	GHashTable	*nsvalues;
	gchar		*buffer = NULL;

	g_assert(NULL != nsinfos);	
	/* we print all channel infos as a (key,value) table */
	if(NULL != (nsvalues = g_hash_table_lookup(nsinfos, (gpointer)ns_slash_prefix))) {
		addToHTMLBuffer(&buffer, SLASH_START);
		g_hash_table_foreach(nsvalues, ns_slash_output, (gpointer)&buffer);
		addToHTMLBuffer(&buffer, SLASH_END);			
	}
	
	return buffer;
}

static gchar * ns_slash_doItemOutput(gpointer obj) {
	
	if(NULL != obj)
		return ns_slash_doOutput(((RSSItemPtr)obj)->nsinfos);
	
	return NULL;
}

RSSNsHandler *ns_slash_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	if(NULL != (nsh = (RSSNsHandler *)g_malloc(sizeof(RSSNsHandler)))) {
		nsh->parseChannelTag		= NULL;
		nsh->parseItemTag		= ns_slash_parseItemTag;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= NULL;
		nsh->doItemHeaderOutput		= ns_slash_doItemOutput;
		nsh->doItemFooterOutput		= NULL;
	}

	return nsh;
}

