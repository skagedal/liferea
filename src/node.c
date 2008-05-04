/**
 * @file node.c common feed list node handling
 * 
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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
 
#include <string.h>

#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "favicon.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "itemset.h"
#include "node.h"
#include "render.h"
#include "script.h"
#include "update.h"
#include "vfolder.h"
#include "fl_sources/node_source.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_node.h"

static GHashTable *nodes = NULL;

/* returns a unique node id */
gchar * node_new_id() {
	int		i;
	gchar		*id, *filename;
	gboolean	already_used;
	
	// FIXME: check DB instead!
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

nodePtr node_from_id(const gchar *id) {
	nodePtr	node;

	if(!id)
		return NULL;
		
	g_assert(NULL != nodes);
	node = (nodePtr)g_hash_table_lookup(nodes, id);
	if(!node)
		debug1(DEBUG_GUI, "Fatal: no node with id \"%s\" found!", id);
		
	return node;
}

nodePtr node_new(void) {
	nodePtr	node;
	gchar	*id;

	node = (nodePtr)g_new0(struct node, 1);
	node->sortColumn = IS_TIME;
	node->sortReversed = TRUE;	/* default sorting is newest date at top */
	node->available = TRUE;
	node_set_icon(node, NULL);	/* initialize favicon file name */

	id = node_new_id();
	node_set_id(node, id);
	g_free(id);
	
	return node;
}

void node_set_data(nodePtr node, gpointer data) {

	g_assert(NULL == node->data);
	g_assert(NULL != node->type);

	node->data = data;
}

void
node_set_subscription (nodePtr node, subscriptionPtr subscription) 
{

	g_assert (NULL == node->subscription);
	g_assert (NULL != node->type);
		
	node->subscription = subscription;
	subscription->node = node;
	
	db_update_state_load (node->id, subscription->updateState);
}

void
node_update_subscription (nodePtr node, gpointer user_data) 
{
	if (node->source->root == node) {
		node_source_update (node);
		return;
	}
	
	if (node->subscription)
		subscription_update (node->subscription, GPOINTER_TO_UINT (user_data));
		
	node_foreach_child_data (node, node_update_subscription, user_data);
}


void
node_auto_update_subscription (nodePtr node) 
{
	if (node->source->root == node) {
		node_source_auto_update (node);
		return;
	}
	
	if (node->subscription)
		subscription_auto_update (node->subscription);	
		
	node_foreach_child (node, node_auto_update_subscription);
}

void
node_reset_update_counter (nodePtr node, GTimeVal *now) 
{
	subscription_reset_update_counter (node->subscription, now);
	
	node_foreach_child_data (node, node_reset_update_counter, now);
}

gboolean node_is_ancestor(nodePtr node1, nodePtr node2) {
	nodePtr	tmp;

	tmp = node2->parent;
	while(tmp) {
		if(node1 == tmp)
			return TRUE;
		tmp = tmp->parent;
	}
	return FALSE;
}

void
node_free (nodePtr node)
{
	if (node->data && NODE_TYPE (node)->free)
		NODE_TYPE (node)->free (node);

	g_assert (NULL == node->children);
	
	g_hash_table_remove (nodes, node->id);
	
	update_job_cancel_by_owner (node);

	if (node->subscription)
		subscription_free (node->subscription);

	if (node->icon)
		g_object_unref (node->icon);
	g_free (node->iconFile);
	g_free (node->title);
	g_free (node->id);
	g_free (node);
}

static void
node_calc_counters (nodePtr node)
{
	/* vfolder unread counts are not interesting
	   in the following propagation handling */
	if (IS_VFOLDER (node))
		return;

	/* Order is important! First update all children
	   so that hierarchical nodes (folders and feed
	   list sources) can determine their own unread
	   count as the sum of all childs afterwards */
	node_foreach_child (node, node_calc_counters);
	
	NODE_TYPE (node)->update_counters (node);
}

static void node_update_parent_counters(nodePtr node) {
	guint old;

	if(!node)
		return;
		
	old = node->unreadCount;

	NODE_TYPE(node)->update_counters(node);
	
	if (old != node->unreadCount) {
		ui_node_update (node->id);
		ui_tray_update ();
		ui_mainwindow_update_feedsinfo ();
	}
	
	if(node->parent)
		node_update_parent_counters(node->parent);
}

void node_update_counters(nodePtr node) {
	guint old = node->unreadCount;

	/* Update the node itself and its children */
	node_calc_counters(node);
	
	if (old != node->unreadCount);
		ui_node_update (node->id);
		
	/* Update the unread count of the parent nodes,
	   usually them just add all child unread counters */
	node_update_parent_counters(node->parent);
}

// FIXME: bad interface, do not imply node icons are favicons
void
node_update_favicon (nodePtr node, GTimeVal *now)
{
	if (IS_FEED (node)) {
		debug1 (DEBUG_UPDATE, "favicon of node %s needs to be updated...", node->title);
		subscription_update_favicon (node->subscription, now);
	}
	
	/* Recursion */
	if (node->children)
		node_foreach_child_data (node, node_update_favicon, now);
}

itemSetPtr
node_get_itemset (nodePtr node)
{
	return NODE_TYPE (node)->load (node);
}

void
node_mark_all_read (nodePtr node)
{
	if (!node)
		return;

	if (0 != node->unreadCount) {
		item_state_set_all_read (node);
		node->unreadCount = 0;
		node->needsUpdate = TRUE;
	}
		
	if (node->children)
		node_foreach_child (node, node_mark_all_read);
}

gchar *
node_render(nodePtr node)
{
	return NODE_TYPE (node)->render (node);
}

/* import callbacks and helper functions */

void
node_add_child (nodePtr parent, nodePtr node, gint position)
{
	if (!parent)
		parent = ui_feedlist_get_target_folder (&position);

	parent->children = g_slist_insert (parent->children, node, position);
	node->parent = parent;
	
	/* new nodes may be provided by another node source, if 
	   not they are handled by the parents node source */
	if (!node->source)
		node->source = parent->source;
	
	ui_node_add (parent, node, position);	
}

void
node_request_remove (nodePtr node)
{
	/* using itemlist_remove_all_items() ensure correct unread
	   and item counters for all parent folders and matching 
	   search folders */
	itemlist_remove_all_items (node);
	
	node_source_remove_node (node->source->root, node);
	
	NODE_TYPE (node)->remove (node);

	node->parent->children = g_slist_remove (node->parent->children, node);

	node_free (node);
}

static xmlDocPtr node_to_xml(nodePtr node) {
	xmlDocPtr	doc;
	xmlNodePtr	rootNode;
	gchar		*tmp;
	
	doc = xmlNewDoc("1.0");
	rootNode = xmlDocGetRootElement(doc);
	rootNode = xmlNewDocNode(doc, NULL, "node", NULL);
	xmlDocSetRootElement(doc, rootNode);

	xmlNewTextChild(rootNode, NULL, "title", node_get_title(node));
	
	tmp = g_strdup_printf("%u", node->unreadCount);
	xmlNewTextChild(rootNode, NULL, "unreadCount", tmp);
	g_free(tmp);
	
	tmp = g_strdup_printf("%u", g_slist_length(node->children));
	xmlNewTextChild(rootNode, NULL, "children", tmp);
	g_free(tmp);
	
	return doc;
}

gchar * node_default_render(nodePtr node) {
	gchar		*result;
	xmlDocPtr	doc;

	doc = node_to_xml(node);
	result = render_xml(doc, NODE_TYPE(node)->id, NULL);	
	xmlFreeDoc(doc);
		
	return result;
}

/* helper functions to be used with node_foreach* */

void
node_save(nodePtr node)
{
	NODE_TYPE(node)->save(node);
}

/* node attributes encapsulation */

void
node_set_title (nodePtr node, const gchar *title)
{
	g_free (node->title);
	node->title = g_strstrip (g_strdelimit (g_strdup (title), "\r\n", ' '));
}

const gchar * node_get_title(nodePtr node) { return node->title; }

void node_set_icon(nodePtr node, gpointer icon) {

	if(node->icon) 
		g_object_unref(node->icon);
	node->icon = icon;
	
	if(node->iconFile)
		g_free(node->iconFile);
	if(node->icon)
		node->iconFile = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", node->id, "png");
	else
		node->iconFile = g_strdup(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "default.png");
}

/** determines the nodes favicon or default icon */
gpointer node_get_icon(nodePtr node) { 
	gpointer icon;

	icon = node->icon;

	if(!icon)
		icon = NODE_TYPE(node)->icon;

	if(!node->available)
		icon = icons[ICON_UNAVAILABLE];

	return icon;
}

const gchar * node_get_favicon_file(nodePtr node) { return node->iconFile; }

void node_set_id(nodePtr node, const gchar *id) {

	if(!nodes)
		nodes = g_hash_table_new(g_str_hash, g_str_equal);

	if(node->id) {
		g_hash_table_remove(nodes, node->id);
		g_free(node->id);
	}
	node->id = g_strdup(id);
	
	g_hash_table_insert(nodes, node->id, node);
}

const gchar *node_get_id(nodePtr node) { return node->id; }

void
node_set_sort_column (nodePtr node, gint sortColumn, gboolean reversed)
{
	node->sortColumn = sortColumn;
	node->sortReversed = reversed;
}

void node_set_view_mode(nodePtr node, guint newMode) { node->viewMode = newMode; }

gboolean node_get_view_mode(nodePtr node) { return node->viewMode; }

gboolean
node_load_link_preferred (nodePtr node)
{
	if (IS_FEED (node))
		return ((feedPtr)node->data)->loadItemLink;

	return FALSE;
}

const gchar *
node_get_base_url(nodePtr node)
{
	const gchar 	*baseUrl = NULL;

	if (IS_FEED (node))
		baseUrl = feed_get_html_url ((feedPtr)node->data);

	/* prevent feed scraping commands to end up as base URI */
	if (!((baseUrl != NULL) &&
	      (baseUrl[0] != '|') &&
	      (strstr(baseUrl, "://") != NULL)))
	   	baseUrl = NULL;

	return baseUrl;
}

/* node children iterating interface */

void node_foreach_child_full(nodePtr node, gpointer func, gint params, gpointer user_data) {
	GSList		*children, *iter;
	
	g_assert(NULL != node);

	/* We need to copy because func might modify the list */
	iter = children = g_slist_copy(node->children);
	while(iter) {
		nodePtr childNode = (nodePtr)iter->data;
		
		/* Apply the method to the child */
		if(0 == params)
			((nodeActionFunc)func)(childNode);
		else 
			((nodeActionDataFunc)func)(childNode, user_data);
			
		/* Never descend! */

		iter = g_slist_next(iter);
	}
	
	g_slist_free(children);
}

