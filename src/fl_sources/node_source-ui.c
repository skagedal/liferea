/*
 * DO NOT EDIT THIS FILE - it is generated by Glade.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include ".node_source-cb.h"
#include "node_source-ui.h"
#include "support.h"

#define GLADE_HOOKUP_OBJECT(component,widget,name) \
  g_object_set_data_full (G_OBJECT (component), name, \
    gtk_widget_ref (widget), (GDestroyNotify) gtk_widget_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name) \
  g_object_set_data (G_OBJECT (component), name, widget)

GtkWidget*
create_node_source_type_dialog (void)
{
  GtkWidget *node_source_type_dialog;
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox1;
  GtkWidget *label2;
  GtkWidget *scrolledwindow1;
  GtkWidget *type_list;
  GtkWidget *dialog_action_area1;
  GtkWidget *cancelbutton1;
  GtkWidget *ok_button;

  node_source_type_dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (node_source_type_dialog), _("Source Selection"));
  gtk_window_set_modal (GTK_WINDOW (node_source_type_dialog), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (node_source_type_dialog), 300, 200);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (node_source_type_dialog), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (node_source_type_dialog), GDK_WINDOW_TYPE_HINT_DIALOG);

  dialog_vbox1 = GTK_DIALOG (node_source_type_dialog)->vbox;
  gtk_widget_show (dialog_vbox1);
  gtk_widget_set_size_request (dialog_vbox1, 450, -1);

  vbox1 = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 6);

  label2 = gtk_label_new (_("Select the source type you want to add..."));
  gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (vbox1), label2, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (label2), 0, 0.5);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow1);
  gtk_box_pack_start (GTK_BOX (vbox1), scrolledwindow1, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_SHADOW_IN);

  type_list = gtk_tree_view_new ();
  gtk_widget_show (type_list);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), type_list);
  gtk_widget_set_size_request (type_list, 400, -1);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (type_list), FALSE);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (type_list), TRUE);

  dialog_action_area1 = GTK_DIALOG (node_source_type_dialog)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);

  cancelbutton1 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (node_source_type_dialog), cancelbutton1, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton1, GTK_CAN_DEFAULT);

  ok_button = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (ok_button);
  gtk_dialog_add_action_widget (GTK_DIALOG (node_source_type_dialog), ok_button, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (ok_button, GTK_CAN_DEFAULT);

  g_signal_connect_swapped ((gpointer) cancelbutton1, "clicked",
                            G_CALLBACK (gtk_widget_destroy),
                            GTK_OBJECT (node_source_type_dialog));

  /* Store pointers to all widgets, for use by lookup_widget(). */
  GLADE_HOOKUP_OBJECT_NO_REF (node_source_type_dialog, node_source_type_dialog, "node_source_type_dialog");
  GLADE_HOOKUP_OBJECT_NO_REF (node_source_type_dialog, dialog_vbox1, "dialog_vbox1");
  GLADE_HOOKUP_OBJECT (node_source_type_dialog, vbox1, "vbox1");
  GLADE_HOOKUP_OBJECT (node_source_type_dialog, label2, "label2");
  GLADE_HOOKUP_OBJECT (node_source_type_dialog, scrolledwindow1, "scrolledwindow1");
  GLADE_HOOKUP_OBJECT (node_source_type_dialog, type_list, "type_list");
  GLADE_HOOKUP_OBJECT_NO_REF (node_source_type_dialog, dialog_action_area1, "dialog_action_area1");
  GLADE_HOOKUP_OBJECT (node_source_type_dialog, cancelbutton1, "cancelbutton1");
  GLADE_HOOKUP_OBJECT (node_source_type_dialog, ok_button, "ok_button");

  return node_source_type_dialog;
}

