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
#include <QDir>

#include "window.h"
#include "vikutils.h"
#include "map_ids.h"
#include "map_utils.h"
#include "mapcoord.h"
#include "map_cache.h"
#include "dir.h"
#include "util.h"
#include "ui_util.h"
#include "preferences.h"
typedef int GdkPixdata; /* TODO: remove sooner or later. */
#include "icons/icons.h"
#include "mapnik_interface.h"
#include "background.h"
#include "dialog.h"
#include "globals.h"

#include "layer_map.h"
#include "layer_mapnik.h"
#include "widget_file_entry.h"
#include "file.h"
#include "file_utils.h"




using namespace SlavGPS;




extern Tree * g_tree;




static SGVariant file_default(void)      { return SGVariant(""); }
static SGVariant size_default(void)      { return SGVariant((uint32_t) 256); }
static SGVariant cache_dir_default(void) { return SGVariant(maps_layer_default_dir() + "MapnikRendering"); }


static ParameterScale scale_alpha   = { 0,  255, SGVariant((int32_t) 255),  5, 0 }; /* PARAM_ALPHA */
static ParameterScale scale_timeout = { 0, 1024, SGVariant((int32_t) 168), 12, 0 }; /* Renderer timeout hours. Value of hardwired default is one week. */


enum {
	PARAM_CONFIG_CSS = 0,
	PARAM_CONFIG_XML,
	PARAM_ALPHA,
	PARAM_USE_FILE_CACHE,
	PARAM_FILE_CACHE_DIR,
	NUM_PARAMS
};



SGFileTypeFilter file_type_css[1] = { SGFileTypeFilter::CARTO };
SGFileTypeFilter file_type_xml[1] = { SGFileTypeFilter::XML };



