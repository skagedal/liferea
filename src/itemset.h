/**
 * @file itemset.h interface for different item list implementations
 * 
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifndef _ITEMSET_H
#define _ITEMSET_H

#include "item.h"

/**
 * The itemset interface processes item list actions
 * based on the itemset type specified by the node
 * the itemset belongs to.
 *
 * Currently there are three types of itemsets:
 *   - Feed
 *   - Folder
 *   - VFolder
 *
 * The type of the itemset can be determined from
 * the node type (valid values: FST_FEED, FST_FOLDER
 * and FST_VFOLDER).
 */

enum itemSetTypes {
	ITEMSET_TYPE_FEED,
	ITEMSET_TYPE_FOLDER,
	ITEMSET_TYPE_VFOLDER
};

typedef struct itemSet {
	guint	type;
	GSList	*items;

	guint	unreadCount;
	guint	newCount;
} *itemSetPtr;

/**
 * Renders a single item out of a given itemset.
 *
 * @param sp	the itemset
 * @param ip	the item to render
 *
 * @returns rendered HTML 
 */
gchar * itemset_render_item(itemSetPtr sp, itemPtr ip);

/**
 * Renders all items of a given itemset (condensed mode) 
 *
 * @param sp	the itemset
 *
 * @returns rendered HTML 
 */
gchar * itemset_render_all(itemSetPtr sp);

/**
 * Removes a single item of a given itemset.
 *
 * @param sp	the itemset
 * @param ip	the item to remove
 */
void itemset_remove_item(itemSetPtr sp, itemPtr ip);

/**
 * Removes all items of a given itemset.
 *
 * @param sp	the itemset
 */
void itemset_remove_all_items(itemSetPtr sp);

#endif
