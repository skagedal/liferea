/**
 * @file itemlist.h  itemlist handling
 *
 * Copyright (C) 2004-2011 Lars Lindner <lars.lindner@gmail.com>
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

#ifndef _ITEMLIST_H
#define _ITEMLIST_H

#include <gtk/gtk.h>

#include "item.h"
#include "item_loader.h"
#include "itemset.h"

G_BEGIN_DECLS

#define ITEMLIST_TYPE		(itemlist_get_type ())
#define ITEMLIST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), ITEMLIST_TYPE, ItemList))
#define ITEMLIST_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), ITEMLIST_TYPE, ItemListClass))
#define IS_ITEMLIST(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ITEMLIST_TYPE))
#define IS_ITEMLIST_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), ITEMLIST_TYPE))

typedef struct ItemList		ItemList;
typedef struct ItemListClass	ItemListClass;
typedef struct ItemListPrivate	ItemListPrivate;

struct ItemList
{
	GObject		parent;
	
	/*< private >*/
	ItemListPrivate	*priv;
};

struct ItemListClass 
{
	GObjectClass parent_class;	
};

GType itemlist_get_type (void);

/**
 * Set up the feed list.
 *
 * @returns the feed list instance
 */
ItemList * itemlist_create (void);

/**
 * Returns the currently displayed node.
 *
 * @returns displayed node (or NULL)
 */
struct node * itemlist_get_displayed_node(void);

/**
 * Returns a copy of the currently selected item.
 * Note: selected = displayed item
 *
 * @returns displayed item (or NULL) to be free'd using item_unload()
 */
itemPtr itemlist_get_selected (void);

/**
 * Returns the id of the currently selected item.
 *
 * @returns displayed item id (or 0)
 */
gulong itemlist_get_selected_id (void);

/**
 * To be called whenever a feed was updated. If it is a somehow
 * displayed feed it is loaded this method decides if the
 * and how the item list GUI needs to be updated.
 *
 * @param itemSet	the item set to be merged
 */
void itemlist_merge_itemset(itemSetPtr itemSet);

/** 
 * Loads the passed nodes items into the item list.
 *
 * @param node 		the node
 */
void itemlist_load(struct node *node);

/**
 * Clears the item list. Unsets the currently
 * displayed item set. Optionally marks every item read.
 *
 * @param markRead	if TRUE all items are marked as read
 */
void itemlist_unload(gboolean markRead);

/**
 * Changes the viewing mode property of the item list.
 * Do not use this method to change the viewing mode
 * of a displayed node!
 *
 * @param newMode	(0 = normal, 1 = wide, 2 = combined view)
 */
void itemlist_set_view_mode(guint newMode);

/**
 * Returns the viewing mode property of the currently displayed item set.
 *
 * @returns viewing mode (0 = normal, 1 = wide, 2 = combined view)
 */
guint itemlist_get_view_mode(void);

/**
 * Menu callback that toggles the different viewing modes
 *
 * @param action	the action that emitted the signal
 * @param current	the member of action which was activated
 * @param user_data	unused
 */
void on_view_activate(GtkRadioAction *action, GtkRadioAction *current, gpointer user_data);

/**
 * Menu callback to select the previously read item from the item history
 */
void on_prev_read_item_activate (GtkMenuItem *menuitem, gpointer user_data);

/**
 * Menu callback to select the next read item from the item history
 */
void on_next_read_item_activate (GtkMenuItem *menuitem, gpointer user_data);


/* item handling functions */

void itemlist_update_item(itemPtr item);

void itemlist_request_remove_item(itemPtr item);

/**
 * To be called whenever the user wants to remove
 * a single item. If necessary the item will be unselected.
 * The item will be removed immediately.
 *
 * @param item	the item
 */
void itemlist_remove_item(itemPtr item);

/**
 * To be called whenever some of the items of an item set
 * are to be removed. In difference to itemlist_remove_item()
 * this function will remove all items first and update the
 * GUI once.
 *
 * @param itemSet	the item set from which items are to be removed
 * @param items		the items to be removed
 */
void itemlist_remove_items(itemSetPtr itemSet, GList *items);

/**
 * To be called whenever the user wants to remove 
 * all items of a node. Item list selection will be
 * resetted. All items are removed immediately.
 *
 * @param node		the node whose item list is to be removed
 */
void itemlist_remove_all_items(struct node *node);

/**
 * Called from GUI when item list selection changes.
 *
 * @param item	new selected item 
 */
void itemlist_selection_changed(itemPtr item);

/**
 * Tries to select the next unread item that is currently in the
 * item list. Or does nothing if there are no unread items left.
 */
void itemlist_select_next_unread(void);

/**
 * Toggle the flag of the given item.
 *
 * @param item		the item
 */
void itemlist_toggle_flag(itemPtr item);

/**
 * Toggle the read status of the given item.
 *
 * @param item		the item
 */
void itemlist_toggle_read_status(itemPtr item);

/**
 * Adding a loader for item batch loading (e.g. search folder)
 *
 * @param loader	the loader to add to default item list view
 */
void itemlist_add_loader (ItemLoader *loader); 

/**
 * Register a search result item loader.
 *
 * @param loader	the search result item loader
 */
void itemlist_add_search_result (ItemLoader *loader);

G_END_DECLS

#endif
