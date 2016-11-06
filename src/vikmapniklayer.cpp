/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cctype>

//#include "viking.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "vikutils.h"
#include "map_ids.h"
#include "maputils.h"
#include "mapcoord.h"
#include "mapcache.h"
#include "dir.h"
#include "util.h"
#include "ui_util.h"
#include "preferences.h"
#include "icons/icons.h"
#include "mapnik_interface.h"
#include "background.h"
#include "dialog.h"
#include "globals.h"

#include "vikmapslayer.h"
#include "vikmapniklayer.h"
#include "vikfileentry.h"
#include "file.h"
#include "vik_compat.h"




using namespace SlavGPS;




static LayerParamData file_default(void)
{
	LayerParamData data;
	data.s = "";
	return data;
}

static LayerParamData size_default(void) { return VIK_LPD_UINT (256); }
static LayerParamData alpha_default(void) { return VIK_LPD_UINT (255); }

static LayerParamData cache_dir_default(void)
{
	LayerParamData data;
	data.s = g_strconcat(maps_layer_default_dir(), "MapnikRendering", NULL);
	return data;
}

static ParameterScale scales[] = {
	{ 0, 255, 5, 0 },   /* Alpha. */
	{ 64, 1024, 8, 0 }, /* Tile size. */
	{ 0, 1024, 12, 0 }, /* Rerender timeout hours. */
};

