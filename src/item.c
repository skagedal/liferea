/**
 * @file item.c common item handling
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <glib.h>
#include <time.h>
#include <string.h> /* For memset() */
#include <stdlib.h>
#include <libxml/uri.h>

#include "vfolder.h"
#include "item.h"
#include "support.h"
#include "common.h"
#include "ui_htmlview.h"
#include "callbacks.h"
#include "ui_tray.h"
#include "metadata.h"
#include "debug.h"

/* function to create a new feed structure */
itemPtr item_new(void) {
	itemPtr		ip;
	
	ip = g_new0(struct item, 1);
	ip->newStatus = TRUE;

	return ip;
}

void item_copy(itemPtr from, itemPtr to) {

	item_set_title(to, from->title);
	item_set_source(to, from->source);
	item_set_real_source_url(to, from->real_source_url);
	item_set_real_source_title(to, from->real_source_title);
	item_set_description(to, from->description);
	item_set_id(to, from->id);
	
	to->updateStatus = from->updateStatus;
	to->readStatus = from->readStatus;
	to->newStatus = FALSE;
	to->marked = from->marked;
	to->time = from->time;
	to->nr = from->nr;
	
	/* the following line allows state propagation in item.c */
	to->sourceFeed = from->fp;	
	
	/* this copies metadata */
	metadata_list_free(to->metadata);
	to->metadata = NULL;
	to->metadata = metadata_list_copy(from->metadata, to->metadata);
}

void item_set_title(itemPtr ip, const gchar * title) {

	g_free(ip->title);
	ip->title = g_strdup(title);
}

void item_set_description(itemPtr ip, const gchar * description) {

	g_free(ip->description);
	ip->description = g_strdup(description);
}

void item_set_source(itemPtr ip, const gchar * source) { g_free(ip->source);ip->source = g_strdup(source); }
void item_set_real_source_url(itemPtr ip, const gchar * source) { g_free(ip->real_source_url);ip->real_source_url = g_strdup(source); }
void item_set_real_source_title(itemPtr ip, const gchar * source) { g_free(ip->real_source_title);ip->real_source_title = g_strdup(source); }
void item_set_time(itemPtr ip, const time_t t) { ip->time = t; }

void item_set_id(itemPtr ip, const gchar * id) {
	g_free(ip->id);
	ip->id = g_strdup(id);
}

void item_set_hidden(itemPtr ip, const gboolean hidden) { ip->hidden = hidden; }

const gchar *	item_get_id(itemPtr ip) { return (ip != NULL ? ip->id : NULL); }
const gchar *	item_get_title(itemPtr ip) {return (ip != NULL ? ip->title : NULL); }
const gchar *	item_get_description(itemPtr ip) { return (ip != NULL ? ip->description : NULL); }
const gchar *	item_get_source(itemPtr ip) { return (ip != NULL ? ip->source : NULL); }
const gchar *	item_get_real_source_url(itemPtr ip) { return (ip != NULL ? ip->real_source_url : NULL); }
const gchar *	item_get_real_source_title(itemPtr ip) { return (ip != NULL ? ip->real_source_title : NULL); }
const time_t	item_get_time(itemPtr ip) { return (ip != NULL ? ip->time : 0); }
const gboolean item_get_read_status(itemPtr ip) { return (ip != NULL ? ip->readStatus : FALSE); }
const gboolean item_get_flag(itemPtr ip) { g_assert(ip != NULL); return ip->marked; }
const gboolean item_get_hidden(itemPtr ip) { g_assert(ip != NULL); return ip->hidden; }
const gboolean item_get_new_status(itemPtr ip) { g_assert(ip != NULL); return ip->newStatus; }
const gboolean item_get_update_status(itemPtr ip) { g_assert(ip != NULL); return ip->updateStatus; }

void item_set_flag(itemPtr ip, gboolean newStatus) {

	if(newStatus != ip->marked) {
		ip->marked = newStatus;
		if(ip->fp != NULL)
			ip->fp->needsCacheSave = TRUE;
	}
}

