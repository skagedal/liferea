/*
 * HTTP feed download, Liferea reuses the only slightly
 * adapted Snownews code...
 * 
 * Snownews - A lightweight console RSS newsreader
 * 
 * Copyright 2003 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * netio.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* OS X needs this, otherwise socklen_t is not defined. */
#ifdef __APPLE__
#       define _BSD_SOCKLEN_T_
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>

#include "conversions.h"

#ifdef SUN
#	include "os-support.h"
#endif

/* some Liferea specific includes */
#include "callbacks.h"
#include "support.h"
#include "netio.h"

#define MAX_HTTP_REDIRECTS 10		/* Maximum number of redirects we will follow. */

extern char *proxyname;			/* Hostname of proxyserver. */
extern unsigned short proxyport;	/* Port on proxyserver to use. */
extern char *useragent;
int connectresult;

/* Connect network sockets.
 *
 * Return codes:	 1	socket creation error
 *			 2	hostname resolve error
 *			 3	couldn't connect
 *                 	-1	aborted by user
 */
int NetConnect (int * my_socket, char * host, int httpproto, int suppressoutput) {
	int retval;
	struct sockaddr_in address;	
	struct hostent *remotehost;
	char *uistring;
	int uistringlength;
	socklen_t len;
	char *realhost;
	unsigned short port;
	
	realhost = strdup(host);
	if (sscanf (host, "%[^:]:%hd", realhost, &port) != 2) {
		port = 80;
	}

	uistringlength = strlen(_("Connecting to ")) + strlen(host) + 4;
	uistring = malloc (uistringlength);
	snprintf (uistring, uistringlength, _("Connecting to %s..."), host);
	if (!suppressoutput)
		UIStatus (uistring, 0);
	free (uistring);
	
	/* Create a inet stream TCP socket. */
	*my_socket = socket (AF_INET, SOCK_STREAM, 0);
	if (*my_socket == -1) {
		return 1;
	}
	
	/* If proxyport is 0 we didn't execute the if http_proxy statement in main
	   so there is no proxy. On any other value of proxyport do proxyrequests instead. */
	if (proxyport == 0) {
		/* Lookup remote IP. */
		remotehost = gethostbyname (realhost);
		if (remotehost == NULL) {
			close (*my_socket);
			free (realhost);
			return 2;
		}
		
		/* Set the remote address. */
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		memcpy (&address.sin_addr.s_addr, remotehost->h_addr_list[0], remotehost->h_length);
			
		/* Connect socket. */
		connectresult = connect (*my_socket, (struct sockaddr *) &address, sizeof(address));
		
		/* Check if we're already connected.
		   BSDs will return 0 on connect even in nonblock if connect was fast enough. */
		if (connectresult != 0) {
			/* If errno is not EINPROGRESS, the connect went wrong. */
			if (errno != EINPROGRESS) {
				close (*my_socket);
				free (realhost);
				return 3;
			}
			
			/* We get errno of connect back via getsockopt SO_ERROR (into connectresult). */
			len = sizeof(connectresult);
			getsockopt(*my_socket, SOL_SOCKET, SO_ERROR, &connectresult, &len);
			
			if (connectresult != 0) {
				close (*my_socket);
				free (realhost);
				return 3;
			}
		}
	} else {
		/* Lookup proxyserver IP. */
		remotehost = gethostbyname (proxyname);
		if (remotehost == NULL) {
			close (*my_socket);
			free (realhost);
			return 2;
		}
		
		/* Set the remote address. */
		address.sin_family = AF_INET;
		address.sin_port = htons(proxyport);
		memcpy (&address.sin_addr.s_addr, remotehost->h_addr_list[0], remotehost->h_length);
		
		/* Connect socket. */
		connectresult = connect (*my_socket, (struct sockaddr *) &address, sizeof(address));
		
		/* Check if we're already connected.
		   BSDs will return 0 on connect even in nonblock if connect was fast enough. */
		if (connectresult != 0) {
			if (errno != EINPROGRESS) {
				close (*my_socket);
				free (realhost);
				return 3;
			}
			
			len = sizeof(connectresult);
			getsockopt(*my_socket, SOL_SOCKET, SO_ERROR, &connectresult, &len);
			
			if (connectresult != 0) {
				close (*my_socket);
				free (realhost);
				return 3;
			}
		}
	}
	
	free (realhost);
	return 0;
}


