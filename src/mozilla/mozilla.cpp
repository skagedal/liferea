/**
 * @file mozilla.cpp C++ portion of GtkMozEmbed support
 *
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
 *
 * The preference handling was taken from the Galeon source
 *
 *  Copyright (C) 2000 Marco Pesenti Gritti 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "mozilla.h"
#include <gtk/gtk.h>
#include <gtkmozembed.h>
#include <gtkmozembed_internal.h>
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowser.h"
#include "nsIDeviceContext.h"
#include "nsIPresContext.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMWindow.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"

extern "C" {
#include "../callbacks.h"
#include "../conf.h"
}

extern "C" 
gint mozilla_key_press_cb(GtkWidget *widget, gpointer ev) {
	nsIDOMKeyEvent	*event = (nsIDOMKeyEvent*)ev;
	PRUint32	keyCode = 0;
	PRBool		alt, ctrl, shift;
	
	/* This key interception is necessary to catch
	   spaces which are an internal Mozilla key binding.
	   All other combinations like Ctrl-Space, Alt-Space
	   can be caught with the GTK code in ui_mainwindow.c.
	   This is a bad case of code duplication... */
	
	event->GetCharCode(&keyCode);
	if(keyCode == nsIDOMKeyEvent::DOM_VK_SPACE) {
     		event->GetShiftKey(&shift);
     		event->GetCtrlKey(&ctrl);
     		event->GetAltKey(&alt);
		if((1 == getNumericConfValue(BROWSE_KEY_SETTING)) &&
		   !(alt | shift | ctrl)) {
			if(mozilla_scroll_pagedown(widget) == FALSE)
				on_next_unread_item_activate(NULL, NULL);
			return TRUE;
		}
	}	
	return FALSE;
}
/**
 * Takes a pointer to a mouse event and returns the mouse
 *  button number or -1 on error.
 */
extern "C" 
gint mozilla_get_mouse_event_button(gpointer event) {
	gint	button = 0;
	
	g_return_val_if_fail (event, -1);

	/* the following lines were found in the Galeon source */	
	nsIDOMMouseEvent *aMouseEvent = (nsIDOMMouseEvent *) event;
	aMouseEvent->GetButton ((PRUint16 *) &button);

	/* for some reason we get different numbers on PPC, this fixes
	 * that up... -- MattA */
	if (button == 65536)
	{
		button = 1;
	}
	else if (button == 131072)
	{
		button = 2;
	}

	return button;
}

static nsCOMPtr<nsIMarkupDocumentViewer> mozilla_get_mdv(GtkWidget *widget) {
	nsresult result;
	nsCOMPtr<nsIWebBrowser>	mWebBrowser;	

	gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(widget), getter_AddRefs(mWebBrowser));
	
	nsCOMPtr<nsIDocShellTreeItem> browserAsItem;
	browserAsItem = do_QueryInterface(mWebBrowser);
	if (!browserAsItem) return NULL;
	
	// get the owner for that item
	nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
	browserAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
	if (!treeOwner) return NULL;
	
	// get the primary content shell as an item
	nsCOMPtr<nsIDocShellTreeItem> contentItem;
	treeOwner->GetPrimaryContentShell(getter_AddRefs(contentItem));
	if (!contentItem) return NULL;
	
	// QI that back to a docshell
	nsCOMPtr<nsIDocShell> DocShell;
	DocShell = do_QueryInterface(contentItem);
	if (!DocShell) return NULL;
	
	nsCOMPtr<nsIContentViewer> contentViewer;	
	result = DocShell->GetContentViewer (getter_AddRefs(contentViewer));
	if (!NS_SUCCEEDED (result) || !contentViewer) return NULL;
	
	return do_QueryInterface(contentViewer, &result);
}

extern "C" void
mozilla_set_zoom (GtkWidget *embed, float aZoom) {
	nsCOMPtr<nsIMarkupDocumentViewer> mdv;
	
	if ((mdv = mozilla_get_mdv(embed)) == NULL)
		return;
	
	/* Ignore return because we can't do anything if it fails.... */
	mdv->SetTextZoom (aZoom);
}

extern "C" gfloat
mozilla_get_zoom (GtkWidget *widget) {
	nsCOMPtr<nsIMarkupDocumentViewer> mdv;
	float zoom;
     nsresult result;
	
	if ((mdv = mozilla_get_mdv(widget)) == NULL)
		return 1.0;
	
	result = mdv->GetTextZoom (&zoom);
	
	return zoom;
}

extern "C" void mozilla_scroll_to_top(GtkWidget *widget) {
	nsIWebBrowser *browser;
	nsIDOMWindow *DOMWindow;

	gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(widget), &browser);
	
	browser->GetContentDOMWindow(&DOMWindow);
	DOMWindow->ScrollTo(0, 0);
}

extern "C" gboolean mozilla_scroll_pagedown(GtkWidget *widget) {
	gint initial_y, final_y;
	nsIWebBrowser *browser;
	nsIDOMWindow *DOMWindow;

	gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(widget), &browser);
	
	browser->GetContentDOMWindow(&DOMWindow);
	DOMWindow->GetScrollY(&initial_y);
	DOMWindow->ScrollByPages(1);
	DOMWindow->GetScrollY(&final_y);
	
	return initial_y != final_y;
}

/* the following code is from the Galeon source mozilla/mozilla.cpp */

extern "C" gboolean
mozilla_save_prefs (void)
{
	nsCOMPtr<nsIPrefService> prefService = 
				 do_GetService (NS_PREFSERVICE_CONTRACTID);
	g_return_val_if_fail (prefService != nsnull, FALSE);

	nsresult rv = prefService->SavePrefFile (nsnull);
	return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}

/**
 * mozilla_preference_set: set a string mozilla preference
 */
extern "C" gboolean
mozilla_preference_set(const char *preference_name, const char *new_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);

	/*It is legitimate to pass in a NULL value sometimes. So let's not
	 *assert and just check and return.
	 */
	if (!new_value) return FALSE;

	nsCOMPtr<nsIPrefService> prefService = 
				do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs(pref));

	if (pref)
	{
		nsresult rv = pref->SetCharPref (preference_name, new_value);            
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

/**
 * mozilla_preference_set_boolean: set a boolean mozilla preference
 */
extern "C" gboolean
mozilla_preference_set_boolean (const char *preference_name,
				gboolean new_boolean_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);
  
	nsCOMPtr<nsIPrefService> prefService = 
				do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs(pref));
  
	if (pref)
	{
		nsresult rv = pref->SetBoolPref (preference_name,
				new_boolean_value ? PR_TRUE : PR_FALSE);
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

/**
 * mozilla_preference_set_int: set an integer mozilla preference
 */
extern "C" gboolean
mozilla_preference_set_int (const char *preference_name, int new_int_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);

	nsCOMPtr<nsIPrefService> prefService = 
				do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs(pref));

	if (pref)
	{
		nsresult rv = pref->SetIntPref (preference_name, new_int_value);
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}
