/*
   generic OPML 1.0 support
   
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>

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

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "support.h"
#include "conf.h"
#include "common.h"
#include "feed.h"
#include "item.h"
#include "opml.h"
#include "callbacks.h"
#include "htmlview.h"

/* you can find the OPML specification at Userland:

   http://www.opml.org/
   
 */
 
/* this is a generic subtag list for directory, channel and format description tags */
#define OPML_TITLE		0
#define OPML_CREATED		1
#define OPML_MODIFIED		2
#define OPML_OWNERNAME		3
#define OPML_OWNEREMAIL		4
#define OPML_MAX_TAG		5

/* note: the tag order has to correspond with the OCS_* defines in the header file */
static gchar *opmlTagList[] = {	"title",
				"dateCreated",
				"dateModified",
				"ownerName",
				"ownerEmail",
				NULL
 			      };

/* ---------------------------------------------------------------------------- */
/* OPML parsing and HTML output	 						*/
/* ---------------------------------------------------------------------------- */
			      
/* retruns a HTML string containing the text and attributes of the outline */
static gchar * getOutlineContents(xmlNodePtr cur) {
	gchar		*buffer = NULL;
	gchar		*tmp, *value;
	xmlAttrPtr	attr;

	attr = cur->properties;
	while(NULL != attr) {
		/* get prop value */
 		value = CONVERT(xmlGetProp(cur, attr->name));
		if(NULL != value) {
			if(!xmlStrcmp(attr->name, BAD_CAST"text")) {		
				tmp = g_strdup_printf("<p class=\"opmltext\">%s</p>", value);
				addToHTMLBuffer(&buffer, tmp);
				g_free(tmp);

			} else if(!xmlStrcmp(attr->name, BAD_CAST"isComment")) {
				/* don't output anything */

			} else if(!xmlStrcmp(attr->name, BAD_CAST"type")) {
				/* don't output anything */

			} else if(!xmlStrcmp(attr->name, BAD_CAST"url")) {		
				tmp = g_strdup_printf("<p class=\"opmlurl\">URL : <a href=\"%s\">%s</a></p>", value, value);
				addToHTMLBuffer(&buffer, tmp);
				g_free(tmp);

			} else if(!xmlStrcmp(attr->name, BAD_CAST"htmlUrl") ||
			          !xmlStrcmp(attr->name, BAD_CAST"htmlurl")) {		
				tmp = g_strdup_printf("<p class=\"opmlhtmlurl\">HTML : <a href=\"%s\">%s</a></p>", value, value);
				addToHTMLBuffer(&buffer, tmp);
				g_free(tmp);
				
			} else if(!xmlStrcmp(attr->name, BAD_CAST"xmlUrl") ||
			          !xmlStrcmp(attr->name, BAD_CAST"xmlurl")) {		
				tmp = g_strdup_printf("<p class=\"opmlxmlurl\">XML : <a href=\"%s\">%s</a></p>", value, value);
				addToHTMLBuffer(&buffer, tmp);
				g_free(tmp);

			} else {		
				tmp = g_strdup_printf("<p class=\"opmlanyattribute\">%s : %s\n</p>", (gchar *)attr->name, value);
				addToHTMLBuffer(&buffer, tmp);
				g_free(tmp);
			}

			g_free(value);
		}
		attr = attr->next;
	}
			
	/* check for <outline> subtags */
	if(NULL != cur->xmlChildrenNode) {
		addToHTMLBuffer(&buffer, "<ul style=\"opmlchilds\">");
		cur = cur->xmlChildrenNode;
		while(NULL != cur) {
			if(!xmlStrcmp(cur->name, BAD_CAST"outline")) {
				tmp = g_strdup_printf("<li class=\"opmllistitem\">%s</li>", getOutlineContents(cur));
				addToHTMLBuffer(&buffer, tmp);
				g_free(tmp);
			}

			cur = cur->next;
		}
		addToHTMLBuffer(&buffer, "</ul>");
	}

	return buffer;
}

