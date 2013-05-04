/**
 * @file update.c  generic update request and state processing
 *
 * Copyright (C) 2003-2012 Lars Windolf <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2009 Adrian Bunk <bunk@users.sourceforge.net>
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

#include "update.h"

#include <libxml/parser.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <libpeas/peas-extension-set.h>

#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>

#include "auth_activatable.h"
#include "common.h"
#include "debug.h"
#include "net.h"
#include "plugins_engine.h"
#include "xml.h"
#include "ui/liferea_shell.h"

/** global update job list, used for lookups when cancelling */
static GSList	*jobs = NULL;

static GAsyncQueue *pendingHighPrioJobs = NULL;
static GAsyncQueue *pendingJobs = NULL;
static guint numberOfActiveJobs = 0;
#define MAX_ACTIVE_JOBS	5

/* update state interface */

updateStatePtr
update_state_new (void)
{
	return g_new0 (struct updateState, 1);
}

glong
update_state_get_lastmodified (updateStatePtr state)
{
	return state->lastModified;
}

void
update_state_set_lastmodified (updateStatePtr state, glong lastModified)
{
	state->lastModified = lastModified;
}

const gchar *
update_state_get_cookies (updateStatePtr state)
{
	return state->cookies;
}

void
update_state_set_cookies (updateStatePtr state, const gchar *cookies)
{
	g_free (state->cookies);
	state->cookies = NULL;
	if (cookies)
		state->cookies = g_strdup (cookies);
}

updateStatePtr
update_state_copy (updateStatePtr state)
{
	updateStatePtr newState;
	
	newState = update_state_new ();
	update_state_set_lastmodified (newState, update_state_get_lastmodified (state));
	update_state_set_cookies (newState, update_state_get_cookies (state));
	
	return newState;
}

void
update_state_free (updateStatePtr updateState)
{
	if (!updateState)
		return;

	g_free (updateState->cookies);
	g_free (updateState);
}

/* update request processing */

updateRequestPtr
update_request_new (void)
{
	return g_new0 (struct updateRequest, 1);
}

void
update_request_free (updateRequestPtr request)
{
	if (!request)
		return;
	
	update_state_free (request->updateState);
	update_options_free (request->options);

	g_free (request->postdata);
	g_free (request->source);
	g_free (request->filtercmd);
	g_free (request);
}

void
update_request_set_source(updateRequestPtr request, const gchar* source) 
{
	g_free (request->source);
	request->source = g_strdup(source) ;
}

void
update_request_set_auth_value(updateRequestPtr request, const gchar* authValue)
{
	g_free(request->authValue);
	request->authValue = g_strdup(authValue);
}

updateResultPtr
update_result_new (void)
{
	updateResultPtr	result;
	
	result = g_new0 (struct updateResult, 1);
	result->updateState = update_state_new ();
	
	return result;
}

void
update_result_free (updateResultPtr result)
{
	if (!result)
		return;
		
	update_state_free (result->updateState);

	g_free (result->data);
	g_free (result->source);
	g_free (result->contentType);
	g_free (result->filterErrors);
	g_free (result);
}

updateOptionsPtr
update_options_copy (updateOptionsPtr options)
{
	updateOptionsPtr newOptions;
	
	newOptions = g_new0 (struct updateOptions, 1);
	newOptions->username = g_strdup (options->username);
	newOptions->password = g_strdup (options->password);
	newOptions->dontUseProxy = options->dontUseProxy;
	
	return newOptions;
}

void
update_options_free (updateOptionsPtr options)
{
	if (!options)
		return;
		
	g_free (options->username);
	g_free (options->password);
	g_free (options);
}

/* update job handling */

static updateJobPtr
update_job_new (gpointer owner,
                updateRequestPtr request,
		update_result_cb callback,
		gpointer user_data,
		updateFlags flags)
{
	updateJobPtr	job;
	
	job = g_new0 (struct updateJob, 1);
	job->owner = owner;
	job->request = request;
	job->result = update_result_new ();
	job->callback = callback;
	job->user_data = user_data;
	job->flags = flags;	
	job->state = REQUEST_STATE_INITIALIZED;
	
	return job;
}

