/*
   CDF channel parsing
      
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

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

#ifndef _CDF_CHANNEL_H
#define _CDF_CHANNEL_H

#include "feed.h"

#define CDF_CHANNEL_TITLE		0
#define CDF_CHANNEL_DESCRIPTION		1
#define CDF_CHANNEL_IMAGE		2
#define CDF_CHANNEL_COPYRIGHT		3
#define CDF_CHANNEL_PUBDATE		4
#define CDF_CHANNEL_WEBMASTER		5
#define CDF_CHANNEL_CATEGORY		6

#define CDF_CHANNEL_MAX_TAG		7

typedef struct CDFChannel {
	/* standard namespace infos */
	gchar		*tags[CDF_CHANNEL_MAX_TAG];

	GHashTable	*nsinfos;	/* list to store pointers to namespace
					   specific informations */
	
	/* other information */
	time_t		time;		/* last feed build/creation time */	
} *CDFChannelPtr;

feedHandlerPtr initCDFFeedHandler(void);

#endif