static void opml_parse(feedPtr fp, xmlDocPtr doc, xmlNodePtr cur) {
	xmlNodePtr 	 child;
	itemPtr		ip;
	gchar		*buffer, *tmp;
	gchar		*headTags[OPML_MAX_TAG];
	int 		i, error = 0;

	do {

		if(!xmlStrcmp(cur->name, BAD_CAST"opml") ||
		   !xmlStrcmp(cur->name, BAD_CAST"oml") ||
		   !xmlStrcmp(cur->name, BAD_CAST"outlineDocument")) {
		   	// nothing
		} else {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Could not find OPML header!</p>"));
			xmlFreeDoc(doc);
			error = 1;
			break;			
		}

		cur = cur->xmlChildrenNode;
		while (cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}

		memset(headTags, 0, sizeof(gchar *)*OPML_MAX_TAG);		
		while (cur != NULL) {
			if(!xmlStrcmp(cur->name, BAD_CAST"head")) {
				/* check for <head> tags */
				child = cur->xmlChildrenNode;
				while(child != NULL) {
					for(i = 0; i < OPML_MAX_TAG; i++) {
						if (!xmlStrcmp(child->name, (const xmlChar *)opmlTagList[i])) {
							tmp = CONVERT(xmlNodeListGetString(doc, child->xmlChildrenNode, 1));						
							if(NULL != tmp) {
								g_free(headTags[i]);
								headTags[i] = tmp;
							}
						}		
					}
					child = child->next;
				}
			}
			
			if(!xmlStrcmp(cur->name, BAD_CAST"body")) {
				/* process all <outline> tags */
				child = cur->xmlChildrenNode;
				while(child != NULL) {
					if(!xmlStrcmp(child->name, BAD_CAST"outline")) {
						buffer = NULL;
						addToHTMLBuffer(&buffer, tmp = getOutlineContents(child));
						g_free(tmp);
						
						ip = item_new();
						if(NULL == (tmp = CONVERT(xmlGetProp(child, BAD_CAST"text"))))
							tmp = CONVERT(xmlGetProp(child, BAD_CAST"title"));
						ip->title = tmp;
						ip->description = buffer;
						ip->readStatus = TRUE;
						feed_add_item(fp, ip);
					}
					child = child->next;
				}
			}
			
			cur = cur->next;
		}

		/* after parsing we fill in the infos into the feedPtr structure */		
		fp->type = FST_OPML;
		feed_set_update_interval(fp, -1);
		if(NULL == (fp->title = headTags[OPML_TITLE]))
			fp->title = g_strdup(fp->source);
		
		if(0 == error) {
			/* prepare HTML output */
			buffer = NULL;
			addToHTMLBuffer(&buffer, FEED_HEAD_START);	
			addToHTMLBuffer(&buffer, FEED_HEAD_CHANNEL);
			addToHTMLBuffer(&buffer, fp->title);
			addToHTMLBuffer(&buffer, HTML_NEWLINE);	
			addToHTMLBuffer(&buffer, FEED_HEAD_SOURCE);	
			if(NULL != fp->source) {
				tmp = g_strdup_printf("<a href=\"%s\">%s</a>", fp->source, fp->source);
				addToHTMLBuffer(&buffer, tmp);
				g_free(tmp);
			}

			addToHTMLBuffer(&buffer, FEED_HEAD_END);	

			addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_START);
			FEED_FOOT_WRITE(buffer, "title",		headTags[OPML_TITLE]);
			FEED_FOOT_WRITE(buffer, "creation date",	headTags[OPML_CREATED]);
			FEED_FOOT_WRITE(buffer, "last modified",	headTags[OPML_MODIFIED]);
			FEED_FOOT_WRITE(buffer, "owner name",		headTags[OPML_OWNERNAME]);
			FEED_FOOT_WRITE(buffer, "owner email",		headTags[OPML_OWNEREMAIL]);
			addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_END);
			
			fp->description = buffer;
			fp->available = TRUE;
		} else {
			ui_mainwindow_set_status_bar(_("There were errors while parsing this feed!"));
		}
		
		break;
	} while (FALSE);
}

static gboolean opml_format_check(xmlDocPtr doc, xmlNodePtr cur) {
	if(!xmlStrcmp(cur->name, BAD_CAST"opml") ||
	   !xmlStrcmp(cur->name, BAD_CAST"oml") || 
	   !xmlStrcmp(cur->name, BAD_CAST"outlineDocument")) {
		
		return TRUE;
	}
	return FALSE;
}
/* ---------------------------------------------------------------------------- */
/* initialization								*/
/* ---------------------------------------------------------------------------- */

feedHandlerPtr initOPMLFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);
	
	/* prepare feed handler structure */
	fhp->feedParser	= opml_parse;
	fhp->checkFormat = opml_format_check;
	fhp->merge		= FALSE;
	
	return fhp;
}
