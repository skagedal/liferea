/**
 * @file selfoss_source.h tt-rss feed list source support
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
 
#ifndef _SELFOSS_SOURCE_H
#define _SELFOSS_SOURCE_H

#include "fl_sources/node_source.h"

/**
 * A nodeSource specific for tt-rss
 */
typedef struct selfossSource {
	nodePtr		root;		/**< the root node in the feed list */
	GQueue		*actionQueue;
	gint		state;
} *selfossSourcePtr;

enum {
	SELFOSS_SOURCE_STATE_NONE = 0,
	SELFOSS_SOURCE_STATE_AUTH_OK = 1
};

enum  { 
	/**
	 * Update only the subscription list, and not each node underneath it.
	 * Note: Uses higher 16 bits to avoid conflict.
	 */
	SELFOSS_SOURCE_UPDATE_ONLY_LIST = (1<<16),
};

/**
 * Selfoss API URL's
 *
 * https://github.com/SSilence/selfoss/wiki/Restful-API-for-Apps-or-any-other-external-access
 */

#define SELFOSS_URL_LIST_SOURCES "%s/sources/list"

/**
 * Returns selfoss source type implementation info.
 */
nodeSourceTypePtr selfoss_source_get_type (void);

void selfoss_source_login (selfossSourcePtr source, guint32 flags);

extern struct subscriptionType selfossSourceFeedSubscriptionType;
extern struct subscriptionType selfossSourceSubscriptionType;

#endif
