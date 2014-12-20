/**
* +----------------------------------------------------------------------+
* | This file is part of Samplecat. http://ayyi.github.io/samplecat/     |
* | copyright (C) 2007-2014 Tim Orford <tim@orford.org>                  |
* +----------------------------------------------------------------------+
* | This program is free software; you can redistribute it and/or modify |
* | it under the terms of the GNU General Public License version 3       |
* | as published by the Free Software Foundation.                        |
* +----------------------------------------------------------------------+
*
* This file is partially based on vala/application.vala but is manually synced.
*
*/
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include "debug/debug.h"
#include <sample.h>
#include "support.h"
#include "model.h"
#include "db/db.h"
#include "list_store.h"
#include "listview.h"
#include "progress_dialog.h"
#include "application.h"

#ifdef HAVE_AYYIDBUS
  #include "auditioner.h"
#endif
#ifdef HAVE_JACK
  #include "jack_player.h"
#endif
#ifdef HAVE_GPLAYER
  #include "gplayer.h"
#endif

#define CHECK_SIMILAR
#undef INTERACTIVE_IMPORT

extern void colour_box_init();

static gpointer application_parent_class = NULL;

enum  {
	APPLICATION_DUMMY_PROPERTY
};
static GObject* application_constructor    (GType, guint n_construct_properties, GObjectConstructParam*);
static void     application_finalize       (GObject*);
static void     application_set_auditioner (Application*);


Application*
application_construct (GType object_type)
{
	Application* app = g_object_new (object_type, NULL);
	app->cache_dir = g_build_filename (g_get_home_dir(), ".config", PACKAGE, "cache", NULL);
	app->config_dir = g_build_filename (g_get_home_dir(), ".config", PACKAGE, NULL);
	return app;
}


Application*
application_new ()
{
	Application* app = application_construct (TYPE_APPLICATION);

	colour_box_init();

	memset(app->config.colour, 0, PALETTE_SIZE * 8);

	app->config_filename = g_strdup_printf("%s/.config/" PACKAGE "/" PACKAGE, g_get_home_dir());

#if (defined HAVE_JACK)
	app->enable_effect = true;
	app->link_speed_pitch = true;
	app->effect_param[0] = 0.0; /* cent transpose [-100 .. 100] */
	app->effect_param[1] = 0.0; /* semitone transpose [-12 .. 12] */
	app->effect_param[2] = 0.0; /* octave [-3 .. 3] */
	app->playback_speed = 1.0;
#endif

	app->model = samplecat_model_new();

	samplecat_model_add_filter (app->model, app->model->filters.search   = samplecat_filter_new("search"));
	samplecat_model_add_filter (app->model, app->model->filters.dir      = samplecat_filter_new("directory"));
	samplecat_model_add_filter (app->model, app->model->filters.category = samplecat_filter_new("category"));

	void on_filter_changed(GObject* _filter, gpointer user_data)
	{
		//SamplecatFilter* filter = (SamplecatFilter*)_filter;
		application_search();
	}

	GList* l = app->model->filters_;
	for(;l;l=l->next){
		g_signal_connect((SamplecatFilter*)l->data, "changed", G_CALLBACK(on_filter_changed), NULL);
	}

	application_set_auditioner(app);

	return app;
}


void
application_emit_icon_theme_changed (Application* self, const gchar* s)
{
	g_return_if_fail (self != NULL);
	g_return_if_fail (s != NULL);
	g_signal_emit_by_name (self, "icon-theme", s);
}


static GObject*
application_constructor (GType type, guint n_construct_properties, GObjectConstructParam* construct_properties)
{
	GObjectClass* parent_class = G_OBJECT_CLASS (application_parent_class);
	GObject* obj = parent_class->constructor (type, n_construct_properties, construct_properties);
	return obj;
}


