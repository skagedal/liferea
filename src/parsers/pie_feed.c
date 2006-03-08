/**
 * @file pie_feed.c Atom/Echo/PIE 0.2/0.3 channel parsing
 * 
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

#include <sys/time.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "support.h"
#include "conf.h"
#include "common.h"
#include "feed.h"
#include "pie_feed.h"
#include "pie_entry.h"
#include "ns_dc.h"
#include "callbacks.h"
#include "metadata.h"

/* to store the PIENsHandler structs for all supported RDF namespace handlers */
GHashTable	*pie_nstable = NULL;
GHashTable	*ns_pie_ns_uri_table = NULL;

/* note: the tag order has to correspond with the PIE_FEED_* defines in the header file */
/*
  The follow are not used, but had been recognized:
                                   "language", <---- Not in atom 0.2 or 0.3. We should use xml:lang
							"lastBuildDate", <--- Where is this from?
							"issued", <-- Not in the specs for feeds
							"created",  <---- Not in the specs for feeds
*/
gchar* pie_parse_content_construct(xmlNodePtr cur) {
	gchar	*mode, *type, *tmp, *ret;

	g_assert(NULL != cur);
	ret = NULL;
	
	/* determine encoding mode */
	mode = utf8_fix(xmlGetProp(cur, BAD_CAST"mode"));
	type = utf8_fix(xmlGetProp(cur, BAD_CAST"type"));

	/* Modes are used in older versions of ATOM, including 0.3. It
	   does not exist in the newer IETF drafts.*/
	if(NULL != mode) {
		if(!strcmp(mode, "escaped")) {
			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if(NULL != tmp)
				ret = tmp;
			
		} else if(!strcmp(mode, "xml")) {
			ret = extractHTMLNode(cur, 1,"http://default.base.com/");
			
		} else if(!strcmp(mode, "base64")) {
			g_warning("Base64 encoded <content> in Atom feeds not supported!\n");
			
		} else if(!strcmp(mode, "multipart/alternative")) {
			if(NULL != cur->xmlChildrenNode)
				ret = pie_parse_content_construct(cur->xmlChildrenNode);
		}
		g_free(mode);
	} else {
		/* some feeds don'ts specify a mode but a MIME type in the
		   type attribute... */
		/* not sure what MIME types are necessary... */

		/* This that need to be de-encoded and should not contain sub-tags.*/
		if (NULL == type || (
						 !strcmp(type, "TEXT") ||
						 !strcmp(type, "text/plain") ||
						 !strcmp(type, "HTML") ||
						 !strcmp(type, "text/html"))) {
			ret = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			/* Next are things that contain subttags */
		} else if((NULL == type) ||
				/* HTML types */
				!strcmp(type, "XHTML") ||
				!strcmp(type, "application/xhtml+xml")) {
			/* Text types */
			ret = extractHTMLNode(cur, 1,"http://default.base.com/");
		}
	}
	/* If the type was text, everything must be now escaped and
	   wrapped in pre tags.... Also, the atom 0.3 spec says that the
	   default type MUST be considered to be text/plain. The type tag
	   is required in 0.2.... */
	//if (ret != NULL && (type == NULL || !strcmp(type, "text/plain") || !strcmp(type,"TEXT")))) {
	if((ret != NULL) && (type != NULL) && (!strcmp(type, "text/plain") || !strcmp(type,"TEXT"))) {
		gchar *tmp = g_markup_printf_escaped("<pre>%s</pre>", ret);
		g_free(ret);
		ret = tmp;
	}
	g_free(type);
	
	return ret;
}


