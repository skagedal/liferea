/*
   admin namespace support
   
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

#include <string.h> /* For strcmp() */
#include "htmlview.h"
#include "support.h"
#include "ns_admin.h"
#include "common.h"

#define TABLE_START	"<div class=\"feedfoottitle\">administrative information</div><table class=\"addfoot\">"
#define FIRSTTD		"<tr class=\"feedfoot\"><td class=\"feedfootname\"><span class=\"feedfootname\">"
#define NEXTTD		"</span></td><td class=\"feedfootvalue\"><span class=\"feedfootvalue\">"
#define LASTTD		"</span></td></tr>"
#define TABLE_END	"</table>"

static gchar ns_admin_prefix[] = "admin";

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

gchar * ns_admin_getRSSNsPrefix(void) { return ns_admin_prefix; }

static void ns_admin_addInfoStruct(GHashTable *nslist, gchar *tagname, gchar *tagvalue) {
	GHashTable	*nsvalues;
	
	g_assert(nslist != NULL);

	if(tagvalue == NULL)
		return;
			
	if(NULL == (nsvalues = (GHashTable *)g_hash_table_lookup(nslist, ns_admin_prefix))) {
		nsvalues = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(nslist, (gpointer)ns_admin_prefix, (gpointer)nsvalues);
	}
	g_hash_table_insert(nsvalues, (gpointer)tagname, (gpointer)tagvalue);
}

void ns_admin_parseChannelTag(RSSChannelPtr cp, xmlNodePtr cur) {
	
	if(!xmlStrcmp("errorReportsTo", cur->name)) 
		ns_admin_addInfoStruct(cp->nsinfos, "errorReportsTo", CONVERT(xmlGetProp(cur, "resource")));

	if(!xmlStrcmp("generatorAgent", cur->name)) 
		ns_admin_addInfoStruct(cp->nsinfos, "generatorAgent", CONVERT(xmlGetProp(cur, "resource")));
}

/* maybe I should overthink method names :-) */
void ns_admin_output(gpointer key, gpointer value, gpointer userdata) {
	gchar 	**buffer = (gchar **)userdata;
	
	addToHTMLBuffer(buffer, FIRSTTD);
	if(!strcmp(key, "errorReportsTo")) 
		addToHTMLBuffer(buffer, (gchar *)_("report errors to"));
	else if(!strcmp(key, "generatorAgent"))
		addToHTMLBuffer(buffer, (gchar *)_("feed generator"));
	else g_assert(1==2);
	
	addToHTMLBuffer(buffer, NEXTTD);
	addToHTMLBuffer(buffer, "<a href=\"");
	addToHTMLBuffer(buffer, value);
	addToHTMLBuffer(buffer, "\">");
	addToHTMLBuffer(buffer, value);
	addToHTMLBuffer(buffer, "</a>");	
	addToHTMLBuffer(buffer, LASTTD);		
}

gchar * ns_admin_doOutput(GHashTable *nsinfos) {
	GHashTable	*nsvalues;
	gchar		*buffer = NULL;
	
	/* we print all channel infos as a (key,value) table */
	g_assert(NULL != nsinfos);
	if(NULL != (nsvalues = g_hash_table_lookup(nsinfos, (gpointer)ns_admin_prefix))) {
		addToHTMLBuffer(&buffer, TABLE_START);
		g_hash_table_foreach(nsvalues, ns_admin_output, (gpointer)&buffer);
		addToHTMLBuffer(&buffer, TABLE_END);
	}	
	return buffer;
}

gchar * ns_admin_doChannelOutput(gpointer obj) {

	if(NULL != obj)
		return ns_admin_doOutput(((RSSChannelPtr)obj)->nsinfos);
	
	return NULL;
}

RSSNsHandler *ns_admin_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	if(NULL != (nsh = (RSSNsHandler *)g_malloc(sizeof(RSSNsHandler)))) {
		nsh->parseChannelTag		= ns_admin_parseChannelTag;;
		nsh->parseItemTag		= NULL;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= ns_admin_doChannelOutput;
		nsh->doItemHeaderOutput		= NULL;
		nsh->doItemFooterOutput		= NULL;
	}
	return nsh;
}