/*
 * Main network function.
 * (Now with a useful function description *g*)
 *
 * This function returns the HTTP request's body (deflating gzip encoded data
 * if needed).
 * Updates passed feed struct with values gathered from webserver.
 * Handles all redirection and HTTP status decoding.
 * Returns NULL pointer if no data was received. Check httpstatus == 304,
 * otherwise an error occured.
 */
char * NetIO (int * my_socket, char * host, char * url, struct feed_request * cur_ptr, int httpproto, char * authinfo, int suppressoutput) {
	char netbuf[4096];			/* Network read buffer. */
	char *body;					/* XML body. */
	int length;
	FILE *stream;				/* Stream socket. */
	int chunked = 0;			/* Content-Encoding: chunked received? */
	int redirectcount;			/* Number of HTTP redirects followed. */
	char httpstatus[4];			/* HTTP status sent by server. */
	char tmp[1024];
	char *tmpstatus;
	char *savestart;			/* Save start position of pointers. */
	char *tmphost;				/* Pointers needed to strsep operation. */
	char *newhost;				/* New hostname if we need to redirect. */
	char *newurl;				/* New document name ". */
	char *newlocation;
	char *tmpstring;			/* Temp pointers. */
	char *freeme;
	char *redirecttarget;
	int retval;
	int handled;
	int tmphttpstatus;
	int inflate = 0;			/* Whether feed data needs decompressed with deflate(1), gzip(2). */
	int contentlength = 0;		/* Content-Length of server reply. */
	int len;
	char * inflatedbody;
	int quirksmode = 0;
	
	snprintf (tmp, sizeof(tmp), _("Downloading http://%s%s..."), host, url);
	if (!suppressoutput)
		UIStatus (tmp, 0);

	redirectcount = 0;
	
	/* Goto label to redirect reconnect. */
	tryagain:
	
	/* Open socket. */	
	stream = fdopen (*my_socket, "r+");
	if (stream == NULL) {
		/* This is a serious non-continueable OS error as it will probably not
		   go away if we retry.
		   
		   BeOS will stupidly return SUCCESS here making this code silently fail on BeOS. */
		MainQuit (_("socket connect"), strerror(errno));
		return NULL;
	}
	
	/* Again is proxyport == 0, non proxy mode, otherwise make proxy requests. */
	if (proxyport == 0) {
		/* Request URL from HTTP server. */
		if (cur_ptr->lastmodified != NULL)
			fprintf(stream, "GET %s HTTP/1.0\r\nAccept-Encoding: gzip\r\nUser-Agent: %s\r\nConnection: close\r\nHost: %s\r\nIf-Modified-Since: %s\r\n%s\r\n", url, useragent, host, cur_ptr->lastmodified, (authinfo ? authinfo : ""));
		else
			fprintf(stream, "GET %s HTTP/1.0\r\nAccept-Encoding: gzip\r\nUser-Agent: %s\r\nConnection: close\r\nHost: %s\r\n%s\r\n", url, useragent, host, (authinfo ? authinfo : ""));

		fflush(stream);
	} else {
		/* Request URL from HTTP server. */
		if (cur_ptr->lastmodified != NULL)
			fprintf(stream, "GET http://%s%s HTTP/1.0\r\nAccept-Encoding: gzip\r\nUser-Agent: %s\r\nConnection: close\r\nIf-Modified-Since: %s\r\n\r\n", host, url, useragent, cur_ptr->lastmodified);
		else
			fprintf(stream, "GET http://%s%s HTTP/1.0\r\nAccept-Encoding: gzip\r\nUser-Agent: %s\r\nConnection: close\r\n\r\n", host, url, useragent);
		
		fflush(stream);
	}
	
	if ((fgets (tmp, sizeof(tmp), stream)) == NULL) {
		fclose (stream);
		return NULL;
	}
	tmpstatus = malloc (strlen(tmp)+1);
	strcpy (tmpstatus, tmp);
	savestart = tmpstatus;

	memset (httpstatus, 0, 4);	/* Nullify string so valgrind shuts up. */
	/* Set pointer to char after first space.
	   HTTP/1.0 200 OK
	            ^
	   Copy three bytes into httpstatus. */
	strsep (&tmpstatus, " ");
	strncpy (httpstatus, tmpstatus, 3);
	free (savestart);
	
	cur_ptr->lasthttpstatus = atoi (httpstatus);
	
	/* If the redirectloop was run newhost and newurl were allocated.
	   We need to free them here. */
	if (redirectcount > 0) {
		free (host);
		free (url);
	}

	tmphttpstatus = cur_ptr->lasthttpstatus;
	handled = 1;
	/* Check HTTP server response and handle redirects. */
	do {
		switch (tmphttpstatus) {
			case 200:	/* OK */
				/* Received good status from server, clear problem field. */
				cur_ptr->problem = 0;
				break;
			case 300:	/* Multiple choice and everything 300 not handled is fatal. */
				cur_ptr->problem = 1;
				fclose (stream);
				return NULL;
			case 301:
				/* Permanent redirect. Change feed->feedurl to new location.
				   Done some way down when we have extracted the new url. */
			case 302:	/* Found */
			case 303:	/* See Other */
			case 307:	/* Temp redirect. This is HTTP/1.1 */
				redirectcount++;
			
				/* Give up if we reach MAX_HTTP_REDIRECTS to avoid loops. */
				if (redirectcount > MAX_HTTP_REDIRECTS) {
					fclose (stream);
					if (!suppressoutput)
						UIStatus (_("Too many HTTP redirects encountered! Giving up."), 2);
					return NULL;
				}
				
				while (!feof(stream)) {
					if ((fgets (netbuf, sizeof(netbuf), stream)) == NULL) {
						/* Something bad happened. Server sent stupid stuff. */
						MainQuit (_("Following HTTP redirects"), strerror(errno));
					}
					/* Split netbuf into hostname and trailing url.
					   Place hostname in *newhost and tail into *newurl.
					   Close old connection and reconnect to server.
					   
					   Do not touch any of the following code! :P */
					if (strncasecmp (netbuf, "Location", 8) == 0) {
						//tmpstring = malloc(strlen(netbuf)+1);
						
						redirecttarget = strdup (netbuf);
						freeme = redirecttarget;
						
						/* Remove trailing \r\n from line. */
						redirecttarget[strlen(redirecttarget)-2] = 0;
						
						/* In theory pointer should now be after the space char
						   after the word "Location:" */
						strsep (&redirecttarget, " ");
						
						/* Location must start with "http", otherwise switch on quirksmode. */
						if (strncmp(redirecttarget, "http", 4) != 0)
							quirksmode = 1;
						
						/* If the Location header is invalid we need to construct
						   a correct one here before proceeding with the program.
						   This makes headers like
						   "Location: fuck-the-protocol.rdf" work.
						   In violalation of RFC1945, RFC2616. */
						if (quirksmode) {
							len = 7 + strlen(host) + strlen(redirecttarget) + 3;
							newlocation = malloc(len);
							memset (newlocation, 0, len);
							strcat (newlocation, "http://");
							strcat (newlocation, host);
							if (redirecttarget[0] != '/')
								strcat (newlocation, "/");
							strcat (newlocation, redirecttarget);
						} else
							newlocation = strdup (redirecttarget);
						
						/* This also frees redirecttarget. */
						free (freeme);
						
						/* Change cur_ptr->feedurl on 301. */
						if (cur_ptr->lasthttpstatus == 301) {
							if (!suppressoutput)
								UIStatus (_("URL points to permanent redirect, updating with new location..."), 2);
							free (cur_ptr->feedurl);
							cur_ptr->feedurl = strdup (newlocation);
						}
						
						freeme = newlocation;
						strsep (&newlocation, "/");
						strsep (&newlocation, "/");
						tmphost = newlocation;
						/* The following line \0-terminates tmphost in overwriting the first
						   / after the hostname. */
						strsep (&newlocation, "/");
						
						/* newlocation must now be the absolute path on newhost.
						   If not we've been redirected to somewhere totally stupid
						   (oh yeah, no offsite linking, go to our fucking front page).
						   Say goodbye to the webserver in this case. In fact, we don't
						   even say goodbye, but just drop the connection. */
						if (newlocation == NULL) {
							if (!suppressoutput)
								UIStatus (_("Webserver sent an invalid redirect!"), 2);
							fclose (stream);
							return NULL;
						}
						
						newhost = strdup (tmphost);
						newlocation--;
						newlocation[0] = '/';
						newurl = strdup (newlocation);
					
						free (freeme);
					
						/* Close connection. */	
						fclose (stream);
					
						/* Reconnect to server. */
						if ((NetConnect (my_socket, newhost, httpproto, suppressoutput)) != 0) {
							/* Add error handling/reporting. */
							return NULL;
						}
					
						host = newhost;
						url = newurl;
					
						goto tryagain;
					}
				}
				break;
			case 304:
				/* Not modified received. We can close stream and return from here.
				   Not very friendly though. :) */
				fclose (stream);
				/* Received good status from server, clear problem field. */
				cur_ptr->problem = 0;
				return NULL;
			case 410: /* The feed is gone. Politely remind the user to unsubscribe. */
				snprintf (tmp, sizeof(tmp), _("The feed no longer exists. Please unsubscribe!"));
				if (!suppressoutput)
					UIStatus (tmp, 3);
			case 400:
				cur_ptr->problem = 1;
				fclose (stream);
				return NULL;
			default:
				/* unknown error codes have to be treated like the base class */
				if (handled) {
					/* first pass, modify error code to base class */
					handled = 0;
					tmphttpstatus -= tmphttpstatus % 100;
				} else {
					/* second pass, give up on unknown error base class */
					cur_ptr->problem = 1;
					fclose (stream);
					return NULL;
				}
		}
	} while(!handled);
	
	/* Read rest of HTTP header and parse what we need. */
	while (!feof(stream)) {
	
		/* Max line length of sizeof(netbuf) is assumed here.
		   If header has longer lines than 4096 bytes something may go wrong. :) */
		if ((fgets (netbuf, sizeof(netbuf), stream)) == NULL)
			break;
		if (strstr (netbuf, "chunked") != NULL) {
			/* Server sent junked encoding. Until I understand how it works
			   and snownews uses HTTP/1.1 we must reject this answer.
			   Chunked encoding is not defined in HTTP/1.0. */
			chunked = 1;
			snprintf (tmp, sizeof(tmp), _("The webserver %s sent illegal HTTP/1.0 reply! I cannot parse this."), host);
			if (!suppressoutput)
				UIStatus (tmp, 2);
			cur_ptr->problem = 1;
			fclose (stream);
			return NULL;
		}
		/* Get last modified date. This is only relevant on HTTP 200. */
		if ((strncasecmp (netbuf, "Last-Modified", 13) == 0) &&
			(cur_ptr->lasthttpstatus == 200)) {
			tmpstring = strdup(netbuf);
			freeme = tmpstring;
			strsep (&tmpstring, ":");
			tmpstring++;
			if (cur_ptr->lastmodified != NULL)
				free(cur_ptr->lastmodified);
			cur_ptr->lastmodified = malloc (strlen(tmpstring)+1);
			strncpy (cur_ptr->lastmodified, tmpstring, strlen(tmpstring)+1);
			if (cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] == '\n')
				cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] = '\0';
			if (cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] == '\r')
				cur_ptr->lastmodified[strlen(cur_ptr->lastmodified)-1] = '\0';
			free(freeme);
		}
		/* Check and parse Content-Encoding header. */
		if (strncasecmp (netbuf, "Content-Encoding", 16) == 0) {
			/*if (strstr (netbuf, "deflate") != NULL)
				inflate = 1;*/
			/* Will also catch x-gzip. */
			if (strstr (netbuf, "gzip") != NULL)
				inflate = 2;
		}
		/* Check and parse Content-Length header (needed for zlib). */
		if (strncasecmp (netbuf, "Content-Length", 14) == 0) {
	 		tmpstring = strdup (netbuf);
			freeme = tmpstring;
			/* Cut string at ":" sign */
			strsep (&tmpstring, ":");
			/* Pointer points to space after ":", advance one char */
			tmpstring++;
			contentlength = atoi (tmpstring);
			free (freeme);
	 	}
		
		/* HTTP RFC 2616, Section 19.3 Tolerant Applications.
		   Accept CRLF and LF line ends in the header field. */
		if ((strcmp(netbuf, "\r\n") == 0) || (strcmp(netbuf, "\n") == 0))
			break;
	}
	
	/**********************
	 * End of HTTP header *
	 **********************/
	
	/* Init pointer so strncat works.
	   Workaround class hack. */
	body = malloc(1);
	body[0] = '\0';
	
	length = 0;

	/* Read stream until EOF and return it to parent. */
	while (!feof(stream)) {

		/* Since we handle binary data if we read compressed input we
		   need to use fread instead of fgets after reading the header. */ 
		retval = fread (netbuf, 1, sizeof(netbuf), stream);
		if (retval == 0)
			break;
		body = realloc (body, length+retval);
		memcpy (body+length, netbuf, retval);
		length += retval;
		if (retval != 4096)
			break;
	}
	body = realloc(body, length+1);
	body[length] = '\0';
	
	/* Close connection. */
	fclose (stream);
	
	/* If inflate==1 we need to decompress content with deflate.
	   Probably not needed since every webserver seems to send gzip. */
	if (inflate == 1) {
		inflatedbody = zlib_uncompress (body, contentlength, NULL, 0);
		
		if (inflatedbody == NULL) {
			free (body);
			return NULL;
		}
		
		/* Copy uncompressed data back to body. */
		free (body);
		body = strdup (inflatedbody);
		free (inflatedbody);
	} else if (inflate == 2) {
		/* gzipinflate */
		inflatedbody = gzip_uncompress (body, contentlength, NULL);
		
		if (inflatedbody == NULL) {
			free (body);
			return NULL;
		}
		
		/* Copy uncompressed data back to body. */
		free (body);
		body = strdup (inflatedbody);
		free (inflatedbody);
	}
		
	return body;
}