gint
update_job_get_state (updateJobPtr job)
{
	return job->state;
}

static void
update_job_free (updateJobPtr job)
{
	if (!job)
		return;
		
	jobs = g_slist_remove (jobs, job);
	
	update_request_free (job->request);
	update_result_free (job->result);
	g_free (job);
}

/* filter idea (and some of the code) was taken from Snownews */
static gchar *
update_exec_filter_cmd (gchar *cmd, gchar *data, gchar **errorOutput, size_t *size)
{
	int		fd, status;
	gchar		*command;
	const gchar	*tmpdir = g_get_tmp_dir();
	char		*tmpfilename;
	char		*out = NULL;
	FILE		*file, *p;
	
	*errorOutput = NULL;
	tmpfilename = g_build_filename (tmpdir, "liferea-XXXXXX", NULL);
	
	fd = g_mkstemp(tmpfilename);
	
	if(fd == -1) {
		debug1(DEBUG_UPDATE, "Error opening temp file %s to use for filtering!", tmpfilename);
		*errorOutput = g_strdup_printf(_("Error opening temp file %s to use for filtering!"), tmpfilename);
		g_free(tmpfilename);
		return NULL;
	}	
		
	file = fdopen(fd, "w");
	fwrite(data, strlen(data), 1, file);
	fclose(file);

	*size = 0;
	command = g_strdup_printf("%s < %s", cmd, tmpfilename);
	p = popen(command, "r");
	g_free(command);
	if(NULL != p) {
		while(!feof(p) && !ferror(p)) {
			size_t len;
			out = g_realloc(out, *size+1025);
			len = fread(&out[*size], 1, 1024, p);
			if(len > 0)
				*size += len;
		}
		status = pclose(p);
		if(!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
			*errorOutput = g_strdup_printf(_("%s exited with status %d"),
			                              cmd, WEXITSTATUS(status));
			*size = 0;
		}
		out[*size] = '\0';
	} else {
		g_warning(_("Error: Could not open pipe \"%s\""), command);
		*errorOutput = g_strdup_printf(_("Error: Could not open pipe \"%s\""), command);
	}
	/* Clean up. */
	unlink (tmpfilename);
	g_free (tmpfilename);
	return out;
}

static gchar *
update_apply_xslt (updateJobPtr job)
{
	xsltStylesheetPtr	xslt = NULL;
	xmlOutputBufferPtr	buf;
	xmlDocPtr		srcDoc = NULL, resDoc = NULL;
	gchar			*output = NULL;

	g_assert (NULL != job->result);
	
	do {
		srcDoc = xml_parse (job->result->data, job->result->size, NULL);
		if (!srcDoc) {
			g_warning("fatal: parsing request result XML source failed (%s)!", job->request->filtercmd);
			break;
		}

		/* load localization stylesheet */
		xslt = xsltParseStylesheetFile (job->request->filtercmd);
		if (!xslt) {
			g_warning ("fatal: could not load filter stylesheet \"%s\"!", job->request->filtercmd);
			break;
		}

		resDoc = xsltApplyStylesheet (xslt, srcDoc, NULL);
		if (!resDoc) {
			g_warning ("fatal: applying stylesheet \"%s\" failed!", job->request->filtercmd);
			break;
		}

		buf = xmlAllocOutputBuffer (NULL);
		if (-1 == xsltSaveResultTo (buf, resDoc, xslt)) {
			g_warning ("fatal: retrieving result of filter stylesheet failed (%s)!", job->request->filtercmd);
			break;
		}
		
		if (xmlBufferLength (buf->buffer) > 0)
			output = xmlCharStrdup (xmlBufferContent (buf->buffer));
 
		xmlOutputBufferClose (buf);
	} while (FALSE);

	if (srcDoc)
		xmlFreeDoc (srcDoc);
	if (resDoc)
		xmlFreeDoc (resDoc);
	if (xslt)
		xsltFreeStylesheet (xslt);
	
	return output;
}

