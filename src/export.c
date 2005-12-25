/**
 * @file export.c OPML feedlist import&export
 *
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <libxml/tree.h>
#include <string.h>
#include "feed.h"
#include "vfolder.h"
#include "rule.h"
#include "conf.h"
#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "debug.h"
#include "plugin.h"
#include "fl_providers/fl_plugin.h"
#include "ui/ui_node.h"
#include "ui/ui_feedlist.h"

struct exportData {
	gboolean internal; /**< Include all the extra Liferea-specific tags */
	xmlNodePtr cur;
};

/* Used for exporting, this adds a folder or feed's node to the XML tree */
static void append_node_tag(nodePtr np, gpointer userdata) {
	xmlNodePtr 	cur = ((struct exportData*)userdata)->cur;
	gboolean	internal = ((struct exportData*)userdata)->internal;
	xmlNodePtr	childNode;
	struct exportData data;

	if((FST_PLUGIN == np->type) && (FALSE == internal))
		return;
	
	debug_enter("append_node_tag");

	childNode = xmlNewChild(cur, NULL, BAD_CAST"outline", NULL);

	/* 1. write generic node attributes */
	xmlNewProp(childNode, BAD_CAST"title", BAD_CAST node_get_title(np));
	xmlNewProp(childNode, BAD_CAST"text", BAD_CAST node_get_title(np)); /* The OPML spec requires "text" */
	xmlNewProp(childNode, BAD_CAST"description", BAD_CAST node_get_title(np));
	
	switch(np->type) {
		case FST_FEED:
			xmlNewProp(childNode, BAD_CAST"type", BAD_CAST node_type_to_str(np));
			break;
		case FST_PLUGIN:
			xmlNewProp(childNode, BAD_CAST"type", BAD_CAST "plugin");
			break;
		case FST_FOLDER:
		default:
			break;
	}

	if(internal) {
		xmlNewProp(childNode, BAD_CAST"id", BAD_CAST node_get_id(np));

		if(np->sortColumn == IS_LABEL)
			xmlNewProp(childNode, BAD_CAST"sortColumn", BAD_CAST"title");
		if(np->sortColumn == IS_TIME)
			xmlNewProp(childNode, BAD_CAST"sortColumn", BAD_CAST"time");

		if(FALSE == np->sortReversed)
			xmlNewProp(childNode, BAD_CAST"sortReversed", BAD_CAST"false");
			
		if(TRUE == node_get_two_pane_mode(np))
			xmlNewProp(childNode, BAD_CAST"twoPane", BAD_CAST"true");
	}

	/* 2. add node type specific stuff */
	switch(np->type) {
		case FST_FEED:
			feed_export((feedPtr)np->data, childNode, internal);
			break;
		case FST_FOLDER:
			/* add folder children */
			if(internal) {
				if(ui_node_is_folder_expanded(np))
					xmlNewProp(childNode, BAD_CAST"expanded", BAD_CAST"true");
				else
					xmlNewProp(childNode, BAD_CAST"collapsed", BAD_CAST"true");
			}
			debug1(DEBUG_CACHE, "adding folder %s...", node_get_title(np));
			data.cur = childNode;
			data.internal = internal;
			ui_feedlist_do_for_all_data(np, ACTION_FILTER_CHILDREN, append_node_tag, (gpointer)&data);
			break;
		case FST_VFOLDER:
			if(internal)
				vfolder_export((vfolderPtr)np->data, childNode);
			break;
		case FST_PLUGIN:
			if(internal)
				fl_plugin_export(np, childNode);
			break;
	}
	
	debug_exit("append_node_tag");
}


int export_OPML_feedlist(const gchar *filename, gboolean internal) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur, opmlNode;
	gint		error = 0;
	int		old_umask;

	debug_enter("export_OPML_feedlist");
	
	if(NULL != (doc = xmlNewDoc("1.0"))) {	
		if(NULL != (opmlNode = xmlNewDocNode(doc, NULL, BAD_CAST"opml", NULL))) {
			xmlNewProp(opmlNode, BAD_CAST"version", BAD_CAST"1.0");
			
			/* create head */
			if(NULL != (cur = xmlNewChild(opmlNode, NULL, BAD_CAST"head", NULL))) {
				xmlNewTextChild(cur, NULL, BAD_CAST"title", BAD_CAST"Liferea Feed List Export");
			}
			
			/* create body with feed list */
			if(NULL != (cur = xmlNewChild(opmlNode, NULL, BAD_CAST"body", NULL))) {
				struct exportData data;
				data.internal = internal;
				data.cur = cur;
				ui_feedlist_do_for_all_data(NULL, ACTION_FILTER_CHILDREN, append_node_tag, (gpointer)&data);
			}
			
			xmlDocSetRootElement(doc, opmlNode);		
		} else {
			g_warning("could not create XML feed node for feed cache document!");
			error = 1;
		}
		
		if(internal)
			old_umask = umask(077);
			
		if(-1 == xmlSaveFormatFileEnc(filename, doc, NULL, 1)) {
			g_warning("Could not export to OPML file!!");
			error = 1;
		}
		
		if(internal)
			umask(old_umask);
			
		xmlFreeDoc(doc);
	} else {
		g_warning("could not create XML document!");
		error = 1;
	}
	
	debug_exit("export_OPML_feedlist");
	return error;
}