/* Returns allocated string with body of webserver reply.
   Various status info put into struct feed * cur_ptr.
   Set suppressoutput=1 to disable ncurses calls. */
char * DownloadFeed (char * url, struct feed_request * cur_ptr, int suppressoutput) {
	int my_socket = 0;
	int result;
	int len;
	char *host;					/* Needs to freed. */
	char *tmphost;
	char *freeme;
	char *returndata;
	char *authinfo = NULL;		/* base64 encoded HTTP auth. Default to NULL! */
	char *authstring;			/* "username:password" - to be base64 encoded */
	char *username, *password;	/* plaintext username/password for HTTP auth */
	char *tmpstr;
	char tmp[1024];
	int httpproto = 0;			/* 0: http; 1: https */
	
	/* strstr will match _any_ substring. Not good, use strncasecmp with length 5! */
	if (strncasecmp (url, "https", 5) == 0)
		httpproto = 1;
	else
		httpproto = 0;
	
	/* hahteeteepeh://foo.bar/jaaaaaguar.html.
	                00^      0^
	
	   Warning!
	   This "function" will not handle the following URLs:
	   http://foo.bar/
	   
	   Must add index.rdf or whatever. */
	strsep (&url, "/");
	strsep (&url, "/");
	tmphost = url;
	strsep (&url, "/");
	if (url == NULL) {
		/* End of string url. Probably entered invalid stuff like
		   http://asdf or something. Since we don't want to crash and burn
		   return the dreaded NULL pointer. */
		if (!suppressoutput)
			UIStatus (_("This URL doesn't look too valid to me, don't you think?"), 2);
		return NULL;
	}
		
	/* If tmphost contains an '@' we have to do HTTP authentication. */
	if (strchr (tmphost, '@') != NULL) {
		tmpstr = tmphost;
		strsep (&tmphost, ":");
		username = strdup (tmpstr);
		tmpstr = tmphost;
		strsep (&tmphost, "@");
		password = strdup (tmpstr);
		
		/* Create base64 authinfo.
		
		   RFC 2617. Basic HTTP authentication.
		   Authorization: Basic username:password[base64 encoded] */
		
		/* Construct the cleartext authstring. */
		len = strlen(username) + 1 + strlen(password) + 1;
		authstring = malloc (len);
		strcpy (authstring, username);
		strcat (authstring, ":");
		strcat (authstring, password);
		
		tmpstr = base64encode (authstring, len);
		
		/* "Authorization: Basic " + base64str + \r\n\0 */
		len = 21 + strlen(tmpstr) + 3;
		authinfo = malloc (len);
		strcpy (authinfo, "Authorization: Basic ");
		strcat (authinfo, tmpstr);
		strcat (authinfo, "\r\n");
		
		free (username);
		free (password);
		free (tmpstr);
		free (authstring);
	}
	
	host = strdup (tmphost);
	
	/* netio() might change pointer of host to something else if redirect
	   loop is executed. Make a copy so we can correctly free everything. */
	freeme = host;
	url--;
	url[0] = '/';
	
	if (url[strlen(url)-1] == '\n') {
		url[strlen(url)-1] = '\0';
	}
	
	result = NetConnect (&my_socket, host, httpproto, suppressoutput);
	
	switch (result) {
		case 1:
			if (!suppressoutput)
				UIStatus (_("Couldn't create network socket!"), 2);
			cur_ptr->problem = 1;
			free (freeme);
			return NULL;
		case 2:
			snprintf (tmp, sizeof(tmp), _("Can't resolve host %s!"), host);
			if (!suppressoutput)
				UIStatus (tmp, 2);
			cur_ptr->problem = 1;
			free (freeme);
			return NULL;
		case 3:
			snprintf (tmp, sizeof(tmp), _("Could not connect to server %s: %s"), host, strerror(connectresult));
			if (!suppressoutput)
				UIStatus (tmp, 2);
			cur_ptr->problem = 1;
			free (freeme);
			return NULL;
		case -1:
			if (!suppressoutput)
				UIStatus (_("Aborted."), 2);
			free (freeme);
			return NULL;
		default:
			break;
	}
	
	returndata = NetIO (&my_socket, host, url, cur_ptr, httpproto, authinfo, suppressoutput);
	
	/* url will be freed in the calling function. */
	free (freeme);		/* This is *host. */
	
	if (authinfo != NULL)
		free (authinfo);
	
	return returndata;
}

