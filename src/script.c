/**
 * @file script.c generic scripting support implementation
 *
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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
#  include <config.h>
#endif

#include <glib.h>
#include "common.h"
#include "debug.h"
#include "script.h"
#include "xml.h"

/** hash of the scripts registered for different hooks */
static GHashTable *scripts = NULL;
static gboolean scriptConfigLoading = FALSE;
static scriptSupportImplPtr scriptImpl = NULL;

gboolean script_support_enabled(void) { return (scriptImpl != NULL); }

static gchar *script_get_name_for_hook (hookType hook)
{
	switch (hook)
	{
		case SCRIPT_HOOK_INVALID:
			return NULL;
			break;

		case SCRIPT_HOOK_STARTUP:
			return g_strdup("startup");
			break;

		case SCRIPT_HOOK_FEED_UPDATED:
			return g_strdup("feed_updated");
			break;

		case SCRIPT_HOOK_ITEM_SELECTED:
			return g_strdup("item_selected");
			break;

		case SCRIPT_HOOK_FEED_SELECTED:
			return g_strdup("feed_selcted");
			break;

		case SCRIPT_HOOK_ITEM_UNSELECT:
			return g_strdup("item_unselect");
			break;

		case SCRIPT_HOOK_FEED_UNSELECT:
			return g_strdup("feed_unselect");
			break;

		case SCRIPT_HOOK_SHUTDOWN:
			return g_strdup("shutdown");
			break;

		case SCRIPT_HOOK_NEW_SUBSCRIPTION:
			return g_strdup("feed_added");
			break;
	}

	return NULL;
}

static void script_config_load_hook_script(xmlNodePtr match, gpointer user_data) {
	gint	type = GPOINTER_TO_INT(user_data);
	gchar	*name;
	
	name = xmlNodeListGetString(match->doc, match->xmlChildrenNode, 1);
	if(name) {
		script_hook_add(type, name);
		g_free(name);
	}
}

static void
script_config_load_hook (xmlNodePtr match, gpointer user_data)
{
	gint	type;
	gchar	*tmp;
	
	tmp = xmlGetProp (match, BAD_CAST"type");
	type = atoi (tmp);
	g_free (tmp);
	if (!type)
		return;
	xpath_foreach_match (match, "script", script_config_load_hook_script, GINT_TO_POINTER (type));
}

static void
script_config_load (void)
{
	xmlDocPtr	doc;
	gchar		*filename, *data = NULL;
	gsize		len;

	scriptConfigLoading = TRUE;
	
	filename = common_create_cache_filename (NULL, "scripts", "xml");	
	if (g_file_get_contents (filename, &data, &len, NULL)) {
		doc = xml_parse (data, len, NULL);
		if (doc) {
			xpath_foreach_match (xmlDocGetRootElement (doc),
			                     "/scripts/hook",
			                     script_config_load_hook,
			                     NULL);
			xmlFreeDoc (doc);
		}
	}

	g_free (data);
	g_free (filename);
	scriptConfigLoading = FALSE;
}

static void script_config_save_hook(gpointer key, gpointer value, gpointer user_data) {
	xmlNodePtr	hookNode, rootNode = (xmlNodePtr)user_data;
	GSList		*list = (GSList *)value;
	gchar		*tmp;
	
	tmp = g_strdup_printf("%d", GPOINTER_TO_INT(key));
	hookNode = xmlNewChild(rootNode, NULL, "hook", NULL);
	xmlNewProp(hookNode, BAD_CAST"type", BAD_CAST tmp);
	g_free(tmp);
	 
	while(list) {
		xmlNewTextChild(hookNode, NULL, "script", (gchar *)list->data);
		list = g_slist_next(list);
	}
}

static void script_config_save(void) {
	xmlDocPtr	doc = NULL;
	xmlNodePtr	rootNode;
	gchar		*filename;
	
	if(scriptConfigLoading)
		return;
	
	doc = xmlNewDoc("1.0");
	rootNode = xmlDocGetRootElement(doc);
	rootNode = xmlNewDocNode(doc, NULL, "scripts", NULL);
	xmlDocSetRootElement(doc, rootNode);
	
	g_hash_table_foreach(scripts, script_config_save_hook, (gpointer)rootNode);

	filename = common_create_cache_filename(NULL, "scripts", "xml");
	if(-1 == xmlSaveFormatFileEnc(filename, doc, NULL, 1))
		g_warning("Could save script config %s!", filename);
		
	xmlFreeDoc(doc);
	g_free(filename);
}

void script_init(void) {
	scripts = g_hash_table_new(g_direct_hash, g_direct_equal);
	script_config_load();
}

void script_add_impl(struct scriptSupportImpl *impl) {

	scriptImpl = impl;
	scriptImpl->init();
}

void script_run_cmd(const gchar *cmd) {

	if(scriptImpl)
		scriptImpl->run_cmd(cmd);
}

void script_run(const gchar *name, hookType hook) {
	
	if(scriptImpl) {
		gchar	*filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "scripts", name, "lua");
		gchar	*hook_name = script_get_name_for_hook(hook);
		scriptImpl->run_script(filename, hook_name);
		g_free(filename);
		g_free(hook_name);
	}
}

void script_run_for_hook(hookType type) {
	GSList	*hook;

	hook = g_hash_table_lookup(scripts, GINT_TO_POINTER(type));
	while(hook) {
		script_run((gchar *)hook->data, type);
		hook = g_slist_next(hook);
	}
}

void script_hook_add(hookType type, const gchar *scriptname) {
	GSList	*hook;

	hook = g_hash_table_lookup(scripts, GINT_TO_POINTER(type));
	hook = g_slist_append(hook, g_strdup(scriptname));
	g_hash_table_insert(scripts, GINT_TO_POINTER(type), hook);
	script_config_save();
}

void script_hook_remove(hookType type, gchar *scriptname) {
	GSList	*hook;

	hook = g_hash_table_lookup(scripts, GINT_TO_POINTER(type));
	hook = g_slist_remove(hook, scriptname);
	g_hash_table_insert(scripts, GINT_TO_POINTER(type), hook);
	script_config_save();
	g_free(scriptname);
}

GSList *script_hook_get_list(hookType type) {

	return g_hash_table_lookup(scripts, GINT_TO_POINTER(type));
}

static void
script_hook_free (gpointer key, gpointer value, gpointer user_data)
{
	GSList	*iter, *list;
	
	iter = list = (GSList *) value;
	while (iter) {
		g_free (iter->data);	/* free script name */
		iter = g_slist_next (iter);
	}	
	g_slist_free (list);
}

void
script_deinit (void)
{
	if (scriptImpl)
		scriptImpl->deinit ();

	if (scripts) {
		g_hash_table_foreach (scripts, script_hook_free, NULL);
		// FIXME: foreach hook free script list
		g_hash_table_destroy (scripts);
		scripts = NULL;
	}
}