static void import_parse_outline(xmlNodePtr cur, nodePtr parentNode, flNodeHandler *handler, gboolean trusted) {
	gchar		*title, *source, *typeStr, *tmp, *sortStr;
	gchar		*id = NULL;
	nodePtr		np = NULL;
	gpointer	data = NULL;
	guint		type;
	gboolean	dontParseChildren = FALSE;
	
	debug_enter("import_parse_outline");

	/* 1. do general node parsing */	
	np = node_new();
	np->handler = handler;

	/* The id should only be used from feedlist.opml. Otherwise,
	   it could cause corruption if the same id was imported
	   multiple times. */
	if(trusted) {
		id = xmlGetProp(cur, BAD_CAST"id");
		node_set_id(np, id);
		xmlFree(id);
	}
	
	/* title */
	title = xmlGetProp(cur, BAD_CAST"title");
	if(title == NULL || !xmlStrcmp(title, BAD_CAST"")) {
		if(title != NULL)
			xmlFree(title);
		title = xmlGetProp(cur, BAD_CAST"text");
	}
	node_set_title(np, title);

	if(title != NULL)
		xmlFree(title);

	/* sorting order */
	sortStr = xmlGetProp(cur, BAD_CAST"sortColumn");
	if(sortStr != NULL) {
		if(!xmlStrcmp(sortStr, "title"))
			np->sortColumn = IS_LABEL;
		else if(!xmlStrcmp(sortStr, "time"))
			np->sortColumn = IS_TIME;
		xmlFree(sortStr);
	}
	sortStr = xmlGetProp(cur, BAD_CAST"sortReversed");
	if(sortStr != NULL && !xmlStrcmp(sortStr, BAD_CAST"false"))
		np->sortReversed = FALSE;
	if(sortStr != NULL)
		xmlFree(sortStr);
	
	tmp = xmlGetProp(cur, BAD_CAST"twoPane");
	if(NULL != tmp && !xmlStrcmp(tmp, BAD_CAST"true"))
		node_set_two_pane_mode(np, TRUE);
	if(tmp != NULL)
		xmlFree(tmp);

	/* 2. determine node type */
	if(NULL != (typeStr = xmlGetProp(cur, BAD_CAST"type"))) {
		type = node_str_to_type(typeStr);
	} else {
		/* if the outline has no type it is propably a folder */
		type = FST_FOLDER;
		/* but we better checked for a source URL */
		if(NULL == (tmp = xmlGetProp(cur, BAD_CAST"xmlUrl")));
			tmp = xmlGetProp(cur, BAD_CAST"xmlUrl");
		
		if(NULL != tmp) {
			type = FST_FEED;
			xmlFree(tmp);
		}
	}
	
	/* 3. do node type specific parsing */
	switch(type) {
		case FST_FEED:
			data = feed_import(typeStr, cur, trusted);
			break;
		case FST_FOLDER:
			data = NULL;
			break;
		case FST_VFOLDER:
			data = vfolder_import(np, cur);
			break;
		case FST_PLUGIN:
			data = NULL;
			break;
	}

	if(NULL != typeStr)
		xmlFree(typeStr);

	if(NULL == node_get_id(np)) {
		id = node_new_id();
		node_set_id(np, id);
		debug1(DEBUG_CACHE, "seems to be an import, setting new id: %s and doing first download...", id);
		g_free(id);			
		node_request_update(np, (xmlHasProp(cur, BAD_CAST"updateInterval") ? 0 : FEED_REQ_RESET_UPDATE_INT)
			                | FEED_REQ_DOWNLOAD_FAVICON
			                | FEED_REQ_AUTH_DIALOG);
	}

	node_add_data(np, type, data);
	favicon_load(np);
	feedlist_add_node(parentNode, np, -1);
		
	if(FST_FOLDER == type) {
		if(NULL != xmlHasProp(cur, BAD_CAST"expanded"))
			ui_node_set_expansion(np, TRUE);
		if(NULL != xmlHasProp(cur, BAD_CAST"collapsed"))
			ui_node_set_expansion(np, FALSE);
	}

	/* import recursion */
	switch(np->type) {
		case FST_FOLDER:
			/* process any children */
			cur = cur->xmlChildrenNode;
			while(cur != NULL) {
				if((!xmlStrcmp(cur->name, BAD_CAST"outline")))
					import_parse_outline(cur, np, handler, trusted);
				cur = cur->next;				
			}
			break;
		case FST_PLUGIN:
			fl_plugin_import(np, cur);
			break;
		default:
			/* nothing to do */
			break;
	}
	
	debug_exit("import_parse_outline");
}

