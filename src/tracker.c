/*

Copyright (C) Tim Orford 2007-2009

This software is licensed under the GPL. See accompanying file COPYING.

*/
#include "config.h"
#ifdef USE_TRACKER
#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sndfile.h>
#include <gtk/gtk.h>

#include "typedefs.h"
#include "mysql/mysql.h"
#include "support.h"
#include "src/types.h"
#include "tracker.h"
#include <src/tracker_.h>

extern int debug;
//extern struct _app app;
//void set_backend(BackendType);

static TrackerClient* tc = NULL;
static SamplecatResult result;
struct _iter
{
	gchar** result;
	char**  p_strarray;
	int     idx;

	//results from tracker_search_query()
	GPtrArray* qresult;
};
struct _iter iter = {};


gboolean
tracker__init(gpointer data)
{
	PF;
	//note: trackerd doesnt have to be running - it will be auto-started.
	if((tc = tracker_connect(TRUE))){
		//set_backend(BACKEND_TRACKER);

		//temporary:
		//void do_search(char* search, char *dir);
		//do_search(NULL, NULL);

		//tracker_disconnect (tc); //FIXME we dont disconnect!
	}
	else warnprintf("cant connect to tracker daemon.");
	return IDLE_STOP;
}


#if 0
void
tracker__search()
{
	PF;
	char **p_strarray;

	//--------------------------------------------------------------

	void wav_reply(char** result, GError* error, gpointer user_data)
	{
		dbg(0, "reply!");
		if (error) {
			warnprintf("internal tracker error: %s\n", error->message);
			g_error_free (error);
			return;
		}
	}
	char* mimes[4];
	mimes[0] = g_strdup("audio/x-wav");
	mimes[1] = NULL;
	GError* error = NULL;
	//tracker_files_get_by_mime_type_async(tc, 1, mimes, 0, 100, wav_reply, NULL);

	gchar** result = tracker_files_get_by_mime_type(tc, 1, mimes, 0, 100, &error);
	dbg(0, "got tracker result.");
	if (!error) {
		if (!result) dbg(0, "no tracker results");
		if (!*result) dbg(0, "no items found.");
		for (p_strarray = result; *p_strarray; p_strarray++) {
			char *s = g_locale_from_utf8 (*p_strarray, -1, NULL, NULL, NULL);
			if (!s) continue;
			g_print ("  %s\n", s);
			g_free (s);
		}

	} else {
		warnprintf("internal tracker error: %s\n", error->message);
		g_error_free (error);
	}
}
#endif


static gint
str_in_array (const gchar *str, gchar **array)
{
	gchar **st;

	gint i; for(i = 0, st = array; *st; st++, i++){
		if(strcasecmp(*st, str) == 0){
			return i;
		}
	}

	return -1;
}

typedef struct {
	gchar*        service;
	gchar*        display_name;
	gchar*        icon_name;
	GdkPixbuf*    pixbuf;
	ServiceType	  service_type;
	GtkListStore* store;
	gboolean      has_hits;
	gint          hit_count;
	gint          offset;
} service_info_t;

static char *search_service_types[] = {
"Files",
"Folders",
"Documents",
"Images",
"Music",
"Videos",
"Text",
"Development",
"Other",
"VFS",
"VFSFolders",
"VFSDocuments",
"VFSImages",
"VFSMusic",
"VFSVideos",
"VFSText",
"VFSDevelopment",
"VFSOther",
"Conversations",
"Playlists",
"Applications",
"Contacts",
"Emails",
"EmailAttachments",
"EvolutionEmails",
"ModestEmails",
"ThunderbirdEmails",
"Appointments",
"Tasks",
"Bookmarks",
"WebHistory",
"Projects",
NULL
};

