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
}

static gboolean
selfoss_feed_subscription_prepare_update_request (subscriptionPtr subscription, 
                                                 struct updateRequest *request)
{
	debug0 (DEBUG_UPDATE, "selfoss_feed_subscription_prepare_update_request()");

	return FALSE;
	return TRUE;
}

struct subscriptionType selfossSourceFeedSubscriptionType = {
	selfoss_feed_subscription_prepare_update_request,
	selfoss_feed_subscription_process_update_result
};

