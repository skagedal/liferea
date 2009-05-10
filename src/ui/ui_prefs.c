/**
 * @file ui_prefs.c program preferences
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2009 Lars Lindner <lars.lindner@gmail.com>
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

#include "ui/ui_prefs.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "conf.h"
#include "enclosure.h"
#include "favicon.h"
#include "feedlist.h"
#include "itemlist.h"
#include "social.h"
#include "ui/enclosure_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"
#include "ui/ui_common.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_tray.h"
#include "notification/notification.h"

/* file type tree store column ids */
enum fts_columns {
	FTS_TYPE,	/* file type name */
	FTS_CMD,	/* file cmd name */
	FTS_PTR,	/* pointer to config entry */
	FTS_LEN
};

extern GSList *htmlviewPlugins;
extern GSList *bookmarkSites;	/* from social.c */
extern GSList *linkSearchSites;	/* from social.c */

static GtkWidget *prefdialog = NULL;

/** tool commands need to take an absolute file path as first %s and an URL as second %s */
static struct enclosureDownloadTool enclosure_download_commands[] = {
	{ "wget -q -O %s %s", TRUE },
	{ "curl -s -o %s %s", TRUE },
	{ "dbus-send --session --dest=org.gnome.gwget.ApplicationService /org/gnome/gwget/Gwget org.gnome.gwget.Application.OpenURI string:%s uint32:0", FALSE },
	{ "kget %s", FALSE },
};

/** order must match enclosure_download_commands[] */
static gchar *enclosure_download_tool_options[] = { "wget", "curl", "gwget", "kget", NULL };

static struct browser browsers[] = {
	{
		"gnome", N_("GNOME Default Browser"), "gnome-open %s", 
		NULL, NULL,
		NULL, NULL,
		NULL, NULL
	},
	{
		/* tested with SeaMonkey 1.0.6 */
		"mozilla", "Mozilla", "mozilla %s",
		NULL, "mozilla -remote openURL(%s)",
		NULL, "mozilla -remote 'openURL(%s,new-window)'",
		NULL, "mozilla -remote 'openURL(%s,new-tab)'"
	},
	{
		/* tested with Firefox 1.5 and 2.0 */
		"firefox", "Firefox","firefox \"%s\"",
		NULL, "firefox -a firefox -remote \"openURL(%s)\"",
		NULL, "firefox -a firefox -remote 'openURL(%s,new-window)'",
		NULL, "firefox -a firefox -remote 'openURL(%s,new-tab)'"
	},
	{
		"opera", "Opera","opera \"%s\"",
		"opera \"%s\"", "opera -remote \"openURL(%s)\"",
		"opera -newwindow \"%s\"", NULL,
		"opera -newpage \"%s\"", NULL
	},
	{
		"epiphany", "Epiphany","epiphany \"%s\"",
		NULL, NULL,
		"epiphany \"%s\"", NULL,
		"epiphany -n \"%s\"", NULL
	},
	{
		"konqueror", "Konqueror", "kfmclient openURL \"%s\"",
		NULL, NULL,
		NULL, NULL,
		NULL, NULL
	},
	{	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

/** GConf representation of toolbar styles */
static gchar * gui_toolbar_style_values[] = { "", "both", "both-horiz", "icons", "text", NULL };

static gchar * gui_toolbar_style_options[] = {
	N_("GNOME default"),
	N_("Text below icons"),
	N_("Text beside icons"),
	N_("Icons only"),
	N_("Text only"),
	NULL
};

static gchar * startup_update_options[] = {
	N_("Update out-dated feeds"),
	N_("Force update of all feeds"),
	N_("No feed update at all"),
	NULL
};

/* Note: these update interval literal should be kept in sync with the 
   ones in ui_subscription.c! */
    
static gchar * default_update_interval_unit_options[] = {
	N_("minutes"),
	N_("hours"),
	N_("days"),
	NULL
};

static gchar * browser_skim_key_options[] = {
	N_("Space"),
	N_("<Ctrl> Space"),
	N_("<Alt> Space"),
	NULL
};

gchar *
prefs_get_browser_command (struct browser *browser, gboolean remote, gboolean fallback)
{
	gchar	*cmd = NULL;
	gchar	*libname;
	gint	place;

	conf_get_int_value (BROWSER_PLACE, &place);

	/* check for manual browser command */
	conf_get_str_value (BROWSER_ID, &libname);
	if (g_str_equal (libname, "manual")) {
		/* retrieve user defined command... */
		conf_get_str_value (BROWSER_COMMAND, &cmd);
	} else {
		/* non manual browser definitions... */
		if (browser) {
			if (remote) {
				switch (place) {
					case 1:
						cmd = browser->existingwinremote;
						break;
					case 2:
						cmd = browser->newwinremote;
						break;
					case 3:
						cmd = browser->newtabremote;
						break;
				}
			} else {
				switch (place) {
					case 1:
						cmd = browser->existingwin;
						break;
					case 2:
						cmd = browser->newwin;
						break;
					case 3:
						cmd = browser->newtab;
						break;
				}
			}

			if (fallback && !cmd)	/* Default when no special mode defined */
				cmd = browser->defaultplace;
		}

		if (fallback && !cmd)	/* Last fallback: first browser default */
			cmd = browsers[0].defaultplace;
	}
	g_free (libname);
		
	return cmd?g_strdup (cmd):NULL;
}

struct browser *
prefs_get_browser (void)
{
	gchar		*libname;
	struct browser	*browser = NULL;
	
	conf_get_str_value (BROWSER_ID, &libname);
	if (!g_str_equal (libname, "manual")) {
		struct browser *iter;
		for (iter = browsers; iter->id != NULL; iter++) {
			if (g_str_equal (libname, iter->id))
				browser = iter;
		}
	}
	g_free (libname);

	return browser;
}

enclosureDownloadToolPtr
prefs_get_download_tool (void)
{
	gint	enclosure_download_tool;

	conf_get_int_value (ENCLOSURE_DOWNLOAD_TOOL, &enclosure_download_tool);

	/* FIXME: array boundary check */
	return &(enclosure_download_commands[enclosure_download_tool]);
}

/*------------------------------------------------------------------------------*/
/* preference callbacks 							*/
/*------------------------------------------------------------------------------*/

void
on_folderdisplaybtn_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean enabled = gtk_toggle_button_get_active(togglebutton);
	conf_set_int_value(FOLDER_DISPLAY_MODE, (TRUE == enabled)?1:0);
}

void
on_folderhidereadbtn_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	nodePtr	displayedNode;

