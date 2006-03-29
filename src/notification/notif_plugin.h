/**
 * @file notif_plugin.h generic notification interface
 * 
 * Copyright (C) 2006 Norman Jonas <liferea.sf.net@devport.codepilot.net>
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _NOTIF_PLUGIN_H
#define _NOTIF_PLUGIN_H

#include <glib.h>
#include <gmodule.h>
#include "node.h"
#include "item.h"

#define NOTIFICATION_PLUGIN_API_VERSION 1

typedef struct notificationPlugin {
	unsigned int	api_version;
	
	/**
	 * Called once during plugin initialization.
	 * If the plugin returns FALSE it won't be
	 * added to the list of the available 
	 * notification plugins.
	 */
	gboolean (*plugin_init)(void);
	
	/**
	 * Called upon program shutdown.
	 */
	void	(*plugin_deinit)(void);
	
	/**
	 * Called after startup when notifications are
	 * enabled and when displaying notifications should
	 * begin or after notifications are reenabled from
	 * the program preferences.
	 */
	void	(*notification_enable)(void);
	
	/**
	 * Called after notifications were disabled in the
	 * program preferences. The plugin should not display
	 * further notifications and may destroy currently
	 * displayed ones.
	 */
	void	(*notification_disable)(void);
	
	/**
	 * This callback notifies the plugin that the given
	 * node was updated and contains new items (items
	 * with newStatus set to TRUE.
	 */
	void 	(*node_has_new_items)(nodePtr node);
	
	/**
	 * This callback notifies the plugin that the given
	 * node was removed and should not be listed in any
	 * notification anymore.
	 */
	void	(*node_removed)(nodePtr node);
	
	// void 	(*new_item_downloaded)(itemPtr item);
} *notificationPluginPtr;

/** Notification plugins are to be declared with this macro. */
#define DECLARE_NOTIFICATION_PLUGIN(notificationPlugin) \
        G_MODULE_EXPORT notificationPluginPtr notification_plugin_get_info() { \
                return &notificationPlugin; \
        }

/**
 * Plugin event wrapper. Forwards event to all 
 * active notification plugins.
 *
 * @param node	the node that has new items
 */
void notification_node_has_new_items(nodePtr node);

#endif