ParameterSpecification mapnik_layer_param_specs[] = {

	{ PARAM_CONFIG_CSS,     NULL, "config-file-mml", SGVariantType::String,  PARAMETER_GROUP_GENERIC, QObject::tr("CSS (MML) Config File:"), WidgetType::FILEENTRY,   file_type_css, file_default,         NULL, N_("CartoCSS configuration file") },
	{ PARAM_CONFIG_XML,     NULL, "config-file-xml", SGVariantType::String,  PARAMETER_GROUP_GENERIC, QObject::tr("XML Config File:"),       WidgetType::FILEENTRY,   file_type_xml, file_default,         NULL, N_("Mapnik XML configuration file") },
	{ PARAM_ALPHA,          NULL, "alpha",           SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Alpha:"),                 WidgetType::HSCALE,      &scale_alpha,  NULL,                 NULL, NULL },
	{ PARAM_USE_FILE_CACHE, NULL, "use-file-cache",  SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Use File Cache:"),        WidgetType::CHECKBUTTON, NULL,          sg_variant_true,      NULL, NULL },
	{ PARAM_FILE_CACHE_DIR, NULL, "file-cache-dir",  SGVariantType::String,  PARAMETER_GROUP_GENERIC, QObject::tr("File Cache Directory:"),  WidgetType::FOLDERENTRY, NULL,          cache_dir_default,    NULL, NULL },

	{ NUM_PARAMS,           NULL, NULL,              SGVariantType::Empty,   PARAMETER_GROUP_GENERIC, QString(""),                           WidgetType::NONE,        NULL,          NULL,                 NULL, NULL }, /* Guard. */
};




LayerMapnikInterface vik_mapnik_layer_interface;




LayerMapnikInterface::LayerMapnikInterface()
{
	this->parameters_c = mapnik_layer_param_specs;

	this->fixed_layer_type_string = "Mapnik Rendering"; /* Non-translatable. */

	// this->action_accelerator =  ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->menu_items_selection = LayerMenuItem::ALL;

	this->ui_labels.new_layer = QObject::tr("New Mapnik Rendering Layer");
	this->ui_labels.layer_type = QObject::tr("Mapnik Rendering");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of Mapnik Rendering Layer");
}




LayerToolContainer * LayerMapnikInterface::create_tools(Window * window, Viewport * viewport)
{
	/* This method should be called only once. */
	static bool created = false;
	if (created) {
		return NULL;
	}

	auto tools = new LayerToolContainer;

	LayerTool * tool = new LayerToolMapnikFeature(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	created = true;

	return tools;
}




#define PREFERENCES_NAMESPACE_MAPNIK "mapnik"




static SGVariant plugins_default(void)
{
	SGVariant data;

#ifdef WINDOWS
	data = SGVariant("input");
#else
	if (0 == access("/usr/lib/mapnik/input", F_OK)) {
		/* Current Debian locations. */
		data = SGVariant("/usr/lib/mapnik/input");
	} else if (0 == access("/usr/lib/mapnik/3.0/input", F_OK)) {
		data = SGVariant("/usr/lib/mapnik/3.0/input");
	} else if (0 == access("/usr/lib/mapnik/2.2/input", F_OK)) {
		data = SGVariant("/usr/lib/mapnik/2.2/input");
	} else {
		data = SGVariant("");
	}
#endif
	return data;
}




static SGVariant fonts_default(void)
{
	/* Possibly should be string list to allow loading from multiple directories. */
	SGVariant data;
#ifdef WINDOWS
	data = SGVariant("C:\\Windows\\Fonts");
#elif defined __APPLE__
	data = SGVariant("/Library/Fonts");
#else
	data = SGVariant("/usr/share/fonts");
#endif
	return data;
}




static ParameterSpecification prefs[] = {
	/* Changing these values only applies before first mapnik layer is 'created' */
	{ 0, PREFERENCES_NAMESPACE_MAPNIK, "plugins_directory",       SGVariantType::String,  PARAMETER_GROUP_GENERIC,  QObject::tr("Plugins Directory:"),        WidgetType::FOLDERENTRY, NULL,           plugins_default, NULL, N_("You need to restart Viking for a change to this value to be used") },
	{ 1, PREFERENCES_NAMESPACE_MAPNIK, "fonts_directory",         SGVariantType::String,  PARAMETER_GROUP_GENERIC,  QObject::tr("Fonts Directory:"),          WidgetType::FOLDERENTRY, NULL,           fonts_default,   NULL, N_("You need to restart Viking for a change to this value to be used") },
	{ 2, PREFERENCES_NAMESPACE_MAPNIK, "recurse_fonts_directory", SGVariantType::Boolean, PARAMETER_GROUP_GENERIC,  QObject::tr("Recurse Fonts Directory:"),  WidgetType::CHECKBUTTON, NULL,           sg_variant_true, NULL, N_("You need to restart Viking for a change to this value to be used") },
	{ 3, PREFERENCES_NAMESPACE_MAPNIK, "rerender_after",          SGVariantType::Int,     PARAMETER_GROUP_GENERIC,  QObject::tr("Rerender Timeout (hours):"), WidgetType::SPINBOX_INT, &scale_timeout, NULL,            NULL, N_("You need to restart Viking for a change to this value to be used") },
	/* Changeable any time. */
	{ 4, PREFERENCES_NAMESPACE_MAPNIK, "carto",                   SGVariantType::String,  PARAMETER_GROUP_GENERIC,  QObject::tr("CartoCSS:"),                 WidgetType::FILEENTRY,   NULL,           NULL,            NULL, N_("The program to convert CartoCSS files into Mapnik XML") },

	{ 5, NULL,                         "",                        SGVariantType::Empty,   PARAMETER_GROUP_GENERIC,  QString(""),                              WidgetType::NONE,        NULL,           NULL,            NULL, NULL } /* Guard. */
};




static time_t g_planet_import_time;
static std::mutex tp_mutex;
static GHashTable *requests = NULL;




/**
 * Just initialize preferences.
 */
void SlavGPS::vik_mapnik_layer_init(void)
{
	Preferences::register_group(PREFERENCES_NAMESPACE_MAPNIK, QObject::tr("Mapnik"));

	unsigned int i = 0;

	Preferences::register_parameter(prefs[i], plugins_default());
	i++;
	Preferences::register_parameter(prefs[i], fonts_default());
	i++;
	Preferences::register_parameter(prefs[i], SGVariant(true, prefs[i].type_id));
	i++;
	Preferences::register_parameter(prefs[i], scale_timeout.initial);
	i++;
	Preferences::register_parameter(prefs[i], SGVariant("carto", prefs[i].type_id));
	i++;
}




/**
 * Initialize data structures - now that reading preferences is OK to perform.
 */
void SlavGPS::vik_mapnik_layer_post_init(void)
{
	/* Just storing keys only. */
	requests = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	unsigned int hours = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK ".rerender_after").val_uint;
	g_planet_import_time = QDateTime::currentDateTime().addSecs(-1 * hours * 60 * 60).toTime_t(); /* In local time zone. */

	/* Similar to mod_tile method to mark DB has been imported/significantly changed to cause a rerendering of all tiles. */
	const QString import_time_full_path = get_viking_dir() + QDir::separator() + "planet-import-complete";
	struct stat stat_buf;
	if (stat(import_time_full_path.toUtf8().constData(), &stat_buf) == 0) {
		/* Only update if newer. */
		if (g_planet_import_time > stat_buf.st_mtime) {
			g_planet_import_time = stat_buf.st_mtime;
		}
	}
}




void SlavGPS::vik_mapnik_layer_uninit()
{
}




/* NB Only performed once per program run. */
void SlavGPS::layer_mapnik_init(void)
{
	const SGVariant pd = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK ".plugins_directory");
	const SGVariant fd = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK ".fonts_directory");
	const SGVariant rfd = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK ".recurse_fonts_directory");

	if (pd.type_id != SGVariantType::Empty
	    && fd.type_id != SGVariantType::Empty
	    && rfd.type_id != SGVariantType::Empty) {

		mapnik_interface_initialize(pd.val_string.toUtf8().constData(), fd.val_string.toUtf8().constData(), rfd.val_bool);
	} else {
		qDebug() << "EE: Layer Mapnik: Init: Unable to initialize mapnik interface from preferences";
	}
}