	gboolean enabled = gtk_toggle_button_get_active (togglebutton);
	conf_set_bool_value (FOLDER_DISPLAY_HIDE_READ, enabled);
	displayedNode = itemlist_get_displayed_node ();
	if (displayedNode) {
		itemlist_unload (FALSE);
		itemlist_load (displayedNode);
	}
}

void
on_trayiconoptionbtn_clicked (GtkButton *button, gpointer user_data)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));
	conf_set_bool_value (SHOW_TRAY_ICON, enabled);
	gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog, "newcountintraybtn"), enabled);
	gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog, "minimizetotraybtn"), enabled);
	gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog, "startintraybtn"), enabled);
}

void
on_popupwindowsoptionbtn_clicked (GtkButton *button, gpointer user_data)
{
	gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	conf_set_bool_value (SHOW_POPUP_WINDOWS, enabled);
}

static void
on_feed_startup_update_changed (gpointer user_data)
{
	conf_set_int_value (STARTUP_FEED_ACTION, gtk_combo_box_get_active (GTK_COMBO_BOX (liferea_dialog_lookup (prefdialog, "startupActionCombo"))));
}

void
on_browsercmd_changed (GtkEditable *editable, gpointer user_data)
{
	conf_set_str_value (BROWSER_COMMAND, gtk_editable_get_chars (editable,0,-1));
}

static void
on_browser_changed (GtkOptionMenu *optionmenu, gpointer user_data)
{
	gint num = GPOINTER_TO_INT (user_data);

	gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog, "browsercmd"), browsers[num].id == NULL);	
	gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog, "manuallabel"), browsers[num].id == NULL);	

	if (browsers[num].id == NULL)
		conf_set_str_value (BROWSER_ID, "manual");
	else
		conf_set_str_value (BROWSER_ID, browsers[num].id);
}

static void
on_browser_place_changed (GtkOptionMenu *optionmenu, gpointer user_data)
{
	int num = GPOINTER_TO_INT (user_data);
	
	conf_set_int_value (BROWSER_PLACE, num);
}

void
on_openlinksinsidebtn_clicked (GtkToggleButton *button, gpointer user_data)
{
	conf_set_bool_value (BROWSE_INSIDE_APPLICATION, gtk_toggle_button_get_active (button));
}

