/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2015, Rob Norris <rw_norris@hotmail.com>
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


#include <cstdlib>
#include <list>

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "background.h"
#include "settings.h"
#include "util.h"
#include "math.h"
#include "uibuilder.h"
#include "globals.h"
#include "preferences.h"
//#include "viking.h"
#include "vikwindow.h"




using namespace SlavGPS;




static GThreadPool *thread_pool_remote = NULL;
static GThreadPool *thread_pool_local = NULL;
#ifdef HAVE_LIBMAPNIK
static GThreadPool *thread_pool_local_mapnik = NULL;
#endif
static bool stop_all_threads = false;

static GtkWidget *bgwindow = NULL;
static GtkWidget *bgtreeview = NULL;
static GtkListStore *bgstore = NULL;

/* Still only actually updating the statusbar though. */
static std::list<Window *> windows_to_update;

static int bgitemcount = 0;




typedef struct {
	bool some_arg;
	vik_thr_func func;
	void * userdata;
	vik_thr_free_func userdata_free_func;
	vik_thr_free_func userdata_cancel_cleanup_func;
	GtkTreeIter * iter;
	int number_items;
} thread_args;


enum {
	TITLE_COLUMN = 0,
	PROGRESS_COLUMN,
	DATA_COLUMN,
	N_COLUMNS,
};




static void background_thread_update()
{
	static char buf[20];
	for (auto i = windows_to_update.begin(); i != windows_to_update.end(); i++) {
		snprintf(buf, sizeof(buf), _("%d items"), bgitemcount);
		(*i)->statusbar_update(buf, VIK_STATUSBAR_ITEMS);
	}

	return;
}




/**
 * @callbackdata: Thread data
 * @fraction:     The value should be between 0.0 and 1.0 indicating percentage of the task complete
 */
int a_background_thread_progress(void * callbackdata, double fraction)
{
	thread_args * args = (thread_args *) callbackdata;

	int res = a_background_testcancel(callbackdata);
	if (args->iter) {
		double myfraction = fabs(fraction);
		if (myfraction > 1.0) {
			myfraction = 1.0;
		}
		gdk_threads_enter();
		gtk_list_store_set(GTK_LIST_STORE(bgstore), args->iter, PROGRESS_COLUMN, myfraction * 100, -1);
		gdk_threads_leave();
	}

	args->number_items--;
	bgitemcount--;
	background_thread_update();
	return res;
}




static void thread_die(thread_args * args)
{
	if (args->userdata_free_func) {
		args->userdata_free_func(args->userdata);
	}

	if (args->number_items) {
		bgitemcount -= args->number_items;
		background_thread_update();
	}

	free(args->iter);
	free(args);
}




int a_background_testcancel(void * callbackdata)
{
	if (stop_all_threads) {
		return -1;
	}

	thread_args * args = (thread_args *) callbackdata;

	if (args && args->some_arg) {
		if (args->userdata_free_func) {
			args->userdata_free_func(args->userdata);
		}
		return -1;
	}
	return 0;
}




static void thread_helper(void * args_, void * user_data)
{
	thread_args * args = (thread_args *) args_;

	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);

	args->func(args->userdata, args);

	gdk_threads_enter();
	if (!args->some_arg) {
		gtk_list_store_remove(bgstore, args->iter);
	}
	gdk_threads_leave();

	thread_die(args);
}

/**
 * @bp:      Which pool this thread should run in
 * @parent:
 * @message:
 * @func: worker function
 * @userdata:
 * @userdata_free_func: free function for userdata
 * @userdata_cancel_cleanup_func:
 * @number_items:
 *
 * Function to enlist new background function.
 */
void a_background_thread(Background_Pool_Type bp, GtkWindow *parent, const char *message, vik_thr_func func, void * userdata, vik_thr_free_func userdata_free_func, vik_thr_free_func userdata_cancel_cleanup_func, int number_items)
{
	GtkTreeIter * iter = (GtkTreeIter *) malloc(sizeof (GtkTreeIter));
	thread_args * args = (thread_args *) malloc(sizeof (thread_args));

	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);

	args->some_arg = 0;
	args->func = func;
	args->userdata = userdata;
	args->userdata_free_func = userdata_free_func;
	args->userdata_cancel_cleanup_func = userdata_cancel_cleanup_func;
	args->iter = iter;
	args->number_items = number_items;

	bgitemcount += number_items;

	gtk_list_store_append(bgstore, iter);
	gtk_list_store_set(bgstore, iter,
			   TITLE_COLUMN, message,
			   PROGRESS_COLUMN, 0.0,
			   DATA_COLUMN, args,
			   -1);

	/* Run the thread in the background. */
	if (bp == BACKGROUND_POOL_REMOTE) {
		g_thread_pool_push(thread_pool_remote, args, NULL);
#ifdef HAVE_LIBMAPNIK
	} else if (bp == BACKGROUND_POOL_LOCAL_MAPNIK) {
		g_thread_pool_push(thread_pool_local_mapnik, args, NULL);

#endif
	} else {
		g_thread_pool_push(thread_pool_local, args, NULL);
	}
}




/**
 * Display the background window.
 */
void a_background_show_window()
{
	gtk_widget_show_all(bgwindow);
}




static void cancel_job_with_iter(GtkTreeIter * iter)
{
	thread_args * args = NULL;

	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);

	gtk_tree_model_get(GTK_TREE_MODEL(bgstore), iter, DATA_COLUMN, &args, -1);

	/* We know args still exists because it is free _after_ the list item is destroyed. */
	/* Need MUTEX? */
	args->some_arg = 1; /* Set killswitch. */

	gtk_list_store_remove(bgstore, iter);
	args->iter = NULL;
}