/*-----------------------------------------------------------------------*/
/* some Liferea specific code...					 */

/* Downloads a feed and returns the data or NULL as return value.
   The url of the has to be passed in the feed_request structure.
   If the the webserver reports a permanent redirection, the
   feed url will be modified and the old URL 'll be freed. The
   request structure 'll also contain the HTTP status and the
   last modified string.
 */
char * downloadURL(struct feed_request *request) {
	FILE		*f;
	gchar		*msg, *tmpurl = NULL;
	int 		i;
	char		*data = NULL;
	struct stat	statinfo;

	request->problem = 0;

	if(NULL != strstr(request->feedurl, "://")) {
		/* :// means it an URL */
		tmpurl = g_strdup(request->feedurl);	/* necessary because DownloadFeed() works on the URL string */
		if(NULL == (data = DownloadFeed(tmpurl, request, 0)))
			request->problem = 1;
		g_free(tmpurl);
	} else {
		/* no :// so we assume its a local path or command */
		
		if(request->feedurl[0] == '|') {
			/* if the first char is a | we have a pipe else a file */
			f = popen(&request->feedurl[1], "r");
			if(NULL != f) {
				i = 0;
				while(!feof(f)) {
					++i;
					data = g_realloc(data, i*1024);
					fread(&data[(i-1)*1024], 1024, 1, f);
				}
				fclose(f);
			} else {
				print_status(g_strdup_printf(_("Could not open pipe \"%s\"!"), request->feedurl));
			}
		} else if(g_file_test(request->feedurl, G_FILE_TEST_EXISTS)) {
			/* we have a file... */
			if(!g_file_get_contents(request->feedurl, &data, NULL, NULL)) {
				request->problem = 1;
				msg = g_strdup_printf(_("Could not open file \"%s\"!"), request->feedurl);
				print_status(msg);
				g_free(msg);
			} else {
				g_assert(NULL != data);
			}
		} else {
			request->problem = 1;
			msg = g_strdup_printf(_("There is no file \"%s\"!"), request->feedurl);
			print_status(msg);
			g_free(msg);
		}
		
		/* fake a status */
		request->lasthttpstatus = 0;
		request->lastmodified = NULL;
	}

	return data;
}
