/*
 * common routines for Liferea
 * 
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004       Karl Soderstrom <ks@xanadunet.net>
 *
 * parts of the RFC822 timezone decoding were taken from the gmime 
 * source written by 
 *
 * Authors: Michael Zucchi <notzed@helixcode.com>
 *          Jeffrey Stedfast <fejj@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERuri[i]ANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define _XOPEN_SOURCE /* glibc2 needs this (man strptime) */
#include <time.h>
#include <langinfo.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <glib.h>
#include <sys/stat.h>
#include <string.h>
#include <locale.h>
#include "support.h"
#include "common.h"
#include "conf.h"
#include "support.h"
#include "feed.h"

#define	TIMESTRLEN	256

#define VFOLDER_EXTENSION	"vfolder"
#define OCS_EXTENSION		"ocs"

static gchar *standard_encoding = { "UTF-8" };
static gchar *CACHEPATH = NULL;

gchar * convertCharSet(gchar * from_encoding, gchar * to_encoding, gchar * string);

void addToHTMLBuffer(gchar **buffer, gchar *string) {
	gchar	*newbuffer;
	
	if(NULL == string)
		return;
	
	if(NULL != *buffer) {
		newbuffer = g_strdup_printf("%s%s", *buffer, string);
		g_free(*buffer);
		*buffer = newbuffer;
	} else {
		*buffer = g_strdup(string);
	}
}

/* converts the string string encoded in from_encoding (which
   can be NULL) to to_encoding, frees the original string and 
   returns the result */
gchar * convertCharSet(gchar * from_encoding, gchar * to_encoding, gchar * string) {
	gint	bw, br;
	gchar	*new = NULL;
	
	if(NULL == from_encoding)
		from_encoding = standard_encoding;
		
	if(NULL != string) {		
		new = g_convert(string, strlen(string), from_encoding, to_encoding, &br, &bw, NULL);
		
		if(NULL != new)
			g_free(string);
		else
			new = string;
	} else {	
		return g_strdup("");
	}

	return new;
}

gchar * convertToHTML(gchar * string) { return convertCharSet("UTF-8", "HTML", string); }

/* the code of this function was taken from a GTK tutorial */
static gchar* convert(unsigned char *in, gchar *encoding)
{
	unsigned char *out;
        int ret,size,out_size,temp;
        xmlCharEncodingHandlerPtr handler;

	if(NULL == in)
		return NULL;
	
        size = (int)strlen(in)+1; 
        out_size = size*2-1; 
        out = g_malloc((size_t)out_size); 

        if (out) {
                handler = xmlFindCharEncodingHandler(encoding);
                
                if (!handler) {
                        g_free(out);
                        out = NULL;
                }
        }
        if (out) {
                temp=size-1;
                ret = handler->input(out, &out_size, in, &temp);
                if (ret || temp-size+1) {
                        if (ret) {
                                g_print(_("conversion wasn't successful.\n"));
                        } else {
                                g_print(_("conversion wasn't successful. converted: %i octets.\n"), temp);
                        }
                        g_free(out);
                        out = NULL;
                } else {
                        out = g_realloc(out,out_size+1); 
                        out[out_size]=0; /*null terminating out*/
                        
                }
        } else {
                g_error(_("not enough memory\n"));
        }
		
        return out;
}

/* Conversion function which should be applied to all read XML strings, 
   to ensure proper UTF8. doc points to the xml document and its encoding and
   string is a xmlchar pointer to the read string. The result gchar
   string is returned, the original XML string is freed. */
gchar * CONVERT(xmlChar *string) {
	gchar	*result;
	
	result = convert(string, "UTF-8");
	xmlFree(string);
	return result;
}

gchar * extractHTMLNode(xmlNodePtr cur) {
	xmlBufferPtr	buf = NULL;
	gchar		*result = NULL;
	
	buf = xmlBufferCreate();
	
	if(-1 != xmlNodeDump(buf, cur->doc, cur, 0, 0))
		result = xmlCharStrdup(xmlBufferContent(buf));

	xmlBufferFree(buf);
	
	return result;
}