static void
update_apply_filter (updateJobPtr job)
{
	gchar	*filterResult;
	size_t	len = 0;

	g_assert (NULL == job->result->filterErrors);

	/* we allow two types of filters: XSLT stylesheets and arbitrary commands */
	if ((strlen (job->request->filtercmd) > 4) &&
	    (0 == strcmp (".xsl", job->request->filtercmd + strlen (job->request->filtercmd) - 4))) {
		filterResult = update_apply_xslt (job);
		len = strlen (filterResult);
	} else {
		filterResult = update_exec_filter_cmd (job->request->filtercmd, job->result->data, &(job->result->filterErrors), &len);
	}

	if (filterResult) {
		g_free (job->result->data);
		job->result->data = filterResult;
		job->result->size = len;
	}
}

static void
update_exec_cmd (updateJobPtr job)
{
	FILE	*f;
	int	status;
	size_t	len;
	
	job->result = update_result_new ();
		
	/* if the first char is a | we have a pipe else a file */
	debug1 (DEBUG_UPDATE, "executing command \"%s\"...", (job->request->source) + 1);	
	f = popen ((job->request->source) + 1, "r");
	if (f) {
		while (!feof (f) && !ferror (f)) {
			job->result->data = g_realloc (job->result->data, job->result->size + 1025);
			len = fread (&job->result->data[job->result->size], 1, 1024, f);
			if (len > 0)
				job->result->size += len;
		}
		status = pclose (f);
		if (WIFEXITED (status) && WEXITSTATUS (status) == 0)
			job->result->httpstatus = 200;
		else 
			job->result->httpstatus = 404;	/* FIXME: maybe setting request->returncode would be better */

		if (job->result->data)
			job->result->data[job->result->size] = '\0';
	} else {
		liferea_shell_set_status_bar (_("Error: Could not open pipe \"%s\""), (job->request->source) + 1);
		job->result->httpstatus = 404;	/* FIXME: maybe setting request->returncode would be better */
	}
	
	update_process_finished_job (job);
}

static void
update_load_file (updateJobPtr job)
{
	gchar *filename = job->request->source;
	gchar *anchor;
	
	job->result = update_result_new ();
	
	if (!strncmp (filename, "file://",7))
		filename += 7;

	anchor = strchr (filename, '#');
	if (anchor)
		*anchor = 0;	 /* strip anchors from filenames */

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		/* we have a file... */
		if ((!g_file_get_contents (filename, &(job->result->data), &(job->result->size), NULL)) || (job->result->data[0] == '\0')) {
			job->result->httpstatus = 403;	/* FIXME: maybe setting request->returncode would be better */
			liferea_shell_set_status_bar (_("Error: Could not open file \"%s\""), filename);
		} else {
			job->result->httpstatus = 200;
			debug2 (DEBUG_UPDATE, "Successfully read %d bytes from file %s.", job->result->size, filename);
		}
	} else {
		liferea_shell_set_status_bar (_("Error: There is no file \"%s\""), filename);
		job->result->httpstatus = 404;	/* FIXME: maybe setting request->returncode would be better */
	}
	
	update_process_finished_job (job);
}

static void
update_job_run (updateJobPtr job)
{
	/* Here we decide on the source type and the proper execution
	   methods which then do anything they want with the job and
	   pass the processed job to update_process_finished_job()
	   for result dequeuing */
	
	/* everything starting with '|' is a local command */
	if (*(job->request->source) == '|') {
		debug1 (DEBUG_UPDATE, "Recognized local command: %s", job->request->source);
		update_exec_cmd (job);
		return;
	}
	
	/* if it has a protocol "://" prefix, but not "file://" it is an URI */
	if (strstr (job->request->source, "://") && strncmp (job->request->source, "file://", 7)) {
		network_process_request (job);
		return;
	}
	
	/* otherwise it must be a local file... */
	{
		debug1 (DEBUG_UPDATE, "Recognized file URI: %s", job->request->source);
		update_load_file (job);
		return;
	}
}