void item_set_new_status(itemPtr ip, const gboolean newStatus) { 

	if(newStatus != ip->newStatus) {
		if(TRUE == newStatus)
			feed_increase_new_counter((feedPtr)(ip->fp));
		else
			feed_decrease_new_counter((feedPtr)(ip->fp));
			
		ip->newStatus = newStatus; 
		/* no need to save feed */
	}
}

void item_set_update_status(itemPtr ip, const gboolean newStatus) { 
	
	if(newStatus != ip->updateStatus) {
		ip->updateStatus = newStatus; 
		if(ip->fp != NULL)
			ip->fp->needsCacheSave = TRUE;
	}
}

void item_set_read_status(itemPtr ip, gboolean newStatus) { 
	
	if(newStatus != ip->readStatus) {
		if(FALSE == newStatus)
			feed_increase_unread_counter((feedPtr)(ip->fp));
		else
			feed_decrease_unread_counter((feedPtr)(ip->fp));

		ip->readStatus = newStatus;
		if(ip->fp != NULL)
			ip->fp->needsCacheSave = TRUE;
	}
}

void item_free(itemPtr ip) {
	
	if(FALSE == ip->readStatus)
		feed_decrease_unread_counter(ip->fp);
	if(TRUE == ip->newStatus)
		feed_decrease_new_counter(ip->fp);

	g_free(ip->title);
	g_free(ip->source);
	g_free(ip->real_source_url);
	g_free(ip->real_source_title);
	g_free(ip->description);
	g_free(ip->id);
	g_assert(NULL == ip->tmpdata);	/* should be free after rendering */
	metadata_list_free(ip->metadata);

	g_free(ip);
}

