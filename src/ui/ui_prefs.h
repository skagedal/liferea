/**
 * @file ui_prefs.h program preferences
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _UI_PREFS_H
#define _UI_PREFS_H

#include <gtk/gtk.h>

/** Browser preference definition structure */
struct browser {
	gchar *id;			/**< Unique ID used in storing the prefs */
	gchar *display;			/**< Name to display in the prefs */
	gchar *defaultplace;		/**< Default command.... Use %s to specify URL. This command is called in the background. */
	gchar *existingwin;		/**< Optional command variant for opening in existing window */
	gchar *existingwinremote;	/**< Optional command variant for opening in existing window with remote protocol */
	gchar *newwin;			/**< Optional command variant for opening in new window */
	gchar *newwinremote;		/**< Optional command variant for opening in new window with remote protocol */
	gchar *newtab;			/**< Optional command variant for opening in new tab */
	gchar *newtabremote;		/**< Optional command variant for opening in new tab with remote protocol */
	gboolean escapeRemote;		/**< Flag to indicate wether "," escaping for remote commands is necessary */
};

/** 
 * Returns the browser definition structure for the currently
 * configured external browser or NULL if a user defined 
 * browser command is defined.
 *
 * @returns browser definition
 */
struct browser * prefs_get_browser(void);

/**
 * Returns a shell command format string which can be used to create
 * a browser launch command. The string will contain exactly one %s
 * to fill in the URL. 
 *
 * @param browser	browser definition (or NULL) as returned
 *			by prefs_get_browser()
 * @param remote	TRUE if remote command variant is requested
 * @param fallback	TRUE if the default command is to be returned
 *			if the specific launch type is not available.
 *			If set to FALSE no command might be returned.
 *
 * @returns a newly allocated command string
 */
gchar * prefs_get_browser_command(struct browser *browser, gboolean remote, gboolean fallback);

/**
 * Returns a shell command line with exactly two %s format codes
 * that can be used for launching an enclosure download program.
 * The first %s is to be replaced with the output filename, the
 * second %s is to be replaced with the URL to download.
 *
 * @return the command string
 */
const gchar * prefs_get_download_cmd(void);

/* GUI callbacks */

void 
on_prefbtn_clicked                     (GtkButton       *button,
                                        gpointer user_data);

void
on_trayiconoptionbtn_clicked           (GtkButton       *button,
                                        gpointer         user_data);
										
void
on_popupwindowsoptionbtn_clicked       (GtkButton       *button,
                                        gpointer         user_data);

void
on_browsercmd_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_timeformatselection_clicked         (GtkButton       *button,
                                        gpointer         user_data);

void
on_timeformatentry_changed             (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_itemCountBtn_value_changed          (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);
									
void
on_default_update_interval_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_menuselection_clicked               (GtkButton       *button,
                                        gpointer         user_data);
					
void
on_placement_radiobtn_clicked          (GtkButton       *button,
                                        gpointer         user_data);

void
on_proxyhostentry_changed              (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_proxyportentry_changed              (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_proxyusernameentry_changed          (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_proxypasswordentry_changed          (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_openlinksinsidebtn_clicked          (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_feedsinmemorybtn_clicked            (GtkButton       *button,
                                        gpointer         user_data);
					
void
on_browsekey_space_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_browsekey_ctrl_space_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_browsekey_alt_space_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_disablejavascript_toggled           (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_folderdisplaybtn_toggled            (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_folderhidereadbtn_toggled            (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_enc_action_change_btn_clicked       (GtkButton       *button,
                                        gpointer         user_data);

void
on_enc_action_remove_btn_clicked       (GtkButton       *button,
                                        gpointer         user_data);

void
on_save_download_select_btn_clicked    (GtkButton       *button,
                                        gpointer         user_data);
					
void
on_save_download_entry_changed         (GtkEditable     *editable,
                                        gpointer         user_data);

#endif