static gboolean
update_dequeue_job (gpointer user_data)
{
	updateJobPtr job;
	
	if (!pendingJobs)
		return FALSE;	/* we must be in shutdown */
		
	if (numberOfActiveJobs >= MAX_ACTIVE_JOBS) 
		return FALSE;	/* we'll be called again when a job finishes */
	

	job = (updateJobPtr)g_async_queue_try_pop(pendingHighPrioJobs);

	if (!job)
		job = (updateJobPtr)g_async_queue_try_pop(pendingJobs);

	if(!job)
		return FALSE;	/* no request at the moment */

	numberOfActiveJobs++;

	job->state = REQUEST_STATE_PROCESSING;

	debug1 (DEBUG_UPDATE, "processing request (%s)", job->request->source);
	if (job->callback == NULL) {
		update_process_finished_job (job);
	} else {
		update_job_run (job);
	}
		
	return TRUE; /* since I got a job now, there may be more in the queue */
}

updateJobPtr
update_execute_request (gpointer owner, 
                        updateRequestPtr request, 
			update_result_cb callback, 
			gpointer user_data, 
			updateFlags flags)
{
	updateJobPtr job;
	
	g_assert (request->options != NULL);
	
	job = update_job_new (owner, request, callback, user_data, flags);
	job->state = REQUEST_STATE_PENDING;	
	jobs = g_slist_append (jobs, job);

	if (flags & FEED_REQ_PRIORITY_HIGH) {
		g_async_queue_push (pendingHighPrioJobs, (gpointer)job);
	} else {
		g_async_queue_push (pendingJobs, (gpointer)job);
	}

	g_idle_add (update_dequeue_job, NULL);
	return job;
}

void
update_job_cancel_by_owner (gpointer owner)
{
	GSList	*iter = jobs;

	while (iter) {
		updateJobPtr job = (updateJobPtr)iter->data;
		if (job->owner == owner)
			job->callback = NULL;
		iter = g_slist_next (iter);
	}
}

static gboolean
update_process_result_idle_cb (gpointer user_data)
{
	updateJobPtr job = (updateJobPtr)user_data;
	
	if (job->callback)
		(job->callback) (job->result, job->user_data, job->flags);

	update_job_free (job);
		
	return FALSE;
}

void
update_process_finished_job (updateJobPtr job)
{
	job->state = REQUEST_STATE_DEQUEUE;
	
	g_assert(numberOfActiveJobs > 0);
	numberOfActiveJobs--;
	g_idle_add (update_dequeue_job, NULL);

	/* Handling abandoned requests (e.g. after feed deletion) */
	if (job->callback == NULL) {	
		debug1 (DEBUG_UPDATE, "freeing cancelled request (%s)", job->request->source);
		update_job_free (job);
		return;
	} 

	/* Finally execute the postfilter */
	if (job->result->data && job->request->filtercmd) 
		update_apply_filter (job);
		
	g_idle_add (update_process_result_idle_cb, job);
}


void
update_init (void)
{
	pendingJobs = g_async_queue_new ();
	pendingHighPrioJobs = g_async_queue_new ();
}

void
update_deinit (void)
{
	GSList	*iter = jobs;

	/* Cancel all jobs, to avoid async callbacks accessing the GUI */
	while (iter) {
		updateJobPtr job = (updateJobPtr)iter->data;
		job->callback = NULL;
		iter = g_slist_next (iter);
	}

	g_async_queue_unref (pendingJobs);
	g_async_queue_unref (pendingHighPrioJobs);
	
	g_slist_free (jobs);
	jobs = NULL;
}
