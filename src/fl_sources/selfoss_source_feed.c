/**
 * @file selfoss_source_feed.c  tt-rss feed subscription routines
 * 
 * Copyright (C) 2010-2011 Lars Windolf <lars.lindner@gmail.com>
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
#include <string.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "itemlist.h"
#include "itemset.h"
#include "json.h"
#include "metadata.h"
#include "subscription.h"

#include "fl_sources/selfoss_source.h"

static void
selfoss_feed_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult* const result, updateFlags flags)
{
	selfossSourcePtr source = (selfossSourcePtr) subscription->node->data;
	JsonParser	*parser;
	JsonNode	*root = NULL;
	JsonArray	*array;
	GList		*iter, *elements;
	GList		*items = NULL;
	GError		*err = NULL;

	if (result->data == NULL || result->httpstatus != 200) {
		g_warning ("selfoss_feed_subscription_process_update_result(): Failed to get subscription list\n");
		subscription->node->available = FALSE;
		return;
	}

	/* For an example of what we expect in the JSON, see:
	 * https://github.com/SSilence/selfoss/wiki/Restful-API-for-Apps-or-any-other-external-access#wiki-items_list_items
	 */

	parser = json_parser_new ();

	if (!json_parser_load_from_data (parser, result->data, -1, &err)) {
		g_warning ("selfoss_feed_subscription_process_update_result(): Error parsing JSON: %s\n", err->message);
		debug0 (DEBUG_UPDATE, result->data);
		g_clear_error (&err);
		subscription->node->available = FALSE;
		goto process_update_result_cleanup;
	}

	root = json_parser_get_root (parser);

	if (!JSON_NODE_HOLDS_ARRAY (root)) {
		g_warning ("selfoss_feed_subscription_process_update_result(): Unexpected JSON, not an array\n");
		debug0 (DEBUG_UPDATE, result->data);
		subscription->node->available = FALSE;
		goto process_update_result_cleanup;
	}

	array = json_node_get_array (root);
	elements = iter = json_array_get_elements (array);

	while (iter != NULL) {
		JsonNode *node = (JsonNode *)iter->data;
		itemPtr item = item_new ();
		const gchar *id, *title;

		id = json_get_string (node, "id");
		title = json_get_string (node, "title");
		printf("Feed item '%s': %s\n", id, title);

		item_set_id (item, json_get_string (node, "id"));
		item_set_title (item, json_get_string (node, "title"));
		item_set_source (item, json_get_string (node, "link"));
		// description, time, readStatus, flagStatus
		
		items = g_list_append (items, (gpointer)item);

		iter = g_list_next (iter);
	}
	g_list_free (elements);

	/* merge against feed cache */

	/* merge against feed cache */
	if (items) {
		itemSetPtr itemSet = node_get_itemset (subscription->node);
		gint newCount = itemset_merge_items (itemSet, items, TRUE /* feed valid */, FALSE /* markAsRead */);
		itemlist_merge_itemset (itemSet);
		itemset_free (itemSet);

		feedlist_node_was_updated (subscription->node, newCount);
	}

	subscription->node->available = TRUE;
	
process_update_result_cleanup:
	if (parser != NULL)
		g_object_unref (parser);
}

static gboolean
selfoss_feed_subscription_prepare_update_request (subscriptionPtr subscription, 
                                                 struct updateRequest *request)
{
	selfossSourcePtr source = (selfossSourcePtr) subscription->node->data;
	gchar *uri;
	const gchar *feed_id;

	debug0 (DEBUG_UPDATE, "SELFOSS: prepare_update_request()");

	g_return_val_if_fail (source != NULL, FALSE);

	feed_id = metadata_list_get (subscription->metadata, "selfoss-feed-id");

	uri = selfoss_build_uri (subscription, SELFOSS_ACTION_ITEMS, NULL,
				 "source", feed_id,
				 NULL);

	debug1 (DEBUG_UPDATE, "SELFOSS: items URI: %s", uri);
	update_request_set_source (request, uri);

	g_free (uri);

	return TRUE;
}

struct subscriptionType selfossSourceFeedSubscriptionType = {
	selfoss_feed_subscription_prepare_update_request,
	selfoss_feed_subscription_process_update_result
};

