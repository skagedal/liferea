/**
 * @file google_source.c  Google reader feed list source support
 * 
 * Copyright (C) 2007-2012 Lars Windolf <lars.lindner@gmail.com>
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
 * Copyright (C) 2011 Peter Oliver
 * Copyright (C) 2011 Sergey Snitsaruk <narren96c@gmail.com>
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

#include "fl_sources/google_source.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <libxml/xpath.h>

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "item_state.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "xml.h"
#include "ui/auth_dialog.h"
#include "ui/liferea_dialog.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"
#include "fl_sources/google_source_edit.h"
#include "fl_sources/google_source_opml.h"

/** default Google reader subscription list update interval = once a day */
#define GOOGLE_SOURCE_UPDATE_INTERVAL 60*60*24

/** create a google source with given node as root */ 
static GoogleSourcePtr
google_source_new (nodePtr node) 
{
	GoogleSourcePtr source = g_new0 (struct GoogleSource, 1) ;
	source->root = node; 
	source->actionQueue = g_queue_new (); 
	source->loginState = GOOGLE_SOURCE_STATE_NONE; 
	source->lastTimestampMap = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	
	return source;
}

static void
google_source_free (GoogleSourcePtr gsource) 
{
	if (!gsource)
		return;

	update_job_cancel_by_owner (gsource);
	
	g_free (gsource->authHeaderValue);
	g_queue_free (gsource->actionQueue) ;
	g_hash_table_unref (gsource->lastTimestampMap);
	g_free (gsource);
}

static void
google_source_login_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	GoogleSourcePtr	gsource = (GoogleSourcePtr) userdata;
	gchar		*tmp = NULL;
	subscriptionPtr subscription = gsource->root->subscription;
		
	debug1 (DEBUG_UPDATE, "google login processing... %s", result->data);
	
	g_assert (!gsource->authHeaderValue);
	
	if (result->data && result->httpstatus == 200)
		tmp = strstr (result->data, "Auth=");
		
	if (tmp) {
		gchar *ttmp = tmp; 
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		gsource->authHeaderValue = g_strdup_printf ("GoogleLogin auth=%s", ttmp + 5);

		debug1 (DEBUG_UPDATE, "google reader Auth token found: %s", gsource->authHeaderValue);

		gsource->loginState = GOOGLE_SOURCE_STATE_ACTIVE;
		gsource->authFailures = 0;

		/* now that we are authenticated trigger updating to start data retrieval */
		if (!(flags & GOOGLE_SOURCE_UPDATE_ONLY_LOGIN))
			subscription_update (subscription, flags);

		/* process any edits waiting in queue */
		google_source_edit_process (gsource);

	} else {
		debug0 (DEBUG_UPDATE, "google reader login failed! no Auth token found in result!");
		subscription->node->available = FALSE;

		g_free (subscription->updateError);
		subscription->updateError = g_strdup (_("Google Reader login failed!"));
		gsource->authFailures++;

		if (gsource->authFailures < GOOGLE_SOURCE_MAX_AUTH_FAILURES)
			gsource->loginState = GOOGLE_SOURCE_STATE_NONE;
		else
			gsource->loginState = GOOGLE_SOURCE_STATE_NO_AUTH;
		
		auth_dialog_new (subscription, flags);
	}
}

/**
 * Perform a login to Google Reader, if the login completes the 
 * GoogleSource will have a valid Auth token and will have loginStatus to 
 * GOOGLE_SOURCE_LOGIN_ACTIVE.
 */
