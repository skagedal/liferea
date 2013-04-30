/**
 * @file itemset.h  interface to handle sets of items
 * 
 * Copyright (C) 2005-2012 Lars Windolf <lars.lindner@gmail.com>
 * Copyright (C) 2005-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include "rule.h"

/**
 * The itemset interface processes item list actions
 * based on the item set type specified by the node
 * the item set belongs to.
 */

typedef struct itemSet {
	GSList		*rules;		/**< list of rules each item matches */
	gboolean	anyMatch;	/**< TRUE means only one of the rules must match for item inclusion */
	
	GList		*ids;		/**< the list of item ids */
	gchar		*nodeId;	/**< the feed list node id this item set belongs to */
} *itemSetPtr;

/* item set iterating interface */

typedef void 	(*itemActionFunc)	(itemPtr item);

/**
 * Calls the given callback for each of the items in the item set.
 *
 * @param itemSet	the item set
 * @param callback	the callback
 */
void itemset_foreach (itemSetPtr itemSet, itemActionFunc callback);

/**
 * Merges the given item set into the item set of
 * the given node. Used for node updating.
 *
 * @param itemSet	the item set to merge into
 * @param items		a list of items to merge
 * @param allowUpdates	TRUE if older items may be replaced
 * @param markAsRead	TRUE if all new items should be marked as read
 *
 * @returns the number of new merged items
 */
guint itemset_merge_items(itemSetPtr itemSet, GList *items, gboolean allowUpdates, gboolean markAsRead);

/**
 * Checks if the given item matches the rules of the given item set.
 *
 * @param itemSet	the itemSet
 * @param item		the item
 *
 * @returns TRUE if the item matches the rules of the item set
 */
gboolean itemset_check_item (itemSetPtr itemSet, itemPtr item);

/**
 * Checks whether the given item id is in the given item set.
 *
 * @param itemSet	the itemSet
 * @param id		the item id
 *
 * @returns TRUE if item id is in item set
 */
gboolean itemset_has_item_id (itemSetPtr itemSet, gulong id);

/**
 * Method that creates and adds a rule to an item set. To be used
 * on loading time, when creating searches or when editing
 * search folder properties.
 *
 * @param itemSet	the item set
 * @param ruleId	id string for this rule type
 * @param value		argument string for this rule
 * @param additive	indicates positive or negative logic
 */
void itemset_add_rule (itemSetPtr itemSet, const gchar *ruleId, const gchar *value, gboolean additive);

/**
 * Frees the given item set and all items it contains.
 *
 * @param itemSet	the item set to free
 */
void itemset_free(itemSetPtr itemSet);

#endif