static void import_parse_body(xmlNodePtr n, nodePtr parentNode, flNodeHandler *handler, gboolean trusted) {
	xmlNodePtr cur;
	
	cur = n->xmlChildrenNode;
	while(cur != NULL) {
		if((!xmlStrcmp(cur->name, BAD_CAST"outline")))
			import_parse_outline(cur, parentNode, handler, trusted);
		cur = cur->next;
	}
}

static void import_parse_OPML(xmlNodePtr n, nodePtr parentNode, flNodeHandler *handler, gboolean trusted) {
	xmlNodePtr cur;
	
	cur = n->xmlChildrenNode;
	while(cur != NULL) {
		/* we ignore the head */
		if((!xmlStrcmp(cur->name, BAD_CAST"body"))) {
			import_parse_body(cur, parentNode, handler, trusted);
		}
		cur = cur->next;
	}	
}

void import_OPML_feedlist(const gchar *filename, nodePtr parentNode, flNodeHandler *handler, gboolean showErrors, gboolean trusted) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	
	debug1(DEBUG_CACHE, "Importing OPML file: %s", filename);
	
	/* read the feed list */
	if(NULL == (doc = xmlParseFile(filename))) {
		if(showErrors)
			ui_show_error_box(_("XML error while reading cache file! Could not import \"%s\"!"), filename);
		else
			g_warning(_("XML error while reading cache file! Could not import \"%s\"!"), filename);
	} else {
		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			if(showErrors)
				ui_show_error_box(_("Empty document! OPML document \"%s\" should not be empty when importing."), filename);
			else
				g_warning(_("Empty document! OPML document \"%s\" should not be empty when importing."), filename);
		} else {
			while(cur != NULL) {
				if(!xmlIsBlankNode(cur)) {
					if(!xmlStrcmp(cur->name, BAD_CAST"opml")) {
						import_parse_OPML(cur, parentNode, handler, trusted);
					} else {
						if(showErrors)
							ui_show_error_box(_("\"%s\" is not a valid OPML document! Liferea cannot import this file!"), filename);
						else
							g_warning(_("\"%s\" is not a valid OPML document! Liferea cannot import this file!"), filename);
					}
				}
				cur = cur->next;
			}
		}
		xmlFreeDoc(doc);
	}
}


/* UI stuff */

void on_import_activate_cb(const gchar *filename, gpointer user_data) {
	nodePtr		np;
	
	if(filename != NULL) {
		np = node_new();
		node_set_title(np, _("Imported feed list"));
		node_add_data(np, FST_FOLDER, NULL);
		
		/* add the new folder to the model */
		feedlist_add_node(NULL, np, -1);
		
		import_OPML_feedlist(filename, np, np->handler, TRUE, FALSE);
	}
}

void on_import_activate(GtkMenuItem *menuitem, gpointer user_data) {

	ui_choose_file(_("Import Feed List"), GTK_WINDOW(mainwindow), _("Import"), FALSE, on_import_activate_cb, NULL, NULL, NULL);
}

static void on_export_activate_cb(const gchar *filename, gpointer user_data) {
	gint error = 0;

	if (filename != NULL) {
		error = export_OPML_feedlist(filename, FALSE);
	
		if(0 != error)
			ui_show_error_box(_("Error while exporting feed list!"));
		else 
			ui_show_info_box(_("Feed List exported!"));
	}
}


void on_export_activate(GtkMenuItem *menuitem, gpointer user_data) {
	
	ui_choose_file(_("Export Feed List"), GTK_WINDOW(mainwindow), _("Export"), TRUE, on_export_activate_cb, "feedlist.opml", NULL, NULL);
}

