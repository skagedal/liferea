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
 * Returns Google Reader source type implementation info.
 */
nodeSourceTypePtr google_source_get_type(void);


#endif
