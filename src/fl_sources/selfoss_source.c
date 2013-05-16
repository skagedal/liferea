/**
 * @file ttrss_source.c  tt-rss feed list source support
 * 
 * Copyright (C) 2010-2013 Lars Windolf <lars.lindner@gmail.com>
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

#include "fl_sources/selfoss_source.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "item_state.h"
#include "json.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "ui/liferea_dialog.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"

// FIXME: Avoid doing requests when we are not logged in yet!

/** create a tt-rss source with given node as root */ 
static selfossSourcePtr
selfoss_source_new (nodePtr node) 
{
	selfossSourcePtr source = g_new0 (struct selfossSource, 1) ;
	source->root = node; 
	source->actionQueue = g_queue_new (); 
	source->state = SELFOSS_SOURCE_STATE_NONE; 
	
	return source;
}

static void
selfoss_source_free (selfossSourcePtr source) 
{
	if (!source)
		return;

	update_job_cancel_by_owner (source);
	
	g_queue_free (source->actionQueue) ;
	g_free (source);
}

/* node source type implementation */

static void
selfoss_source_update (nodePtr node)
{
	subscription_update (node->subscription, 0);
}

static void
selfoss_source_auto_update (nodePtr node)
{
	selfossSourcePtr	source = (selfossSourcePtr) node->data;

	if (source->state == SELFOSS_SOURCE_STATE_NONE) 
		return;

	subscription_auto_update (node->subscription);
}

static void
selfoss_source_init (void)
{
	metadata_type_register ("selfoss-url", METADATA_TYPE_URL);
	metadata_type_register ("selfoss-feed-id", METADATA_TYPE_TEXT);
}

static void selfoss_source_deinit (void) { }

static void
selfoss_source_import (nodePtr node)
{
	GSList *iter; 
	opml_source_import (node);
	
	node->subscription->type = &selfossSourceSubscriptionType;
	if (!node->data)
		node->data = (gpointer) selfoss_source_new (node);

	for (iter = node->children; iter; iter = g_slist_next(iter))
		((nodePtr) iter->data)->subscription->type = &selfossSourceFeedSubscriptionType;
}

static void
selfoss_source_export (nodePtr node)
{
	opml_source_export (node);
}

static gchar *
selfoss_source_get_feedlist (nodePtr node)
{
	return opml_source_get_feedlist (node);
}

static void 
selfoss_source_remove (nodePtr node)
{ 
	opml_source_remove (node);
}

/* GUI callbacks */

static void
on_selfoss_source_selected (GtkDialog *dialog,
                           gint response_id,
                           gpointer user_data) 
{
	if (response_id == GTK_RESPONSE_OK) {
		nodePtr		node;
		subscriptionPtr subscription = subscription_new ("", NULL, NULL);
		
		/* The is a bit ugly: we need to prevent the tt-rss base
		   URL from being lost by unwanted permanent redirects on
		   the getFeeds call, so we save it as the homepage meta
		   data value... */
		metadata_list_set (&subscription->metadata, "selfoss-url", gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "serverUrlEntry"))));

		node = node_new (node_source_get_node_type ());
		node_set_title (node, "Selfoss");
		node_source_new (node, selfoss_source_get_type ());
		node_set_subscription (node, subscription);
		
		subscription_set_auth_info (subscription,
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))),
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));
		subscription->type = &selfossSourceSubscriptionType;		

		node->data = selfoss_source_new (node);
		feedlist_node_added (node);
		selfoss_source_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ui_selfoss_source_get_account_info (void)
{
	GtkWidget	*dialog;
	
	dialog = liferea_dialog_new ("selfoss_source.ui", "selfoss_source_dialog");
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_selfoss_source_selected), 
			  NULL);
}

static void
selfoss_source_cleanup (nodePtr node)
{
}

static void 
selfoss_source_item_set_flag (nodePtr node, itemPtr item, gboolean newStatus)
{
}

static void
selfoss_source_item_mark_read (nodePtr node, itemPtr item, gboolean newStatus)
{
}

/* node source type definition */

static struct nodeSourceType nst = {
	.id                  = "fl_selfoss",
	.name                = N_("Selfoss"),
	.description         = N_("Integrate the feed list of your Selfoss account. Liferea will "
	   "present your Selfoss subscriptions, and will synchronize your feed list and reading lists."),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION |
	                       NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC,
	.source_type_init    = selfoss_source_init,
	.source_type_deinit  = selfoss_source_deinit,
	.source_new          = ui_selfoss_source_get_account_info,
	.source_delete       = selfoss_source_remove,
	.source_import       = selfoss_source_import,
	.source_export       = selfoss_source_export,
	.source_get_feedlist = selfoss_source_get_feedlist,
	.source_update       = selfoss_source_update,
	.source_auto_update  = selfoss_source_auto_update,
	.free                = selfoss_source_cleanup,
	.item_set_flag       = selfoss_source_item_set_flag,
	.item_mark_read      = selfoss_source_item_mark_read,
	.add_folder          = NULL,	
	.add_subscription    = NULL,	
	.remove_node         = NULL	
};

nodeSourceTypePtr
selfoss_source_get_type (void)
{
	return &nst;
}