void
on_disablejavascript_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	conf_set_bool_value (DISABLE_JAVASCRIPT, gtk_toggle_button_get_active (togglebutton));
}

void
on_allowflash_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	conf_set_bool_value (ALLOW_FLASH, gtk_toggle_button_get_active (togglebutton));
}

static void
on_socialsite_changed (GtkOptionMenu *optionmenu, gpointer user_data)
{
	social_set_bookmark_site ((gchar *)user_data);
}

static void
on_linksearchsite_changed (GtkOptionMenu *optionmenu, gpointer user_data)
{
	social_set_link_search_site ((gchar *)user_data);
}

static void
on_gui_toolbar_style_changed (gpointer user_data)
{
	gchar *style;
	gint value = gtk_combo_box_get_active (GTK_COMBO_BOX (user_data));
	conf_set_str_value (TOOLBAR_STYLE, gui_toolbar_style_values[value]);

	style = conf_get_toolbar_style ();
	liferea_shell_set_toolbar_style (style);
	g_free (style);
}

void
on_itemCountBtn_value_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkAdjustment	*itemCount;
	
	itemCount = gtk_spin_button_get_adjustment (spinbutton);
	conf_set_int_value (DEFAULT_MAX_ITEMS, gtk_adjustment_get_value (itemCount));
}

void
on_default_update_interval_value_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
	gint		updateInterval, intervalUnit;
	GtkWidget	*unitWidget, *valueWidget;

	unitWidget = liferea_dialog_lookup (prefdialog, "globalRefreshIntervalUnitComboBox");
	valueWidget = liferea_dialog_lookup (prefdialog, "globalRefreshIntervalSpinButton");
	intervalUnit = gtk_combo_box_get_active (GTK_COMBO_BOX (unitWidget));
	updateInterval = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (valueWidget));

	if (intervalUnit == 1)
		updateInterval *= 60;		/* hours */
	else if (intervalUnit == 2)
		updateInterval *= 1440;		/* days */

	conf_set_int_value (DEFAULT_UPDATE_INTERVAL, updateInterval);
}

static void
on_default_update_interval_unit_changed (gpointer user_data)
{
	on_default_update_interval_value_changed (NULL, NULL);
}

/*
void
on_menuselection_clicked (GtkButton *button, gpointer user_data)
{
	gint active_button;
	
	active_button = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT(button), "option_number"));
	switch (active_button) {
		case 1:
			conf_set_bool_value (DISABLE_MENUBAR, FALSE);
			conf_set_bool_value (DISABLE_TOOLBAR, FALSE);
			break;
		case 2:
			conf_set_bool_value (DISABLE_MENUBAR, FALSE);
			conf_set_bool_value (DISABLE_TOOLBAR, TRUE);
			break;
		case 3:
			conf_set_bool_value (DISABLE_MENUBAR, TRUE);
			conf_set_bool_value (DISABLE_TOOLBAR, FALSE);
			break;
		default:
			break;
	}
	
	liferea_shell_update_menubar ();
	liferea_shell_update_toolbar ();
}*/

static void
on_updateallfavicons_clicked (GtkButton *button, gpointer user_data)
{
	feedlist_foreach (node_update_favicon);
}

static void
on_proxyAutoDetect_clicked (GtkButton *button, gpointer user_data)
{
	conf_set_int_value (PROXY_DETECT_MODE, 0);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog, "proxybox")), FALSE);
}

static void
on_noProxy_clicked (GtkButton *button, gpointer user_data)
{
	conf_set_int_value (PROXY_DETECT_MODE, 1);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog, "proxybox")), FALSE);
}

static void
on_manualProxy_clicked (GtkButton *button, gpointer user_data)
{
	conf_set_int_value (PROXY_DETECT_MODE, 2);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog, "proxybox")), TRUE);
}

void
on_useProxyAuth_toggled (GtkToggleButton *button, gpointer user_data)
{
	gboolean enabled = gtk_toggle_button_get_active (button);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog, "proxyauthtable")), enabled);
	conf_set_bool_value (PROXY_USEAUTH, enabled);
}

void
on_proxyhostentry_changed (GtkEditable *editable, gpointer user_data)
{
	conf_set_str_value (PROXY_HOST, gtk_editable_get_chars (editable,0,-1));
}

void
on_proxyportentry_changed (GtkEditable *editable, gpointer user_data)
{
	conf_set_int_value (PROXY_PORT, atoi (gtk_editable_get_chars (editable,0,-1)));
}

