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

#include <mutex>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <QDebug>

#include "vikutils.h"
#include "map_ids.h"
#include "map_utils.h"
#include "mapcoord.h"
#include "map_cache.h"
#include "dir.h"
#include "util.h"
#include "ui_util.h"
#include "preferences.h"
#include "icons/icons.h"
#include "mapnik_interface.h"
#include "background.h"
#include "dialog.h"
#include "globals.h"

#include "layer_map.h"
#include "layer_mapnik.h"
#include "widget_file_entry.h"
#include "file.h"




using namespace SlavGPS;




static ParameterValue file_default(void)      { return ParameterValue(""); }
static ParameterValue size_default(void)      { return ParameterValue((uint32_t) 256); }
static ParameterValue alpha_default(void)     { return ParameterValue((uint32_t) 255); }
static ParameterValue cache_dir_default(void) { return ParameterValue(g_strconcat(maps_layer_default_dir(), "MapnikRendering", NULL)); }


static ParameterScale scales[] = {
	{ 0, 255, 5, 0 },   /* Alpha. */
	{ 64, 1024, 8, 0 }, /* Tile size. */
	{ 0, 1024, 12, 0 }, /* Rerender timeout hours. */
};


enum {
	PARAM_CONFIG_CSS = 0,
	PARAM_CONFIG_XML,
	PARAM_ALPHA,
	PARAM_USE_FILE_CACHE,
	PARAM_FILE_CACHE_DIR,
	NUM_PARAMS
};