gchar *item_render(itemPtr ip) {
	struct displayset	displayset;
	gchar			*buffer = NULL;
	gchar			*tmp, *tmp2;
	xmlChar			*tmp3;
	gboolean		migration = FALSE;
	
	displayset.headtable = NULL;
	displayset.head = NULL;
	displayset.body = g_strdup(item_get_description(ip));
	displayset.foot = NULL;
	displayset.foottable = NULL;
	
	/* FIXME: remove with 0.9.x */
	if((NULL != displayset.body) &&
	   (NULL != strstr(displayset.body, "class=\"itemhead\"")))	/* I hope this is unique enough...*/
		migration = TRUE;

	if(FALSE == migration) {	
		metadata_list_render(ip->metadata, &displayset);	
		/* Head table */
		addToHTMLBufferFast(&buffer, HEAD_START);
		/*  -- Feed line */
		if(feed_get_html_url((NULL == ip->sourceFeed)?ip->fp:ip->sourceFeed) != NULL)
			tmp = g_strdup_printf("<span class=\"feedlink\"><a href=\"%s\">%s</a></span>",
			                      feed_get_html_url((NULL == ip->sourceFeed)?ip->fp:ip->sourceFeed),
			                      feed_get_title((NULL == ip->sourceFeed)?ip->fp:ip->sourceFeed));
		else
			tmp = g_strdup_printf("<span class=\"feedlink\">%s</span>",
			                      feed_get_title((NULL == ip->sourceFeed)?ip->fp:ip->sourceFeed));

		tmp2 = g_strdup_printf(HEAD_LINE, _("Feed:"), tmp);
		g_free(tmp);
		addToHTMLBufferFast(&buffer, tmp2);
		g_free(tmp2);

		/*  -- Item line */
		tmp = NULL;
		
		if((NULL != ip->sourceFeed) && (NULL != ip->sourceFeed->icon))
			tmp = (gchar *)feed_get_id(ip->sourceFeed);
		else if((NULL != ip->fp) && (NULL != ip->fp->icon))
			tmp = (gchar *)feed_get_id(ip->fp);
			
		if(NULL != tmp) {
			tmp2 = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", tmp, "png");
			tmp = g_strdup_printf("<a href=\"%s\"><img class=\"favicon\" src=\"%s\"></a>", feed_get_html_url((NULL == ip->sourceFeed)?ip->fp:ip->sourceFeed), tmp2);
			g_free(tmp2);
		} else {
			tmp2 = g_strdup(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "available.png");
			tmp = g_strdup_printf("<a href=\"%s\"><img class=\"favicon\" src=\"%s\"></a>", feed_get_html_url((NULL == ip->sourceFeed)?ip->fp:ip->sourceFeed), tmp2);
			g_free(tmp2);
		}
		
		if(item_get_source(ip) != NULL)
			tmp = g_strdup_printf("<span class=\"itemtitle\">%s<a href=\"%s\">%s</a></span>",
			                      tmp,
			                      item_get_source(ip),
			                      (item_get_title(ip) != NULL)?item_get_title(ip):_("[No title]"));
		else
			tmp = g_strdup_printf("<span class=\"itemtitle\">%s%s</span>",
			                      tmp,
			                      (item_get_title(ip) != NULL)?item_get_title(ip):_("[No title]"));
					      
		tmp2 = g_strdup_printf(HEAD_LINE, _("Item:"), tmp);
		g_free(tmp);
		addToHTMLBufferFast(&buffer, tmp2);
		g_free(tmp2);
		
		/*  -- real source line */
		tmp = NULL;
		if(item_get_real_source_url(ip) != NULL)
			tmp = g_strdup_printf("<span class=\"itemsource\"><a href=\"%s\">%s</a></span>",
			                      item_get_real_source_url(ip),
			                      (item_get_real_source_title(ip) != NULL)? item_get_real_source_title(ip) : _("[No title]"));
		else if(item_get_real_source_title(ip) != NULL)
			tmp = g_strdup_printf("<span class=\"itemsource\">%s</span>",
			                      item_get_real_source_title(ip));
			
		if(NULL != tmp) {
			tmp2 = g_strdup_printf(HEAD_LINE, _("Source:"), tmp);
			g_free(tmp);
			addToHTMLBufferFast(&buffer, tmp2);
			g_free(tmp2);	
		}

		addToHTMLBufferFast(&buffer, displayset.headtable);
		g_free(displayset.headtable);
		addToHTMLBufferFast(&buffer, HEAD_END);

		/* Head */
		if(displayset.head != NULL) {
			addToHTMLBufferFast(&buffer, displayset.head);
			g_free(displayset.head);
		}

		/* feed/channel image */
		if(NULL != feed_get_image_url(ip->fp)) {
			addToHTMLBufferFast(&buffer, "<img class=\"feed\" src=\"");
			addToHTMLBufferFast(&buffer, feed_get_image_url(ip->fp));
			addToHTMLBufferFast(&buffer, "\"><br>");
		}
	}

	if(displayset.body != NULL) {
		addToHTMLBufferFast(&buffer, displayset.body);
		g_free(displayset.body);
	}

	if(FALSE == migration) {
		if(displayset.foot != NULL) {
			addToHTMLBufferFast(&buffer, displayset.foot);
			g_free(displayset.foot);
		}
		
		/* add technorati link */
		tmp3 = xmlURIEscape(item_get_source(ip));
		tmp2 = g_strdup("file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "technorati.png");
		tmp = g_strdup_printf(TECHNORATI_LINK, tmp3, tmp2);
		addToHTMLBufferFast(&buffer, tmp);
		xmlFree(tmp3);
		g_free(tmp2);
		g_free(tmp);

		if(displayset.foottable != NULL) {
			addToHTMLBufferFast(&buffer, FEED_FOOT_TABLE_START);
			addToHTMLBufferFast(&buffer, displayset.foottable);
			addToHTMLBufferFast(&buffer, FEED_FOOT_TABLE_END);
			g_free(displayset.foottable);
		}
	}
	return buffer;
}

void item_display(itemPtr ip) {
	gchar	*buffer = NULL, *tmp;
	
	ui_htmlview_start_output(&buffer, TRUE);
	tmp = item_render(ip);
	addToHTMLBufferFast(&buffer, tmp);
	g_free(tmp);
	ui_htmlview_finish_output(&buffer);
	if (feed_get_source(ip->fp) != NULL &&
	    ip->fp->source[0] != '|' &&
	    strstr(feed_get_source(ip->fp), "://") != NULL)
		ui_htmlview_write(ui_mainwindow_get_active_htmlview(), buffer, feed_get_source(ip->fp));
	else
		ui_htmlview_write(ui_mainwindow_get_active_htmlview(), buffer, NULL);
	g_free(buffer);
}

itemPtr item_parse_cache(xmlDocPtr doc, xmlNodePtr cur) {
	itemPtr 	ip;
	gchar		*tmp;
	
	g_assert(NULL != doc);
	g_assert(NULL != cur);
	
	ip = item_new();
	ip->newStatus = FALSE;
	
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		
		if(cur->type != XML_ELEMENT_NODE ||
		   NULL == (tmp = utf8_fix(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)))) {
			cur = cur->next;
			continue;
		}
		
		if(!xmlStrcmp(cur->name, BAD_CAST"title"))
			item_set_title(ip, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"description"))
			item_set_description(ip, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"source"))
			item_set_source(ip, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"real_source_url"))
			item_set_real_source_url(ip, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"real_source_title"))
			item_set_real_source_title(ip, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"id"))
			item_set_id(ip, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"nr"))
			ip->nr = atol(tmp);

		else if(!xmlStrcmp(cur->name, BAD_CAST"readStatus"))
			ip->readStatus = (0 == atoi(tmp))?FALSE:TRUE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"updateStatus"))
			ip->updateStatus = (0 == atoi(tmp))?FALSE:TRUE;

		else if(!xmlStrcmp(cur->name, BAD_CAST"mark")) 
			/* we don't call item_set_mark here because it would
			 * update the UI */
			ip->marked = (1 == atoi(tmp))?TRUE:FALSE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"time"))
			item_set_time(ip, atol(tmp));
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"attributes"))
			ip->metadata = metadata_parse_xml_nodes(doc, cur);
		
		g_free(tmp);	
		cur = cur->next;
	}

	return ip;
}

