/**
 * @file google_source.h Google Reader feed list source support
 * 
 * Copyright (C) 2007-2012 Lars Windolf <lars.lindner@gmail.com>
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
 
#ifndef _GOOGLE_SOURCE_H
#define _GOOGLE_SOURCE_H

#include "fl_sources/node_source.h"

/**
 * A nodeSource specific for Google Reader
 */
typedef struct GoogleSource {
	nodePtr		root;		/**< the root node in the feed list */
	gchar		*authHeaderValue; /**< the Google Authorization token */
	GQueue		*actionQueue;
	gint		loginState;	/**< The current login state */
	gint		authFailures;	/**< Number of authentication failures */

	/**
	 * A map from a subscription source to a timestamp when it was last 
	 * updated (provided by Google).
	 */
	GHashTable      *lastTimestampMap; 

	/**
	 * A timestamp when the last Quick update took place.
	 */
	GTimeVal        lastQuickUpdate;
} *GoogleSourcePtr;

enum { 
	GOOGLE_SOURCE_STATE_NONE = 0,
	GOOGLE_SOURCE_STATE_IN_PROGRESS,
	GOOGLE_SOURCE_STATE_ACTIVE,
	GOOGLE_SOURCE_STATE_NO_AUTH
};

enum { 
	/**
	 * Update only the subscription list, and not each node underneath it.
	 * Note: Uses higher 16 bits to avoid conflict.
	 */
	GOOGLE_SOURCE_UPDATE_ONLY_LIST = (1<<16),
	/**
	 * Only login, do not do any updates. 
	 */
	GOOGLE_SOURCE_UPDATE_ONLY_LOGIN = (1<<17)
};

/**
 * Number of auth failures after which we stop bothering the user while
 * auto-updating until he manually updates again.
 */
#define GOOGLE_SOURCE_MAX_AUTH_FAILURES		3

/**
 * Google Source API URL's
 * In each of the following, the _URL indicates the URL to use, and _POST
 * indicates the corresponging postdata to send.
 * @see http://code.google.com/p/pyrfeed/wiki/GoogleReaderAPI
 * However as of now, the GoogleReaderAPI documentation seems outdated, some of
 * mark read/unread API does not work as mentioned in the documentation.
 */

/**
 * Google Reader Login api.
 * @param Email The google account email id.
 * @param Passwd The google account password.
 * @return The return data has a line "Auth=xxxx" which will be used as an
 *         Authorization header in future requests. 
 */ 
#define GOOGLE_READER_LOGIN_URL "https://www.google.com/accounts/ClientLogin" 
#define GOOGLE_READER_LOGIN_POST "service=reader&Email=%s&Passwd=%s&source=liferea&continue=http://www.google.com"

/**
 * Acts like a feed, indicating all the posts shared by the Google Reader
 * friends. Does not take any params, but the Authorization header needs to be set.
 */
#define GOOGLE_READER_BROADCAST_FRIENDS_URL "http://www.google.com/reader/atom/user/-/state/com.google/broadcast-friends" 

/**
 * Get a list of subscriptions.
 */
#define GOOGLE_READER_SUBSCRIPTION_LIST_URL "http://www.google.com/reader/api/0/subscription/list"

/**
 * Get a token for an edit operation. (@todo A token can actually be used
 * for multiple transactions.)
 */
#define GOOGLE_READER_TOKEN_URL "http://www.google.com/reader/api/0/token"

/**
 * Add a subscription
 * @param URL The feed URL, or the page URL for feed autodiscovery.
 * @param T   a token obtained using GOOGLE_READER_TOKEN_URL
 */
#define GOOGLE_READER_ADD_SUBSCRIPTION_URL "http://www.google.com/reader/api/0/subscription/edit?client=liferea"
#define GOOGLE_READER_ADD_SUBSCRIPTION_POST "s=feed%%2F%s&i=null&ac=subscribe&T=%s"

/**
 * Unsubscribe from a subscription.
 * @param url The feed URL
 * @param T   a token obtained using GOOGLE_READER_TOKEN_URL
 */
#define GOOGLE_READER_REMOVE_SUBSCRIPTION_URL "http://www.google.com/reader/api/0/subscription/edit?client=liferea"
#define GOOGLE_READER_REMOVE_SUBSCRIPTION_POST "s=feed%%2F%s&i=null&ac=unsubscribe&T=%s"

/**
 * A list of subscriptions with the unread counters, and the last updated
 * timestamps.
 */
#define GOOGLE_READER_UNREAD_COUNTS_URL "http://www.google.com/reader/api/0/unread-count?all=true&client=liferea"

/**
 * Edit the tags associated with an item. The parameters to this _have_ to be
 * sent as post data. 
 */
#define GOOGLE_READER_EDIT_TAG_URL "http://www.google.com/reader/api/0/edit-tag?client=liferea"

