/**
 * @file google_source.h Google Reader feed list source support
 * 
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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
	nodePtr	        root;	/**< the root node in the feed list */
	gchar		*sid;	/**< session id */
	GTimeVal	*lastSubscriptionListUpdate;
	GQueue          *actionQueue;
	int             loginState; /**< The current login state */
} *GoogleSourcePtr;

 
enum { 
	GOOGLE_SOURCE_STATE_NONE = 0,
	GOOGLE_SOURCE_STATE_IN_PROGRESS,
	GOOGLE_SOURCE_STATE_ACTIVE
} ;

enum  { 
	/**
	 * Update only the subscription list, and not each node underneath it.
	 * Note: Uses higher 16 bits to avoid conflict.
	 */
	GOOGLE_SOURCE_UPDATE_ONLY_LIST = (1<<16),

	/**
	 * Only login, do not do any updates. 
	 */
	GOOGLE_SOURCE_UPDATE_ONLY_LOGIN = ( 1<<17)
} ;

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
 * @return The return data has a line "SID=xxxx" which should be stored to be
 *         used as a cookie in future requests. 
 */ 
#define GOOGLE_READER_LOGIN_URL "https://www.google.com/accounts/ClientLogin" 
#define GOOGLE_READER_LOGIN_POST "service=reader&Email=%s&Passwd=%s&source=liferea&continue=http://www.google.com"

/**
 * Acts like a feed, indicating all the posts shared by the Google Reader
 * friends. Does not take any params, but 'sid' cookie needs to be set.
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
#define GOOGLE_READER_ADD_SUBSCRIPTION_URL "http://www.google.com/reader/api/0/subscription/quickadd?client=liferea"
#define GOOGLE_READER_ADD_SUBSCRIPTION_POST "quickadd=%s&ac=subscribe&T=%s"

/**
 * Unsubscribe from a subscription.
 * @param url The feed URL
 * @param T   a token obtained using GOOGLE_READER_TOKEN_URL
 */
#define GOOGLE_READER_REMOVE_SUBSCRIPTION_URL "http://www.google.com/reader/api/0/subscription/edit?client=liferea"
#define GOOGLE_READER_REMOVE_SUBSCRIPTION_POST "s=feed%%2F%s&i=null&ac=unsubscribe&T=%s"

/**
 * Edit the tags associated with an item. The parameters to this _have_ to be
 * sent as post data. 
 */
#define GOOGLE_READER_EDIT_TAG_URL "http://www.google.com/reader/api/0/edit-tag?client=liferea"

/**
 * Postdata for adding a tag when using GOOGLE_READER_EDIT_TAG_URL.
 * @param i The guid of the item.
 * @param s The URL of the subscription containing the item. (Note that the 
 *          following string adds the "feed/" prefix to this.)
 * @param a The tag to add. 
 * @param T a token obtained using GOOGLE_READER_TOKEN_URL
 */
#define GOOGLE_READER_EDIT_TAG_ADD_TAG "i=%s&s=feed%%2F%s&a=%s&r=%s&ac=edit-tags&T=%s"


/**
 * Postdata for adding a tag, and removing another tag at the same time, 
 * when using GOOGLE_READER_EDIT_TAG_URL.
 * @param i The guid of the item.
 * @param s The URL of the subscription containing the item. (Note that the 
 *          following string adds the "feed/" prefix to this.)
 * @param a The tag to add. 
 * @param r The tag to remove
 * @param T a token obtained using GOOGLE_READER_TOKEN_URL
 */
#define GOOGLE_READER_EDIT_TAG_AR_TAG "i=%s&s=feed%%2F%s&a=%s&r=%s&ac=edit-tags&T=%s"


/** A set of tags (states) defined by Google reader */

#define GOOGLE_READER_TAG_KEPT_UNREAD          "user/-/state/com.google/kept-unread"
#define GOOGLE_READER_TAG_READ                 "user/-/state/com.google/kept-unread"
#define GOOGLE_READER_TAG_TRACKING_KEPT_UNREAD "user/-/state/com.google/tracking-kept-unread"
/**
 * Returns Google Reader source type implementation info.
 */
nodeSourceTypePtr google_source_get_type(void);


#endif