void unhtmlizeHandleCharacters (void *userData_p, const xmlChar *string_p, int len)
{
/*        gchar *result_p = (gchar *) userData_p;
        int curLen = g_utf8_strlen(result_p, -1);

	result_p = g_utf8_offset_to_pointer(result_p, curLen);                                                                                
        g_utf8_strncpy(result_p, (gchar *)string_p, len);
	/ Make sure it's null-terminated
	result_p = g_utf8_offset_to_pointer(result_p, curLen + len + 1);
	*result_p = '\0';*/
        gchar *result_p = (gchar *) userData_p;
        int curLen = strlen(result_p);

        strncpy (&result_p[curLen], (char *) string_p, len);
	// Make sure it's null-terminated
        result_p[curLen + len] = '\0';
}

/* converts a UTF-8 strings containing any HTML stuff to 
   a string without any entities or tags containing all
   text nodes of the given HTML string */
gchar * unhtmlize(gchar *string) {
	htmlDocPtr		doc_p = NULL;
	htmlSAXHandlerPtr	sax_p = NULL;
	int			length;
	gchar			*result_p = NULL;
	
	if(NULL == string)
		return NULL;

	/* only do something if there are any entities or tags */
	if(NULL == (strpbrk(string, "&<>")))
		return string;
	
	sax_p = g_new0(xmlSAXHandler, 1);
 	if (sax_p != NULL) {
 		sax_p->characters = unhtmlizeHandleCharacters;
	
		length = strlen(string);	/* the result should not get bigger than the original string... (is this correct?) */
 		result_p = g_new(gchar, length + 1);
 		if (result_p != NULL) {
 			result_p[0] = '\0';
 
 			doc_p = htmlSAXParseDoc(string, standard_encoding, sax_p, result_p);
 			if (doc_p != NULL) {
 				xmlFreeDoc(doc_p);
 			}
 		}
 
 		g_free(sax_p);
 	}
 
 	if (result_p == NULL || !g_utf8_strlen(result_p, -1)) {
 		/* Something went wrong in the parsing.
 		 * Use original string instead */
 		g_free(result_p);
 		return string;
 	} else {
 		g_free(string);
 		return result_p;
 	}
}

/* Common function to create a XML DOM object from a given
   XML buffer. This function sets up a parser context,
   enables recovery mode and sets up the error handler.
   
   The function returns a XML document and (if errors)
   occur sets the errormsg to the last error message. */
xmlDocPtr parseBuffer(gchar *data, gchar **errormsg) {
	xmlParserCtxtPtr	parser;
	xmlDocPtr		doc;
	
	parser = xmlCreateMemoryParserCtxt(data, strlen(data));
	parser->recovery = 1;
	parser->sax->fatalError = NULL;
	parser->sax->error = NULL;
	parser->sax->warning = NULL;
	parser->vctxt.error = NULL;
	parser->vctxt.warning = NULL;
	xmlParseDocument(parser);	// ignore returned errors
	
	if(*errormsg != NULL)
		g_free(*errormsg);
	*errormsg = g_strdup(parser->lastError.message);

	doc = parser->myDoc;
	xmlFreeParserCtxt(parser);
	
	return doc;
}

/* converts a ISO 8601 time string to a time_t value */
time_t parseISO8601Date(gchar *date) {
	struct tm	tm;
	time_t		t;
	gboolean	success = FALSE;
		
	memset(&tm, 0, sizeof(struct tm));

	/* we expect at least something like "2003-08-07T15:28:19" and
	   don't require the second fractions and the timezone info

	   the most specific format:   YYYY-MM-DDThh:mm:ss.sTZD
	 */
	 
	/* full specified variant */
	if(NULL != strptime((const char *)date, "%t%Y-%m-%dT%H:%M%t", &tm))
		success = TRUE;
	/* only date */
	else if(NULL != strptime((const char *)date, "%t%Y-%m-%d", &tm))
		success = TRUE;
	/* there were others combinations too... */

	if(TRUE == success) {
		if((time_t)(-1) != (t = mktime(&tm))) {
			return t;
		} else {
			g_warning(_("internal error! time conversion error! mktime failed!\n"));
		}
	} else {
		g_print(_("Invalid ISO8601 date format! Ignoring <dc:date> information!\n"));				
	}
	
	return 0;
}

