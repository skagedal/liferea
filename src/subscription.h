/**
 * @file subscription.h  common subscription handling interface
 * 
 * Copyright (C) 2003-2012 Lars Lindner <lars.lindner@gmail.com>
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
#ifndef _SUBSCRIPTION_H
#define _SUBSCRIPTION_H
 
#include <glib.h>
#include <libxml/parser.h>
#include "node.h"
#include "update.h"

/** Caching property constants */
enum cache_limit {
	/* Values > 0 are used to specify certain limits */
	CACHE_DISABLE = 0,
	CACHE_DEFAULT = -1,
	CACHE_UNLIMITED = -2,
};

/** Flags used in the request structure */
enum feed_request_flags {
	FEED_REQ_RESET_TITLE		= (1<<0),	/**< Feed's title should be reset to default upon update */
	FEED_REQ_PRIORITY_HIGH		= (1<<3),	/**< set to signal that this is an important user triggered request */
};
 
/** Common structure to hold all information about a single subscription. */
typedef struct subscription {
	nodePtr		node;			/**< the feed list node the subscription is attached to */
	struct subscriptionType *type;		/**< the subscription type */
	
	gchar		*source;		/**< current source, can be changed by redirects */
	gchar		*origSource;		/**< the source given when creating the subscription */
	updateOptionsPtr updateOptions;		/**< update options for the feed source */
	struct updateJob *updateJob;		/**< update request structure used when downloading the subscribed source */
	
	gint		updateInterval;		/**< user defined update interval in minutes */	
	guint		defaultInterval;	/**< optional update interval as specified by the feed in minutes */
	
	GSList		*metadata;		/**< metadata list assigned to this subscription */
	
	gchar		*updateError;		/**< textual description of processing errors */
	gchar		*httpError;		/**< textual description of HTTP protocol errors */
	gint		httpErrorCode;		/**< last HTTP error code */
	updateStatePtr	updateState;		/**< update states (etag, last modified, cookies, last polling times...) */
	
	gboolean	activeAuth;		/**< TRUE if authentication in progress */

	gboolean	discontinued;		/**< flag to avoid updating after HTTP 410 */

	gchar		*filtercmd;		/**< feed filter command */
	gchar		*filterError;		/**< textual description of filter errors */
} *subscriptionPtr;

/**
 * Generic update result processing callback type.
 * This callback must not free the result structure. It will be
 * free'd by the download system after the callback returns.
 *
 * @param node		the subscriptions node
 * @param result	the update result
 * @param flags		update result processing flags
 */
typedef void (*subscription_update_cb) (nodePtr node, const struct updateResult * const result, guint32 flags);

/**
 * Create a new subscription structure.
 *
 * @param source	the subscription source URL (or NULL)
 * @param filter	a post processing filter (or NULL)
 * @param options	update options (or NULL)
 *
 * @returns the new subscription
 */
subscriptionPtr subscription_new (const gchar *source, const gchar *filter, updateOptionsPtr options);

/**
 * Imports a subscription from the given XML document.
 *
 * @param xml		xml node
 * @param trusted	TRUE when importing from internal XML document
 *
 * @returns a new subscription
 */
subscriptionPtr subscription_import (xmlNodePtr xml, gboolean trusted);

/**
 * Exports the given subscription to the given XML document.
 *
 * @param subscription	the subscription
 * @param xml		xml node
 * @param trusted	TRUE when exporting to internal XML document
 */
void subscription_export (subscriptionPtr subscription, xmlNodePtr xml, gboolean trusted);

/**
 * Serialization helper function for rendering purposes.
 *
 * @param node		the subscription to serialize
 * @param feedNode	XML node to add subscription attributes to
 */
void subscription_to_xml (subscriptionPtr subscription, xmlNodePtr xml);

/**
 * Checks wether a subscription is ready to be updated
 *
 * @param subscription	the subscription to check
 *
 * @returns TRUE if subscription can be updated
 */
gboolean subscription_can_be_updated(subscriptionPtr subscription);