#define N_(A) A
static service_info_t services[17] = {
	{ "Emails",	           N_("Emails"),       "stock_mail",		   NULL, SERVICE_EMAILS,	    NULL, FALSE, 0, 0},
	{ "EvolutionEmails",   N_("Emails"),       "stock_mail",		   NULL, SERVICE_EMAILS,	    NULL, FALSE, 0, 0},
	{ "ModestEmails",      N_("Emails"),       "stock_mail",		   NULL, SERVICE_EMAILS,	    NULL, FALSE, 0, 0},
	{ "ThunderbirdEmails", N_("Emails"),       "stock_mail",		   NULL, SERVICE_EMAILS,	    NULL, FALSE, 0, 0},
	{ "Files",	           N_("All Files"),    "system-file-manager",	   NULL, SERVICE_FILES,		    NULL, FALSE, 0, 0},
	{ "Folders",	       N_("Folders"),      "folder",		   NULL, SERVICE_FOLDERS,	    NULL, FALSE, 0, 0},
	{ "Documents",	       N_("Documents"),    "x-office-document",	   NULL, SERVICE_DOCUMENTS,	    NULL, FALSE, 0, 0},
	{ "Images",	           N_("Images"),       "image-x-generic",	   NULL, SERVICE_IMAGES,	    NULL, FALSE, 0, 0},
	{ "Music",	           N_("Music"),        "audio-x-generic",	   NULL, SERVICE_MUSIC,		    NULL, FALSE, 0, 0},
	{ "Playlists",	       N_("Playlists"),    "audio-x-generic",	   NULL, SERVICE_PLAYLISTS,	    NULL, FALSE, 0, 0},
	{ "Videos",	           N_("Videos"),       "video-x-generic",	   NULL, SERVICE_VIDEOS,	    NULL, FALSE, 0, 0},
	{ "Text",	           N_("Text"),	       "text-x-generic",	   NULL, SERVICE_TEXT_FILES,	    NULL, FALSE, 0, 0},
	{ "Development",       N_("Development"),  "applications-development", NULL, SERVICE_DEVELOPMENT_FILES, NULL, FALSE, 0, 0},
	{ "Conversations",     N_("Chat Logs"),    "stock_help-chat",	   NULL, SERVICE_CONVERSATIONS,     NULL, FALSE, 0, 0},
	{ "Applications",      N_("Applications"), "system-run",		   NULL, SERVICE_APPLICATIONS,	    NULL, FALSE, 0, 0},
	{ "WebHistory",        N_("WebHistory"),   "text-html",		   NULL, SERVICE_WEBHISTORY,	    NULL, FALSE, 0, 0},
	{ NULL,		           NULL,               NULL,			   NULL, -1,			    NULL, FALSE, 0, 0},
};

static void
populate_hit_counts (gpointer value, gpointer data)
{
	gchar **meta;
	gint type;
	service_info_t *service;

	meta = (char **)value;

	if (meta[0] && meta[1]) {

		type = str_in_array (meta[0], (char**) search_service_types);

		if (type != -1) {
			for (service = services; service->service; ++service) {
				if (strcmp(service->service,meta[0]) == 0) {
					service->hit_count = atoi (meta[1]);
					break;
				}
			}
		}
	}
}

static void
get_hit_count (GPtrArray* out_array, GError* error, gpointer user_data)
{
	PF;
	gboolean first_service = FALSE;
	gboolean has_hits = FALSE;

	if(error){
		//display_error_dialog (gsearch->window, _("Could not connect to search service as it may be busy"));
		g_error_free (error);
		return;
	}

	if(out_array){
		g_ptr_array_foreach (out_array, (GFunc) populate_hit_counts, NULL);
		g_ptr_array_free (out_array, TRUE);
		out_array = NULL;
	}

	service_info_t* service;
	for(service = services; service->service; ++service){
		if(!service->hit_count) continue;

		has_hits = TRUE;
		dbg(0, "hits=%i %s", service->hit_count, service->display_name);
	}

	if(!first_service){
		if(!has_hits) return;

		/* old category not found so go to first one with hits */
		for (service = services; service->service; ++service) {
			if(!service->hit_count) continue;
			break;
		}
	}

/*
	do_search (gsearch, gsearch->search_term, TRUE, 0);
*/
}