void
google_source_login (GoogleSourcePtr gsource, guint32 flags) 
{ 
	gchar			*username, *password;
	updateRequestPtr	request;
	subscriptionPtr		subscription = gsource->root->subscription;
	
	if (gsource->loginState != GOOGLE_SOURCE_STATE_NONE) {
		/* this should not happen, as of now, we assume the session
		 * doesn't expire. */
		debug1(DEBUG_UPDATE, "Logging in while login state is %d\n", gsource->loginState);
	}

	request = update_request_new ();

	update_request_set_source (request, GOOGLE_READER_LOGIN_URL);

	/* escape user and password as both are passed using an URI */
	username = g_uri_escape_string (subscription->updateOptions->username, NULL, TRUE);
	password = g_uri_escape_string (subscription->updateOptions->password, NULL, TRUE);

	request->postdata = g_strdup_printf (GOOGLE_READER_LOGIN_POST, username, password);
	request->options = update_options_copy (subscription->updateOptions);
	
	g_free (username);
	g_free (password);

	gsource->loginState = GOOGLE_SOURCE_STATE_IN_PROGRESS ;

	update_execute_request (gsource, request, google_source_login_cb, gsource, flags);
}

/* node source type implementation */

static void
google_source_update (nodePtr node)
{
	GoogleSourcePtr gsource = (GoogleSourcePtr) node->data;

	/* Reset GOOGLE_SOURCE_STATE_NO_AUTH as this is a manual
	   user interaction and no auto-update so we can query
	   for credentials again. */
	if (gsource->loginState == GOOGLE_SOURCE_STATE_NO_AUTH)
		gsource->loginState = GOOGLE_SOURCE_STATE_NONE;

	subscription_update (node->subscription, 0);  // FIXME: 0 ?
}

static void
google_source_auto_update (nodePtr node)
{
	GTimeVal	now;
	GoogleSourcePtr gsource = (GoogleSourcePtr) node->data;

	if (gsource->loginState == GOOGLE_SOURCE_STATE_NONE) {
		google_source_update (node);
		return;
	}

	if (gsource->loginState == GOOGLE_SOURCE_STATE_IN_PROGRESS) 
		return; /* the update will start automatically anyway */

	g_get_current_time (&now);
	
	/* do daily updates for the feed list and feed updates according to the default interval */
	if (node->subscription->updateState->lastPoll.tv_sec + GOOGLE_SOURCE_UPDATE_INTERVAL <= now.tv_sec) {
		subscription_update (node->subscription, 0);
		g_get_current_time (&gsource->lastQuickUpdate);
	}
	else if (gsource->lastQuickUpdate.tv_sec + GOOGLE_SOURCE_QUICK_UPDATE_INTERVAL <= now.tv_sec) {
		google_source_opml_quick_update (gsource);
		google_source_edit_process (gsource);
		g_get_current_time (&gsource->lastQuickUpdate);
	}
}

static void
google_source_init (void)
{
	metadata_type_register ("GoogleBroadcastOrigFeed", METADATA_TYPE_URL);
	metadata_type_register ("sharedby", METADATA_TYPE_TEXT);
}

static void google_source_deinit (void) { }

static void
google_source_import_node (nodePtr node)
{
	GSList *iter; 
	for (iter = node->children; iter; iter = g_slist_next(iter)) {
		nodePtr subnode = iter->data;
		if (subnode->subscription)
			subnode->subscription->type = &googleSourceFeedSubscriptionType; 
		if (subnode->type->capabilities
		    & NODE_CAPABILITY_SUBFOLDERS)
			google_source_import_node (subnode);
	}
}

static void
google_source_import (nodePtr node)
{
	opml_source_import (node);
	
	node->subscription->type = &googleSourceOpmlSubscriptionType;
	if (!node->data)
		node->data = (gpointer) google_source_new (node);

	google_source_import_node (node);
}

static void
google_source_export (nodePtr node)
{
	opml_source_export (node);
}

static gchar *
google_source_get_feedlist (nodePtr node)
{
	return opml_source_get_feedlist (node);
}

static void 
google_source_remove (nodePtr node)
{ 
	opml_source_remove (node);
}

static nodePtr
google_source_add_subscription (nodePtr node, subscriptionPtr subscription) 
{ 
	debug_enter ("google_source_add_subscription");
	nodePtr child = node_new (feed_get_node_type ());

	debug0 (DEBUG_UPDATE, "GoogleSource: Adding a new subscription"); 
	node_set_data (child, feed_new ());

	node_set_subscription (child, subscription);
	child->subscription->type = &googleSourceFeedSubscriptionType;
	
	node_set_title (child, _("New Subscription"));

	google_source_edit_add_subscription (node_source_root_from_node (node)->data, subscription->source);
	
	debug_exit ("google_source_add_subscription");
	
	return child;
}