Parameter mapnik_layer_params[] = {
	{ LayerType::MAPNIK, "config-file-mml", LayerParamType::STRING,  VIK_LAYER_GROUP_NONE, N_("CSS (MML) Config File:"), LayerWidgetType::FILEENTRY,   KINT_TO_POINTER(VF_FILTER_CARTO), NULL, N_("CartoCSS configuration file"),   file_default,         NULL, NULL },
	{ LayerType::MAPNIK, "config-file-xml", LayerParamType::STRING,  VIK_LAYER_GROUP_NONE, N_("XML Config File:"),       LayerWidgetType::FILEENTRY,   KINT_TO_POINTER(VF_FILTER_XML),   NULL, N_("Mapnik XML configuration file"), file_default,         NULL, NULL },
	{ LayerType::MAPNIK, "alpha",           LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Alpha:"),                 LayerWidgetType::HSCALE,      &scales[0],                       NULL, NULL,                                alpha_default,        NULL, NULL },
	{ LayerType::MAPNIK, "use-file-cache",  LayerParamType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Use File Cache:"),        LayerWidgetType::CHECKBUTTON, NULL,                             NULL, NULL,                                vik_lpd_true_default, NULL, NULL },
	{ LayerType::MAPNIK, "file-cache-dir",  LayerParamType::STRING,  VIK_LAYER_GROUP_NONE, N_("File Cache Directory:"),  LayerWidgetType::FOLDERENTRY, NULL,                             NULL, NULL,                                cache_dir_default,    NULL, NULL },

	{ LayerType::NUM_TYPES, NUM_PARAMS,       NULL,             LayerParamType::PTR,    VIK_LAYER_GROUP_NONE, NULL,                  LayerWidgetType::CHECKBUTTON, NULL,            NULL, NULL, NULL,                   NULL, NULL }, /* Guard. */
};

enum {
	PARAM_CONFIG_CSS = 0,
	PARAM_CONFIG_XML,
	PARAM_ALPHA,
	PARAM_USE_FILE_CACHE,
	PARAM_FILE_CACHE_DIR,
	NUM_PARAMS
};




static Layer * mapnik_layer_unmarshall(uint8_t *data, int len, Viewport * viewport);
static LayerTool * mapnik_feature_create(Window * window, Viewport * viewport);
static bool mapnik_feature_release_cb(Layer * layer, GdkEventButton *event, LayerTool * tool);

/* See comment in viktrwlayer.c for advice on values used.
   FUTURE: */
static LayerTool * mapnik_tools[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	// Layer Info
	// Zoom All?




VikLayerInterface vik_mapnik_layer_interface = {
	"Mapnik Rendering",
	N_("Mapnik Rendering"),
	NULL,
	&vikmapniklayer_pixbuf, // icon

	{ mapnik_feature_create, NULL, NULL, NULL, NULL, NULL, NULL }, /* (VikToolConstructorFunc) */
	mapnik_tools,
	1,

	mapnik_layer_params,
	NUM_PARAMS,
	NULL,
	0,

	VIK_MENU_ITEM_ALL,

	/* (VikLayerFuncUnmarshall) */    mapnik_layer_unmarshall,
	/* (VikLayerFuncChangeParam) */   NULL,
};




#define MAPNIK_PREFS_GROUP_KEY "mapnik"
#define MAPNIK_PREFS_NAMESPACE "mapnik."




static LayerParamData plugins_default(void)
{
	LayerParamData data;
#ifdef WINDOWS
	data.s = strdup("input");
#else
	if (g_file_test("/usr/lib/mapnik/input", G_FILE_TEST_EXISTS)) {
		data.s = strdup("/usr/lib/mapnik/input");
		/* Current Debian locations. */
	} else if (g_file_test("/usr/lib/mapnik/3.0/input", G_FILE_TEST_EXISTS)) {
		data.s = strdup("/usr/lib/mapnik/3.0/input");
	} else if (g_file_test("/usr/lib/mapnik/2.2/input", G_FILE_TEST_EXISTS)) {
		data.s = strdup("/usr/lib/mapnik/2.2/input");
	} else {
		data.s = strdup("");
	}
#endif
	return data;
}




static LayerParamData fonts_default(void)
{
	/* Possibly should be string list to allow loading from multiple directories. */
	LayerParamData data;
#ifdef WINDOWS
	data.s = strdup("C:\\Windows\\Fonts");
#elif defined __APPLE__
	data.s = strdup("/Library/Fonts");
#else
	data.s = strdup("/usr/share/fonts");
#endif
	return data;
}




static Parameter prefs[] = {
	/* Changing these values only applies before first mapnik layer is 'created' */
	{ LayerType::NUM_TYPES, MAPNIK_PREFS_NAMESPACE"plugins_directory",       LayerParamType::STRING, VIK_LAYER_GROUP_NONE,  N_("Plugins Directory:"),        LayerWidgetType::FOLDERENTRY, NULL,       NULL, N_("You need to restart Viking for a change to this value to be used"), plugins_default, NULL, NULL },
	{ LayerType::NUM_TYPES, MAPNIK_PREFS_NAMESPACE"fonts_directory",         LayerParamType::STRING, VIK_LAYER_GROUP_NONE,  N_("Fonts Directory:"),          LayerWidgetType::FOLDERENTRY, NULL,       NULL, N_("You need to restart Viking for a change to this value to be used"), fonts_default, NULL, NULL },
	{ LayerType::NUM_TYPES, MAPNIK_PREFS_NAMESPACE"recurse_fonts_directory", LayerParamType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Recurse Fonts Directory:"),  LayerWidgetType::CHECKBUTTON, NULL,       NULL, N_("You need to restart Viking for a change to this value to be used"), vik_lpd_true_default, NULL, NULL },
	{ LayerType::NUM_TYPES, MAPNIK_PREFS_NAMESPACE"rerender_after",          LayerParamType::UINT, VIK_LAYER_GROUP_NONE,    N_("Rerender Timeout (hours):"), LayerWidgetType::SPINBUTTON,  &scales[2], NULL, N_("You need to restart Viking for a change to this value to be used"), NULL, NULL, NULL },
	/* Changeable any time. */
	{ LayerType::NUM_TYPES, MAPNIK_PREFS_NAMESPACE"carto",                   LayerParamType::STRING, VIK_LAYER_GROUP_NONE,  N_("CartoCSS:"),                 LayerWidgetType::FILEENTRY,   NULL,       NULL,  N_("The program to convert CartoCSS files into Mapnik XML"), NULL, NULL, NULL },
};




static time_t planet_import_time;
static GMutex *tp_mutex;
static GHashTable *requests = NULL;




/**
 * Just initialize preferences.
 */
void SlavGPS::vik_mapnik_layer_init(void)
{
	a_preferences_register_group(MAPNIK_PREFS_GROUP_KEY, "Mapnik");

	unsigned int i = 0;
	LayerParamData tmp = plugins_default();
	a_preferences_register(&prefs[i++], tmp, MAPNIK_PREFS_GROUP_KEY);

	tmp = fonts_default();
	a_preferences_register(&prefs[i++], tmp, MAPNIK_PREFS_GROUP_KEY);

	tmp.b = true;
	a_preferences_register(&prefs[i++], tmp, MAPNIK_PREFS_GROUP_KEY);

	tmp.u = 168; /* One week. */
	a_preferences_register(&prefs[i++], tmp, MAPNIK_PREFS_GROUP_KEY);

	tmp.s = "carto";
	a_preferences_register(&prefs[i++], tmp, MAPNIK_PREFS_GROUP_KEY);
}




/**
 * Initialize data structures - now that reading preferences is OK to perform.
 */
void SlavGPS::vik_mapnik_layer_post_init(void)
{
	tp_mutex = vik_mutex_new();

	/* Just storing keys only. */
	requests = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	unsigned int hours = a_preferences_get(MAPNIK_PREFS_NAMESPACE"rerender_after")->u;
	GDateTime *now = g_date_time_new_now_local();
	GDateTime *then = g_date_time_add_hours(now, -hours);
	planet_import_time = g_date_time_to_unix(then);
	g_date_time_unref(now);
	g_date_time_unref(then);

	GStatBuf gsb;
	/* Similar to mod_tile method to mark DB has been imported/significantly changed to cause a rerendering of all tiles. */
	char *import_time_file = g_strconcat(get_viking_dir(), G_DIR_SEPARATOR_S, "planet-import-complete", NULL);
	if (g_stat(import_time_file, &gsb) == 0) {
		/* Only update if newer. */
		if (planet_import_time > gsb.st_mtime) {
			planet_import_time = gsb.st_mtime;
		}
	}
	free(import_time_file);
}




void SlavGPS::vik_mapnik_layer_uninit()
{
	vik_mutex_free(tp_mutex);
}




/* NB Only performed once per program run. */
void SlavGPS::layer_mapnik_init(void)
{
	LayerParamData *pd = a_preferences_get(MAPNIK_PREFS_NAMESPACE"plugins_directory");
	LayerParamData *fd = a_preferences_get(MAPNIK_PREFS_NAMESPACE"fonts_directory");
	LayerParamData *rfd = a_preferences_get(MAPNIK_PREFS_NAMESPACE"recurse_fonts_directory");

	if (pd && fd && rfd) {
		mapnik_interface_initialize(pd->s, fd->s, rfd->b);
	} else {
		fprintf(stderr, "CRITICAL: Unable to initialize mapnik interface from preferences\n");
	}
}




char const * LayerMapnik::tooltip()
{
	return this->filename_xml;
}




void LayerMapnik::set_file_xml(char const * name)
{
	if (this->filename_xml) {
		free(this->filename_xml);
	}
	/* Mapnik doesn't seem to cope with relative filenames. */
	if (strcmp(name, "")) {
		this->filename_xml = vu_get_canonical_filename(this, name);
	} else {
		this->filename_xml = g_strdup(name);
	}
}




void LayerMapnik::set_file_css(char const * name)
{
	if (this->filename_css) {
		free(this->filename_css);
	}
	this->filename_css = g_strdup(name);
}




void LayerMapnik::set_cache_dir(char const * name)
{
	if (this->file_cache_dir) {
		free(this->file_cache_dir);
	}
	this->file_cache_dir = g_strdup(name);
}




static Layer * mapnik_layer_unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	LayerMapnik * layer = new LayerMapnik();

	layer->tile_size_x = size_default().u; /* FUTURE: Is there any use in this being configurable? */
	layer->loaded = false;
	layer->mi = mapnik_interface_new();
	layer->unmarshall_params(data, len, viewport);

	return layer;
}




bool LayerMapnik::set_param_value(uint16_t id, LayerParamValue data, Viewport * viewport, bool is_file_operation)
{
	switch (id) {
		case PARAM_CONFIG_CSS:
			this->set_file_css(data.s);
			break;
		case PARAM_CONFIG_XML:
			this->set_file_xml(data.s);
			break;
		case PARAM_ALPHA:
			if (data.u <= 255) {
				this->alpha = data.u;
			}
			break;
		case PARAM_USE_FILE_CACHE:
			this->use_file_cache = data.b;
			break;
		case PARAM_FILE_CACHE_DIR:
			this->set_cache_dir(data.s);
			break;
		default: break;
	}
	return true;
}




LayerParamValue LayerMapnik::get_param_value(layer_param_id_t id, bool is_file_operation) const
{
	LayerParamValue param_value;
	switch (id) {
		case PARAM_CONFIG_CSS: {
			param_value.s = this->filename_css;
			bool set = false;
			if (is_file_operation) {
				if (a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
					char *cwd = g_get_current_dir();
					if (cwd) {
						param_value.s = file_GetRelativeFilename(cwd, this->filename_css);
						if (!param_value.s) {
							param_value.s = "";
						}
						set = true;
					}
				}
			}
			if (!set) {
				param_value.s = this->filename_css ? this->filename_css : "";
			}
			break;
		}
		case PARAM_CONFIG_XML: {
			param_value.s = this->filename_xml;
			bool set = false;
			if (is_file_operation) {
				if (a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
					char *cwd = g_get_current_dir();
					if (cwd) {
						param_value.s = file_GetRelativeFilename(cwd, this->filename_xml);
						if (!param_value.s) {
							param_value.s = "";
						}
						set = true;
					}
				}
			}
			if (!set) {
				param_value.s = this->filename_xml ? this->filename_xml : "";
			}
			break;
		}
		case PARAM_ALPHA:
			param_value.u = this->alpha;
			break;
		case PARAM_USE_FILE_CACHE:
			param_value.b = this->use_file_cache;
			break;
		case PARAM_FILE_CACHE_DIR:
			param_value.s = this->file_cache_dir;
			break;
		default: break;
	}
	return param_value;
}




/**
 * Run carto command.
 * ATM don't have any version issues AFAIK.
 * Tested with carto 0.14.0.
 */
bool LayerMapnik::carto_load(Viewport * viewport)
{
	char *mystdout = NULL;
	char *mystderr = NULL;
	GError *error = NULL;

	LayerParamData *vlpd = a_preferences_get(MAPNIK_PREFS_NAMESPACE"carto");
	char *command = g_strdup_printf("%s %s", vlpd->s, this->filename_css);

	bool answer = true;
	//char *args[2]; args[0] = vlpd->s; args[1] = this->filename_css;
	//GPid pid;
	//if (g_spawn_async_with_pipes(NULL, args, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &pid, NULL, &carto_stdout, &carto_error, &error)) {
	// cf code in babel.c to handle stdout

	/* NB Running carto may take several seconds, especially for
	   large style sheets like the default OSM Mapnik style (~6
	   seconds on my system). */
	Window * window = viewport->get_window();
	if (window) {
		char *msg = g_strdup_printf("%s: %s", _("Running"), command);
		window->statusbar_update(msg, VIK_STATUSBAR_INFO);
		window->set_busy_cursor();
	}

	int64_t tt1 = 0;
	int64_t tt2 = 0;
	/* You won't get a sensible timing measurement if running too old a GLIB. */
#if GLIB_CHECK_VERSION (2, 28, 0)
	tt1 = g_get_real_time();
#endif

	if (g_spawn_command_line_sync(command, &mystdout, &mystderr, NULL, &error)) {
#if GLIB_CHECK_VERSION (2, 28, 0)
		tt2 = g_get_real_time();
#endif
		if (mystderr)
			if (strlen(mystderr) > 1) {
				a_dialog_error_msg_extra(viewport->get_toolkit_window(), _("Error running carto command:\n%s"), mystderr);
				answer = false;
			}
		if (mystdout) {
			/* NB This will overwrite the specified XML file. */
			if (! (this->filename_xml && strlen(this->filename_xml) > 1)) {
				/* XML Not specified so try to create based on CSS file name. */
				GRegex *regex = g_regex_new("\\.mml$|\\.mss|\\.css$", G_REGEX_CASELESS, (GRegexMatchFlags) 0, &error);
				if (error) {
					fprintf(stderr, "CRITICAL: %s: %s\n", __FUNCTION__, error->message);
				}
				if (this->filename_xml) {
					free(this->filename_xml);
				}
				this->filename_xml = g_regex_replace_literal(regex, this->filename_css, -1, 0, ".xml", (GRegexMatchFlags) 0, &error);
				if (error) {
					fprintf(stderr, "WARNING: %s: %s\n", __FUNCTION__, error->message);
				}
				/* Prevent overwriting self. */
				if (!g_strcmp0(this->filename_xml, this->filename_css)) {
					this->filename_xml = g_strconcat(this->filename_css, ".xml", NULL);
				}
			}
			if (!g_file_set_contents(this->filename_xml, mystdout, -1, &error) ) {
				fprintf(stderr, "WARNING: %s: %s\n", __FUNCTION__, error->message);
				g_error_free(error);
			}
		}
		free(mystdout);
		free(mystderr);
	} else {
		fprintf(stderr, "WARNING: %s: %s\n", __FUNCTION__, error->message);
		g_error_free(error);
	}
	free(command);

	if (window) {
		char *msg = g_strdup_printf("%s %s %.1f %s",  vlpd->s, _(" completed in "), (double)(tt2-tt1)/G_USEC_PER_SEC, _("seconds"));
		window->statusbar_update(msg, VIK_STATUSBAR_INFO);
		free(msg);
		window->clear_busy_cursor();
	}
	return answer;
}




void LayerMapnik::post_read(Viewport * viewport, bool from_file)
{
	/* Determine if carto needs to be run. */
	bool do_carto = false;
	if (this->filename_css && strlen(this->filename_css) > 1) {
		if (this->filename_xml && strlen(this->filename_xml) > 1) {
			/* Compare timestamps. */
			GStatBuf gsb1;
			if (g_stat(this->filename_xml, &gsb1) == 0) {
				GStatBuf gsb2;
				if (g_stat(this->filename_css, &gsb2) == 0) {
					/* Is CSS file newer than the XML file. */
					if (gsb2.st_mtime > gsb1.st_mtime) {
						do_carto = true;
					} else {
						fprintf(stderr, "DEBUG: No need to run carto\n");
					}
				}
			} else {
				/* XML file doesn't exist. */
				do_carto = true;
			}
		} else {
			/* No XML specified thus need to generate. */
			do_carto = true;
		}
	}

	if (do_carto)
		/* Don't load the XML config if carto load fails. */
		if (!this->carto_load(viewport)) {
			return;
		}

	char* ans = mapnik_interface_load_map_file(this->mi, this->filename_xml, this->tile_size_x, this->tile_size_x);
	if (ans) {
		a_dialog_error_msg_extra(viewport->get_toolkit_window(),
					 _("Mapnik error loading configuration file:\n%s"),
					 ans);
		free(ans);
	} else {
		this->loaded = true;
		if (!from_file) {
			ui_add_recent_file(this->filename_xml);
		}
	}
}




#define MAPNIK_LAYER_FILE_CACHE_LAYOUT "%s" G_DIR_SEPARATOR_S"%d" G_DIR_SEPARATOR_S"%d" G_DIR_SEPARATOR_S"%d.png"




/* Free returned string after use. */
static char *get_filename(char *dir, unsigned int x, unsigned int y, unsigned int z)
{
	return g_strdup_printf(MAPNIK_LAYER_FILE_CACHE_LAYOUT, dir,(17-z), x, y);
}




void LayerMapnik::possibly_save_pixbuf(GdkPixbuf * pixbuf, TileInfo * ulm)
{
	if (this->use_file_cache) {
		if (this->file_cache_dir) {
			GError *error = NULL;
			char *filename = get_filename(this->file_cache_dir, ulm->x, ulm->y, ulm->scale);

			char *dir = g_path_get_dirname(filename);
			if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
				if (g_mkdir_with_parents(dir , 0777) != 0) {
					fprintf(stderr, "WARNING: %s: Failed to mkdir %s\n", __FUNCTION__, dir);
				}
			}
			free(dir);

			if (!gdk_pixbuf_save(pixbuf, filename, "png", &error, NULL)) {
				fprintf(stderr, "WARNING: %s: %s\n", __FUNCTION__, error->message);
				g_error_free(error);
			}
			free(filename);
		}
	}
}




typedef struct {
	LayerMapnik * lmk;
	VikCoord *ul;
	VikCoord *br;
	TileInfo *ulmc;
	const char* request;
} RenderInfo;




/**
 * Common render function which can run in separate thread.
 */
void LayerMapnik::render(VikCoord * ul, VikCoord * br, TileInfo * ulm)
{
	int64_t tt1 = g_get_real_time();
	GdkPixbuf *pixbuf = mapnik_interface_render(this->mi, ul->north_south, ul->east_west, br->north_south, br->east_west);
	int64_t tt2 = g_get_real_time();
	double tt = (double)(tt2-tt1)/1000000;
	fprintf(stderr, "DEBUG: Mapnik rendering completed in %.3f seconds\n", tt);
	if (!pixbuf) {
		/* A pixbuf to stick into cache incase of an unrenderable area - otherwise will get continually re-requested. */
		pixbuf = gdk_pixbuf_scale_simple(gdk_pixbuf_from_pixdata(&vikmapniklayer_pixbuf, false, NULL), this->tile_size_x, this->tile_size_x, GDK_INTERP_BILINEAR);
	}
	this->possibly_save_pixbuf(pixbuf, ulm);

	/* NB Mapnik can apply alpha, but use our own function for now. */
	if (this->alpha < 255) {
		pixbuf = ui_pixbuf_scale_alpha(pixbuf, this->alpha);
	}
	map_cache_add(pixbuf, (map_cache_extra_t){ tt }, ulm, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);
	g_object_unref(pixbuf);
}




static void render_info_free(RenderInfo *data)
{
	free(data->ul);
	free(data->br);
	free(data->ulmc);
	/* NB No need to free the request/key - as this is freed by the hash table destructor. */
	free(data);
}




static void background(RenderInfo *data, void * threaddata)
{
	int res = a_background_thread_progress(threaddata, 0);
	if (res == 0) {
		data->lmk->render(data->ul, data->br, data->ulmc);
	}

	g_mutex_lock(tp_mutex);
	g_hash_table_remove(requests, data->request);
	g_mutex_unlock(tp_mutex);

	if (res == 0) {
		data->lmk->emit_changed(); /* NB update display from background. */
	}
}




static void render_cancel_cleanup(RenderInfo *data)
{
	/* Anything? */
}




#define REQUEST_HASHKEY_FORMAT "%d-%d-%d-%d-%d"




/**
 * Thread.
 */
void LayerMapnik::thread_add(TileInfo * mul, VikCoord * ul, VikCoord * br, int x, int y, int z, int zoom, char const * name)
{
	/* Create request. */
	unsigned int nn = name ? g_str_hash(name) : 0;
	char *request = g_strdup_printf(REQUEST_HASHKEY_FORMAT, x, y, z, zoom, nn);

	g_mutex_lock(tp_mutex);

	if (g_hash_table_lookup_extended(requests, request, NULL, NULL)) {
		free(request);
		g_mutex_unlock(tp_mutex);
		return;
	}

	RenderInfo * ri = (RenderInfo *) malloc(sizeof(RenderInfo));
	ri->lmk = this;
	ri->ul = (VikCoord *) malloc(sizeof (VikCoord));
	ri->br = (VikCoord *) malloc(sizeof (VikCoord));
	ri->ulmc = (TileInfo *) malloc(sizeof (TileInfo));
	memcpy(ri->ul, ul, sizeof(VikCoord));
	memcpy(ri->br, br, sizeof(VikCoord));
	memcpy(ri->ulmc, mul, sizeof(TileInfo));
	ri->request = request;

	g_hash_table_insert(requests, request, NULL);

	g_mutex_unlock(tp_mutex);

	char *basename = g_path_get_basename(name);
	char * job_description = g_strdup_printf(_("Mapnik Render %d:%d:%d %s"), zoom, x, y, basename);
	free(basename);
	a_background_thread(BACKGROUND_POOL_LOCAL_MAPNIK,
			    job_description,
			    (vik_thr_func) background,             /* Worker function. */
			    ri,                                    /* Worker function. */
			    (vik_thr_free_func) render_info_free,  /* Function to free worker data. */
			    (vik_thr_free_func) render_cancel_cleanup,
			    1);
	free(job_description);
}




/**
 * If function returns GdkPixbuf properly, reference counter to this
 * buffer has to be decreased, when buffer is no longer needed.
 */
GdkPixbuf * LayerMapnik::load_pixbuf(TileInfo * ulm, TileInfo * brm, bool * rerender)
{
	*rerender = false;
	GdkPixbuf *pixbuf = NULL;
	char *filename = get_filename(this->file_cache_dir, ulm->x, ulm->y, ulm->scale);

	GStatBuf gsb;
	if (g_stat(filename, &gsb) == 0) {
		/* Get from disk. */
		GError *error = NULL;
		pixbuf = gdk_pixbuf_new_from_file(filename, &error);
		if (error) {
			fprintf(stderr, "WARNING: %s: %s\n", __FUNCTION__, error->message);
			g_error_free(error);
		} else {
			if (this->alpha < 255) {
				pixbuf = ui_pixbuf_set_alpha(pixbuf, this->alpha);
			}
			map_cache_add(pixbuf, (map_cache_extra_t) { -42.0 }, ulm, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);
		}
		/* If file is too old mark for rerendering. */
		if (planet_import_time < gsb.st_mtime) {
			*rerender = true;
		}
	}
	free(filename);

	return pixbuf;
}




/**
 * Caller has to decrease reference counter of returned GdkPixbuf,
 * when buffer is no longer needed.
 */
GdkPixbuf * LayerMapnik::get_pixbuf(TileInfo * ulm, TileInfo * brm)
{
	VikCoord ul; VikCoord br;

	map_utils_iTMS_to_vikcoord(ulm, &ul);
	map_utils_iTMS_to_vikcoord(brm, &br);

	GdkPixbuf * pixbuf = map_cache_get(ulm, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);
	if (pixbuf) {
		fprintf(stderr, "MapnikLayer: MAP CACHE HIT\n");
	} else {
		fprintf(stderr, "MapnikLayer: MAP CACHE MISS\n");

		bool rerender = false;
		if (this->use_file_cache && this->file_cache_dir)
			pixbuf = this->load_pixbuf(ulm, brm, &rerender);
		if (! pixbuf || rerender) {
			if (true) {
				this->thread_add(ulm, &ul, &br, ulm->x, ulm->y, ulm->z, ulm->scale, this->filename_xml);
			} else {
				/* Run in the foreground. */
				this->render(&ul, &br, ulm);
				this->emit_changed();
			}
		}
	}

	return pixbuf;
}




void LayerMapnik::draw(Viewport * viewport)
{
	if (!this->loaded) {
		return;
	}

	if (viewport->get_drawmode() != VIK_VIEWPORT_DRAWMODE_MERCATOR) {
		this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, _("Mapnik Rendering must be in Mercator mode"));
		return;
	}

	if (this->mi) {
		char *copyright = mapnik_interface_get_copyright(this->mi);
		if (copyright) {
			viewport->add_copyright(copyright);
		}
	}

	VikCoord ul, br;
	ul.mode = VIK_COORD_LATLON;
	br.mode = VIK_COORD_LATLON;
	viewport->screen_to_coord(0, 0, &ul);
	viewport->screen_to_coord(viewport->get_width(), viewport->get_height(), &br);

	double xzoom = viewport->get_xmpp();
	double yzoom = viewport->get_ympp();

	TileInfo ulm, brm;

	if (map_utils_vikcoord_to_iTMS(&ul, xzoom, yzoom, &ulm) &&
	     map_utils_vikcoord_to_iTMS(&br, xzoom, yzoom, &brm)) {
		/* TODO: Understand if tilesize != 256 does this need to use shrinkfactors? */
		GdkPixbuf *pixbuf;
		VikCoord coord;
		int xx, yy;

		int xmin = MIN(ulm.x, brm.x), xmax = MAX(ulm.x, brm.x);
		int ymin = MIN(ulm.y, brm.y), ymax = MAX(ulm.y, brm.y);

		/* Split rendering into a grid for the current viewport
		   thus each individual 'tile' can then be stored in the map cache. */
		for (int x = xmin; x <= xmax; x++) {
			for (int y = ymin; y <= ymax; y++) {
				ulm.x = x;
				ulm.y = y;
				brm.x = x+1;
				brm.y = y+1;

				pixbuf = this->get_pixbuf(&ulm, &brm);

				if (pixbuf) {
					map_utils_iTMS_to_vikcoord(&ulm, &coord);
					viewport->coord_to_screen(&coord, &xx, &yy);
					viewport->draw_pixbuf(pixbuf, 0, 0, xx, yy, this->tile_size_x, this->tile_size_x);
					g_object_unref(pixbuf);
				}
			}
		}

		/* Done after so drawn on top.
		   Just a handy guide to tile blocks. */
		if (vik_debug && vik_verbose) {
			GdkGC *black_gc = viewport->get_toolkit_widget()->style->black_gc;
			int width = viewport->get_width();
			int height = viewport->get_height();
			int xx, yy;
			ulm.x = xmin; ulm.y = ymin;
			map_utils_iTMS_to_center_vikcoord(&ulm, &coord);
			viewport->coord_to_screen(&coord, &xx, &yy);
			xx = xx - (this->tile_size_x/2);
			yy = yy - (this->tile_size_x/2); // Yes use X ATM
			for (int x = xmin; x <= xmax; x++) {
				viewport->draw_line(black_gc, xx, 0, xx, height);
				xx += this->tile_size_x;
			}
			for (int y = ymin; y <= ymax; y++) {
				viewport->draw_line(black_gc, 0, yy, width, yy);
				yy += this->tile_size_x; // Yes use X ATM
			}
		}
	}
}




LayerMapnik::~LayerMapnik()
{
	mapnik_interface_free(this->mi);
	if (this->filename_css) {
		free(this->filename_css);
	}

	if (this->filename_xml) {
		free(this->filename_xml);
	}
}



typedef struct {
	LayerMapnik * lmk;
	Viewport * viewport;
} menu_array_values;




static void mapnik_layer_flush_memory(menu_array_values * values)
{
	map_cache_flush_type(MAP_ID_MAPNIK_RENDER);
}




static void mapnik_layer_reload(menu_array_values * values)
{
	LayerMapnik * lmk = values->lmk;
	Viewport * viewport = values->viewport;

	lmk->post_read(viewport, false);
	lmk->draw(viewport);
}




/**
 * Force carto run.
 *
 * Most carto projects will consist of many files.
 * ATM don't have a way of detecting when any of the included files have changed.
 * Thus allow a manual method to force re-running carto.
 */
static void mapnik_layer_carto(menu_array_values * values)
{
	LayerMapnik * lmk = values->lmk;
	Viewport * viewport = values->viewport;

	/* Don't load the XML config if carto load fails. */
	if (!lmk->carto_load(viewport)) {
		return;
	}

	char * ans = mapnik_interface_load_map_file(lmk->mi, lmk->filename_xml, lmk->tile_size_x, lmk->tile_size_x);
	if (ans) {
		a_dialog_error_msg_extra(viewport->get_toolkit_window(),
					 _("Mapnik error loading configuration file:\n%s"),
					 ans);
		free(ans);
	} else {
		lmk->draw(viewport);
	}
}




/**
 * Show Mapnik configuration parameters.
 */
static void mapnik_layer_information(menu_array_values * values)
{
	LayerMapnik * lmk = values->lmk;

	if (!lmk->mi) {
		return;
	}
	GArray * array = mapnik_interface_get_parameters(lmk->mi);
	if (array->len) {
		a_dialog_list(lmk->get_toolkit_window(), _("Mapnik Information"), array, 1);
		/* Free the copied strings. */
		for (unsigned int i = 0; i < array->len; i++) {
			free(g_array_index(array, char*, i));
		}
	}
	g_array_free(array, false);
}




static void mapnik_layer_about(menu_array_values * values)
{
	LayerMapnik * lmk = values->lmk;

	char * msg = mapnik_interface_about();
	a_dialog_info_msg(lmk->get_toolkit_window(),  msg);
	free(msg);
}




void LayerMapnik::add_menu_items(QMenu & menu)
{
	LayersPanel * panel = (LayersPanel *) panel_;

	static menu_array_values values = {
		this,
		panel->get_viewport()
	};

	GtkWidget *item = gtk_menu_item_new();
	gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	/* Typical users shouldn't need to use this functionality - so debug only ATM. */
	if (vik_debug) {
		item = gtk_image_menu_item_new_with_mnemonic(_("_Flush Memory Cache"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_flush_memory), &values);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
	}

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_REFRESH, NULL);
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_reload), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	if (strcmp("", this->filename_css)) {
		item = gtk_image_menu_item_new_with_mnemonic(_("_Run Carto Command"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_carto), &values);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
	}

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_INFO, NULL);
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_information), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_about), &values);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);
}




