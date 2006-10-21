/**
 * @file itemview.h    item display interface abstraction
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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
 
#ifndef _ITEMVIEW_H
#define _ITEMVIEW_H

#include <gtk/gtk.h>
#include "item.h"
#include "itemset.h"
 
 /* Liferea knows two ways to present items: with a GTK
    tree view and with a HTML rendering widget. This 
    interface generalizes item adding, removing and 
    updating for these two presentation types. */

/**
 * Initial setup of the item view.
 */
void	itemview_init(void);

/** 
 * Removes all currently loaded items from the item view.
 */
void	itemview_clear(void);
    
/**
 * Prepares the view for displaying items of the given item set.
 *
 * @param itemSet	the item set that is to be presented
 */
void	itemview_set_itemset(itemSetPtr itemSet);

/** item view display mode type */
typedef enum {
	ITEMVIEW_SINGLE_ITEM,	/**< 3 panes, item view shows the selected item only in HTML view */
	ITEMVIEW_LOAD_LINK,	/**< 3 panes, item view loads the link of selected item into HTML view */
	ITEMVIEW_ALL_ITEMS,	/**< 2 panes, item view shows all items combined in HTML view */
	ITEMVIEW_NODE_INFO	/**< 3 panes, item view shows the selected node description in HTML view*/
} itemViewMode;

/**
 * Set/unset the display mode of the item view.
 *
 * @param mode		item view mode constant
 */
void	itemview_set_mode(itemViewMode mode);

/**
 * Adds an item to the view for rendering. The item must belong
 * to the item set that was announced with ui_htmlview_load_itemset().
 *
 * @param item		the item to add
 */
void	itemview_add_item(itemPtr item);

/**
 * Removes a given item from the view.
 *
 * @param item	the item to remove
 */
void	itemview_remove_item(itemPtr item);

/**
 * Selects a given item in the view. The item must be
 * added using itemview_add_item before selecting.
 *
 * @param item	the item to select
 */
void	itemview_select_item(itemPtr item);

/**
 * Updates the output of a given item from the view.
 *
 * @param item	the item to update
 */
void	itemview_update_item(itemPtr item);

/**
 * Refreshes the item view. Needs to be called after each
 * add, remove or update of one or more items.
 */
void	itemview_update(void);

#endif