/* converts a RFC822 time string to a time_t value */
time_t parseRFC822Date(gchar *date) {
	struct tm	tm;
	time_t		t;
	char 		*oldlocale;
	char		*pos;
	gboolean	success = FALSE;

	memset(&tm, 0, sizeof(struct tm));

	/* we expect at least something like "03 Dec 12 01:38:34" 
	   and don't require a day of week or the timezone

	   the most specific format we expect:  "Fri, 03 Dec 12 01:38:34 CET"
	 */
	/* skip day of week */
	if(NULL != (pos = g_utf8_strchr(date, -1, ',')))
		date = ++pos;

	/* we expect english month names, so we set the locale */
	oldlocale = setlocale(LC_TIME, NULL);
	setlocale(LC_TIME, "C");
	
	/* standard format with 2 digit year */
	if(NULL != (pos = strptime((const char *)date, "%d %b %y %T", &tm)))
		success = TRUE;
	/* non-standard format with 4 digit year */
	else if(NULL != (pos = strptime((const char *)date, "%d %b %Y %T", &tm)))
		success = TRUE;
	
	setlocale(LC_TIME, oldlocale);	/* and reset it again */
	
	if(TRUE == success) {
		if((time_t)(-1) != (t = mktime(&tm)))
			return t;
		else
			g_warning(_("internal error! time conversion error! mktime failed!\n"));
	} else {

		g_print(_("Invalid RFC822 date format! Ignoring <pubDate> information!\n"));
	}
	
	return 0;
}

gchar * getActualTime(void) {
	time_t		t;
	gchar		*timestr = NULL;
	gchar		*timeformat;
	
	/* get receive time */
	if((time_t)-1 != time(&t)) {
		if(NULL != (timestr = (gchar *)g_malloc(TIMESTRLEN+1))) {
			timeformat = getStringConfValue(TIME_FORMAT);
			
			/* if not set conf.c delivers a "" and D_T_FMT will be used... */
			if(0 == strlen(timeformat)) {
				g_free(timeformat);
				timeformat =  g_strdup_printf("%s %s", nl_langinfo(D_FMT), nl_langinfo(T_FMT));
				
			}
			
			if(NULL != timeformat) {
				strftime(timestr, TIMESTRLEN, (char *)timeformat, gmtime(&t));
				g_free(timeformat);
			}
		}
	}
	
	return timestr;
}

gchar * formatDate(time_t t) {
	gchar		*timestr;
	gchar		*timeformat;
	
	if(NULL != (timestr = (gchar *)g_malloc(TIMESTRLEN+1))) {
		switch(getNumericConfValue(TIME_FORMAT_MODE)) {
			case 1:
				timeformat =  g_strdup_printf("%s", nl_langinfo(T_FMT));	
				break;
			case 3:
				timeformat = getStringConfValue(TIME_FORMAT);				
				break;
			case 2:
			default:
				timeformat =  g_strdup_printf("%s %s", nl_langinfo(D_FMT), nl_langinfo(T_FMT));	
				break;
		}		
		strftime(timestr, TIMESTRLEN, (char *)timeformat, localtime(&t));
		g_free(timeformat);
	}
	
	return timestr;
}

void initCachePath(void) {
	struct stat	statinfo;

	CACHEPATH = g_strdup_printf("%s/.liferea", g_get_home_dir());

	if(!g_file_test(CACHEPATH, G_FILE_TEST_IS_DIR)) {
		if(0 != mkdir(CACHEPATH, S_IRUSR | S_IWUSR | S_IXUSR)) {
			g_error(g_strdup_printf(_("Cannot create cache directory %s!"), CACHEPATH));
		}
	}
}

gchar * getCachePath(void) {
	
	if(NULL == CACHEPATH)
		initCachePath();
		
	return CACHEPATH;
}

