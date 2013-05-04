/** 
 * @file notif_libnotify.c notifications via libnotify
 *
 * Copyright (c) 2006, Norman Jonas <liferea.sf.net@devport.codepilot.net>
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
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>

#include <libnotify/notify.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "item.h"
#include "item_state.h"
#include "feedlist.h"
#include "node.h"
#include "ui/liferea_shell.h"
#include "ui/ui_common.h"
#include "ui/feed_list_view.h"
#include "ui/ui_tray.h"

#include "notification/notification.h"

static gboolean supports_actions = FALSE;
static gboolean supports_append  = FALSE;

static void
notif_libnotify_callback_open (NotifyNotification *n, gchar *action, gpointer user_data)
{
	nodePtr node_p;

	g_assert(action != NULL);
	g_assert(strcmp(action, "open") == 0);

	node_p = node_from_id (user_data);

	if (node_p)
		feed_list_view_select (node_p);
	else
		ui_show_error_box (_("This feed does not exist anymore!"));

	notify_notification_close (n, NULL);

	liferea_shell_present ();
}

static void
notif_libnotify_callback_mark_read (NotifyNotification *n, gchar *action, gpointer user_data)
{
	nodePtr node;

	g_assert (action != NULL);
	g_assert (strcmp (action, "mark_read") == 0);

	node = node_from_id (user_data);

	if (node) {
		feedlist_mark_all_read (node);
		feedlist_reset_new_item_count ();
		item_state_set_all_popup (node->id);
	} else {
		ui_show_error_box (_("This feed does not exist anymore!"));
	}

	notify_notification_close (n, NULL);
}

static void
notif_libnotify_callback_show_details (NotifyNotification *n, gchar *action, gpointer user_data)
{
	nodePtr node_p;

	GList *list_p;
	itemPtr item_p;

	gchar *labelText_p;
	gchar *labelText_now_p = NULL;
	gchar *labelText_prev_p;

	gchar *labelHeadline_p;
	const gchar *labelURL_p;

	gint item_count = 0;

	g_assert (action != NULL);
	g_assert (strcmp(action, "show_details") == 0);
	node_p = node_from_id (user_data);

	if (node_p) {
		itemSetPtr itemSet = node_get_itemset (node_p);

		labelText_now_p = g_strdup ("");

		/* Gather the feed's headlines */
		list_p = itemSet->ids;
		while (list_p) {
			item_p = item_load (GPOINTER_TO_UINT (list_p->data));
			if (item_p->popupStatus && !item_p->readStatus) {
				item_p->popupStatus = FALSE;
				item_count += 1;

				labelHeadline_p = g_strdup (item_get_title (item_p));
				if (labelHeadline_p == NULL ) {
					labelHeadline_p = g_strdup_printf (_("This news entry has no headline"));
				}

				labelURL_p = item_get_base_url (item_p);
				if (labelURL_p) {
					labelText_p = g_strdup_printf ("%s <a href='%s'>%s</a>\n", labelHeadline_p, labelURL_p, _("Visit"));
				} else {
					labelText_p = g_strdup_printf ("%s\n", labelHeadline_p);
				}

				labelText_prev_p = labelText_now_p;
				labelText_now_p = g_strconcat(labelText_now_p, labelText_p, NULL);

				g_free(labelHeadline_p);
				g_free(labelText_p);
				g_free(labelText_prev_p);
			}
			item_unload (item_p);
			list_p = g_list_next (list_p);
		}
		itemset_free (itemSet);

		if (item_count == 0) {
			g_free (labelText_now_p);
			return;
		}
	} else {
		ui_show_error_box(_("This feed does not exist anymore!"));
	}

	notify_notification_close (n, NULL);

	if (node_p) {
//		notify_notification_update ( n, node_get_title(node_p), labelText_now_p, NULL);
//		notify_notification_clear_actions(n);

		n = notify_notification_new (node_get_title (node_p), labelText_now_p, NULL);
		notify_notification_set_icon_from_pixbuf (n, node_get_icon (node_p));
		notify_notification_set_category (n, "feed");
		notify_notification_set_timeout (n, NOTIFY_EXPIRES_NEVER);

		if (supports_actions) {
			notify_notification_add_action (n, "open", _("Open feed"),
							(NotifyActionCallback)notif_libnotify_callback_open,
							node_p->id, NULL);
			notify_notification_add_action (n, "mark_read", _("Mark all as read"),
							(NotifyActionCallback)notif_libnotify_callback_mark_read,
							node_p->id, NULL);
		}

		if (!notify_notification_show (n, NULL)) {
			g_warning ("libnotify.c - failed to update notification via libnotify\n");
		}

		g_free (labelText_now_p);
	}
}

