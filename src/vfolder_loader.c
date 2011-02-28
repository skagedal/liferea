/**
 * @file vfolder_loader.c   Loader for search folder items
 *
 * Copyright (C) 2011 Lars Lindner <lars.lindner@gmail.com>
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

#include "vfolder_loader.h"

#include "db.h"
#include "debug.h"
#include "itemset.h"
#include "node.h"
#include "vfolder.h"

#define VFOLDER_LOADER_BATCH_SIZE 	100

static gboolean
vfolder_loader_fetch_cb (gpointer user_data, GSList **resultItems)
{
	vfolderPtr	vfolder = (vfolderPtr)user_data;
	itemSetPtr	items = g_new0 (struct itemSet, 1);
	GList		*iter;
	gboolean	result;

	/* 1. Fetch a batch of items */
	result = db_itemset_get (items, vfolder->maxLoadedId, VFOLDER_LOADER_BATCH_SIZE);
	vfolder->maxLoadedId += VFOLDER_LOADER_BATCH_SIZE;

	if (result) {
		/* 2. Match all items against search folder */
		iter = items->ids;
		while (iter) {
			gulong id = GPOINTER_TO_UINT (iter->data);

			itemPtr	item = db_item_load (id);
			if (itemset_check_item (vfolder->itemset, item))
				*resultItems = g_slist_append (*resultItems, item);
			else
				item_unload (item);

			iter = g_list_next (iter);
		}
	} else {
		debug1 (DEBUG_CACHE, "search folder '%s' reload complete", vfolder->node->title);
		vfolder->reloading = FALSE;
	}

	itemset_free (items);

	/* 3. Return if last fetch */
	return result;
}

ItemLoader *
vfolder_loader_new (nodePtr node) 
{
	vfolderPtr vfolder = (vfolderPtr)node->data;

	if(vfolder->reloading) {
		debug1 (DEBUG_CACHE, "search folder '%s' still reloading", vfolder->node->title);
		return NULL;
	}

	debug1 (DEBUG_CACHE, "search folder '%s' reload started", vfolder->node->title);
	vfolder->reloading = TRUE;
	vfolder->maxLoadedId = 0;

        return item_loader_new (vfolder_loader_fetch_cb, vfolder);
}