/* returns the extension for the type type */
gchar *getExtension(gint type) {
	gchar	*extension;
	
	switch(type) {
		case FST_VFOLDER:
			extension = VFOLDER_EXTENSION;
			break;
		case FST_OCS:
			extension = OCS_EXTENSION;
			break;
		default:
			extension = NULL;
			break;
	}
	
	return extension;
}

gchar * getCacheFileName(gchar *keyprefix, gchar *key, gchar *extension) {
	gchar	*keypos;
	
	/* build filename */	
	keypos = strrchr(key, '/');
	if(NULL == keypos)
		keypos = key;
	else
		keypos++;
	
	if(NULL != extension)	
		return g_strdup_printf("%s/%s_%s.%s", getCachePath(), keyprefix, keypos, extension);
	else
		return g_strdup_printf("%s/%s_%s", getCachePath(), keyprefix, keypos);
}

static gchar * byte_to_hex(unsigned char nr) {
	gchar *result = NULL;
	
	result = g_strdup_printf("%%%x%x", nr / 0x10, nr % 0x10);
	return result;
}

/* Encodes any UTF-8 string in uriString and returns a 
   valid UTF-8 encoded HTTP URI. Note that the uriString will 
   be freed. This function is actually used to generate Feedster
   search feed URLs. */
gchar * encodeURIString(gchar *uriString) {
	gchar		*newURIString;
	gchar		*tmp, *hex;
	int		i, j, len, bytes;

	/* the UTF-8 string is casted to ASCII to treat
	   the characters bytewise and convert non-ASCII
	   compatible chars to URI hexcodes */
	newURIString = g_strdup("");
	len = strlen(uriString);
	for(i = 0; i < len; i++) {
		if((('A' <= uriString[i]) && (uriString[i] <= 'Z')) ||
		   (('a' <= uriString[i]) && (uriString[i] <= 'z')) ||
		   (('0' <= uriString[i]) && (uriString[i] <= '9')) ||
		   (uriString[i] == '-') || 
		   (uriString[i] == '_') ||
		   (uriString[i] == '.') || 
		   (uriString[i] == '?') || 
		   (uriString[i] == '!') ||
		   (uriString[i] == '~') ||
		   (uriString[i] == '*') ||
		   (uriString[i] == '\'') ||
		   (uriString[i] == '(') ||
		   (uriString[i] == ')'))
		   	tmp = g_strdup_printf("%s%c", newURIString, uriString[i]);
		else if(uriString[i] == ' ')
			tmp = g_strdup_printf("%s%c", newURIString, '+');
		else if((unsigned char)uriString[i] <= 127) {
			tmp = g_strdup_printf(newURIString, hex = byte_to_hex(uriString[i]));g_free(hex);
		} else {
			bytes = 0;
			if(((unsigned char)uriString[i] >= 192) && ((unsigned char)uriString[i] <= 223))
				bytes = 2;
			else if(((unsigned char)uriString[i] > 223) && ((unsigned char)uriString[i] <= 239))
				bytes = 3;
			else if(((unsigned char)uriString[i] > 239) && ((unsigned char)uriString[i] <= 247))
				bytes = 4;
			else if(((unsigned char)uriString[i] > 247) && ((unsigned char)uriString[i] <= 251))
				bytes = 5;
			else if(((unsigned char)uriString[i] > 247) && ((unsigned char)uriString[i] <= 251))
				bytes = 6;
				
			if(0 != bytes) {
				if((i + (bytes - 1)) > len) {
					g_warning(_("Unexpected end of character sequence or corrupt UTF-8 encoding! Some characters were dropped!"));
					break;
				}

				for(j=0; j < (bytes - 1); j++) {
					tmp = g_strdup_printf("%s%s", newURIString, hex = byte_to_hex((unsigned char)uriString[i++]));
					g_free(hex);
					g_free(newURIString);
					newURIString = tmp;
				}
				tmp = g_strdup_printf("%s%s", newURIString, hex = byte_to_hex((unsigned char)uriString[i]));
				g_free(hex);
			} else {
				// sh..!
				g_error(_("Internal error while converting UTF-8 chars to HTTP URI!"));
			}
		}
		g_free(newURIString); 
		newURIString = tmp;
	}
	g_free(uriString);

	return newURIString;
}