void
on_proxyusernameentry_changed (GtkEditable *editable, gpointer user_data)
{
	conf_set_str_value (PROXY_USER, gtk_editable_get_chars (editable,0,-1));
}

void
on_proxypasswordentry_changed (GtkEditable *editable, gpointer user_data)
{
	conf_set_str_value (PROXY_PASSWD, gtk_editable_get_chars (editable,0,-1));
}

static void
on_skim_key_changed (gpointer user_data) 
{
	conf_set_int_value (BROWSE_KEY_SETTING, gtk_combo_box_get_active (GTK_COMBO_BOX (user_data)));
}

static void
on_enclosure_download_tool_changed (gpointer user_data)
{
	conf_set_int_value (ENCLOSURE_DOWNLOAD_TOOL, gtk_combo_box_get_active (GTK_COMBO_BOX (user_data)));
}

void on_enc_action_change_btn_clicked(GtkButton *button, gpointer user_data) {
	GtkTreeModel		*model;
	GtkTreeSelection	*selection;
	GtkTreeIter		iter;
	gpointer		type;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(liferea_dialog_lookup(prefdialog, "enc_action_view")));
	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter, FTS_PTR, &type, -1);
		ui_enclosure_change_type(type);
		gtk_tree_store_set(GTK_TREE_STORE(model), &iter, 
		                   FTS_CMD, ((encTypePtr)type)->cmd, -1);
	}
}

void
on_enc_action_remove_btn_clicked (GtkButton *button, gpointer user_data)
{
	GtkTreeModel		*model;
	GtkTreeSelection	*selection;
	GtkTreeIter		iter;
	gpointer		type;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (liferea_dialog_lookup (prefdialog, "enc_action_view")));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, FTS_PTR, &type, -1);
		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
		enclosure_mime_type_remove (type);
	}
}

void on_save_download_entry_changed(GtkEditable *editable, gpointer user_data) {

	conf_set_str_value(ENCLOSURE_DOWNLOAD_PATH, gtk_editable_get_chars(editable , 0, -1));
}

static void on_save_download_finished(const gchar *filename, gpointer user_data) {
	GtkWidget	*dialog = (GtkWidget *)user_data;
	gchar		*utfname;
	
	if(filename == NULL)
		return;
	
	utfname = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

	if(utfname != NULL) {
		gtk_entry_set_text(GTK_ENTRY(liferea_dialog_lookup(dialog, "save_download_entry")), utfname);
		conf_set_str_value(ENCLOSURE_DOWNLOAD_PATH, utfname);
	}
	
	g_free(utfname);
}

void on_save_download_select_btn_clicked(GtkButton *button, gpointer user_data) {
	const gchar *path = gtk_editable_get_chars(GTK_EDITABLE(liferea_dialog_lookup(prefdialog, "save_download_entry")), 0, -1);
	
	ui_choose_directory(_("Choose download directory"), GTK_STOCK_OPEN, on_save_download_finished, path, prefdialog);
}

void on_newcountintraybtn_clicked(GtkButton *button, gpointer user_data) {

	conf_set_bool_value(SHOW_NEW_COUNT_IN_TRAY, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}

void on_minimizetotraybtn_clicked(GtkButton *button, gpointer user_data) {

	conf_set_bool_value(DONT_MINIMIZE_TO_TRAY, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}

void
on_startintraybtn_clicked (GtkButton *button, gpointer user_data)
{
	conf_set_bool_value (START_IN_TRAY, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
}

void
on_useAvahiSync_toggled (GtkToggleButton *button, gpointer user_data)
{
	gboolean enabled = gtk_toggle_button_get_active (button);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog, "avahiServiceNameEntry")), enabled);
	gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog, "avahiServicePasswordEntry")), enabled);
	conf_set_bool_value (SYNC_AVAHI_ENABLED, enabled);
}

void
on_hidetoolbar_toggled (GtkToggleButton *button, gpointer user_data)
{
	conf_set_bool_value (DISABLE_TOOLBAR, gtk_toggle_button_get_active (button));
	liferea_shell_update_toolbar ();
}

/*------------------------------------------------------------------------------*/
/* preferences dialog callbacks 						*/
/*------------------------------------------------------------------------------*/

static void ui_pref_destroyed_cb(GtkWidget *widget, void *data) {

	prefdialog = NULL;
}