static gboolean
notif_libnotify_init (void)
{
	GList *caps;

	/* Check whether the notification daemon supports actions, per Actions
	   in http://www.galago-project.org/specs/notification/0.9/x81.html */
	if (notify_init ("liferea")) {
		caps = notify_get_server_caps ();

		if (g_list_find_custom (caps, "actions", (GCompareFunc) strcmp))
			supports_actions = TRUE;
		if (g_list_find_custom (caps, "append", (GCompareFunc) strcmp))
			supports_append = TRUE;

		g_list_foreach (caps, (GFunc)g_free, NULL);
		g_list_free (caps);

		return TRUE;
	} else {
		debug0 (DEBUG_GUI, "libnotify.c: notify_init returned FALSE");
		return FALSE;
	}
}

static void
notif_libnotify_deinit (void)
{
	notify_uninit();
}

static void
notif_libnotify_node_has_new_items (nodePtr node, gboolean enforced)
{
	itemSetPtr	itemSet;
	GList		*iter;

	NotifyNotification *n;

	gchar		*labelSummary_p;
	gint		item_count = 0;

	gboolean	show_popup_windows;

	conf_get_bool_value(SHOW_POPUP_WINDOWS, &show_popup_windows);

	if (!show_popup_windows && !enforced)
		return;

	/* Count updated feed */
	itemSet = node_get_itemset (node);
	iter = itemSet->ids;
	while (iter) {
		itemPtr item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item->popupStatus && !item->readStatus)
			item_count++;
		item_unload (item);
		iter = g_list_next (iter);
	}
	itemset_free (itemSet);

	if (item_count == 0)
		return;

	labelSummary_p = g_strdup_printf (ngettext ("<b>%s</b> has <b>%d</b> update", "<b>%s</b> has <b>%d</b> updates", item_count), 
	                                  node_get_title (node), item_count);
	n = notify_notification_new (_("Feed Update"), labelSummary_p, "liferea");
	g_free (labelSummary_p);

 	if (supports_append) {
 		notify_notification_set_hint_string(n, "append", "allow");
 	} else {
		notify_notification_set_icon_from_pixbuf (n, node_get_icon (node));
	}
	
	notify_notification_set_timeout (n, NOTIFY_EXPIRES_DEFAULT);
	if (supports_actions) {
		notify_notification_add_action (n, "show_details", _("Show details"),
	                                (NotifyActionCallback)notif_libnotify_callback_show_details,
	                                node->id, NULL);
		notify_notification_add_action (n, "open", _("Open feed"),
	                                (NotifyActionCallback)notif_libnotify_callback_open,
	                                node->id, NULL);
		notify_notification_add_action (n, "mark_read", _("Mark all as read"),
	                                (NotifyActionCallback)notif_libnotify_callback_mark_read,
	                                node->id, NULL);
	}
	notify_notification_set_category (n, "feed");

	if (!notify_notification_show (n, NULL))
		g_warning ("notif_libnotify.c - failed to send notification via libnotify");
}

struct notificationPlugin libnotify_plugin = {
	"libnotify",
	notif_libnotify_init,
	notif_libnotify_deinit,
	notif_libnotify_node_has_new_items,
};
