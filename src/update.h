/*
   auto update functionality
     
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _UPDATE_H
#define _UPDATE_H

#include <glib.h>
#include "feed.h"

/* the feed request structure is used in two places, on the one
   hand to put update requests into the request and result queue 
   between GUI and update thread and on the other hand to 
   persistently store HTTP status information written by
   the SnowNews netio.c code which is displayed in the GUI. */
struct feed_request {

        char * 	feedurl;		/* Non hashified URL set from the requestion function */
        char * 	lastmodified; 		/* Content of header as sent by the server. */
	int 	lasthttpstatus;	
	int 	problem;		/* Set if there was a problem downloading the feed. */
	char *	data;			/* newly downloaded feed data to be parsed */

	feedPtr	fp;			/* pointer to feed structure which is to be updated */
};

gpointer getNewRequestStruct(feedPtr fp);
void freeRequest(gpointer request);

GThread * initUpdateThread(void);
GThread * initAutoUpdateThread(void);
void requestUpdate(feedPtr fp);
void updateAllFeeds(void);

#endif