void on_prefbtn_clicked(void) {
	GtkWidget		*widget, *entry, *menu;
	GtkAdjustment		*itemCount;
	GtkTreeStore		*treestore;
	GtkTreeViewColumn 	*column;
	GSList			*list;
	gchar			*proxyport;
	gchar			*configuredBrowser, *name;
	gboolean		enabled;
	int			tmp, i;
	static int		manual;
	struct browser		*iter;
	gint			startup_feed_action, default_max_items;
	gint			folder_display_mode, browse_key_setting;
	gint			proxy_port, browser_place;
	gint			enclosure_download_tool;
	gboolean		folder_display_hide_read, disable_javascript;
	gboolean		browse_inside_application, allow_flash;
	gboolean		show_tray_icon, show_popup_windows;
	gboolean		show_new_count_in_tray, dont_minimize_to_tray;
	gboolean		start_in_tray, disable_toolbar;
	gchar			*proxy_host, *proxy_user, *proxy_passwd;
	gchar			*browser_command, *enclosure_download_path;
	
	if (!prefdialog) {
		prefdialog = liferea_dialog_new (NULL, "prefdialog");
		g_signal_connect (G_OBJECT (prefdialog), "destroy", G_CALLBACK (ui_pref_destroyed_cb), NULL);

		/* Set up browser selection popup */
		menu = gtk_menu_new();
		for(i=0, iter = browsers; iter->id != NULL; iter++, i++) {
			entry = gtk_menu_item_new_with_label(_(iter->display));
			gtk_widget_show(entry);
			gtk_container_add(GTK_CONTAINER(menu), entry);
			g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_browser_changed), GINT_TO_POINTER(i));
		}
		manual = i;
		/* This allows the user to choose their own browser by typing in the command. */
		entry = gtk_menu_item_new_with_label(_("Manual"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_browser_changed), GINT_TO_POINTER(i));

		gtk_option_menu_set_menu(GTK_OPTION_MENU(liferea_dialog_lookup(prefdialog, "browserpopup")), menu);

		/* Create location menu */
		menu = gtk_menu_new();

		entry = gtk_menu_item_new_with_label(_("Browser default"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_browser_place_changed), GINT_TO_POINTER(0));

		entry = gtk_menu_item_new_with_label(_("Existing window"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_browser_place_changed), GINT_TO_POINTER(1));

		entry = gtk_menu_item_new_with_label(_("New window"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_browser_place_changed), GINT_TO_POINTER(2));

		entry = gtk_menu_item_new_with_label(_("New tab"));
		gtk_widget_show(entry);
		gtk_container_add(GTK_CONTAINER(menu), entry);
		g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_browser_place_changed), GINT_TO_POINTER(3));

		gtk_option_menu_set_menu(GTK_OPTION_MENU(liferea_dialog_lookup(prefdialog, "browserlocpopup")), menu);

		/* ================== panel 1 "feeds" ==================== */

		/* menu for feed startup update action */
		conf_get_int_value (STARTUP_FEED_ACTION, &startup_feed_action);
		ui_common_setup_combo_menu (liferea_dialog_lookup (prefdialog, "startupActionCombo"),
		                            startup_update_options, 
		                            G_CALLBACK (on_feed_startup_update_changed),
		                            startup_feed_action);

		/* cache size setting */
		widget = liferea_dialog_lookup (prefdialog, "itemCountBtn");
		itemCount = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
		conf_get_int_value (DEFAULT_MAX_ITEMS, &default_max_items);
		gtk_adjustment_set_value (itemCount, default_max_items);

		/* set default update interval spin button and unit combo box */
		ui_common_setup_combo_menu (liferea_dialog_lookup (prefdialog, "globalRefreshIntervalUnitComboBox"),
		                            default_update_interval_unit_options,
		                            G_CALLBACK (on_default_update_interval_unit_changed),
					    -1);
					   
		widget = liferea_dialog_lookup (prefdialog, "globalRefreshIntervalUnitComboBox");
		conf_get_int_value (DEFAULT_UPDATE_INTERVAL, &tmp);
		if (tmp % 1440 == 0) {		/* days */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
			tmp /= 1440;
		} else if (tmp % 60 == 0) {	/* hours */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
			tmp /= 60;
		} else {			/* minutes */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
		}
		widget = liferea_dialog_lookup (prefdialog,"globalRefreshIntervalSpinButton");
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), tmp);
		g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK (on_default_update_interval_value_changed), NULL);

		/* ================== panel 2 "folders" ==================== */

		g_signal_connect(GTK_OBJECT(liferea_dialog_lookup(prefdialog, "updateAllFavicons")), "clicked", G_CALLBACK(on_updateallfavicons_clicked), NULL);

		conf_get_int_value (FOLDER_DISPLAY_MODE, &folder_display_mode);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(prefdialog, "folderdisplaybtn")), folder_display_mode?TRUE:FALSE);
		conf_get_bool_value (FOLDER_DISPLAY_HIDE_READ, &folder_display_hide_read);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(liferea_dialog_lookup(prefdialog, "hidereadbtn")), folder_display_hide_read?TRUE:FALSE);

		/* ================== panel 3 "headlines" ==================== */

		conf_get_int_value (BROWSE_KEY_SETTING, &browse_key_setting);
		ui_common_setup_combo_menu (liferea_dialog_lookup (prefdialog, "skimKeyCombo"),
		                            browser_skim_key_options,
		                            G_CALLBACK (on_skim_key_changed),
		                            browse_key_setting);
					  
		/* Setup social bookmarking list */
		i = 0;
		conf_get_str_value (SOCIAL_BM_SITE, &name);
		menu = gtk_menu_new ();
		list = bookmarkSites;
		while (list) {
			socialSitePtr siter = list->data;
			if (name && !strcmp (siter->name, name))
				tmp = i;
			entry = gtk_menu_item_new_with_label (siter->name);
			gtk_widget_show (entry);
			gtk_container_add (GTK_CONTAINER (menu), entry);
			g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (on_socialsite_changed), (gpointer)siter->name);
			list = g_slist_next (list);
			i++;
		}
		gtk_option_menu_set_menu (GTK_OPTION_MENU (liferea_dialog_lookup (prefdialog, "socialpopup")), menu);
		gtk_option_menu_set_history (GTK_OPTION_MENU (liferea_dialog_lookup (prefdialog, "socialpopup")), tmp);
		
		/* Setup link cosmos search engine list */
		i = 0;
		conf_get_str_value (SOCIAL_LINK_SEARCH_SITE, &name);
		menu = gtk_menu_new ();
		list = linkSearchSites;
		while (list) {
			socialSitePtr siter = list->data;
			if (name && !strcmp (siter->name, name))
				tmp = i;
			entry = gtk_menu_item_new_with_label (siter->name);
			gtk_widget_show (entry);
			gtk_container_add (GTK_CONTAINER (menu), entry);
			g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (on_linksearchsite_changed), (gpointer)siter->name);
			list = g_slist_next (list);
			i++;
		}
		gtk_option_menu_set_menu (GTK_OPTION_MENU (liferea_dialog_lookup (prefdialog, "searchpopup")), menu);
		gtk_option_menu_set_history (GTK_OPTION_MENU (liferea_dialog_lookup (prefdialog, "searchpopup")), tmp);
		

		/* ================== panel 4 "browser" ==================== */

		/* set the inside browsing flag */
		widget = liferea_dialog_lookup(prefdialog, "browseinwindow");
		conf_get_bool_value(BROWSE_INSIDE_APPLICATION, &browse_inside_application);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), browse_inside_application);

		/* set the javascript-disabled flag */
		widget = liferea_dialog_lookup(prefdialog, "disablejavascript");
		conf_get_bool_value(DISABLE_JAVASCRIPT, &disable_javascript);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), disable_javascript);
		
		/* set the allow Flash flag */
		widget = liferea_dialog_lookup(prefdialog, "allowflash");
		conf_get_bool_value(ALLOW_FLASH, &allow_flash);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), allow_flash);

		tmp = 0;
		conf_get_str_value(BROWSER_ID, &configuredBrowser);

		if(!strcmp(configuredBrowser, "manual"))
			tmp = manual;
		else
			for(i=0, iter = browsers; iter->id != NULL; iter++, i++)
				if(!strcmp(configuredBrowser, iter->id))
					tmp = i;

		gtk_option_menu_set_history(GTK_OPTION_MENU(liferea_dialog_lookup(prefdialog, "browserpopup")), tmp);
		g_free(configuredBrowser);

		conf_get_int_value (BROWSER_PLACE, &browser_place);
		gtk_option_menu_set_history(GTK_OPTION_MENU(liferea_dialog_lookup(prefdialog, "browserlocpopup")), browser_place);

		conf_get_str_value (BROWSER_COMMAND, &browser_command);
		entry = liferea_dialog_lookup(prefdialog, "browsercmd");
		gtk_entry_set_text(GTK_ENTRY(entry), browser_command);
		g_free (browser_command);

		gtk_widget_set_sensitive(GTK_WIDGET(entry), tmp==manual);
		gtk_widget_set_sensitive(liferea_dialog_lookup(prefdialog, "manuallabel"), tmp==manual);	

		/* ================== panel 4 "GUI" ================ */

		conf_get_bool_value (SHOW_TRAY_ICON, &show_tray_icon);

		widget = liferea_dialog_lookup (prefdialog, "popupwindowsoptionbtn");
		conf_get_bool_value (SHOW_POPUP_WINDOWS, &show_popup_windows);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), show_popup_windows);
		
		widget = liferea_dialog_lookup (prefdialog, "trayiconoptionbtn");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), show_tray_icon);

		widget = liferea_dialog_lookup (prefdialog, "newcountintraybtn");
		conf_get_bool_value (SHOW_NEW_COUNT_IN_TRAY, &show_new_count_in_tray);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), show_new_count_in_tray);
		gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog, "newcountintraybtn"), show_tray_icon);

		widget = liferea_dialog_lookup (prefdialog, "minimizetotraybtn");
		conf_get_bool_value (DONT_MINIMIZE_TO_TRAY, &dont_minimize_to_tray);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), dont_minimize_to_tray);
		gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog, "minimizetotraybtn"), show_tray_icon);
		
		widget = liferea_dialog_lookup (prefdialog, "startintraybtn");
		conf_get_bool_value (START_IN_TRAY, &start_in_tray);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), start_in_tray);
		gtk_widget_set_sensitive (liferea_dialog_lookup (prefdialog, "startintraybtn"), show_tray_icon);

		/* tool bar settings */	
		widget = liferea_dialog_lookup (prefdialog, "hidetoolbarbtn");
		conf_get_bool_value(DISABLE_TOOLBAR, &disable_toolbar);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), disable_toolbar);

		/* select currently active toolbar style option */
		conf_get_str_value (TOOLBAR_STYLE, &name);
		for (i = 0; gui_toolbar_style_values[i] != NULL; ++i) {
			if (strcmp (name, gui_toolbar_style_values[i]) == 0)
				break;
		}
		g_free (name);

		/* On invalid key value: revert to default */
		if (gui_toolbar_style_values[i] == NULL)
			i = 0;

		/* create toolbar style menu */
		ui_common_setup_combo_menu (liferea_dialog_lookup (prefdialog, "toolbarCombo"),
		                            gui_toolbar_style_options,
		                            G_CALLBACK (on_gui_toolbar_style_changed),
		                            i);

		/* ================= panel 5 "proxy" ======================== */
		conf_get_str_value (PROXY_HOST, &proxy_host);
		gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (prefdialog, "proxyhostentry")), proxy_host);
		g_free (proxy_host);

		conf_get_int_value (PROXY_PORT, &proxy_port);
		proxyport = g_strdup_printf ("%d", proxy_port);
		gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (prefdialog, "proxyportentry")), proxyport);
		g_free (proxyport);

		conf_get_bool_value (PROXY_USEAUTH, &enabled);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (prefdialog, "useProxyAuth")), enabled);

		conf_get_str_value (PROXY_USER, &proxy_user);
		gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (prefdialog, "proxyusernameentry")), proxy_user);
		g_free (proxy_user);

		conf_get_str_value (PROXY_PASSWD, &proxy_passwd);
		gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (prefdialog, "proxypasswordentry")), proxy_passwd);
		g_free (proxy_passwd);

		gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup(prefdialog, "proxyauthtable")), enabled);
			
		conf_get_int_value (PROXY_DETECT_MODE, &i);
		switch (i) {
			default:
			case 0: /* proxy auto detect */
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (prefdialog, "proxyAutoDetectRadio")), TRUE);
				enabled = FALSE;
				break;
			case 1: /* no proxy */
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (prefdialog, "noProxyRadio")), TRUE);
				enabled = FALSE;
				break;
			case 2: /* manual proxy */
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (prefdialog, "manualProxyRadio")), TRUE);
				enabled = TRUE;
				break;
		}
		gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog, "proxybox")), enabled);
		g_signal_connect (G_OBJECT (liferea_dialog_lookup (prefdialog, "proxyAutoDetectRadio")), "clicked", G_CALLBACK (on_proxyAutoDetect_clicked), NULL);
		g_signal_connect (G_OBJECT (liferea_dialog_lookup (prefdialog, "noProxyRadio")), "clicked", G_CALLBACK (on_noProxy_clicked), NULL);
		g_signal_connect (G_OBJECT (liferea_dialog_lookup (prefdialog, "manualProxyRadio")), "clicked", G_CALLBACK (on_manualProxy_clicked), NULL);
		g_signal_connect (G_OBJECT (liferea_dialog_lookup (prefdialog, "proxyhostentry")), "changed", G_CALLBACK (on_proxyhostentry_changed), NULL);
		g_signal_connect (G_OBJECT (liferea_dialog_lookup (prefdialog, "proxyportentry")), "changed", G_CALLBACK (on_proxyportentry_changed), NULL);
		g_signal_connect (G_OBJECT (liferea_dialog_lookup (prefdialog, "proxyusernameentry")), "changed", G_CALLBACK (on_proxyusernameentry_changed), NULL);
		g_signal_connect (G_OBJECT (liferea_dialog_lookup (prefdialog, "proxypasswordentry")), "changed", G_CALLBACK (on_proxypasswordentry_changed), NULL);

		/* ================= panel 6 "enclosures" ======================== */

		/* menu for download tool */
		conf_get_int_value (ENCLOSURE_DOWNLOAD_TOOL, &enclosure_download_tool);
		ui_common_setup_combo_menu (liferea_dialog_lookup (prefdialog, "downloadToolCombo"),
		                            enclosure_download_tool_options,
		                            G_CALLBACK (on_enclosure_download_tool_changed),
		                            enclosure_download_tool);

		/* set enclosure download path entry */
		conf_get_str_value (ENCLOSURE_DOWNLOAD_PATH, &enclosure_download_path);
		gtk_entry_set_text(GTK_ENTRY(liferea_dialog_lookup(prefdialog, "save_download_entry")), enclosure_download_path);
		g_free (enclosure_download_path);

		/* set up list of configured enclosure types */
		treestore = gtk_tree_store_new(FTS_LEN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
		list = (GSList *)enclosure_mime_types_get ();
		while (list) {
			GtkTreeIter *newIter = g_new0(GtkTreeIter, 1);
			gtk_tree_store_append(treestore, newIter, NULL);
			gtk_tree_store_set(treestore, newIter,
		                	   FTS_TYPE, (NULL != ((encTypePtr)(list->data))->mime)?((encTypePtr)(list->data))->mime:((encTypePtr)(list->data))->extension, 
		                	   FTS_CMD, ((encTypePtr)(list->data))->cmd,
		                	   FTS_PTR, list->data, 
					   -1);
			list = g_slist_next(list);
		}

		widget = liferea_dialog_lookup(prefdialog, "enc_action_view");
		gtk_tree_view_set_model(GTK_TREE_VIEW(widget), GTK_TREE_MODEL(treestore));

		column = gtk_tree_view_column_new_with_attributes(_("Type"), gtk_cell_renderer_text_new(), "text", FTS_TYPE, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);
		gtk_tree_view_column_set_sort_column_id(column, FTS_TYPE);
		column = gtk_tree_view_column_new_with_attributes(_("Program"), gtk_cell_renderer_text_new(), "text", FTS_CMD, NULL);
		gtk_tree_view_column_set_sort_column_id(column, FTS_CMD);
		gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);

		gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), GTK_SELECTION_SINGLE);

		/* ================= panel 7 "sync" ======================== */
		/* FIXME: Avahi Sync is not yet imlemented

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (prefdialog, "useAvahiSync")), conf_get_bool_value (SYNC_AVAHI_ENABLED));
		gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (prefdialog, "avahiServiceNameEntry")), conf_get_str_value (SYNC_AVAHI_SERVICE_NAME));		
		gtk_entry_set_text (GTK_ENTRY (liferea_dialog_lookup (prefdialog, "avahiServicePasswordEntry")), conf_get_str_value (SYNC_AVAHI_PASSWORD));
		gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog, "avahiServiceNameEntry")), conf_get_bool_value (SYNC_AVAHI_ENABLED));
		gtk_widget_set_sensitive (GTK_WIDGET (liferea_dialog_lookup (prefdialog, "avahiServicePasswordEntry")), conf_get_bool_value (SYNC_AVAHI_ENABLED));
		*/
	}	
	gtk_window_present(GTK_WINDOW(prefdialog));
}
