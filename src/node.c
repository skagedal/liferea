/**
 * @file node.c common feed list node handling
 * 
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include "node.h"
#include "common.h"
#include "conf.h"
#include "callbacks.h"
#include "favicon.h"
#include "feed.h"
#include "fl_providers/fl_plugin.h"
#include "ui/ui_itemlist.h"

/* returns a unique node id */
gchar * node_new_id() {
	int		i;
	gchar		*id, *filename;
	gboolean	already_used;
	
	id = g_new0(gchar, 10);
	do {
		for(i=0;i<7;i++)
			id[i] = (char)g_random_int_range('a', 'z');
		id[7] = '\0';
		
		filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", id, NULL);
		already_used = g_file_test(filename, G_FILE_TEST_EXISTS);
		g_free(filename);
	} while(already_used);
	
	return id;
}

nodePtr node_new() {
	nodePtr	np;

	np = (nodePtr)g_new0(struct node, 1);
	np->id = node_new_id();
	np->sortColumn = IS_TIME;
	np->sortReversed = TRUE;	/* default sorting is newest date at top */
	np->available = FALSE;
	np->title = g_strdup("FIXME");

	return np;
}

void node_add_data(nodePtr np, guint type, gpointer data) {

	np->type = type;
	np->data = data;
}

void node_free(nodePtr np) {

	// nothing to do
}

void node_load(nodePtr np) {

	np->loaded++;

	if(1 < np->loaded)
		return;
	
	switch(np->type) {
		case FST_FEED:
		case FST_PLUGIN:
			FL_PLUGIN(np)->node_load(np);
			break;
		case FST_FOLDER:
		case FST_VFOLDER:
			/* not loading vfolders and other types! */
			break;
		default:
			g_warning("internal error: unknown node type!");
			break;
	}

	g_assert(NULL == np->itemSet);
	g_warning("implement merging! node_load()!!!");
	//itemset_add_items(np->itemSet, itemList);
}

void node_save(nodePtr np) {

	g_assert(0 < np->loaded);

	if(FALSE == np->needsCacheSave)
		return;

	FL_PLUGIN(np)->node_save(np);
	np->needsCacheSave = FALSE;
}

void node_unload(nodePtr np) {

	g_assert(0 < np->loaded);

	node_save(np);	/* save before unloading */

	if(!getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) {
		if(1 == np->loaded) {
			switch(np->type) {
				case FST_FEED:
				case FST_PLUGIN:
					FL_PLUGIN(np)->node_unload(np);
					break;
				case FST_FOLDER:
				case FST_VFOLDER:
					/* not unloading vfolders and other types! */
					break;
				default:
					g_warning("internal error: unknown node type!");
					break;
			}
		} else {
			/* not unloading when multiple references */
		}
	}
}

/* This method is called after parsing newly downloaded
   feeds or any other item aquiring was done. This method
   adds the item to the given node and does additional
   updating according to the node type. */
void node_add_item(nodePtr np, itemPtr ip) {
	gboolean added;

	/* step 1: merge into node type internal data structures */
	switch(np->itemSet->type) {
		case FST_PLUGIN:
			added = TRUE; /* no merging for now */
			break;
		case FST_FEED:
			added = feed_merge_check(np->itemSet, ip);
			break;
		case FST_FOLDER:
		case FST_VFOLDER:
			g_warning("If this happens something is wrong!");
			break;
	}

	if(added) {
		/* step 2: add to itemset */
		itemset_add_item(np->itemSet, ip);

		/* step 3: update feed list statistics */

		/* Never update the overall feed list statistic 
		   for folders and vfolders (because this are item
		   list types with item copies or references)! */
		if((FST_FOLDER != np->type) && (FST_VFOLDER != np->type))
			feedlist_update_counters(ip->readStatus?0:1,
						 ip->newStatus?1:0);
	}
}

/**
 * This method can be used to merge an ordered list of items
 * into the item list of the given item set.
 */
void node_add_items(nodePtr np, GList *list) {
	GList	*iter;
	
	/* Items are given in top to bottom display order. 
	   Adding them in this order would mean to reverse 
	   their order in the merged list, so merging needs
	   to be done bottom to top. */
	iter = g_list_last(list);
	while(iter != NULL) {
		node_add_item(np, ((itemPtr)iter->data));
		iter = g_list_previous(iter);
	}
	g_list_free(list);
}


gchar * node_render(nodePtr np) {

	return FL_PLUGIN(np)->node_render(np);
}

void node_request_update(nodePtr np, guint flags) {

	if(FST_VFOLDER == np->type)
		return;

	FL_PLUGIN(np)->node_update(np, flags);
}

void node_request_auto_update(nodePtr np) {

	if(FST_VFOLDER == np->type)
		return;

	FL_PLUGIN(np)->node_auto_update(np);
}

void node_remove(nodePtr np) {

	if(NULL != np->icon) {
		g_object_unref(np->icon);
		favicon_remove(np);
	}

	switch(np->type) {
		case FST_FEED:
		case FST_FOLDER:
		case FST_VFOLDER:
			FL_PLUGIN(np)->node_remove(np);
			break;
		case FST_PLUGIN:
			FL_PLUGIN(np)->handler_delete(np);
			break;
		default:
			g_warning("internal error: unknown node type!");
			break;
	}

	node_free(np);
}

void node_add(nodePtr parent, guint type) {
	nodePtr	child;

	child = node_new();
	child->type = type;

	switch(child->type) {
		case FST_FEED:
			child->icon = icons[ICON_AVAILABLE];
			FL_PLUGIN(parent)->node_add(child);
		case FST_FOLDER:
			child->icon = icons[ICON_FOLDER];
			FL_PLUGIN(parent)->node_add(child);
		case FST_VFOLDER:
			child->icon = icons[ICON_VFOLDER];
			FL_PLUGIN(parent)->node_add(child);
			break;
		case FST_PLUGIN:
			//FL_PLUGIN(parent)->handler_new(child);
			g_warning("not yet implemented!");
			break;
		default:
			g_warning("internal error: unknown node type!");
			break;
	}
}

/* ---------------------------------------------------------------------------- */
/* node attributes encapsulation						*/
/* ---------------------------------------------------------------------------- */

void node_set_title(nodePtr np, const gchar *title) {

	g_free(np->title);
	np->title = g_strdup(title);
}

const gchar * node_get_title(nodePtr np) { return np->title; }

void node_set_unread_count(nodePtr np, guint unreadCount) {

	/* unread count propagation to folders
	   is done by specific implementations
	   to be more flexible */
	np->itemSet->unreadCount = unreadCount;
}

guint node_get_unread_count(nodePtr np) { return np->itemSet->unreadCount; }

void node_set_id(nodePtr np, const gchar *id) {

	g_free(np->id);
	np->id = g_strdup(id);
}

const gchar *node_get_id(nodePtr np) { return np->id; }

void node_set_sort_column(nodePtr np, gint sortColumn, gboolean reversed) {

	np->sortColumn = sortColumn;
	np->sortReversed = reversed;
	feedlist_schedule_save();
}

void node_set_two_pane_mode(nodePtr np, gboolean newMode) { np->twoPane = newMode; }

gboolean node_get_two_pane_mode(nodePtr np) { return np->twoPane; }