static void bgwindow_response(GtkDialog * dialog, int arg1)
{
	/* Note this function is a signal handler called back from the
	   GTK main loop, so GDK is already locked.  We need to
	   release the lock before calling thread-safe routines. */
	if (arg1 == 1) { /* Cancel. */
		GtkTreeIter iter;
		if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection (GTK_TREE_VIEW(bgtreeview)), NULL, &iter)) {
			cancel_job_with_iter(&iter);
		}
		gdk_threads_leave();
		background_thread_update();
		gdk_threads_enter();
	} else if (arg1 == 2) { /* Clear. */
		GtkTreeIter iter;
		while (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(bgstore), &iter)) {
			cancel_job_with_iter(&iter);
		}
		gdk_threads_leave();
		background_thread_update();
		gdk_threads_enter();
	} else { /* OK. */
		gtk_widget_hide(bgwindow);
	}
}




#define VIK_SETTINGS_BACKGROUND_MAX_THREADS "background_max_threads"
#define VIK_SETTINGS_BACKGROUND_MAX_THREADS_LOCAL "background_max_threads_local"

#ifdef HAVE_LIBMAPNIK
VikLayerParamScale params_threads[] = { {1, 64, 1, 0} }; /* 64 threads should be enough for anyone... */
/* Implicit use of 'MAPNIK_PREFS_NAMESPACE' to avoid dependency issues. */
static VikLayerParam prefs_mapnik[] = {
	{ LayerType::NUM_TYPES, "mapnik.background_max_threads_local_mapnik", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Threads:"), VIK_LAYER_WIDGET_SPINBUTTON, params_threads, NULL,
	  N_("Number of threads to use for Mapnik tasks. You need to restart Viking for a change to this value to be used"), NULL, NULL, NULL },
};
#endif




/**
 * Just setup any preferences.
 */
void a_background_init()
{
#ifdef HAVE_LIBMAPNIK
	VikLayerParamData tmp;
	/* Implicit use of 'MAPNIK_PREFS_NAMESPACE' to avoid dependency issues. */
	tmp.u = 1; /* Default to 1 thread due to potential crashing issues. */
	a_preferences_register(&prefs_mapnik[0], tmp, "mapnik");
#endif
}




/**
 * Initialize background feature.
 */
void a_background_post_init()
{
	/* Initialize thread pools. */
	int max_threads = 10;  /* Limit maximum number of threads running at one time. */
	int maxt;
	if (a_settings_get_integer(VIK_SETTINGS_BACKGROUND_MAX_THREADS, &maxt)) {
		max_threads = maxt;
	}

	thread_pool_remote = g_thread_pool_new((GFunc) thread_helper, NULL, max_threads, false, NULL);

	if (a_settings_get_integer(VIK_SETTINGS_BACKGROUND_MAX_THREADS_LOCAL, &maxt)) {
		max_threads = maxt;
	} else {
		unsigned int cpus = util_get_number_of_cpus();
		max_threads = cpus > 1 ? cpus-1 : 1; /* Don't use all available CPUs! */
	}

	thread_pool_local = g_thread_pool_new((GFunc) thread_helper, NULL, max_threads, false, NULL);

#ifdef HAVE_LIBMAPNIK
	/* Implicit use of 'MAPNIK_PREFS_NAMESPACE' to avoid dependency issues. */
	unsigned int mapnik_threads = a_preferences_get("mapnik.background_max_threads_local_mapnik")->u;
	thread_pool_local_mapnik = g_thread_pool_new((GFunc) thread_helper, NULL, mapnik_threads, false, NULL);
#endif

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkWidget *scrolled_window;

	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);

	/* Store & treeview. */
	bgstore = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_POINTER);
	bgtreeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(bgstore));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW (bgtreeview), true);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW (bgtreeview)),
				    GTK_SELECTION_SINGLE);

	/* Add columns. */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Job"), renderer, "text", TITLE_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(bgtreeview), column);

	renderer = gtk_cell_renderer_progress_new();
	column = gtk_tree_view_column_new_with_attributes(_("Progress"), renderer, "value", PROGRESS_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(bgtreeview), column);

	/* Setup window. */
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scrolled_window), bgtreeview);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	bgwindow = gtk_dialog_new_with_buttons("", NULL, (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_DELETE, 1, GTK_STOCK_CLEAR, 2, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(bgwindow), GTK_RESPONSE_ACCEPT);
	GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
	response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(bgwindow), GTK_RESPONSE_ACCEPT);
#endif
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(bgwindow))), scrolled_window, true, true, 0);
	gtk_window_set_default_size(GTK_WINDOW(bgwindow), 400, 400);
	gtk_window_set_title(GTK_WINDOW(bgwindow), _("Viking Background Jobs"));
	if (response_w) {
		gtk_widget_grab_focus(response_w);
	}
	/* Don't destroy win. */
	g_signal_connect(G_OBJECT(bgwindow), "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

	g_signal_connect(G_OBJECT(bgwindow), "response", G_CALLBACK(bgwindow_response), 0);

}




/**
 * Uninitialize background feature.
 */
void a_background_uninit()
{
	stop_all_threads = true;
	/* Wait until these threads stop. */
	g_thread_pool_free(thread_pool_remote, true, true);
	/* Don't wait for these. */
	g_thread_pool_free(thread_pool_local, true, false);
#ifdef HAVE_LIBMAPNIK
	g_thread_pool_free(thread_pool_local_mapnik, true, false);
#endif

	gtk_list_store_clear(bgstore);
	g_object_unref(bgstore);

	gtk_widget_destroy(bgwindow);
}




void a_background_add_window(Window * window)
{
	windows_to_update.push_front(window);
}




void a_background_remove_window(Window * window)
{
	windows_to_update.remove(window);
}