Parameter mapnik_layer_params[] = {

#ifdef K
	{ PARAM_CONFIG_CSS,     "config-file-mml", ParameterType::STRING,  VIK_LAYER_GROUP_NONE, N_("CSS (MML) Config File:"), WidgetType::FILEENTRY,   KINT_TO_POINTER(VF_FILTER_CARTO), NULL, N_("CartoCSS configuration file"),   file_default,         NULL, NULL },
	{ PARAM_CONFIG_XML,     "config-file-xml", ParameterType::STRING,  VIK_LAYER_GROUP_NONE, N_("XML Config File:"),       WidgetType::FILEENTRY,   KINT_TO_POINTER(VF_FILTER_XML),   NULL, N_("Mapnik XML configuration file"), file_default,         NULL, NULL },
#else
	{ PARAM_CONFIG_CSS,     "config-file-mml", ParameterType::STRING,  VIK_LAYER_GROUP_NONE, N_("CSS (MML) Config File:"), WidgetType::FILEENTRY,   0,                                NULL, N_("CartoCSS configuration file"),   file_default,         NULL, NULL },
	{ PARAM_CONFIG_XML,     "config-file-xml", ParameterType::STRING,  VIK_LAYER_GROUP_NONE, N_("XML Config File:"),       WidgetType::FILEENTRY,   0,                                NULL, N_("Mapnik XML configuration file"), file_default,         NULL, NULL },
#endif
	{ PARAM_ALPHA,          "alpha",           ParameterType::UINT,    VIK_LAYER_GROUP_NONE, N_("Alpha:"),                 WidgetType::HSCALE,      &scales[0],                       NULL, NULL,                                alpha_default,        NULL, NULL },
	{ PARAM_USE_FILE_CACHE, "use-file-cache",  ParameterType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Use File Cache:"),        WidgetType::CHECKBUTTON, NULL,                             NULL, NULL,                                vik_lpd_true_default, NULL, NULL },
	{ PARAM_FILE_CACHE_DIR, "file-cache-dir",  ParameterType::STRING,  VIK_LAYER_GROUP_NONE, N_("File Cache Directory:"),  WidgetType::FOLDERENTRY, NULL,                             NULL, NULL,                                cache_dir_default,    NULL, NULL },

	{ NUM_PARAMS,           NULL,              ParameterType::PTR,     VIK_LAYER_GROUP_NONE, NULL,                         WidgetType::NONE,        NULL,                             NULL, NULL, NULL,                   NULL, NULL }, /* Guard. */
};





static LayerTool * mapnik_feature_create(Window * window, Viewport * viewport);




LayerMapnikInterface vik_mapnik_layer_interface;




LayerMapnikInterface::LayerMapnikInterface()
{
	this->params = mapnik_layer_params; /* Parameters. */
	this->params_count = NUM_PARAMS,

	strncpy(this->layer_type_string, "Mapnik Rendering", sizeof (this->layer_type_string)); /* Non-translatable. */
	this->layer_type_string[sizeof (this->layer_type_string) - 1] = '\0';

	this->layer_name = QObject::tr("Mapnik Rendering");
	// this->action_accelerator =  ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->layer_tool_constructors.insert({{ 0, mapnik_feature_create }});

	this->menu_items_selection = VIK_MENU_ITEM_ALL;
}




#define MAPNIK_PREFS_GROUP_KEY "mapnik"
#define MAPNIK_PREFS_NAMESPACE "mapnik."




static ParameterValue plugins_default(void)
{
	ParameterValue data;
#ifdef WINDOWS
	data.s = strdup("input");
#else
	if (0 == access("/usr/lib/mapnik/input", F_OK)) {
		data.s = strdup("/usr/lib/mapnik/input");
		/* Current Debian locations. */
	} else if (0 == access("/usr/lib/mapnik/3.0/input", F_OK)) {
		data.s = strdup("/usr/lib/mapnik/3.0/input");
	} else if (0 == access("/usr/lib/mapnik/2.2/input", F_OK)) {
		data.s = strdup("/usr/lib/mapnik/2.2/input");
	} else {
		data.s = strdup("");
	}
#endif
	return data;
}




static ParameterValue fonts_default(void)
{
	/* Possibly should be string list to allow loading from multiple directories. */
	ParameterValue data;
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
	{ 0, MAPNIK_PREFS_NAMESPACE"plugins_directory",       ParameterType::STRING, VIK_LAYER_GROUP_NONE,  N_("Plugins Directory:"),        WidgetType::FOLDERENTRY, NULL,       NULL, N_("You need to restart Viking for a change to this value to be used"), plugins_default, NULL, NULL },
	{ 1, MAPNIK_PREFS_NAMESPACE"fonts_directory",         ParameterType::STRING, VIK_LAYER_GROUP_NONE,  N_("Fonts Directory:"),          WidgetType::FOLDERENTRY, NULL,       NULL, N_("You need to restart Viking for a change to this value to be used"), fonts_default, NULL, NULL },
	{ 2, MAPNIK_PREFS_NAMESPACE"recurse_fonts_directory", ParameterType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Recurse Fonts Directory:"),  WidgetType::CHECKBUTTON, NULL,       NULL, N_("You need to restart Viking for a change to this value to be used"), vik_lpd_true_default, NULL, NULL },
	{ 3, MAPNIK_PREFS_NAMESPACE"rerender_after",          ParameterType::UINT, VIK_LAYER_GROUP_NONE,    N_("Rerender Timeout (hours):"), WidgetType::SPINBUTTON,  &scales[2], NULL, N_("You need to restart Viking for a change to this value to be used"), NULL, NULL, NULL },
	/* Changeable any time. */
	{ 4, MAPNIK_PREFS_NAMESPACE"carto",                   ParameterType::STRING, VIK_LAYER_GROUP_NONE,  N_("CartoCSS:"),                 WidgetType::FILEENTRY,   NULL,       NULL, N_("The program to convert CartoCSS files into Mapnik XML"), NULL, NULL, NULL },

	{ 5, NULL,                                            ParameterType::STRING, VIK_LAYER_GROUP_NONE,  "",                              WidgetType::NONE,        NULL,       NULL, "", NULL, NULL, NULL } /* Guard. */
};




static time_t planet_import_time;
static std::mutex tp_mutex;
static GHashTable *requests = NULL;




/**
 * Just initialize preferences.
 */
void SlavGPS::vik_mapnik_layer_init(void)
{
	a_preferences_register_group(MAPNIK_PREFS_GROUP_KEY, "Mapnik");

	unsigned int i = 0;
	ParameterValue tmp = plugins_default();
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
	/* Just storing keys only. */
	requests = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	unsigned int hours = a_preferences_get(MAPNIK_PREFS_NAMESPACE"rerender_after")->u;
	GDateTime *now = g_date_time_new_now_local();
	GDateTime *then = g_date_time_add_hours(now, -hours);
	planet_import_time = g_date_time_to_unix(then);
	g_date_time_unref(now);
	g_date_time_unref(then);

	/* Similar to mod_tile method to mark DB has been imported/significantly changed to cause a rerendering of all tiles. */
	char *import_time_file = g_strconcat(get_viking_dir(), G_DIR_SEPARATOR_S, "planet-import-complete", NULL);
	struct stat stat_buf;
	if (stat(import_time_file, &stat_buf) == 0) {
		/* Only update if newer. */
		if (planet_import_time > stat_buf.st_mtime) {
			planet_import_time = stat_buf.st_mtime;
		}
	}
	free(import_time_file);
}




void SlavGPS::vik_mapnik_layer_uninit()
{
}




/* NB Only performed once per program run. */
void SlavGPS::layer_mapnik_init(void)
{
	ParameterValue *pd = a_preferences_get(MAPNIK_PREFS_NAMESPACE"plugins_directory");
	ParameterValue *fd = a_preferences_get(MAPNIK_PREFS_NAMESPACE"fonts_directory");
	ParameterValue *rfd = a_preferences_get(MAPNIK_PREFS_NAMESPACE"recurse_fonts_directory");

	if (pd && fd && rfd) {
		mapnik_interface_initialize(pd->s, fd->s, rfd->b);
	} else {
		fprintf(stderr, "CRITICAL: Unable to initialize mapnik interface from preferences\n");
	}
}




QString LayerMapnik::tooltip()
{
	return this->filename_xml;
}




void LayerMapnik::set_file_xml(char const * name_)
{
	if (this->filename_xml) {
		free(this->filename_xml);
	}
	/* Mapnik doesn't seem to cope with relative filenames. */
	if (strcmp(name_, "")) {
		this->filename_xml = vu_get_canonical_filename(this, name_);
	} else {
		this->filename_xml = g_strdup(name_);
	}
}




void LayerMapnik::set_file_css(char const * name_)
{
	if (this->filename_css) {
		free(this->filename_css);
	}
	this->filename_css = g_strdup(name_);
}




void LayerMapnik::set_cache_dir(char const * name_)
{
	if (this->file_cache_dir) {
		free(this->file_cache_dir);
	}
	this->file_cache_dir = g_strdup(name_);
}




Layer * LayerMapnikInterface::unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	LayerMapnik * layer = new LayerMapnik();

	layer->tile_size_x = size_default().u; /* FUTURE: Is there any use in this being configurable? */
	layer->loaded = false;
	layer->mi = mapnik_interface_new();
	layer->unmarshall_params(data, len);

	return layer;
}




bool LayerMapnik::set_param_value(uint16_t id, ParameterValue data, bool is_file_operation)
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




ParameterValue LayerMapnik::get_param_value(param_id_t id, bool is_file_operation) const
{
	ParameterValue param_value;
	switch (id) {
		case PARAM_CONFIG_CSS: {
			param_value.s = this->filename_css;
			bool set = false;
			if (is_file_operation) {
				if (Preferences::get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
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
				if (Preferences::get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
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
bool LayerMapnik::carto_load(void)
{
	char *mystdout = NULL;
	char *mystderr = NULL;
	GError *error = NULL;

	ParameterValue *vlpd = a_preferences_get(MAPNIK_PREFS_NAMESPACE"carto");
	char *command = g_strdup_printf("%s %s", vlpd->s, this->filename_css);

	bool answer = true;
	//char *args[2]; args[0] = vlpd->s; args[1] = this->filename_css;
	//GPid pid;
	//if (g_spawn_async_with_pipes(NULL, args, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &pid, NULL, &carto_stdout, &carto_error, &error)) {
	// cf code in babel.c to handle stdout

	/* NB Running carto may take several seconds, especially for
	   large style sheets like the default OSM Mapnik style (~6
	   seconds on my system). */
	Window * window = this->get_window();
	if (window) {
		// char *msg = g_strdup_printf(); // kamil kamil
		window->statusbar_update(StatusBarField::INFO, QString("%1: %2").arg("Running").arg(command));
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
				dialog_error(QString("Error running carto command:\n%1").arg(QString(mystderr)), this->get_window());
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
#ifdef K
		QString msg = QString("%s %s %.1f %s").arg(vlpd->s).arg(" completed in ").arg((double)(tt2-tt1)/G_USEC_PER_SEC, _("seconds"))
		window->statusbar_update(StatusBarField::INFO, msg);
		window->clear_busy_cursor();
#endif
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
			struct stat stat_buf1;
			if (stat(this->filename_xml, &stat_buf1) == 0) {
				struct stat stat_buf2;
				if (stat(this->filename_css, &stat_buf2) == 0) {
					/* Is CSS file newer than the XML file. */
					if (stat_buf2.st_mtime > stat_buf1.st_mtime) {
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
		if (!this->carto_load()) {
			return;
		}

	char * ans = mapnik_interface_load_map_file(this->mi, this->filename_xml, this->tile_size_x, this->tile_size_x);
	if (ans) {
		dialog_error(QString("Mapnik error loading configuration file:\n%1").arg(QString(ans)), this->get_window());
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




void LayerMapnik::possibly_save_pixmap(QPixmap * pixmap, TileInfo * ti_ul)
{
	if (this->use_file_cache) {
		if (this->file_cache_dir) {
			char * filename = get_filename(this->file_cache_dir, ti_ul->x, ti_ul->y, ti_ul->scale);

			char *dir = g_path_get_dirname(filename);
			if (0 != access(filename, F_OK)) {
				if (g_mkdir_with_parents(dir , 0777) != 0) {
					fprintf(stderr, "WARNING: %s: Failed to mkdir %s\n", __FUNCTION__, dir);
				}
			}
			free(dir);

			if (!pixmap->save(filename, "png")) {
				qDebug() << "WW: Layer Mapnik: failed to save pixmap to" << filename;
			}
			free(filename);
		}
	}
}




class RenderInfo : public BackgroundJob {
public:
	RenderInfo(LayerMapnik * layer, Coord * ul, Coord * br, TileInfo * ti_ul, const char * request);

	LayerMapnik * lmk = NULL;

	Coord coord_ul;
	Coord coord_br;
	TileInfo ti_ul;

	const char * request = NULL;
};



static int render_info_background_fn(BackgroundJob * bg_job);

RenderInfo::RenderInfo(LayerMapnik * layer, Coord * ul, Coord * br, TileInfo * ti_ul_, const char * request_)
{
	this->thread_fn = render_info_background_fn;
	this->n_items = 1;

	this->lmk = layer;

	this->coord_ul = *ul;
	this->coord_br = *br;
	this->ti_ul = *ti_ul_;

	this->request = request_;
}




/**
 * Common render function which can run in separate thread.
 */
void LayerMapnik::render(Coord * coord_ul, Coord * coord_br, TileInfo * ti_ul)
{
	int64_t tt1 = g_get_real_time();
	QPixmap *pixmap = mapnik_interface_render(this->mi, coord_ul->ll.lat, coord_ul->ll.lon, coord_br->ll.lat, coord_br->ll.lon);
	int64_t tt2 = g_get_real_time();
	double tt = (double)(tt2-tt1)/1000000;
	fprintf(stderr, "DEBUG: Mapnik rendering completed in %.3f seconds\n", tt);
	if (!pixmap) {
#ifdef K
		/* A pixmap to stick into cache incase of an unrenderable area - otherwise will get continually re-requested. */
		pixmap = gdk_pixbuf_scale_simple(gdk_pixbuf_from_pixdata(&vikmapniklayer_pixmap, false, NULL), this->tile_size_x, this->tile_size_x, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
#endif
	}
	this->possibly_save_pixmap(pixmap, ti_ul);

	/* NB Mapnik can apply alpha, but use our own function for now. */
	if (this->alpha < 255) {
		pixmap = ui_pixmap_scale_alpha(pixmap, this->alpha);
	}
	map_cache_add(pixmap, (map_cache_extra_t){ tt }, ti_ul, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);
#ifdef K
	g_object_unref(pixmap);
#endif
}




static int render_info_background_fn(BackgroundJob * bg_job)
{
	RenderInfo * data = (RenderInfo *) bg_job;

	int res = a_background_thread_progress(bg_job, 0);
	if (res == 0) {
		data->lmk->render(&data->coord_ul, &data->coord_br, &data->ti_ul);
	}

	tp_mutex.lock();
	g_hash_table_remove(requests, data->request);
	tp_mutex.unlock();

	if (res == 0) {
		data->lmk->emit_changed(); /* NB update display from background. */
	}
	return res;
}




#define REQUEST_HASHKEY_FORMAT "%d-%d-%d-%d-%d"




/**
 * Thread.
 */
void LayerMapnik::thread_add(TileInfo * ti_ul, Coord * coord_ul, Coord * coord_br, int x, int y, int z, int zoom, char const * name_)
{
	/* Create request. */
	unsigned int nn = name_ ? g_str_hash(name_) : 0;
	char *request = g_strdup_printf(REQUEST_HASHKEY_FORMAT, x, y, z, zoom, nn);

	tp_mutex.lock();

	if (g_hash_table_lookup_extended(requests, request, NULL, NULL)) {
		free(request);
		tp_mutex.unlock();
		return;
	}

	RenderInfo * ri = new RenderInfo(this, coord_ul, coord_br, ti_ul, request);

#ifdef K
	g_hash_table_insert(requests, request, NULL);

	tp_mutex.unlock();

	char * basename = g_path_get_basename(name_);
	const QString job_description = QString(tr("Mapnik Render %1:%2:%3 %4")).arg(zoom).arg(x).arg(y).arg(basename);
	free(basename);
	a_background_thread(ri, ThreadPoolType::LOCAL_MAPNIK, job_description);
#endif
}




/**
 * If function returns QPixmap properly, reference counter to this
 * buffer has to be decreased, when buffer is no longer needed.
 */
QPixmap * LayerMapnik::load_pixmap(TileInfo * ti_ul, TileInfo * ti_br, bool * rerender_)
{
	*rerender_ = false;
	QPixmap *pixmap = NULL;
	char *filename = get_filename(this->file_cache_dir, ti_ul->x, ti_ul->y, ti_ul->scale);

	struct stat stat_buf;
	if (stat(filename, &stat_buf) == 0) {
		/* Get from disk. */
#ifdef K
		pixmap = new QPixmap();
		if (!pixmap->load(filename)) {
			delete pixmap;
			pixmap = NULL;
			qDebug() << "WW: Layer Mapnik: failed to load pixmap from" << filename;
		} else {
			if (this->alpha < 255) {
				pixmap = ui_pixmap_set_alpha(pixmap, this->alpha);
			}
			map_cache_add(pixmap, (map_cache_extra_t) { -42.0 }, ti_ul, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);
		}
		/* If file is too old mark for rerendering. */
		if (planet_import_time < stat_buf.st_mtime) {
			*rerender_ = true;
		}
#endif
	}
	free(filename);

	return pixmap;
}




/**
 * Caller has to decrease reference counter of returned QPixmap,
 * when buffer is no longer needed.
 */
QPixmap * LayerMapnik::get_pixmap(TileInfo * ti_ul, TileInfo * ti_br)
{
	Coord ul; Coord br;

	map_utils_iTMS_to_vikcoord(ti_ul, &ul);
	map_utils_iTMS_to_vikcoord(ti_br, &br);

	QPixmap * pixmap = map_cache_get(ti_ul, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);
	if (pixmap) {
		fprintf(stderr, "MapnikLayer: MAP CACHE HIT\n");
	} else {
		fprintf(stderr, "MapnikLayer: MAP CACHE MISS\n");

		bool rerender_ = false;
		if (this->use_file_cache && this->file_cache_dir)
			pixmap = this->load_pixmap(ti_ul, ti_br, &rerender_);
		if (! pixmap || rerender_) {
			if (true) {
				this->thread_add(ti_ul, &ul, &br, ti_ul->x, ti_ul->y, ti_ul->z, ti_ul->scale, this->filename_xml);
			} else {
				/* Run in the foreground. */
				this->render(&ul, &br, ti_ul);
				this->emit_changed();
			}
		}
	}

	return pixmap;
}




void LayerMapnik::draw(Viewport * viewport)
{
	if (!this->loaded) {
		return;
	}

	if (viewport->get_drawmode() != ViewportDrawMode::MERCATOR) {
		this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, _("Mapnik Rendering must be in Mercator mode"));
		return;
	}

	if (this->mi) {
		char * copyright = mapnik_interface_get_copyright(this->mi);
		if (copyright) {
			viewport->add_copyright(QString(copyright));
		}
	}

#if 0 /* kamilTODO: this code would be overwritten in screen_to_coord() */
	ul.mode = CoordMode::LATLON;
	br.mode = CoordMode::LATLON;
#endif
	Coord ul = viewport->screen_to_coord(0, 0);
	Coord br = viewport->screen_to_coord(viewport->get_width(), viewport->get_height());

	double xzoom = viewport->get_xmpp();
	double yzoom = viewport->get_ympp();

	TileInfo ti_ul, ti_br;

	if (map_utils_vikcoord_to_iTMS(&ul, xzoom, yzoom, &ti_ul) &&
	     map_utils_vikcoord_to_iTMS(&br, xzoom, yzoom, &ti_br)) {
		/* TODO: Understand if tilesize != 256 does this need to use shrinkfactors? */
		QPixmap * pixmap;
		Coord coord;
		int xx, yy;

		int xmin = MIN(ti_ul.x, ti_br.x), xmax = MAX(ti_ul.x, ti_br.x);
		int ymin = MIN(ti_ul.y, ti_br.y), ymax = MAX(ti_ul.y, ti_br.y);

		/* Split rendering into a grid for the current viewport
		   thus each individual 'tile' can then be stored in the map cache. */
		for (int x = xmin; x <= xmax; x++) {
			for (int y = ymin; y <= ymax; y++) {
				ti_ul.x = x;
				ti_ul.y = y;
				ti_br.x = x+1;
				ti_br.y = y+1;

				pixmap = this->get_pixmap(&ti_ul, &ti_br);

				if (pixmap) {
					map_utils_iTMS_to_vikcoord(&ti_ul, &coord);
					viewport->coord_to_screen(&coord, &xx, &yy);
					viewport->draw_pixmap(*pixmap, 0, 0, xx, yy, this->tile_size_x, this->tile_size_x);
#ifdef K
					g_object_unref(pixmap);
#endif
				}
			}
		}

		/* Done after so drawn on top.
		   Just a handy guide to tile blocks. */
		if (vik_debug && vik_verbose) {
#ifdef K
			QPen * black_pen = viewport->get_widget()->style->black_pen;
			int width = viewport->get_width();
			int height = viewport->get_height();
			int xx, yy;
			ti_ul.x = xmin; ti_ul.y = ymin;
			map_utils_iTMS_to_center_vikcoord(&ti_ul, &coord);
			viewport->coord_to_screen(&coord, &xx, &yy);
			xx = xx - (this->tile_size_x/2);
			yy = yy - (this->tile_size_x/2); // Yes use X ATM
			for (int x = xmin; x <= xmax; x++) {
				viewport->draw_line(black_pen, xx, 0, xx, height);
				xx += this->tile_size_x;
			}
			for (int y = ymin; y <= ymax; y++) {
				viewport->draw_line(black_pen, 0, yy, width, yy);
				yy += this->tile_size_x; // Yes use X ATM
			}
#endif
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
	if (!lmk->carto_load()) {
		return;
	}
	char * ans = mapnik_interface_load_map_file(lmk->mi, lmk->filename_xml, lmk->tile_size_x, lmk->tile_size_x);
	if (ans) {
		dialog_error(QString("Mapnik error loading configuration file:\n%1").arg(QString(ans)), viewport->get_window());
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
#ifdef K
		a_dialog_list(lmk->get_window(), _("Mapnik Information"), array, 1);
		/* Free the copied strings. */
		for (unsigned int i = 0; i < array->len; i++) {
			free(g_array_index(array, char*, i));
		}
#endif
	}
	g_array_free(array, false);
}




static void mapnik_layer_about(menu_array_values * values)
{
	LayerMapnik * lmk = values->lmk;
	char * msg = mapnik_interface_about();
	dialog_info(msg, lmk->get_window());
	free(msg);
}




void LayerMapnik::add_menu_items(QMenu & menu)
{
#ifdef K
	LayersPanel * panel = (LayersPanel *) panel_;

	static menu_array_values values = {
		this,
		panel->get_viewport()
	};

	QAction * action = NULL;

	/* Typical users shouldn't need to use this functionality - so debug only ATM. */
	if (vik_debug) {
		action = new QAction(QObject::tr("&Flush Memory Cache"), this);
		action->setIcon(QIcon::fromTheme("GTK_STOCK_REMOVE"));
		QObject::connect(action, SIGNAL (triggered(bool)), &values, SLOT (mapnik_layer_flush_memory));
		menu->addAction(action);
	}

	action = new QAction(QObject::tr("Re&fresh"), this);
	QObject::connect(action, SIGNAL (triggered(bool)), &values, SLOT (mapnik_layer_reload));
	menu->addAction(action);

	if (strcmp("", this->filename_css)) {
		action = new QAction(QObject::tr("&Run Carto Command"), this);
		action->setIcon(QIcon::fromTheme("GTK_STOCK_EXECUTE"));
		QObject::connect(action, SIGNAL (triggered(bool)), &values, SLOT (mapnik_layer_carto));
		menu->addAction(action);
	}

	action = new QAction(QObject::tr("&Info"), this);
	QObject::connect(action, SIGNAL (triggered(bool)), &values, SLOT (mapnik_layer_information));
	menu->addAction(action);

	action = new QAction(QObject::tr("&About"), this);
	QObject::connect(action, SIGNAL (triggered(bool)), &values, SLOT (mapnik_layer_about));
	menu->addAction(action);
#endif
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
	TileInfo ti_ul;
	/* Requested position to map coord. */
	map_utils_vikcoord_to_iTMS(&this->rerender_ul, this->rerender_zoom, this->rerender_zoom, &ti_ul);
	/* Reconvert back - thus getting the coordinate at the tile *ul corner*. */
	map_utils_iTMS_to_vikcoord(&ti_ul, &this->rerender_ul);
	/* Bottom right bound is simply +1 in TMS coords. */
	TileInfo ti_br = ti_ul;
	ti_br.x = ti_br.x+1;
	ti_br.y = ti_br.y+1;
	map_utils_iTMS_to_vikcoord(&ti_br, &this->rerender_br);
	this->thread_add(&ti_ul, &this->rerender_ul, &this->rerender_br, ti_ul.x, ti_ul.y, ti_ul.z, ti_ul.scale, this->filename_xml);
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
	TileInfo ti_ul;
	/* Requested position to map coord. */
	map_utils_vikcoord_to_iTMS(&this->rerender_ul, this->rerender_zoom, this->rerender_zoom, &ti_ul);

	map_cache_extra_t extra = map_cache_get_extra(&ti_ul, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);

	char *filename = get_filename(this->file_cache_dir, ti_ul.x, ti_ul.y, ti_ul.scale);
	char *filemsg = NULL;
	char *timemsg = NULL;
#ifdef K
	if (0 == access(filename, F_OK)) {
		filemsg = g_strconcat("Tile File: ", filename, NULL);
		/* Get some timestamp information of the tile. */
		struct stat stat_buf;
		if (stat(filename, &stat_buf) == 0) {
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

	a_dialog_list(this->get_window(), _("Tile Information"), array, 5);
	g_array_free(array, false);

	free(rendmsg);
	free(timemsg);
	free(filemsg);
	free(filename);
#endif
}




static LayerTool * mapnik_feature_create(Window * window, Viewport * viewport)
{
	return new LayerToolMapnikFeature(window, viewport);
}




LayerToolMapnikFeature::LayerToolMapnikFeature(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::MAPNIK)
{
	this->id_string = QString("mapnik.features");
#ifdef K
	this->action_icon_path   = GTK_STOCK_INFO;
#endif
	this->action_label       = QObject::tr("&Mapnik Features");
	this->action_tooltip     = QObject::tr("Mapnik Features");
	// this->action_accelerator = ...; /* Empty accelerator. */
#ifdef K
	this->cursor_shape = Qt::ArrowCursor;
	this->cursor_data = NULL;
#endif

	Layer::get_interface(LayerType::MAPNIK)->layer_tools.insert({{ 0, this }});
}




LayerToolFuncStatus LayerToolMapnikFeature::release_(Layer * layer, QMouseEvent * ev)
{
	if (!layer) {
		return (LayerToolFuncStatus) false; /* kamilFIXME: check this cast of returned value. */
	}

	return (LayerToolFuncStatus) ((LayerMapnik *) layer)->feature_release(ev, this); /* kamilFIXME: check this cast of returned value. */
}




bool LayerMapnik::feature_release(QMouseEvent * ev, LayerTool * tool)
{
	if (ev->button() == Qt::RightButton) {
		this->rerender_ul = tool->viewport->screen_to_coord(MAX(0, ev->x()), MAX(0, ev->y()));
		this->rerender_zoom = tool->viewport->get_zoom();
#ifdef K
		if (!this->right_click_menu) {
			QAction * action = NULL;
			this->right_click_menu = new QMenu();

			action = new QAction(QObject::tr("&Rerender Tile"), this);
			action->setIcon(QIcon::fromTheme("GTK_STOCK_REFRESH"));
			QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (mapnik_layer_rerender_cb));
			menu->addAction(action);

			action = new QAction(QObject::tr("&Info"), this);
			action->setIcon(QIcon::fromTheme("GTK_STOCK_INFO"));
			QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (mapnik_layer_tile_info_cb));
			menu->addAction(action);
		}

		this->right_click_menu->exec(QCursor::pos());
#endif
	}

	return false;
}




LayerMapnik::LayerMapnik()
{
	this->type = LayerType::MAPNIK;
	strcpy(this->debug_string, "MAPNIK");
	this->interface = &vik_mapnik_layer_interface;

	this->set_initial_parameter_values();
	this->tile_size_x = size_default().u; /* FUTURE: Is there any use in this being configurable? */
	this->loaded = false;
	this->mi = mapnik_interface_new();

	/* kamilTODO: initialize this? */
	//this->rerender_ul;
	//this->rerender_br;
}
