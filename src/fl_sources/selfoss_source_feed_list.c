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

/* source subscription type implementation */

static void
selfoss_subscription_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	selfossSourcePtr source = (selfossSourcePtr) subscription->node->data;

	debug1 (DEBUG_UPDATE,"selfoss_subscription_cb(): %s", result->data);
	
	if (result->data && result->httpstatus == 200) {
		JsonParser	*parser = json_parser_new ();

		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonArray	*array = json_node_get_array (json_get_node (json_parser_get_root (parser), "content"));
			GList		*iter, *elements;
			GSList		*siter;
		
			/* We expect something like this:

			[{
			    "id":"2",
			    "title":"devart",
			    "tags":"da",
			    "spout":"spouts\\deviantart\\dailydeviations",
			    "params":[],
			    "error":"",
			    "icon":"8f05d7bb1e00caeb7a279037f129e1eb.png"
			 },{
			    "id":"1",
			    "title":"Tobis Blog",
			    "tags":"blog",
			    "spout":"spouts\\rss\\feed",
			    "params":{
			        "url":"http:\/\/blog.aditu.de\/feed"
			    },
			    "error":"",
			    "icon":"7fe3d2c0fc27994dd267b3961d64226e.png"
			 },
			 ...
			]
			   
			   */

#if 0
			elements = iter = json_array_get_elements (array);
			/* Add all new nodes we find */
			while (iter) {
				JsonNode *node = (JsonNode *)iter->data;
				
				/* ignore everything without a feed url */
				if (json_get_string (node, "feed_url")) {
					/*
					selfoss_source_merge_feed (source, 
					                         json_get_string (node, "feed_url"),
					                         json_get_string (node, "title"),
					                         json_get_int (node, "id"));
					*/
				}
				iter = g_list_next (iter);
			}
			g_list_free (elements);

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
		} else {
			g_warning ("Invalid JSON returned on Selfoss request! >>>%s<<<", result->data);
		}

		g_object_unref (parser);
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "selfoss_subscription_cb(): ERROR: failed to get subscription list!");
	}

	if (!(flags & SELFOSS_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));

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
	debug0 (DEBUG_UPDATE, "ttrs_subscription_prepare_update_request");

	return FALSE;
	return TRUE;
}

/* Selfoss subscription type definition */

struct subscriptionType selfossSourceSubscriptionType = {
	selfoss_subscription_prepare_update_request,
	selfoss_subscription_process_update_result
};