gboolean
tracker__search_iter_new(char* search, char* dir)
{
	PF;

	char* mimes[4];
	mimes[0] = g_strdup("audio/x-wav");
	mimes[1] = NULL;
	GError* error = NULL;

	gint limit = 100; //FIXME
	ServiceType type = SERVICE_FILES;

	iter.idx = 0;
	//warning! this is a synchronous call, so app will be blocked if no quick reply.
	//iter.result = tracker_files_get_by_mime_type(tc, 1, mimes, 0, limit, &error);
	iter.result = tracker_search_text (tc, -1, type, search, 0, limit, &error);
	iter.p_strarray = iter.result;
	if(iter.result && !error){
#if 0
		//TODO this gives us the number of results, but they are currently not returned anywhere useful.
		tracker_search_text_get_hit_count_all_async(tc, search, (TrackerGPtrArrayReply)get_hit_count, NULL);
#endif

		//there is also tracker_search_text_get_hit_count_async() that gets counts for an individual category.

		/*
		Tracker command line tools:
			* tracker-tag for setting and searching tags/keywords
			* tracker-extract FILE this extracts embedded metadata from FILE and prints to stdout
			* tracker-query this reads an RDF Query that specifies the search criteria for various fields.

		tracker_search_query_async() - fields** ?

		full text search *is* supposed to return matches in the filename and path....

		example of rdf string searching on two fields:
		char* rdf__ = "\
		<rdfq:Condition>"
		" <rdfq:or>"
		"  <rdfq:inSet>\
		   <rdfq:Property name='User:Keywords' />\
		   <rdf:String>bill</rdf:String>\
		  </rdfq:inSet>"
		"  <rdfq:contains>\
		   <rdfq:Property name='File:NameDelimited' />\
		   <rdf:String>test</rdf:String>\
		  </rdfq:contains>"
		" </rdfq:or>\"
		"</rdfq:Condition>";

		*/

		ServiceType type = SERVICE_MUSIC;

		char* rdf = g_strdup_printf(
			"<rdfq:Condition>"
			"  <rdfq:contains>"
			"   <rdfq:Property name='File:NameDelimited' />"
			"   <rdf:String>%s</rdf:String>"
			"  </rdfq:contains>"
			"</rdfq:Condition>", search);

		int offset = 0;

		//search by filename and type:
		//note: "File:NameDelimited" appears to search the whole filepath, including both filename and directory.
		iter.qresult = tracker_search_query(tc, time(NULL),
                    type,
                    NULL,         // Fields      <----- what?
                    NULL,         // Search text
                    NULL,         // Keywords
                    rdf,
                    offset,
                    limit,
                    FALSE,        // Sort by service
                    &error);
		g_free(rdf);
		if(error){
			gwarn("query error: %s", error->message);
			g_error_free (error);
			error = NULL;
		}
		else if(iter.qresult){
			//int length = qresult->len;

			dbg(0, "query: nresults=%i (%s)", iter.qresult->len, search);
		}
		else dbg(0, "no results! search=%s", search);

		//test tracker_files_get_by_service_type:
		//-this gives us *all* Audio files, but not other search criteria can be applied.
#if 0
		gchar** array = tracker_files_get_by_service_type (tc, time(NULL), type, offset, limit, &error);
		if(!error){
			if(array){
				gchar** p; for (p = array; *p; p++) {
					g_print ("by_type:  %s\n", *p);
				}
			}
			else dbg(0, "no results for type=%i", type);
		}
		else gwarn("(by service type) error: %s", error->message);
#endif

		return true;
	}

	if(error){
		warnprintf("internal tracker error: %s\n", error->message);
		g_error_free (error);
	}
	return false;
}


SamplecatResult*
tracker__search_iter_next()
{
	if(iter.qresult){
		memset(&result, 0, sizeof(SamplecatResult));

		if(iter.idx >= iter.qresult->len) return NULL;

		gpointer val = g_ptr_array_index(iter.qresult, iter.idx);
		gchar** meta = val;
		result.sample_name = g_path_get_basename(meta[0]);
		result.dir = g_path_get_dirname(meta[0]);
		result.idx = iter.idx;           //TODO this idx is pretty meaningless.
		result.mimetype = "audio/x-wav"; //TODO

		iter.idx++;
	}else{
		if(!iter.p_strarray) return NULL;
		if(!*iter.p_strarray) return NULL;

		memset(&result, 0, sizeof(SamplecatResult));

		char* s = g_locale_from_utf8(*iter.p_strarray, -1, NULL, NULL, NULL);
		if(s){
			//g_print("  %i: %s\n", iter.idx, s);
			result.sample_name = g_path_get_basename(s);
			result.dir = g_path_get_dirname(s);
			result.idx = iter.idx; //TODO this idx is pretty meaningless.
			result.mimetype = "audio/x-wav";
			g_free (s);

			iter.idx++;
			iter.p_strarray++;
		}
		else return NULL;
	}

	return &result;
}


void
tracker__search_iter_free()
{
	if(iter.qresult){
		g_ptr_array_free(iter.qresult, TRUE);
	}else{
		g_strfreev(iter.result);
	}
}


#endif //USE_TRACKER