/* nonstatic because used by pie_entry.c too */
gchar * parseAuthor(xmlNodePtr cur) {
	gchar	*tmp = NULL;
	gchar	*tmp2, *tmp3;

	g_assert(NULL != cur);
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if(NULL == cur->name) {
			g_warning("invalid XML: parser returns NULL value -> tag ignored!");
			cur = cur->next;
			continue;
		}
		
		if (!xmlStrcmp(cur->name, BAD_CAST"name"))
			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));

		if (!xmlStrcmp(cur->name, BAD_CAST"email")) {
			tmp2 = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			tmp3 = g_strdup_printf("%s <a href=\"mailto:%s\">%s</a>", tmp, tmp2, tmp2);
			g_free(tmp);
			g_free(tmp2);
			tmp = tmp3;
		}
					
		if (!xmlStrcmp(cur->name, BAD_CAST"url")) {
			tmp2 = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			tmp3 = g_strdup_printf("%s (<a href=\"%s\">Website</a>)", tmp, tmp2);
			g_free(tmp);
			g_free(tmp2);
			tmp = tmp3;
		}
		cur = cur->next;
	}

	return tmp;
}

/* reads a PIE feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void pie_parse(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	itemPtr 		ip;
	gchar			*tmp2, *tmp = NULL, *tmp3;
	int 			error = 0;
	NsHandler		*nsh;
	parseChannelTagFunc	pf;
	
	while(TRUE) {
		if(xmlStrcmp(cur->name, BAD_CAST"feed")) {
			addToHTMLBuffer(&(ctxt->feed->parseErrors), _("<p>Could not find Atom/Echo/PIE header!</p>"));
			error = 1;
			break;			
		}

		/* parse feed contents */
		cur = cur->xmlChildrenNode;
		while(cur != NULL) {
			if(NULL == cur->name || cur->type != XML_ELEMENT_NODE) {
				cur = cur->next;
				continue;
			}
			
			/* check namespace of this tag */
			if(NULL != cur->ns) {
				if(((cur->ns->href != NULL) &&
				    NULL != (nsh = (NsHandler *)g_hash_table_lookup(ns_pie_ns_uri_table, (gpointer)cur->ns->href))) ||
				   ((cur->ns->prefix != NULL) &&
				    NULL != (nsh = (NsHandler *)g_hash_table_lookup(pie_nstable, (gpointer)cur->ns->prefix)))) {
					pf = nsh->parseChannelTag;
					if(NULL != pf)
						(*pf)(ctxt->feed, cur);
					cur = cur->next;
					continue;
				} else {
					/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
				}
			} /* explicitly no following else !!! */
			
			if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
				tmp = unhtmlize(utf8_fix(pie_parse_content_construct(cur)));
				if (tmp != NULL)
					node_set_title(ctxt->node, tmp);
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
				if(NULL != (tmp = utf8_fix(xmlGetProp(cur, BAD_CAST"href")))) {
					/* 0.3 link : rel, type and href attribute */
					tmp2 = utf8_fix(xmlGetProp(cur, BAD_CAST"rel"));
					if(tmp2 != NULL && !xmlStrcmp(tmp2, BAD_CAST"alternate"))
						feed_set_html_url(ctxt->feed, tmp);
					else
						/* FIXME: Maybe do something with other links? */;
					g_free(tmp2);
					g_free(tmp);
				} else {
					/* 0.2 link : element content is the link, or non-alternate link in 0.3 */
					tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
					if(NULL != tmp)
						feed_set_html_url(ctxt->feed, tmp);
					g_free(tmp);
				}
				
			/* parse feed author */
			} else if(!xmlStrcmp(cur->name, BAD_CAST"author")) {
				/* parse feed author */
				tmp = parseAuthor(cur);
				ctxt->feed->metadata = metadata_list_append(ctxt->feed->metadata, "author", tmp);
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"tagline")) {
				tmp = convertToHTML(utf8_fix(pie_parse_content_construct(cur)));
				if (tmp != NULL)
					feed_set_description(ctxt->feed, tmp);
				g_free(tmp);				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"generator")) {
				tmp = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));
				if (tmp != NULL && tmp[0] != '\0') {
					tmp2 = utf8_fix(xmlGetProp(cur, BAD_CAST"version"));
					if (tmp2 != NULL) {
						tmp3 = g_strdup_printf("%s %s", tmp, tmp2);
						g_free(tmp);
						g_free(tmp2);
						tmp = tmp3;
					}
					tmp2 = utf8_fix(xmlGetProp(cur, BAD_CAST"url"));
					if (tmp2 != NULL) {
						tmp3 = g_strdup_printf("<a href=\"%s\">%s</a>", tmp2, tmp);
						g_free(tmp2);
						g_free(tmp);
						tmp = tmp3;
					}
					ctxt->feed->metadata = metadata_list_append(ctxt->feed->metadata, "feedgenerator", tmp);
				}
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"copyright")) {
				tmp = utf8_fix(pie_parse_content_construct(cur));
				if(NULL != tmp)
					ctxt->feed->metadata = metadata_list_append(ctxt->feed->metadata, "copyright", tmp);
				g_free(tmp);
				
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"modified")) { /* Modified was last used in IETF draft 02) */
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				if(NULL != tmp) {
					ctxt->feed->metadata = metadata_list_append(ctxt->feed->metadata, "pubDate", tmp);
					ctxt->feed->time = parseISO8601Date(tmp);
					g_free(tmp);
				}

			} else if(!xmlStrcmp(cur->name, BAD_CAST"updated")) { /* Updated was added in IETF draft 03 */
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				if(NULL != tmp) {
					ctxt->feed->metadata = metadata_list_append(ctxt->feed->metadata, "pubDate", tmp);
					ctxt->feed->time = parseISO8601Date(tmp);
					g_free(tmp);
				}

			} else if(!xmlStrcmp(cur->name, BAD_CAST"contributor")) { 
				/* parse feed contributors */
				tmp = parseAuthor(cur);
				ctxt->feed->metadata = metadata_list_append(ctxt->feed->metadata, "contributor", tmp);
				g_free(tmp);
				
			} else if((!xmlStrcmp(cur->name, BAD_CAST"entry"))) {
				if(NULL != (ip = parseEntry(ctxt->feed, cur))) {
					if(0 == item_get_time(ip))
						item_set_time(ip, ctxt->feed->time);
					itemset_append_item(ctxt->itemSet, ip);
				}
			}
			
			/* collect PIE feed entries */
			cur = cur->next;
		}
		
		/* after parsing we fill in the infos into the feedPtr structure */		
		if(0 == error) {
			ctxt->feed->available = TRUE;
		} else {
			ui_mainwindow_set_status_bar(_("There were errors while parsing this feed!"));
		}

		break;
	}
}

static gboolean pie_format_check(xmlDocPtr doc, xmlNodePtr cur) {
	if(!xmlStrcmp(cur->name, BAD_CAST"feed")) {
		return TRUE;
	}
	return FALSE;
}

static void pie_add_ns_handler(NsHandler *handler) {

	g_assert(NULL != pie_nstable);
	g_hash_table_insert(pie_nstable, handler->prefix, handler);
	g_assert(handler->registerNs != NULL);
	handler->registerNs(handler, pie_nstable, ns_pie_ns_uri_table);
}

feedHandlerPtr pie_init_feed_handler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);
	
	if(NULL == pie_nstable) {
		pie_nstable = g_hash_table_new(g_str_hash, g_str_equal);
		ns_pie_ns_uri_table = g_hash_table_new(g_str_hash, g_str_equal);
		
		/* register RSS name space handlers */
		pie_add_ns_handler(ns_dc_getRSSNsHandler());
	}	


	/* prepare feed handler structure */
	fhp->typeStr = "pie";
	fhp->icon = ICON_AVAILABLE;
	fhp->directory = FALSE;
	fhp->feedParser	= pie_parse;
	fhp->checkFormat = pie_format_check;
	fhp->merge = TRUE;

	return fhp;
}

