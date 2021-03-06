/**
* +----------------------------------------------------------------------+
* | This file is part of Samplecat. http://ayyi.github.io/samplecat/     |
* | copyright (C) 2012-2016 Tim Orford <tim@orford.org>                  |
* +----------------------------------------------------------------------+
* | This program is free software; you can redistribute it and/or modify |
* | it under the terms of the GNU General Public License version 3       |
* | as published by the Free Software Foundation.                        |
* +----------------------------------------------------------------------+
*
*/
#define __wf_private__
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <GL/gl.h>
#include <pango/pangofc-font.h>
#include <pango/pangofc-fontmap.h>
#include "agl/ext.h"
#include "agl/utils.h"
#include "agl/actor.h"
#include "agl/pango_render.h"
#include "waveform/waveform.h"
#include "waveform/peakgen.h"
#include "waveform/shader.h"
#include "waveform/actors/text.h"
#include "samplecat.h"
#include "views/search.h"

extern int need_draw;

#define _g_object_unref0(var) ((var == NULL) ? NULL : (var = (g_object_unref (var), NULL)))

#define PADDING 1
#define BORDER 1

static AGl* agl = NULL;
static int instance_count = 0;

static void
_init()
{
	static bool init_done = false;

	if(!init_done){
		agl = agl_get_instance();
		agl_set_font_string("Roboto 10"); // initialise the pango context

		init_done = true;
	}
}

AGlActor*
search_view(WaveformActor* _)
{
	instance_count++;

	_init();

	bool search_paint(AGlActor* actor)
	{
		SearchView* view = (SearchView*)actor;

		agl_enable_stencil(0, 0, actor->region.x2, actor->region.y2);

		// border
		agl->shaders.plain->uniform.colour = 0x6677ff77;
		agl_use_program((AGlShader*)agl->shaders.plain);
		agl_rect_((AGlRect){0, 0, agl_actor__width(actor), BORDER});
		agl_rect_((AGlRect){agl_actor__width(actor) - BORDER, 0, BORDER, agl_actor__height(actor) - 2});
		agl_rect_((AGlRect){0, agl_actor__height(actor) - 2, agl_actor__width(actor), BORDER});
		agl_rect_((AGlRect){0, 0, BORDER, agl_actor__height(actor) - 2});

		if(actor->root->selected == actor){
			agl->shaders.plain->uniform.colour = 0x777777ff;
			agl_use_program((AGlShader*)agl->shaders.plain);
			agl_rect_((AGlRect){0, 0, agl_actor__width(actor), agl_actor__height(actor)});

			//cursor
  			PangoRectangle rect;
			pango_layout_get_cursor_pos(view->layout, view->cursor_pos, &rect, NULL);
			agl->shaders.plain->uniform.colour = 0xffffffff;
			agl_use_program((AGlShader*)agl->shaders.plain);
			agl_rect_((AGlRect){2 + PANGO_PIXELS(rect.x), 2, 1, agl_actor__height(actor) - 4});
		}

		agl_print_layout(2, 2, 0, 0xffffffff, view->layout);

		agl_disable_stencil();

		return true;
	}

	void search_init(AGlActor* a)
	{
#ifdef AGL_ACTOR_RENDER_CACHE
		a->fbo = agl_fbo_new(agl_actor__width(a), agl_actor__height(a), 0, AGL_FBO_HAS_STENCIL);
		a->cache.enabled = true;
#endif
	}

	void search_size(AGlActor* actor)
	{
	}

	void search_layout(SearchView* view)
	{
		if(!view->layout){
			PangoGlRendererClass* PGRC = g_type_class_peek(PANGO_TYPE_GL_RENDERER);
			view->layout = pango_layout_new (PGRC->context);
		}
		pango_layout_set_text(view->layout, view->text ? view->text : ((SamplecatFilter*)samplecat.model->filters.search)->value, -1);

		agl_actor__invalidate((AGlActor*)view);
	}

	bool search_event(AGlActor* actor, GdkEvent* event, AGliPt xy)
	{
		PF0;
		SearchView* view = (SearchView*)actor;

		switch(event->type){
			case GDK_KEY_PRESS:
				g_return_val_if_fail(actor->root->selected, AGL_NOT_HANDLED);
				dbg(0, "Keypress");
				int val = ((GdkEventKey*)event)->keyval;
				switch(val){
					case XK_Left:
						view->cursor_pos = MAX(0, view->cursor_pos - 1);
						agl_actor__invalidate(actor);
						break;
					case XK_Right:
						view->cursor_pos = MIN(strlen(((SamplecatFilter*)samplecat.model->filters.search)->value), view->cursor_pos + 1);
						agl_actor__invalidate(actor);
						break;
					case XK_Return:
						dbg(0, "RET");
						samplecat_filter_set_value(samplecat.model->filters.search, view->text);
						view->text = NULL;
						break;
					default:
						if(view->text){
							char* t = view->text;
							view->text = g_strdup_printf("%s%c", view->text, val);
							g_free(t);
						}else{
							view->text = g_strdup_printf("%s%c", ((SamplecatFilter*)samplecat.model->filters.search)->value, val);
						}
						search_layout(view);
						break;
				}
				break;
			default:
				break;
		}
		return AGL_HANDLED;
	}

	void search_free(AGlActor* actor)
	{
		SearchView* view = (SearchView*)actor;

		if(view->layout) _g_object_unref0(view->layout);

		if(!--instance_count){
		}
	}

	SearchView* view = g_new0(SearchView, 1);
	AGlActor* actor = (AGlActor*)view;
#ifdef AGL_DEBUG_ACTOR
	actor->name = "Search";
#endif
	actor->init = search_init;
	actor->free = search_free;
	actor->paint = search_paint;
	actor->set_size = search_size;
	actor->on_event = search_event;

	void on_search_filter_changed(GObject* _filter, gpointer _actor)
	{
		search_layout(_actor);
	}
	g_signal_connect(samplecat.model->filters.search, "changed", G_CALLBACK(on_search_filter_changed), actor);

	search_layout(view);

	return actor;
}


int
search_view_height(SearchView* view)
{
	if(view->layout){
		PangoRectangle logical_rect;
		pango_layout_get_pixel_extents(view->layout, NULL, &logical_rect);
		return logical_rect.height - logical_rect.y + 2 * PADDING + 2 * BORDER;
	}
	return 23;
}