static void mapnik_layer_rerender_cb(LayerMapnik * lmk)
{
	lmk->rerender();
}




/**
 * Rerender a specific tile.
 */
void LayerMapnik::rerender()
{
	TileInfo ulm;
	/* Requested position to map coord. */
	map_utils_vikcoord_to_iTMS(&this->rerender_ul, this->rerender_zoom, this->rerender_zoom, &ulm);
	/* Reconvert back - thus getting the coordinate at the tile *ul corner*. */
	map_utils_iTMS_to_vikcoord(&ulm, &this->rerender_ul);
	/* Bottom right bound is simply +1 in TMS coords. */
	TileInfo brm = ulm;
	brm.x = brm.x+1;
	brm.y = brm.y+1;
	map_utils_iTMS_to_vikcoord(&brm, &this->rerender_br);
	this->thread_add(&ulm, &this->rerender_ul, &this->rerender_br, ulm.x, ulm.y, ulm.z, ulm.scale, this->filename_xml);
}




static void mapnik_layer_tile_info_cb(LayerMapnik * lmk)
{
	lmk->tile_info();
}




/**
 * Info.
 */
void LayerMapnik::tile_info()
{
	TileInfo ulm;
	/* Requested position to map coord. */
	map_utils_vikcoord_to_iTMS(&this->rerender_ul, this->rerender_zoom, this->rerender_zoom, &ulm);

	map_cache_extra_t extra = map_cache_get_extra(&ulm, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);

	char *filename = get_filename(this->file_cache_dir, ulm.x, ulm.y, ulm.scale);
	char *filemsg = NULL;
	char *timemsg = NULL;

	if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
		filemsg = g_strconcat("Tile File: ", filename, NULL);
		/* Get some timestamp information of the tile. */
		GStatBuf stat_buf;
		if (g_stat(filename, &stat_buf) == 0) {
			char time_buf[64];
			strftime(time_buf, sizeof(time_buf), "%c", gmtime((const time_t *)&stat_buf.st_mtime));
			timemsg = g_strdup_printf(_("Tile File Timestamp: %s"), time_buf);
		} else {
			timemsg = strdup(_("Tile File Timestamp: Not Available"));
		}
	} else {
		filemsg = g_strdup_printf("Tile File: %s [Not Available]", filename);
		timemsg = strdup("");
	}

	GArray *array = g_array_new(false, true, sizeof(char*));
	g_array_append_val(array, filemsg);
	g_array_append_val(array, timemsg);

	char *rendmsg = NULL;
	/* Show the info. */
	if (extra.duration > 0.0) {
		rendmsg = g_strdup_printf(_("Rendering time %.2f seconds"), extra.duration);
		g_array_append_val(array, rendmsg);
	}

	a_dialog_list(this->get_toolkit_window(), _("Tile Information"), array, 5);
	g_array_free(array, false);

	free(rendmsg);
	free(timemsg);
	free(filemsg);
	free(filename);
}




