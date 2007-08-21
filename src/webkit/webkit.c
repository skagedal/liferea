/**
 * @file webkit.c WebKit browser module for Liferea
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

#include <webkitgtkpage.h>
#include <webkitgtkglobal.h>
#include <webkitgtksettings.h>

#include "conf.h"
#include "ui/ui_htmlview.h"

static void
webkit_init (void)
{
	webkit_gtk_init ();
}

static void webkit_deinit (void) { }

static void
webkit_write_html (GtkWidget *scrollpane,
                   const gchar *string,
		   guint length,
		   const gchar *base,
		   const gchar *contentType)
{
	GtkWidget *htmlwidget = gtk_bin_get_child (GTK_BIN (scrollpane));
	
	webkit_gtk_page_load_string (WEBKIT_GTK_PAGE (htmlwidget), string, "application/xhtml", "UTF-8", base);
}

static void
webkit_title_changed (WebKitGtkPage *page, const gchar* title, const gchar* url, gpointer user_data)
{
	ui_tabs_set_title(GTK_WIDGET(page), title);
}

static void
webkit_progress_changed (WebKitGtkPage *page, gint progress, gpointer user_data)
{
}

static void
webkit_on_url (WebKitGtkPage *page, const gchar *url, gpointer user_data)
{
	gchar *selectedURL;
	
	selectedURL = g_object_get_data (G_OBJECT (page), "selectedURL");
	g_free (selectedURL);
		
	if (url) {
		selectedURL = g_strdup (url);

		/* overwrite or clear last status line text */
		ui_htmlview_on_url (selectedURL);
	} else {
		selectedURL = NULL;
	}
	
	g_object_set_data (G_OBJECT (page), "selectedURL", selectedURL);
}

static GtkWidget *
webkit_new (gboolean forceInternalBrowsing) 
{
	gulong	handler;
	GtkWidget *htmlwidget;
	GtkWidget *scrollpane;
	WebKitGtkSettings *settings;
	
	scrollpane = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollpane), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrollpane), GTK_SHADOW_IN);
	
	/* create html widget and pack it into the scrolled window */
	htmlwidget = webkit_gtk_page_new ();
	
/*	// empty functions in current webkit code...
	settings = webkit_gtk_web_settings_copy (webkit_gtk_web_settings_get_global ());
	settings->is_java_script_enabled = !conf_get_bool_value (DISABLE_JAVASCRIPT);
	settings->java_script_can_open_windows_automatically = FALSE;
	webkit_gtk_page_set_settings (WEBKIT_GTK_PAGE (htmlwidget), settings);
*/
	gtk_container_add (GTK_CONTAINER (scrollpane), GTK_WIDGET (htmlwidget));
	
	g_object_set_data (G_OBJECT (scrollpane), "internal_browsing", GINT_TO_POINTER (forceInternalBrowsing));

	g_signal_connect (htmlwidget, "title-changed", G_CALLBACK (webkit_title_changed), htmlwidget);
	g_signal_connect (htmlwidget, "load-progress-changed", G_CALLBACK (webkit_progress_changed), htmlwidget);
	g_signal_connect (htmlwidget, "hovering_over_link", G_CALLBACK (webkit_on_url), htmlwidget);

	gtk_widget_show (htmlwidget);
	return scrollpane;
}

static void
webkit_launch_url (GtkWidget *scrollpane, const gchar *url)
{
	webkit_gtk_page_open (WEBKIT_GTK_PAGE (gtk_bin_get_child (GTK_BIN (scrollpane))), url);
}

static gboolean
webkit_launch_inside_possible (void)
{
	return TRUE;
}

static void
webkit_change_zoom_level (GtkWidget *scrollpane, gfloat zoomLevel)
{
	// FIXME!
}

static gfloat
webkit_get_zoom_level (GtkWidget *scrollpane)
{
	// FIXME!
	return 1.0;
}

static gboolean
webkit_scroll_pagedown (GtkWidget *scrollpane)
{
	// FIXME!
}

static struct htmlviewPlugin webkitInfo = {
	.api_version	= HTMLVIEW_PLUGIN_API_VERSION,
	.name		= "WebKit",
	.priority	= 100,
	.externalCss	= FALSE,
	.plugin_init	= webkit_init,
	.plugin_deinit	= webkit_deinit,
	.create		= webkit_new,
	.write		= webkit_write_html,
	.launch		= webkit_launch_url,
	.launchInsidePossible = webkit_launch_inside_possible,
	.zoomLevelGet	= webkit_get_zoom_level,
	.zoomLevelSet	= webkit_change_zoom_level,
	.scrollPagedown	= webkit_scroll_pagedown,
	.setProxy	= NULL,
	.setOffLine	= NULL
};

static struct plugin pi = {
	PLUGIN_API_VERSION,
	"WebKit Rendering Plugin",
	PLUGIN_TYPE_HTML_RENDERER,
	&webkitInfo
};

DECLARE_PLUGIN(pi);
DECLARE_HTMLVIEW_PLUGIN(webkitInfo);