QString LayerMapnik::get_tooltip()
{
	return this->filename_xml;
}




void LayerMapnik::set_file_xml(const QString & name_)
{
	/* Mapnik doesn't seem to cope with relative filenames. */
	if (name_ != "") {
		this->filename_xml = vu_get_canonical_filename(this, name_, this->get_window()->get_current_document_full_path());
	} else {
		this->filename_xml = name_;
	}
}




void LayerMapnik::set_file_css(const QString & name_)
{
	this->filename_css = name_;
}




void LayerMapnik::set_cache_dir(const QString & name_)
{
	this->file_cache_dir = name_;
}




Layer * LayerMapnikInterface::unmarshall(uint8_t * data, size_t data_len, Viewport * viewport)
{
	LayerMapnik * layer = new LayerMapnik();

	layer->tile_size_x = size_default().val_uint; /* FUTURE: Is there any use in this being configurable? */
	layer->loaded = false;
	layer->mi = mapnik_interface_new();
	layer->unmarshall_params(data, data_len);

	return layer;
}




bool LayerMapnik::set_param_value(uint16_t id, const SGVariant & data, bool is_file_operation)
{
	switch (id) {
		case PARAM_CONFIG_CSS:
			this->set_file_css(data.val_string);
			break;
		case PARAM_CONFIG_XML:
			this->set_file_xml(data.val_string);
			break;
		case PARAM_ALPHA:
			if (data.val_int >= scale_alpha.min && data.val_int <= scale_alpha.max) {
				this->alpha = data.val_int;
			}
			break;
		case PARAM_USE_FILE_CACHE:
			this->use_file_cache = data.val_bool;
			break;
		case PARAM_FILE_CACHE_DIR:
			this->set_cache_dir(data.val_string);
			break;
		default:
			break;
	}
	return true;
}