/**
 * Triggers updating a subscription. Will download the 
 * the document indicated by the source URL of the subscription.
 * Will call the node type specific update callback to process
 * the downloaded data.
 *
 * @param subscription	the subscription
 * @param flags		update flags
 */
void subscription_update (subscriptionPtr subscription, guint flags);

/**
 * Called when auto updating. Checks wether the subscription
 * needs to be updated (according to it's update interval) and
 * if necessary calls subscription_update().
 *
 * @param subscription	the subscription
 */
void subscription_auto_update (subscriptionPtr subscription);

/**
 * Cancels a currently running subscription update. This is to
 * be called when removing subscriptions or retriggering the update
 * upon user request.
 *
 * @param subscription	the subscription
 */
void subscription_cancel_update (subscriptionPtr subscription);

/**
 * Get the update interval setting of a given subscription
 *
 * @param subscription	the subscription
 *
 * @returns the currently configured update interval (in minutes)
 */
gint subscription_get_update_interval(subscriptionPtr subscription);

/**
 * Set the update interval setting for the given subscription
 *
 * @param subscription	the subscription
 * @param interval	the new update interval (in minutes)
 */
void subscription_set_update_interval(subscriptionPtr subscription, gint interval);

/**
 * Get the default update interval setting of a given subscription
 *
 * @param subscription	the subscription
 *
 * @returns the default update interval (in minutes) or 0
 */
guint subscription_get_default_update_interval(subscriptionPtr subscription);

/**
 * Set the default update interval setting for the given subscription
 *
 * @param subscription	the subscription
 * @param interval	the default update interval (in minutes)
 */
void subscription_set_default_update_interval(subscriptionPtr subscription, guint interval);

/**
 * Reset the update counter for the given subscription.
 *
 * @param subscription	the subscription
 */
void subscription_reset_update_counter (subscriptionPtr subscription, GTimeVal *now);

void subscription_update_favicon (subscriptionPtr subscription);

/**
 * Get the source URL of a given subscription
 *
 * @param subscription	the subscription
 *
 * @returns the source URL
 */
const gchar * subscription_get_source(subscriptionPtr subscription);

/**
 * Set a new source URL for the given subscription
 *
 * @param subscription	the subscription
 * @param source	the new source URL
 */
void subscription_set_source(subscriptionPtr subscription, const gchar *source);

/**
 * Get the original subscription URL
 *
 * @param subscription	the subscription
 *
 * @returns the original source URL
 */
const gchar * subscription_get_orig_source(subscriptionPtr subscription);

/**
 * Set the original subscription URL
 *
 * @param subscription	the subscription
 * @param source	the new original source URL
 */ 
void subscription_set_orig_source(subscriptionPtr subscription, const gchar *source);

/**
 * Returns the homepage URL of the given subscription.
 *
 * @param subscription	the subscription
 *
 * @returns the homepage URL or NULL
 */
const gchar * subscription_get_homepage(subscriptionPtr subscription);

/**
 * Set the homepage URL of the given subscription. If the passed
 * URL is a relative one it will be expanded using the
 * given base URL.
 *
 * @param subscription	the subscription
 * @param url		the new HTML URL
 */
void subscription_set_homepage(subscriptionPtr subscription, const gchar *url);

/**
 * Get the configured filter command for a given subscription
 *
 * @param subscription	the subscription
 *
 * @returns the filter command
 */
const gchar * subscription_get_filter(subscriptionPtr subscription);

/**
 * Set a new filter command for a given subscription
 *
 * @param subscription	the subscription
 * @param filter	the new filter command
 */
void subscription_set_filter(subscriptionPtr subscription, const gchar * filter);

/**
 * Set authentication information for a given subscription
 * 
 * @param subscription	the subscription
 * @param username	the user name
 * @param password	the password
 */
void subscription_set_auth_info (subscriptionPtr subscription, const gchar *username, const gchar *password);

/**
 * Frees the given subscription structure.
 *
 * @param subscription	the subscription
 */ 
void subscription_free(subscriptionPtr subscription);

#endif