static LayerTool * mapnik_feature_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::MAPNIK);

	mapnik_tools[0] = layer_tool;

	layer_tool->layer_type = LayerType::MAPNIK;
	layer_tool->id_string = QString("MapnikFeatures");

	layer_tool->radioActionEntry.stock_id = strdup(GTK_STOCK_INFO);
	layer_tool->radioActionEntry.label = strdup(N_("_Mapnik Features"));
	layer_tool->radioActionEntry.accelerator = NULL;
	layer_tool->radioActionEntry.tooltip = strdup(N_("Mapnik Features"));
	layer_tool->radioActionEntry.value = 0;

	layer_tool->release = (VikToolMouseFunc) mapnik_feature_release_cb;

	layer_tool->cursor_shape = Qt::ArrowCursor;
	layer_tool->cursor_data = NULL;

	return layer_tool;
}




static bool mapnik_feature_release_cb(Layer * layer, GdkEventButton *event, LayerTool * tool)
{
	if (!layer) {
		return false;
	}

	return ((LayerMapnik *) layer)->feature_release(event, tool);
}




bool LayerMapnik::feature_release(GdkEventButton * event, LayerTool * tool)
{
	if (event->button() == Qt::RightButton) {
		tool->viewport->screen_to_coord(MAX(0, event->x), MAX(0, event->y), &this->rerender_ul);
		this->rerender_zoom = tool->viewport->get_zoom();

		if (!this->right_click_menu) {
			GtkWidget *item;
			this->right_click_menu = gtk_menu_new();

			item = gtk_image_menu_item_new_with_mnemonic(_("_Rerender Tile"));
			gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
			g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_rerender_cb), this);
			gtk_menu_shell_append(GTK_MENU_SHELL(this->right_click_menu), item);

			item = gtk_image_menu_item_new_with_mnemonic(_("_Info"));
			gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INFO, GTK_ICON_SIZE_MENU));
			g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(mapnik_layer_tile_info_cb), this);
			gtk_menu_shell_append(GTK_MENU_SHELL(this->right_click_menu), item);
		}

		gtk_menu_popup(GTK_MENU(this->right_click_menu), NULL, NULL, NULL, NULL, event->button, event->time);
		gtk_widget_show_all(GTK_WIDGET(this->right_click_menu));
	}

	return false;
}




LayerMapnik::LayerMapnik()
{
	this->type = LayerType::MAPNIK;
	strcpy(this->type_string, "MAPNIK");
	this->configure_interface(&vik_mapnik_layer_interface, layer_params);

	/* kamilTODO: initialize this? */
	//this->rerender_ul;
	//this->rerender_br;
}




LayerMapnik::LayerMapnik(Viewport * viewport) : LayerMapnik()
{
	this->set_defaults(viewport);
	this->tile_size_x = size_default().u; /* FUTURE: Is there any use in this being configurable? */
	this->loaded = false;
	this->mi = mapnik_interface_new();
}