static void
application_class_init (ApplicationClass* klass)
{
	application_parent_class = g_type_class_peek_parent (klass);
	G_OBJECT_CLASS (klass)->constructor = application_constructor;
	G_OBJECT_CLASS (klass)->finalize = application_finalize;
	g_signal_new ("config_loaded", TYPE_APPLICATION, G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	g_signal_new ("icon_theme", TYPE_APPLICATION, G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
	g_signal_new ("on_quit", TYPE_APPLICATION, G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	g_signal_new ("theme_changed", TYPE_APPLICATION, G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	g_signal_new ("layout_changed", TYPE_APPLICATION, G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	g_signal_new ("audio_ready", TYPE_APPLICATION, G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}


static void
application_instance_init (Application* self)
{
	self->state = NONE;
}


static void
application_finalize (GObject* obj)
{
	G_OBJECT_CLASS (application_parent_class)->finalize (obj);
}


GType
application_get_type ()
{
	static volatile gsize application_type_id__volatile = 0;
	if (g_once_init_enter (&application_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = { sizeof (ApplicationClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) application_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (Application), 0, (GInstanceInitFunc) application_instance_init, NULL };
		GType application_type_id;
		application_type_id = g_type_register_static (G_TYPE_OBJECT, "Application", &g_define_type_info, 0);
		g_once_init_leave (&application_type_id__volatile, application_type_id);
	}
	return application_type_id__volatile;
}


void
application_quit(Application* app)
{
	g_signal_emit_by_name (app, "on-quit");
}


int  auditioner_nullC() {return 0;}
void auditioner_null() {;}
void auditioner_nullP(const char* p) {;}
void auditioner_nullS(Sample* s) {;}

static void
_set_auditioner() /* tentative - WIP */
{
	printf("auditioner backend: "); fflush(stdout);
	const static Auditioner a_null = {
		&auditioner_nullC,
		&auditioner_null,
		&auditioner_null,
		&auditioner_nullP,
		&auditioner_nullS,
		&auditioner_nullS,
		&auditioner_null,
		&auditioner_null,
		NULL, NULL, NULL, NULL
	};
#ifdef HAVE_JACK
  const static Auditioner a_jack = {
		&jplay__check,
		&jplay__connect,
		&jplay__disconnect,
		&jplay__play_path,
		&jplay__play,
		&jplay__toggle,
		&jplay__play_all,
		&jplay__stop,
		&jplay__play_selected,
		&jplay__pause,
		&jplay__seek,
		&jplay__getposition
	};
#endif
#ifdef HAVE_AYYIDBUS
	const static Auditioner a_ayyidbus = {
		&auditioner_check,
		&auditioner_connect,
		&auditioner_disconnect,
		&auditioner_play_path,
		&auditioner_play,
		&auditioner_toggle,
		&auditioner_play_all,
		&auditioner_stop,
		NULL,
		NULL,
		NULL,
		NULL
	};
#endif
#ifdef HAVE_GPLAYER
	const static Auditioner a_gplayer = {
		&gplayer_check,
		&gplayer_connect,
		&gplayer_disconnect,
		&gplayer_play_path,
		&gplayer_play,
		&gplayer_toggle,
		&gplayer_play_all,
		&gplayer_stop,
		NULL,
		NULL,
		NULL,
		NULL
	};
#endif

	gboolean connected = false;
#ifdef HAVE_JACK
	if(!connected && can_use(app->players, "jack")){
		app->auditioner = & a_jack;
		if (!app->auditioner->check()) {
			connected = true;
			printf("JACK playback.\n");
		}
	}
#endif
#ifdef HAVE_AYYIDBUS
	if(!connected && can_use(app->players, "ayyi")){
		app->auditioner = & a_ayyidbus;
		if (!app->auditioner->check()) {
			connected = true;
			printf("ayyi_audition.\n");
		}
	}
#endif
#ifdef HAVE_GPLAYER
	if(!connected && can_use(app->players, "cli")){
		app->auditioner = & a_gplayer;
		if (!app->auditioner->check()) {
			connected = true;
			printf("using CLI player.\n");
		}
	}
#endif
	if (!connected) {
		printf("no playback support.\n");
		app->auditioner = & a_null;
	}
}


static void
application_set_auditioner(Application* a)
{
	// The gui is allowed to load before connecting the audio.
	// Connecting the audio can sometimes be very slow.

	// TODO starting jack blocks the gui so this needs to be moved to another thread.

	void set_auditioner_on_connected(gpointer _)
	{
		g_signal_emit_by_name (app, "audio-ready");
	}

	bool set_auditioner_on_idle(gpointer data)
	{
		_set_auditioner();

		app->auditioner->connect(set_auditioner_on_connected, data);

		return IDLE_STOP;
	}

	if(!a->no_gui) // TODO too early for this flag to be set ?
		g_idle_add_full(G_PRIORITY_LOW, set_auditioner_on_idle, NULL, NULL);
}


/**
 * fill the display with the results matching the current set of filters.
 */
void
application_search()
{
	PF;

	if(BACKEND_IS_NULL) return;

	if(!backend.search_iter_new(app->model->filters.dir->value, app->model->filters.category->value, NULL)) {
		return;
	}

	if(app->libraryview)
		listview__block_motion_handler(); // TODO make private to listview.

	listmodel__clear();

	int row_count = 0;
	unsigned long* lengths;
	Sample* result;
	while((result = backend.search_iter_next(&lengths)) && row_count < MAX_DISPLAY_ROWS){
		Sample* s = sample_dup(result);
		//listmodel__add_result(s);
		samplecat_list_store_add((SamplecatListStore*)app->store, s);
		sample_unref(s);
		row_count++;
	}

	backend.search_iter_free();

	((SamplecatListStore*)app->store)->row_count = row_count;

	samplecat_list_store_do_search((SamplecatListStore*)app->store);
}


void
application_scan(const char* path, ScanResults* results)
{
	// path must not contain trailing slash

	g_return_if_fail(path);

	app->state = SCANNING;
	statusbar_print(1, "scanning...");

	application_add_dir(path, results);

	gchar* fail_msg = results->n_failed ? g_strdup_printf(", %i failed", results->n_failed) : "";
	gchar* dupes_msg = results->n_dupes ? g_strdup_printf(", %i duplicates", results->n_dupes) : "";
	statusbar_print(1, "add finished: %i files added%s%s", results->n_added, fail_msg, dupes_msg);
	if(results->n_failed) g_free(fail_msg);
	if(results->n_dupes) g_free(dupes_msg);
	app->state = NONE;
}


bool
application_add_file(const char* path, ScanResults* result)
{
	/*
	 *  uri must be "unescaped" before calling this fn. Method string must be removed.
	 */

	/* check if file already exists in the store
	 * -> don't add it again
	 */
	if(backend.file_exists(path, NULL)) {
		if(!app->no_gui) statusbar_print(1, "duplicate: not re-adding a file already in db.");
		g_warning("duplicate file: %s", path);
		Sample* s = sample_get_by_filename(path);
		if (s) {
			//sample_refresh(s, false);
			samplecat_model_refresh_sample (app->model, s, false);
			sample_unref(s);
		} else {
			dbg(1, "sample found in db but not in model.");
		}
		result->n_dupes++;
		return false;
	}

	if(!app->no_gui) dbg(1, "%s", path);

	if(BACKEND_IS_NULL) return false;

	Sample* sample = sample_new_from_filename((char*)path, false);
	if (!sample) {
		if (app->state != SCANNING){
			if (_debug_) gwarn("cannot add file: file-type is not supported");
			statusbar_print(1, "cannot add file: file-type is not supported");
		}
		return false;
	}

	if(app->no_gui){ printf("%s\n", path); fflush(stdout); }

	if(!sample_get_file_info(sample)){
		gwarn("cannot add file: reading file info failed. type=%s", sample->mimetype);
		if(!app->no_gui) statusbar_print(1, "cannot add file: reading file info failed");
		sample_unref(sample);
		return false;
	}
#ifdef CHECK_SIMILAR
	/* check if /same/ file already exists w/ different path */
	GList* existing;
	if((existing = backend.filter_by_audio(sample))) {
		GList* l = existing; int i;
#ifdef INTERACTIVE_IMPORT
		GString* note = g_string_new("Similar or identical file(s) already present in database:\n");
#endif
		for(i=1;l;l=l->next, i++){
			/* TODO :prompt user: ask to delete one of the files
			 * - import/update the remaining file(s)
			 */
			dbg(0, "found similar or identical file: %s", l->data);
#ifdef INTERACTIVE_IMPORT
			if (i < 10)
				g_string_append_printf(note, "%d: '%s'\n", i, (char*) l->data);
#endif
		}
#ifdef INTERACTIVE_IMPORT
		if (i > 9)
			g_string_append_printf(note, "..\n and %d more.", i - 9);
		g_string_append_printf(note, "Add this file: '%s' ?", sample->full_path);
		if (do_progress_question(note->str) != 1) {
			// 0, aborted: -> whole add_file loop is aborted on next do_progress() call.
			// 1, OK
			// 2, cancelled: -> only this file is skipped
			sample_unref(sample);
			g_string_free(note, true);
			return false;
		}
		g_string_free(note, true);
#endif /* END interactive import */
		g_list_foreach(existing, (GFunc)g_free, NULL);
		g_list_free(existing);
	}
#endif /* END check for similar files on import */

	sample->online = 1;
	sample->id = backend.insert(sample);
	if (sample->id < 0) {
		sample_unref(sample);
		return false;
	}

	samplecat_list_store_add((SamplecatListStore*)app->store, sample);

	samplecat_model_add(app->model);

	result->n_added++;

	sample_unref(sample);
	return true;
}


/**
 *	Scan the directory and try and add any files found.
 */
void
application_add_dir(const char* path, ScanResults* result)
{
	PF;

	char filepath[PATH_MAX];
	G_CONST_RETURN gchar* file;
	GError* error = NULL;
	GDir* dir;

	if((dir = g_dir_open(path, 0, &error))){
		while((file = g_dir_read_name(dir))){
			if(file[0] == '.') continue;

			snprintf(filepath, PATH_MAX, "%s%c%s", path, G_DIR_SEPARATOR, file);
			filepath[PATH_MAX - 1] = '\0';
			if (do_progress(0, 0)) break;

			if(!g_file_test(filepath, G_FILE_TEST_IS_DIR)){
				if(g_file_test(filepath, G_FILE_TEST_IS_SYMLINK) && !g_file_test(filepath, G_FILE_TEST_IS_REGULAR)){
					dbg(0, "ignoring dangling symlink: %s", filepath);
				}else{
					if(application_add_file(filepath, result)){
						if(!app->no_gui) statusbar_print(1, "%i files added", result->n_added);
					}
				}
			}
			// IS_DIR
			else if(app->add_recursive){
				application_add_dir(filepath, result);
			}
		}
		//hide_progress(); ///no: keep window open until last recursion.
		g_dir_close(dir);
	}else{
		result->n_failed++;

		if(error->code == 2)
			pwarn("%s\n", error->message); // permission denied
		else
			perr("cannot open directory. %i: %s\n", error->code, error->message);
		g_error_free0(error);
	}
}