/**
 * Postdata for adding a tag when using GOOGLE_READER_EDIT_TAG_URL.
 * @param i The guid of the item.
 * @param prefix The prefix to 's'. For normal feeds this will be "feed", for
 *          links etc, this should be "user".
 * @param s The URL of the subscription containing the item. (Note that the 
 *          following string adds the "feed/" prefix to this.)
 * @param a The tag to add. 
 * @param T a token obtained using GOOGLE_READER_TOKEN_URL
 */
#define GOOGLE_READER_EDIT_TAG_ADD_TAG "i=%s&s=%s%%2F%s&a=%s&ac=edit-tags&T=%s&async=true"

/**
 * Postdata for removing  a tag, when using GOOGLE_READER_EDIT_TAG_URL. Do
 * not use for removing the "read" tag, see GOOGLE_READER_EDIT_TAG_AR_TAG 
 * for that.
 *
 * @param i The guid of the item.
 * @param prefix The prefix to 's'. @see GOOGLE_READER_EDIT_TAG_ADD_TAG
 * @param s The URL of the subscription containing the item. (Note that the 
 *          final value of s is feed + "/" + this string)
 * @param r The tag to remove
 * @param T a token obtained using GOOGLE_READER_TOKEN_URL
 */
#define GOOGLE_READER_EDIT_TAG_REMOVE_TAG "i=%s&s=%s%%2F%s&r=%s&ac=edit-tags&T=%s&async=true"

/**
 * Postdata for adding a tag, and removing another tag at the same time, 
 * when using GOOGLE_READER_EDIT_TAG_URL.
 * @param i The guid of the item.
 * @param prefix The prefix to 's'. @see GOOGLE_READER_EDIT_TAG_ADD_TAG
 * @param s The URL of the subscription containing the item. (Note that the 
 *          final value of s is feed + "/" + this string)
 * @param a The tag to add. 
 * @param r The tag to remove
 * @param T a token obtained using GOOGLE_READER_TOKEN_URL
 */
#define GOOGLE_READER_EDIT_TAG_AR_TAG "i=%s&s=%s%%2F%s&a=%s&r=%s&ac=edit-tags&T=%s&async=true"

/**
 * Postdata for adding a tag, and removing another tag at the same time, for a 
 * _link_ item, when using GOOGLE_READER_EDIT_TAG_URL
 * @param i The guid of the link (as provided by google)
 * @param a The tag to add
 * @param r The tag to remove
 * @param T a token obtained using GOOGLE_READER_TOKEN_URL
 */
#define GOOGLE_READER_EDIT_TAG_ADD_TAG_FOR_LINK "i=%s&s=user%2F-%2Fsource%2Fcom.google%2Flink&a=%s&r=%s&ac=edit-tags&T=%s&async=true"

/** A set of tags (states) defined by Google reader */

#define GOOGLE_READER_TAG_KEPT_UNREAD          "user/-/state/com.google/kept-unread"
#define GOOGLE_READER_TAG_READ                 "user/-/state/com.google/read"
#define GOOGLE_READER_TAG_TRACKING_KEPT_UNREAD "user/-/state/com.google/tracking-kept-unread"
#define GOOGLE_READER_TAG_STARRED              "user/-/state/com.google/starred"

/** Interval (in seconds) for doing a Quick Update: 10min */
#define GOOGLE_SOURCE_QUICK_UPDATE_INTERVAL 600

/**
 * @returns Google Reader source type implementation info.
 */
nodeSourceTypePtr google_source_get_type (void);

extern struct subscriptionType googleSourceFeedSubscriptionType;
extern struct subscriptionType googleSourceOpmlSubscriptionType;

/**
 * Find a child node with the given feed source URL.
 *
 * @param gsource	GoogleSource
 * @param source	a feed source URL to search
 *
 * @returns a node (or NULL)
 */
nodePtr google_source_get_node_from_source (GoogleSourcePtr gsource, const gchar* source);

/**
 * Tries to update the entire source quickly, by updating only those feeds
 * which are known to be updated. Suitable for g_timeout_add. This is an 
 * internal function.
 *
 * @param data A pointer to a node id of the source. This pointer will
 *             be g_free'd if the update fails.
 *
 * @returns FALSE on update failure
 */
gboolean google_source_quick_update_timeout (gpointer gsource);

/**
 * Migrate a google source child-node from a Liferea 1.4 style read-only
 * google source nodes.
 *
 * @param node The node to migrate (not the nodeSource!)
 */
void google_source_migrate_node (nodePtr node);

/**
 * Perform login for the given Google source.
 *
 * @param gsource	a GoogleSource
 * @param flags		network request flags
 */
void google_source_login (GoogleSourcePtr gsource, guint32 flags);

#endif
