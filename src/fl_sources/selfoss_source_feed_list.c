/**
 * @file selfoss_source_feed_list.c  tt-rss feed list handling routines.
 * 
 * Copyright (C) 2010-2011  Lars Windolf <lars.lindner@gmail.com>
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


#include "selfoss_source_feed_list.h"

#include <glib.h>
#include <string.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "json.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "fl_sources/opml_source.h"
#include "fl_sources/selfoss_source.h"

static void
selfoss_source_merge_feed (selfossSourcePtr source, const gchar *url, const gchar *title, const gchar *id)
{
	GSList	*iter;
	nodePtr	node;

	/* check if node to be merged already exists */
	iter = source->root->children;
	while (iter) {
		node = (nodePtr)iter->data;
		if (g_str_equal (node->subscription->source, url))
			return;
		iter = g_slist_next (iter);
	}
	
	debug2 (DEBUG_UPDATE, "SELFOSS: Adding %s (%s)", title, url);
	node = node_new (feed_get_node_type ());
	node_set_title (node, title);
	node_set_data (node, feed_new ());
		
	node_set_subscription (node, subscription_new (url, NULL, NULL));
	node->subscription->type = &selfossSourceFeedSubscriptionType;
	
	/* Save tt-rss feed id which we need to fetch items... */
	metadata_list_set (&node->subscription->metadata, "ttrss-feed-id", id);
	
	node_set_parent (node, source->root, -1);
	feedlist_node_imported (node);
		
	/**
	 * @todo mark the ones as read immediately after this is done
	 * the feed as retrieved by this has the read and unread
	 * status inherently.
	 */
	subscription_update (node->subscription, FEED_REQ_RESET_TITLE | FEED_REQ_PRIORITY_HIGH);
	subscription_update_favicon (node->subscription);
	
	/* Important: we must not loose the feed id! */
	db_subscription_update (node->subscription);
	
}

/* source subscription type implementation */

static void
selfoss_subscription_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	selfossSourcePtr source = (selfossSourcePtr) subscription->node->data;
	GError		*err = NULL;
	JsonParser	*parser = NULL;
	JsonNode	*root = NULL;
	JsonArray	*array;
	GList		*iter, *elements;
	GSList		*siter;

	debug1 (DEBUG_UPDATE,"selfoss_subscription_cb(): %s", result->data);

	/* FIXME: All errors reported by "g_warning" below should also somehow get reported
	 * through the UI. */
	
	if (result->data == NULL || result->httpstatus != 200) {
		g_warning ("selfoss_subscription_cb(): Failed to get subscription list\n");
		subscription->node->available = FALSE;
		return;
	}

	/* For an example of what we expect in the JSON, see:
	 * https://github.com/SSilence/selfoss/wiki/Restful-API-for-Apps-or-any-other-external-access#wiki-sources_list
	 */

	parser = json_parser_new ();

	if (!json_parser_load_from_data (parser, result->data, -1, &err)) {
		g_warning ("selfoss_subscription_cb(): Error parsing JSON: %s\n", err->message);
		debug0 (DEBUG_UPDATE, result->data);
		g_clear_error (&err);
		subscription->node->available = FALSE;
		goto selfoss_subscription_cb_cleanup;
	}

	root = json_parser_get_root (parser);

	if (!JSON_NODE_HOLDS_ARRAY (root)) {
		g_warning ("selfoss_subscription_cb(): Unexpected JSON, not an array\n");
		debug0 (DEBUG_UPDATE, result->data);
		subscription->node->available = FALSE;
		goto selfoss_subscription_cb_cleanup;
	}

	array = json_node_get_array (root);

	elements = iter = json_array_get_elements (array);
	/* Add all new nodes we find */
	while (iter) {
		JsonNode *node = (JsonNode *)iter->data;
		const gchar *id, *title, *spout, *url;
		
		id = json_get_string (node, "id");
		title = json_get_string (node, "title");
		spout = json_get_string (node, "spout");
		printf("Subscription:\n"
		       "    id:    %s\n"
		       "    title: %s\n"
		       "    spout: %s\n", id, title, spout);
		
		if (strcmp (spout, "spouts\\rss\\feed") == 0) {
			JsonNode *params = json_get_node (node, "params");
			const gchar *url = json_get_string (params, "url");
			printf ("    url:   %s\n", url);

			selfoss_source_merge_feed (source, url, title, id);
		}
		iter = g_list_next (iter);
	}
	g_list_free (elements);

#if 0
	/* Remove old nodes we cannot find anymore */
	siter = source->root->children;
	while (siter) {
		nodePtr node = (nodePtr)siter->data;
		gboolean found = FALSE;
				
		elements = iter = json_array_get_elements (array);
		while (iter) {
			JsonNode *json_node = (JsonNode *)iter->data;
			if (g_str_equal (node->subscription->source, json_get_string (json_node, "feed_url"))) {
				debug1 (DEBUG_UPDATE, "node: %s", node->subscription->source);
				found = TRUE;
				break;
			}
			iter = g_list_next (iter);
		}
		g_list_free (elements);

		if (!found)			
			feedlist_node_removed (node);
				
		siter = g_slist_next (siter);
	}
			
	opml_source_export (subscription->node);	/* save new feeds to feed list */				   
	subscription->node->available = TRUE;			
	//return;
#endif 


	if (!(flags & SELFOSS_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));

selfoss_subscription_cb_cleanup:
	if (parser != NULL)
		g_object_unref (parser);
}

static void
selfoss_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	debug0 (DEBUG_UPDATE, "selfoss_subscription_process_update_result");
	selfoss_subscription_cb (subscription, result, flags);
}

static gboolean
selfoss_subscription_prepare_update_request (subscriptionPtr subscription, struct updateRequest *request)
{
	selfossSourcePtr source = (selfossSourcePtr) subscription->node->data;
	gchar *uri;

	debug0 (DEBUG_UPDATE, "SELFOSS: Prepare update request.");

	g_return_val_if_fail (source != NULL, FALSE);

	// FIXME: should include authentication
	uri = g_strdup_printf (SELFOSS_URL_LIST_SOURCES, metadata_list_get (subscription->metadata, "selfoss-url"));
	update_request_set_source (request, uri);
	g_free (uri);

	return TRUE;
}

/* Selfoss subscription type definition */

struct subscriptionType selfossSourceSubscriptionType = {
	selfoss_subscription_prepare_update_request,
	selfoss_subscription_process_update_result
};
