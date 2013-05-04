/*
 * plugins_engine.c: Liferea Plugins using libpeas
 * (derived from gtranslator code)
 *
 * Copyright (C) 2002-2005 Paolo Maggi 
 * Copyright (C) 2010 Steve Frécinaux
 * Copyright (C) 2012 Lars Windolf <lars.lindner@gmail.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <glib.h>
#include <gio/gio.h>
#include <girepository.h>

#include "plugins_engine.h"

G_DEFINE_TYPE (LifereaPluginsEngine, liferea_plugins_engine, PEAS_TYPE_ENGINE)

struct _LifereaPluginsEnginePrivate
{
  GSettings *plugin_settings;
};

LifereaPluginsEngine *default_engine = NULL;

static void
liferea_plugins_engine_init (LifereaPluginsEngine * engine)
{
  gchar *typelib_dir;
  GError *error = NULL;

  engine->priv = G_TYPE_INSTANCE_GET_PRIVATE (engine,
                                              LIFEREA_TYPE_PLUGINS_ENGINE,
                                              LifereaPluginsEnginePrivate);

  peas_engine_enable_loader (PEAS_ENGINE (engine), "python3");

  engine->priv->plugin_settings = g_settings_new ("net.sf.liferea.plugins");

  /* Require Lifereas's typelib. */
  typelib_dir = g_build_filename (PACKAGE_LIB_DIR,
                                  "girepository-1.0", NULL);

  if (!g_irepository_require_private (g_irepository_get_default (),
	  typelib_dir, "Liferea", "3.0", 0, &error))
    {
      g_warning ("Could not load Liferea repository: %s", error->message);
      g_error_free (error);
      error = NULL;
    }

  g_free (typelib_dir);

  /* This should be moved to libpeas */
  if (!g_irepository_require (g_irepository_get_default (),
                              "Peas", "1.0", 0, &error))
    {
      g_warning ("Could not load Peas repository: %s", error->message);
      g_error_free (error);
      error = NULL;
    }

  if (!g_irepository_require (g_irepository_get_default (),
                              "PeasGtk", "1.0", 0, &error))
    {
      g_warning ("Could not load PeasGtk repository: %s", error->message);
      g_error_free (error);
      error = NULL;
    }

  peas_engine_add_search_path (PEAS_ENGINE (engine),
                               g_build_filename (g_get_user_data_dir (), "liferea", "plugins", NULL),
                               g_build_filename (g_get_user_data_dir (), "liferea", "plugins", NULL));

  peas_engine_add_search_path (PEAS_ENGINE (engine),
                               g_build_filename (PACKAGE_LIB_DIR,  "plugins", NULL),
                               g_build_filename (PACKAGE_DATA_DIR, "plugins", NULL));

  g_settings_bind (engine->priv->plugin_settings,
                   "active-plugins",
                   engine, "loaded-plugins", G_SETTINGS_BIND_DEFAULT);
}

static void
liferea_plugins_engine_dispose (GObject * object)
{
	LifereaPluginsEngine *engine = LIFEREA_PLUGINS_ENGINE (object);

	if (engine->priv->plugin_settings) {
		g_object_unref (engine->priv->plugin_settings);
		engine->priv->plugin_settings = NULL;
	}

	G_OBJECT_CLASS (liferea_plugins_engine_parent_class)->dispose (object);
}

static void
liferea_plugins_engine_class_init (LifereaPluginsEngineClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = liferea_plugins_engine_dispose;

	g_type_class_add_private (klass, sizeof (LifereaPluginsEnginePrivate));
}

LifereaPluginsEngine *
liferea_plugins_engine_get_default (void)
{
	if (!default_engine) {
		default_engine = LIFEREA_PLUGINS_ENGINE (g_object_new (LIFEREA_TYPE_PLUGINS_ENGINE, NULL));
		g_object_add_weak_pointer (G_OBJECT (default_engine), (gpointer) &default_engine);
	}

	return default_engine;
}