SGVariant LayerMapnik::get_param_value(param_id_t id, bool is_file_operation) const
{
	SGVariant param_value;
	switch (id) {
		case PARAM_CONFIG_CSS: {
			param_value = SGVariant(this->filename_css);
			bool set = false;
			if (is_file_operation) {
				if (Preferences::get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
					const QString cwd = QDir::currentPath();
					if (!cwd.isEmpty()) {
						param_value = SGVariant(file_GetRelativeFilename(cwd, this->filename_css));
						set = true;
					}
				}
			}
			if (!set) {
				param_value = SGVariant(this->filename_css);
			}
			break;
		}
		case PARAM_CONFIG_XML: {
			param_value = SGVariant(this->filename_xml);
			bool set = false;
			if (is_file_operation) {
				if (Preferences::get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
					const QString cwd = QDir::currentPath();
					if (!cwd.isEmpty()) {
						param_value = SGVariant(file_GetRelativeFilename(cwd, this->filename_xml));
						set = true;
					}
				}
			}
			if (!set) {
				param_value = SGVariant(this->filename_xml);
			}
			break;
		}
		case PARAM_ALPHA:
			param_value = SGVariant((int32_t) this->alpha);
			break;
		case PARAM_USE_FILE_CACHE:
			param_value = SGVariant(this->use_file_cache);
			break;
		case PARAM_FILE_CACHE_DIR:
			param_value = SGVariant(this->file_cache_dir);
			break;
		default:
			break;
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

	const SGVariant pref_value = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK ".carto");
	const QString command = QString("%1 %2").arg(pref_value.val_string).arg(this->filename_css);

	bool answer = true;
	//char *args[2]; args[0] = pref_value.s; args[1] = this->filename_css;
	//GPid pid;
	//if (g_spawn_async_with_pipes(NULL, args, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &pid, NULL, &carto_stdout, &carto_error, &error)) {
	// cf code in babel.c to handle stdout

	/* NB Running carto may take several seconds, especially for
	   large style sheets like the default OSM Mapnik style (~6
	   seconds on my system). */
	Window * window_ = this->get_window();
	if (window_) {
		// char *msg = g_strdup_printf(); // kamil kamil
		window_->statusbar_update(StatusBarField::INFO, QString("%1: %2").arg("Running").arg(command));
		window_->set_busy_cursor();
	}

	int64_t tt1 = 0;
	int64_t tt2 = 0;
	/* You won't get a sensible timing measurement if running too old a GLIB. */
#if GLIB_CHECK_VERSION (2, 28, 0)
	tt1 = g_get_real_time();
#endif

	if (g_spawn_command_line_sync(command.toUtf8().constData(), &mystdout, &mystderr, NULL, &error)) {
#if GLIB_CHECK_VERSION (2, 28, 0)
		tt2 = g_get_real_time();
#endif
		if (mystderr)
			if (strlen(mystderr) > 1) {
				Dialog::error(tr("Error running carto command:\n%1").arg(QString(mystderr)), this->get_window());
				answer = false;
			}
		if (mystdout) {
			/* NB This will overwrite the specified XML file. */
			if (! (this->filename_xml.length() > 1)) {
				/* XML Not specified so try to create based on CSS file name. */
				GRegex *regex = g_regex_new("\\.mml$|\\.mss|\\.css$", G_REGEX_CASELESS, (GRegexMatchFlags) 0, &error);
				if (error) {
					fprintf(stderr, "CRITICAL: %s: %s\n", __FUNCTION__, error->message);
				}
				this->filename_xml = QString(g_regex_replace_literal(regex, this->filename_css.toUtf8().constData(), -1, 0, ".xml", (GRegexMatchFlags) 0, &error)) ;
				if (error) {
					fprintf(stderr, "WARNING: %s: %s\n", __FUNCTION__, error->message);
				}
				/* Prevent overwriting self. */
				if (this->filename_xml == this->filename_css) {
					this->filename_xml = this->filename_css + ".xml";
				}
			}
			if (!g_file_set_contents(this->filename_xml.toUtf8().constData(), mystdout, -1, &error) ) {
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

	if (window_) {
		const QString msg = tr("%1 completed in %.1f seconds").arg(pref_value.val_string).arg((double) (tt2-tt1)/G_USEC_PER_SEC, 0, 'f', 1);
		window_->statusbar_update(StatusBarField::INFO, msg);
		window_->clear_busy_cursor();
	}
	return answer;
}




void LayerMapnik::post_read(Viewport * viewport, bool from_file)
{
	/* Determine if carto needs to be run. */
	bool do_carto = false;
	if (this->filename_css.length() > 1) {
		if (this->filename_xml.length() > 1) {
			/* Compare timestamps. */
			struct stat stat_buf1;
			if (stat(this->filename_xml.toUtf8().constData(), &stat_buf1) == 0) {
				struct stat stat_buf2;
				if (stat(this->filename_css.toUtf8().constData(), &stat_buf2) == 0) {
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

	const QString ans = mapnik_interface_load_map_file(this->mi, this->filename_xml, this->tile_size_x, this->tile_size_x);
	if (!ans.isEmpty()) {
		Dialog::error(tr("Mapnik error loading configuration file:\n%1").arg(ans), this->get_window());
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
		if (!this->file_cache_dir.isEmpty()) {
			char * filename = get_filename(this->file_cache_dir.toUtf8().data(), ti_ul->x, ti_ul->y, ti_ul->scale);

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
	RenderInfo(LayerMapnik * layer, const Coord & new_coord_ul, const Coord & new_coord_br, TileInfo * ti_ul, const char * request);

	LayerMapnik * lmk = NULL;

	Coord coord_ul;
	Coord coord_br;
	TileInfo ti_ul;

	const char * request = NULL;
};



static int render_info_background_fn(BackgroundJob * bg_job);

RenderInfo::RenderInfo(LayerMapnik * layer, const Coord & new_coord_ul, const Coord & new_coord_br, TileInfo * ti_ul_, const char * request_)
{
	this->thread_fn = render_info_background_fn;
	this->n_items = 1;

	this->lmk = layer;

	this->coord_ul = new_coord_ul;
	this->coord_br = new_coord_br;
	this->ti_ul = *ti_ul_;

	this->request = request_;
}




/**
 * Common render function which can run in separate thread.
 */
void LayerMapnik::render(const Coord & coord_ul, const Coord & coord_br, TileInfo * ti_ul)
{
	int64_t tt1 = g_get_real_time();
	QPixmap *pixmap = mapnik_interface_render(this->mi, coord_ul.ll.lat, coord_ul.ll.lon, coord_br.ll.lat, coord_br.ll.lon);
	int64_t tt2 = g_get_real_time();
	double tt = (double)(tt2-tt1)/1000000;
	fprintf(stderr, "DEBUG: Mapnik rendering completed in %.3f seconds\n", tt);
	if (!pixmap) {
#ifdef K_TODO
		/* A pixmap to stick into cache incase of an unrenderable area - otherwise will get continually re-requested. */
		pixmap = gdk_pixbuf_scale_simple(gdk_pixbuf_from_pixdata(&vikmapniklayer_pixmap, false, NULL), this->tile_size_x, this->tile_size_x, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
#endif
	}
	this->possibly_save_pixmap(pixmap, ti_ul);

	/* NB Mapnik can apply alpha, but use our own function for now. */
	if (this->alpha < 255) {
		pixmap = ui_pixmap_scale_alpha(pixmap, this->alpha);
	}
	map_cache_extra_t arg;
	arg.duration = tt;
	map_cache_add(pixmap, arg, ti_ul, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);
#ifdef K_TODO
	g_object_unref(pixmap);
#endif
}




static int render_info_background_fn(BackgroundJob * bg_job)
{
	RenderInfo * data = (RenderInfo *) bg_job;

	int res = a_background_thread_progress(bg_job, 0);
	if (res == 0) {
		data->lmk->render(data->coord_ul, data->coord_br, &data->ti_ul);
	}

	tp_mutex.lock();
	g_hash_table_remove(requests, data->request);
	tp_mutex.unlock();

	if (res == 0) {
		data->lmk->emit_layer_changed(); /* NB update display from background. */
	}
	return res;
}




#define REQUEST_HASHKEY_FORMAT "%d-%d-%d-%d-%d"




/**
 * Thread.
 */
void LayerMapnik::thread_add(TileInfo * ti_ul, const Coord & coord_ul, const Coord & coord_br, int x, int y, int z, int zoom, const QString & file_name)
{
	/* Create request. */
	unsigned int nn = (file_name.isEmpty() ? 0 : g_str_hash(file_name.toUtf8().constData()));
	char *request = g_strdup_printf(REQUEST_HASHKEY_FORMAT, x, y, z, zoom, nn);

	tp_mutex.lock();

	if (g_hash_table_lookup_extended(requests, request, NULL, NULL)) {
		free(request);
		tp_mutex.unlock();
		return;
	}

	RenderInfo * ri = new RenderInfo(this, coord_ul, coord_br, ti_ul, request);

#ifdef K_TODO
	g_hash_table_insert(requests, request, NULL);
#endif
	tp_mutex.unlock();

	const QString base_name = FileUtils::get_base_name(file_name);
	const QString job_description = QObject::tr("Mapnik Render %1:%2:%3 %4").arg(zoom).arg(x).arg(y).arg(base_name);
	a_background_thread(ri, ThreadPoolType::LOCAL_MAPNIK, job_description);
}




/**
 * If function returns QPixmap properly, reference counter to this
 * buffer has to be decreased, when buffer is no longer needed.
 */
QPixmap * LayerMapnik::load_pixmap(TileInfo * ti_ul, TileInfo * ti_br, bool * rerender_)
{
	*rerender_ = false;
	QPixmap * pixmap = NULL;
	char *filename = get_filename(this->file_cache_dir.toUtf8().data(), ti_ul->x, ti_ul->y, ti_ul->scale);

	struct stat stat_buf;
	if (stat(filename, &stat_buf) == 0) {
		/* Get from disk. */
		pixmap = new QPixmap();
		if (!pixmap->load(filename)) {
			delete pixmap;
			pixmap = NULL;
			qDebug() << "WW: Layer Mapnik: failed to load pixmap from" << filename;
		} else {
			if (this->alpha < 255) {
				pixmap = ui_pixmap_set_alpha(pixmap, this->alpha);
			}
			map_cache_extra_t arg;
			arg.duration = -42.0;
			map_cache_add(pixmap, arg, ti_ul, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);
		}
		/* If file is too old mark for rerendering. */
		if (g_planet_import_time < stat_buf.st_mtime) {
			*rerender_ = true;
		}
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
	const Coord coord_ul = map_utils_iTMS_to_coord(ti_ul);
	const Coord coord_br = map_utils_iTMS_to_coord(ti_br);

	QPixmap * pixmap = map_cache_get(ti_ul, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);
	if (pixmap) {
		fprintf(stderr, "MapnikLayer: MAP CACHE HIT\n");
	} else {
		fprintf(stderr, "MapnikLayer: MAP CACHE MISS\n");

		bool rerender_ = false;
		if (this->use_file_cache && !this->file_cache_dir.isEmpty()) {
			pixmap = this->load_pixmap(ti_ul, ti_br, &rerender_);
		}

		if (! pixmap || rerender_) {
			if (true) {
				this->thread_add(ti_ul, coord_ul, coord_br, ti_ul->x, ti_ul->y, ti_ul->z, ti_ul->scale, this->filename_xml.toUtf8().constData());
			} else {
				/* Run in the foreground. */
				this->render(coord_ul, coord_br, ti_ul);
				this->emit_layer_changed();
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
		this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, tr("Mapnik Rendering must be in Mercator mode"));
		return;
	}

	if (this->mi) {
		const QString copyright = mapnik_interface_get_copyright(this->mi);
		if (!copyright.isEmpty()) {
			viewport->add_copyright(copyright);
		}
	}

#if 0 /* kamilTODO: this code would be overwritten in screen_pos_to_coord() */
	ul.mode = CoordMode::LATLON;
	br.mode = CoordMode::LATLON;
#endif
	const Coord coord_ul = viewport->screen_pos_to_coord(0, 0);
	const Coord coord_br = viewport->screen_pos_to_coord(viewport->get_width(), viewport->get_height());

	double xzoom = viewport->get_xmpp();
	double yzoom = viewport->get_ympp();

	TileInfo ti_ul, ti_br;

	if (map_utils_coord_to_iTMS(coord_ul, xzoom, yzoom, &ti_ul) &&
	     map_utils_coord_to_iTMS(coord_br, xzoom, yzoom, &ti_br)) {
		/* TODO: Understand if tilesize != 256 does this need to use shrinkfactors? */
		QPixmap * pixmap;
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
					const Coord coord = map_utils_iTMS_to_coord(&ti_ul);
					viewport->coord_to_screen_pos(coord, &xx, &yy);
					viewport->draw_pixmap(*pixmap, 0, 0, xx, yy, this->tile_size_x, this->tile_size_x);
#ifdef K_TODO
					g_object_unref(pixmap);
#endif
				}
			}
		}

		/* Done after so drawn on top.
		   Just a handy guide to tile blocks. */
		if (vik_debug && vik_verbose) {
#ifdef K_TODO
			QPen * black_pen = viewport->get_widget()->style->black_pen;
			int width = viewport->get_width();
			int height = viewport->get_height();
			int xx, yy;
			ti_ul.x = xmin; ti_ul.y = ymin;

			const Coord coord = map_utils_iTMS_to_center_coord(&ti_ul);
			viewport->coord_to_screen_pos(coord, &xx, &yy);
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
}



typedef struct {
	LayerMapnik * lmk;
} menu_array_values;




static void mapnik_layer_flush_memory(menu_array_values * values)
{
	map_cache_flush_type(MAP_ID_MAPNIK_RENDER);
}




static void mapnik_layer_reload(menu_array_values * values)
{
	LayerMapnik * lmk = values->lmk;
	Viewport * viewport = g_tree->tree_get_main_viewport();

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

	Viewport * viewport = g_tree->tree_get_main_viewport();

	/* Don't load the XML config if carto load fails. */
	if (!lmk->carto_load()) {
		return;
	}
	const QString ans = mapnik_interface_load_map_file(lmk->mi, lmk->filename_xml, lmk->tile_size_x, lmk->tile_size_x);
	if (!ans.isEmpty()) {
		Dialog::error(QObject::tr("Mapnik error loading configuration file:\n%1").arg(ans), lmk->get_window());
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
	QStringList * params = mapnik_interface_get_parameters(lmk->mi);
	if (params->size()) {
		a_dialog_list(QObject::tr("Mapnik Information"), *params, 1, lmk->get_window());
	}
	delete params;
}




static void mapnik_layer_about(menu_array_values * values)
{
	LayerMapnik * lmk = values->lmk;
	Dialog::info(mapnik_interface_about(), lmk->get_window());
}




void LayerMapnik::add_menu_items(QMenu & menu)
{
	static menu_array_values values = {
		this,
	};

	QAction * action = NULL;

	/* Typical users shouldn't need to use this functionality - so debug only ATM. */
	if (vik_debug) {
		action = new QAction(QObject::tr("&Flush Memory Cache"), this);
		action->setIcon(QIcon::fromTheme("GTK_STOCK_REMOVE"));
#ifdef K_TODO
		QObject::connect(action, SIGNAL (triggered(bool)), &values, SLOT (mapnik_layer_flush_memory));
#endif
		menu.addAction(action);
	}

	action = new QAction(QObject::tr("Re&fresh"), this);
#ifdef K_TODO
	QObject::connect(action, SIGNAL (triggered(bool)), &values, SLOT (mapnik_layer_reload));
#endif
	menu.addAction(action);

	if ("" != this->filename_css) {
		action = new QAction(QObject::tr("&Run Carto Command"), this);
		action->setIcon(QIcon::fromTheme("GTK_STOCK_EXECUTE"));
#ifdef K_TODO
		QObject::connect(action, SIGNAL (triggered(bool)), &values, SLOT (mapnik_layer_carto));
#endif
		menu.addAction(action);
	}

	action = new QAction(QObject::tr("&Info"), this);
#ifdef K_TODO
	QObject::connect(action, SIGNAL (triggered(bool)), &values, SLOT (mapnik_layer_information));
#endif
	menu.addAction(action);

	action = new QAction(QObject::tr("&About"), this);
#ifdef K_TODO
	QObject::connect(action, SIGNAL (triggered(bool)), &values, SLOT (mapnik_layer_about));
#endif
	menu.addAction(action);
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
	map_utils_coord_to_iTMS(this->rerender_ul, this->rerender_zoom, this->rerender_zoom, &ti_ul);
	/* Reconvert back - thus getting the coordinate at the tile *ul corner*. */
	this->rerender_ul = map_utils_iTMS_to_coord(&ti_ul);
	/* Bottom right bound is simply +1 in TMS coords. */
	TileInfo ti_br = ti_ul;
	ti_br.x = ti_br.x+1;
	ti_br.y = ti_br.y+1;
	this->rerender_br = map_utils_iTMS_to_coord(&ti_br);
	this->thread_add(&ti_ul, this->rerender_ul, this->rerender_br, ti_ul.x, ti_ul.y, ti_ul.z, ti_ul.scale, this->filename_xml.toUtf8().constData());
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
	map_utils_coord_to_iTMS(this->rerender_ul, this->rerender_zoom, this->rerender_zoom, &ti_ul);

	map_cache_extra_t extra = map_cache_get_extra(&ti_ul, MAP_ID_MAPNIK_RENDER, this->alpha, 0.0, 0.0, this->filename_xml);

	char * tile_filename = get_filename(this->file_cache_dir.toUtf8().data(), ti_ul.x, ti_ul.y, ti_ul.scale);

	QStringList items;

	/* kamilTODO: you have very similar code in LayerMap::tile_info. */

	QString file_message;
	QString time_message;

	if (0 == access(tile_filename, F_OK)) {
		file_message = QString(tr("Tile File: %1")).arg(tile_filename);
		/* Get some timestamp information of the tile. */
		struct stat stat_buf;
		if (stat(tile_filename, &stat_buf) == 0) {
			char time_buf[64];
			strftime(time_buf, sizeof (time_buf), "%c", gmtime((const time_t *) &stat_buf.st_mtime));
			time_message = QString(tr("Tile File Timestamp: %1")).arg(time_buf);
		} else {
			time_message = QString(tr("Tile File Timestamp: Not Available"));
		}
	} else {
		file_message = QString(tr("Tile File: %1 [Not Available]")).arg(tile_filename);
		time_message = QString("");
	}

	items.push_back(file_message);
	items.push_back(time_message);

	/* Show the info. */
	if (extra.duration > 0.0) {
		QString render_message = QObject::tr("Rendering time %1 seconds").arg(extra.duration, 0, 'f', 2);
		items.push_back(render_message);
	}

	a_dialog_list(tr("Tile Information"), items, 5, this->get_window());

	free(tile_filename);
}




LayerToolMapnikFeature::LayerToolMapnikFeature(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::MAPNIK)
{
	this->id_string = "sg.tool.layer_mapnik.feature";
#ifdef K_TODO
	this->action_icon_path   = GTK_STOCK_INFO;
#endif
	this->action_label       = QObject::tr("&Mapnik Features");
	this->action_tooltip     = QObject::tr("Mapnik Features");
	// this->action_accelerator = ...; /* Empty accelerator. */
#ifdef K_TODO
	this->cursor_shape = Qt::ArrowCursor;
	this->cursor_data = NULL;
#endif
}




ToolStatus LayerToolMapnikFeature::handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	if (!layer) {
		return (ToolStatus) false; /* kamilFIXME: check this cast of returned value. */
	}

	return (ToolStatus) ((LayerMapnik *) layer)->feature_release(ev, this); /* kamilFIXME: check this cast of returned value. */
}




bool LayerMapnik::feature_release(QMouseEvent * ev, LayerTool * tool)
{
	if (ev->button() == Qt::RightButton) {
		this->rerender_ul = tool->viewport->screen_pos_to_coord(MAX(0, ev->x()), MAX(0, ev->y()));
		this->rerender_zoom = tool->viewport->get_zoom();

		if (!this->right_click_menu) {
			QAction * action = NULL;
			this->right_click_menu = new QMenu();

			action = new QAction(QObject::tr("&Rerender Tile"), this);
			action->setIcon(QIcon::fromTheme("GTK_STOCK_REFRESH"));
			QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (mapnik_layer_rerender_cb));
			this->right_click_menu->addAction(action);

			action = new QAction(QObject::tr("&Info"), this);
			action->setIcon(QIcon::fromTheme("GTK_STOCK_INFO"));
			QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (mapnik_layer_tile_info_cb));
			this->right_click_menu->addAction(action);
		}

		this->right_click_menu->exec(QCursor::pos());
	}

	return false;
}




LayerMapnik::LayerMapnik()
{
	this->type = LayerType::MAPNIK;
	strcpy(this->debug_string, "MAPNIK");
	this->interface = &vik_mapnik_layer_interface;

	this->set_initial_parameter_values();
	this->set_name(Layer::get_type_ui_label(this->type));

	this->tile_size_x = size_default().val_uint; /* FUTURE: Is there any use in this being configurable? */
	this->loaded = false;
	this->mi = mapnik_interface_new();

	/* kamilTODO: initialize this? */
	//this->rerender_ul;
	//this->rerender_br;
}