void item_save(itemPtr ip, xmlNodePtr feedNode) {
	xmlNodePtr	itemNode;
	gchar		*tmp;
	
	if(NULL != (itemNode = xmlNewChild(feedNode, NULL, "item", NULL))) {

		/* should never happen... */
		if(NULL == item_get_title(ip))
			item_set_title(ip, "");
		xmlNewTextChild(itemNode, NULL, "title", item_get_title(ip));

		if(NULL != item_get_description(ip))
			xmlNewTextChild(itemNode, NULL, "description", item_get_description(ip));

		if(NULL != item_get_source(ip))
			xmlNewTextChild(itemNode, NULL, "source", item_get_source(ip));

		if(NULL != item_get_real_source_title(ip))
			xmlNewTextChild(itemNode, NULL, "real_source_title", item_get_real_source_title(ip));

		if(NULL != item_get_real_source_url(ip))
			xmlNewTextChild(itemNode, NULL, "real_source_url", item_get_real_source_url(ip));

		if(NULL != item_get_id(ip))
			xmlNewTextChild(itemNode, NULL, "id", item_get_id(ip));

		tmp = g_strdup_printf("%ld", ip->nr);
		xmlNewTextChild(itemNode, NULL, "nr", tmp);
		g_free(tmp);

		tmp = g_strdup_printf("%d", (TRUE == item_get_read_status(ip))?1:0);
		xmlNewTextChild(itemNode, NULL, "readStatus", tmp);
		g_free(tmp);
		
		tmp = g_strdup_printf("%d", (TRUE == item_get_update_status(ip))?1:0);
		xmlNewTextChild(itemNode, NULL, "updateStatus", tmp);
		g_free(tmp);

		tmp = g_strdup_printf("%d", (TRUE == item_get_flag(ip))?1:0);
		xmlNewTextChild(itemNode, NULL, "mark", tmp);
		g_free(tmp);

		tmp = g_strdup_printf("%ld", item_get_time(ip));
		xmlNewTextChild(itemNode, NULL, "time", tmp);
		g_free(tmp);

		metadata_add_xml_nodes(ip->metadata, itemNode);

	} else {
		g_warning("could not write XML item node!\n");
	}
}
