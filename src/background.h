/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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
 *
 */

#ifndef _SG_BACKGROUND_H_
#define _SG_BACKGROUND_H_




#include <cstdint>

#include "slav_qt.h"


#include "window.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




typedef void(* vik_thr_free_func)(void *);
typedef void(* vik_thr_func)(void *,void *);

typedef enum {
	BACKGROUND_POOL_REMOTE, /* i.e. Network requests - can have an arbitary large pool. */
	BACKGROUND_POOL_LOCAL,  /* i.e. CPU bound tasks - pool should be no larger than available CPUs for best performance. */
#ifdef HAVE_LIBMAPNIK
	BACKGROUND_POOL_LOCAL_MAPNIK,  /* Due to potential issues with multi-threading a separate configurable pool for Mapnik. */
#endif
} Background_Pool_Type;




void a_background_thread(Background_Pool_Type bp, GtkWindow * parent, char const * message, vik_thr_func func, void * userdata, vik_thr_free_func userdata_free_func, vik_thr_free_func userdata_cancel_cleanup_func, int number_items);
int a_background_thread_progress(void * callbackdata, double fraction);
int a_background_testcancel(void * callbackdata);
void a_background_show_window();
void a_background_init();
void a_background_post_init();
void a_background_uninit();
void a_background_add_window(SlavGPS::Window * window);
void a_background_remove_window(SlavGPS::Window * window);




#endif /* #ifndef _SG_BACKGROUND_H_ */