static void
google_source_remove_node (nodePtr node, nodePtr child) 
{ 
	gchar           *source; 
	GoogleSourcePtr gsource = node->data; 
	
	if (child == node) { 
		feedlist_node_removed (child);
		return; 
	}

	source = g_strdup (child->subscription->source);

	feedlist_node_removed (child);

	/* propagate the removal only if there aren't other copies */
	if (!google_source_opml_get_node_by_source (gsource, source)) 
		google_source_edit_remove_subscription (gsource, source);
	
	g_free (source);
}

/* GUI callbacks */

static void
on_google_source_selected (GtkDialog *dialog,
                           gint response_id,
                           gpointer user_data) 
{
	nodePtr		node;
	subscriptionPtr	subscription;

	if (response_id == GTK_RESPONSE_OK) {
		subscription = subscription_new ("http://www.google.com/reader", NULL, NULL);
		node = node_new (node_source_get_node_type ());
		node_set_title (node, "Google Reader");
		node_source_new (node, google_source_get_type ());
		node_set_subscription (node, subscription);

		subscription_set_auth_info (subscription,
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))),
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));

		subscription->type = &googleSourceOpmlSubscriptionType ; 

		node->data = google_source_new (node);
		feedlist_node_added (node);
		google_source_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ui_google_source_get_account_info (void)
{
	GtkWidget	*dialog;

	
	dialog = liferea_dialog_new ("google_source.ui", "google_source_dialog");
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_google_source_selected), 
			  NULL);
}

static void
google_source_cleanup (nodePtr node)
{
	GoogleSourcePtr reader = (GoogleSourcePtr) node->data;
	google_source_free(reader);
	node->data = NULL ;
}

static void 
google_source_item_set_flag (nodePtr node, itemPtr item, gboolean newStatus)
{
	const gchar* sourceUrl = metadata_list_get (item->metadata, "GoogleBroadcastOrigFeed");
	if (!sourceUrl)
		sourceUrl = node->subscription->source;
	nodePtr root = node_source_root_from_node (node);
	google_source_edit_mark_starred ((GoogleSourcePtr)root->data, item->sourceId, sourceUrl, newStatus);
	item_flag_state_changed (item, newStatus);
}

static void
google_source_item_mark_read (nodePtr node, itemPtr item, gboolean newStatus)
{
	const gchar* sourceUrl = metadata_list_get(item->metadata, "GoogleBroadcastOrigFeed");
	if (!sourceUrl)
		sourceUrl = node->subscription->source;
	nodePtr root = node_source_root_from_node (node);
	google_source_edit_mark_read ((GoogleSourcePtr)root->data, item->sourceId, sourceUrl, newStatus);
	item_read_state_changed (item, newStatus);
}

/* node source type definition */

static struct nodeSourceType nst = {
	.id                  = "fl_google",
	.name                = N_("Google Reader"),
	.description         = N_("Integrate the feed list of your Google Reader account. Liferea will "
	                          "present your Google Reader subscriptions, and will synchronize your feed list and reading lists."),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION | 
	                       NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
	                       NODE_SOURCE_CAPABILITY_ADD_FEED |
	                       NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC,
	.source_type_init    = google_source_init,
	.source_type_deinit  = google_source_deinit,
	.source_new          = ui_google_source_get_account_info,
	.source_delete       = google_source_remove,
	.source_import       = google_source_import,
	.source_export       = google_source_export,
	.source_get_feedlist = google_source_get_feedlist,
	.source_update       = google_source_update,
	.source_auto_update  = google_source_auto_update,
	.free                = google_source_cleanup,
	.item_set_flag       = google_source_item_set_flag,
	.item_mark_read      = google_source_item_mark_read,
	.add_folder          = NULL, 
	.add_subscription    = google_source_add_subscription,
	.remove_node         = google_source_remove_node
};

nodeSourceTypePtr
google_source_get_type (void)
{
	return &nst;
}
