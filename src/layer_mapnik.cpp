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
 */




#include <mutex>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cctype>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif




#include <glib.h>




#include <QDebug>
#include <QDir>




#include "window.h"
#include "vikutils.h"
#include "map_utils.h"
#include "mapcoord.h"
#include "map_cache.h"
#include "dir.h"
#include "util.h"
#include "ui_util.h"
#include "preferences.h"
#include "mapnik_interface.h"
#include "background.h"
#include "dialog.h"
#include "statusbar.h"
#include "layer_map.h"
#include "layer_mapnik.h"
#include "widget_file_entry.h"
#include "file.h"
#include "file_utils.h"
#include "viewport_internal.h"




using namespace SlavGPS;




#define SG_MODULE "Mapnik Layer"




extern bool vik_debug;
extern bool vik_verbose;




static SGVariant file_default(void)      { return SGVariant(""); }
static SGVariant size_default(void)      { return SGVariant(256, SGVariantType::Int); }
static SGVariant cache_dir_default(void) { return SGVariant(MapCache::get_default_maps_dir() + "MapnikRendering"); }


static ParameterScale<int> scale_alpha(0,  255, SGVariant(255),  5, 0); /* PARAM_ALPHA */
static ParameterScale<int> scale_timeout(0, 1024, SGVariant(168), 12, 0); /* Renderer timeout hours. Value of hardcoded default is one week. */
static ParameterScale<int> scale_threads(1, 64, SGVariant(1), 1, 0); /* 64 threads should be enough for anyone... */


enum {
	PARAM_CONFIG_CSS = 0,
	PARAM_CONFIG_XML,
	PARAM_ALPHA,
	PARAM_USE_FILE_CACHE,
	PARAM_FILE_CACHE_DIR,
	NUM_PARAMS
};



FileSelectorWidget::FileTypeFilter file_type_css[1] = { FileSelectorWidget::FileTypeFilter::Carto };
FileSelectorWidget::FileTypeFilter file_type_xml[1] = { FileSelectorWidget::FileTypeFilter::XML };



static ParameterSpecification mapnik_layer_param_specs[] = {
	{ PARAM_CONFIG_CSS,     "config-file-mml", SGVariantType::String,  PARAMETER_GROUP_GENERIC, QObject::tr("CSS (MML) Config File:"), WidgetType::FileSelector, file_type_css, file_default,       QObject::tr("CartoCSS configuration file") },
	{ PARAM_CONFIG_XML,     "config-file-xml", SGVariantType::String,  PARAMETER_GROUP_GENERIC, QObject::tr("XML Config File:"),       WidgetType::FileSelector, file_type_xml, file_default,       QObject::tr("Mapnik XML configuration file") },
	{ PARAM_ALPHA,          "alpha",           SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Alpha:"),                 WidgetType::HScale,       &scale_alpha,  NULL,               "" },
	{ PARAM_USE_FILE_CACHE, "use-file-cache",  SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Use File Cache:"),        WidgetType::CheckButton,  NULL,          sg_variant_true,    "" },
	{ PARAM_FILE_CACHE_DIR, "file-cache-dir",  SGVariantType::String,  PARAMETER_GROUP_GENERIC, QObject::tr("File Cache Directory:"),  WidgetType::FolderEntry,  NULL,          cache_dir_default,  "" },

	{ NUM_PARAMS,           "",                SGVariantType::Empty,   PARAMETER_GROUP_GENERIC, "",                                    WidgetType::None,         NULL,          NULL,               "" }, /* Guard. */
};




LayerMapnikInterface vik_mapnik_layer_interface;




LayerMapnikInterface::LayerMapnikInterface()
{
	this->parameters_c = mapnik_layer_param_specs;

	this->fixed_layer_type_string = "Mapnik Rendering"; /* Non-translatable. */

	// this->action_accelerator =  ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

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




#define PREFERENCES_NAMESPACE_MAPNIK "mapnik."




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
	{ 0, PREFERENCES_NAMESPACE_MAPNIK "plugins_directory",                   SGVariantType::String,  PARAMETER_GROUP_GENERIC,  QObject::tr("Plugins Directory:"),        WidgetType::FolderEntry,   NULL,           plugins_default,  QObject::tr("You need to restart Viking for a change to this value to be used") },
	{ 1, PREFERENCES_NAMESPACE_MAPNIK "fonts_directory",                     SGVariantType::String,  PARAMETER_GROUP_GENERIC,  QObject::tr("Fonts Directory:"),          WidgetType::FolderEntry,   NULL,           fonts_default,    QObject::tr("You need to restart Viking for a change to this value to be used") },
	{ 2, PREFERENCES_NAMESPACE_MAPNIK "recurse_fonts_directory",             SGVariantType::Boolean, PARAMETER_GROUP_GENERIC,  QObject::tr("Recurse Fonts Directory:"),  WidgetType::CheckButton,   NULL,           sg_variant_true,  QObject::tr("You need to restart Viking for a change to this value to be used") },
	{ 3, PREFERENCES_NAMESPACE_MAPNIK "rerender_after",                      SGVariantType::Int,     PARAMETER_GROUP_GENERIC,  QObject::tr("Rerender Timeout (hours):"), WidgetType::SpinBoxInt,    &scale_timeout, NULL,             QObject::tr("You need to restart Viking for a change to this value to be used") },
	/* Changeable any time. */
	{ 4, PREFERENCES_NAMESPACE_MAPNIK "carto",                               SGVariantType::String,  PARAMETER_GROUP_GENERIC,  QObject::tr("CartoCSS:"),                 WidgetType::FileSelector,  NULL,           NULL,             QObject::tr("The program to convert CartoCSS files into Mapnik XML") },
	{ 5, PREFERENCES_NAMESPACE_MAPNIK "background_max_threads_local_mapnik", SGVariantType::Int,     PARAMETER_GROUP_GENERIC,  QObject::tr("Threads:"),                  WidgetType::SpinBoxInt,    &scale_threads, NULL,             QObject::tr("Number of threads to use for Mapnik tasks. You need to restart Viking for a change to this value to be used") },
	{ 6,                              "",                                    SGVariantType::Empty,   PARAMETER_GROUP_GENERIC,  "",                                       WidgetType::None,          NULL,           NULL,             "" } /* Guard. */
};




static time_t g_planet_import_time;
static std::mutex tp_mutex;
static QHash<QString, bool> mapnik_requests; /* Just for storing of QStrings. */




/**
 * Just initialize preferences.
 */
void LayerMapnik::init(void)
{
#ifdef HAVE_LIBMAPNIK
	Preferences::register_parameter_group(PREFERENCES_NAMESPACE_MAPNIK, QObject::tr("Mapnik"));

	unsigned int i = 0;

	Preferences::register_parameter_instance(prefs[i], plugins_default());
	i++;
	Preferences::register_parameter_instance(prefs[i], fonts_default());
	i++;
	Preferences::register_parameter_instance(prefs[i], SGVariant(true, prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(prefs[i], scale_timeout.initial);
	i++;
	Preferences::register_parameter_instance(prefs[i], SGVariant("carto", prefs[i].type_id));
	i++;
	/* Default to 1 thread due to potential crashing issues. */
	Preferences::register_parameter_instance(prefs[i], SGVariant((int32_t) 1, prefs[i].type_id));
	i++;
#endif
}




/**
   Initialize data structures - now that reading preferences is OK to perform.
*/
void LayerMapnik::post_init(void)
{
	const int hours = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK "rerender_after").u.val_int;
	g_planet_import_time = QDateTime::currentDateTime().addSecs(-1 * hours * 60 * 60).toTime_t(); /* In local time zone. */

	/* Similar to mod_tile method to mark DB has been imported/significantly changed to cause a rerendering of all tiles. */
	const QString import_time_full_path = SlavGPSLocations::get_file_full_path("planet-import-complete");
	struct stat stat_buf;
	if (stat(import_time_full_path.toUtf8().constData(), &stat_buf) == 0) {
		/* Only update if newer. */
		if (g_planet_import_time > stat_buf.st_mtime) {
			g_planet_import_time = stat_buf.st_mtime;
		}
	}
}




void LayerMapnik::uninit(void)
{
}




/* NB Only performed once per program run. */
void SlavGPS::layer_mapnik_init(void)
{
#ifdef HAVE_LIBMAPNIK
	const SGVariant plugins_dir = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK "plugins_directory");
	const SGVariant fonts_dir = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK "fonts_directory");
	const SGVariant recurse = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK "recurse_fonts_directory");

	if (plugins_dir.type_id != SGVariantType::Empty
	    && fonts_dir.type_id != SGVariantType::Empty
	    && recurse.type_id != SGVariantType::Empty) {

		MapnikInterface::initialize(plugins_dir.val_string, fonts_dir.val_string, recurse.u.val_bool);
	} else {
		qDebug() << SG_PREFIX_E << "Unable to initialize Mapnik interface from preferences";
	}
#endif
}




QString LayerMapnik::get_tooltip(void) const
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




Layer * LayerMapnikInterface::unmarshall(Pickle & pickle, Viewport * viewport)
{
	LayerMapnik * layer = new LayerMapnik();

	layer->tile_size_x = size_default().u.val_int; /* FUTURE: Is there any use in this being configurable? */
	layer->loaded = false;
	layer->mi = new MapnikInterface();
	layer->unmarshall_params(pickle);

	return layer;
}




bool LayerMapnik::set_param_value(param_id_t param_id, const SGVariant & data, bool is_file_operation)
{
	switch (param_id) {
		case PARAM_CONFIG_CSS:
			this->set_file_css(data.val_string);
			break;
		case PARAM_CONFIG_XML:
			this->set_file_xml(data.val_string);
			break;
		case PARAM_ALPHA:
			if (data.u.val_int >= scale_alpha.min && data.u.val_int <= scale_alpha.max) {
				this->alpha = data.u.val_int;
			}
			break;
		case PARAM_USE_FILE_CACHE:
			this->use_file_cache = data.u.val_bool;
			break;
		case PARAM_FILE_CACHE_DIR:
			this->set_cache_dir(data.val_string);
			break;
		default:
			break;
	}
	return true;
}




SGVariant LayerMapnik::get_param_value(param_id_t param_id, bool is_file_operation) const
{
	SGVariant param_value;
	switch (param_id) {
	case PARAM_CONFIG_CSS: {
		param_value = SGVariant(this->filename_css);
		bool set = false;
		if (is_file_operation) {
			if (Preferences::get_file_path_format() == FilePathFormat::Relative) {
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
			if (Preferences::get_file_path_format() == FilePathFormat::Relative) {
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

	const SGVariant pref_value = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK "carto");
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
		window_->statusbar_update(StatusBarField::Info, tr("Running: %2").arg(command));
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
		const QString msg = tr("%1 completed in %2 seconds").arg(pref_value.val_string).arg((double) (tt2-tt1)/G_USEC_PER_SEC, 0, 'f', 1);
		window_->statusbar_update(StatusBarField::Info, msg);
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

	const QString ans = this->mi->load_map_file(this->filename_xml, this->tile_size_x, this->tile_size_x);
	if (ans.isEmpty()) {
		this->loaded = true;
		if (!from_file) {
			/* TODO_LATER: shouldn't we use Window::update_recent_files()? */
			update_desktop_recent_documents(this->get_window(), this->filename_xml, ""); /* TODO_LATER: provide correct mime data type for mapnik data. */
		}
	} else {
		Dialog::error(tr("Mapnik error loading configuration file:\n%1").arg(ans), this->get_window());
	}
}




static QString get_filename(const QString dir, int x, int y, const TileScale & scale)
{
	return QDir::toNativeSeparators(QString("%1/%2/%3/%4.png").arg(dir).arg(scale.get_tile_zoom_level()).arg(x).arg(y));
}




void LayerMapnik::possibly_save_pixmap(QPixmap & pixmap, const TileInfo & ti_ul)
{
	if (!this->use_file_cache) {
		return;
	}

	if (this->file_cache_dir.isEmpty()) {
		return;
	}

	const QString filename = get_filename(this->file_cache_dir, ti_ul.x, ti_ul.y, ti_ul.scale);

	char *dir = g_path_get_dirname(filename.toUtf8().constData());
	if (0 != access(filename.toUtf8().constData(), F_OK)) {
		if (g_mkdir_with_parents(dir , 0777) != 0) {
			fprintf(stderr, "WARNING: %s: Failed to mkdir %s\n", __FUNCTION__, dir);
		}
	}
	free(dir);

	if (!pixmap.save(filename, "png")) {
		qDebug() << "WW: Layer Mapnik: failed to save pixmap to" << filename;
	}
}




class RenderInfo : public BackgroundJob {
public:
	RenderInfo(LayerMapnik * layer, const Coord & new_coord_ul, const Coord & new_coord_br, const TileInfo & ti_ul, const QString & new_request);

	void run(void);

	LayerMapnik * lmk = NULL;

	Coord coord_ul;
	Coord coord_br;
	TileInfo ti_ul;

	QString request;
};




RenderInfo::RenderInfo(LayerMapnik * layer, const Coord & new_coord_ul, const Coord & new_coord_br, const TileInfo & new_ti_ul, const QString & new_request)
{
	this->n_items = 1;

	this->lmk = layer;

	this->coord_ul = new_coord_ul;
	this->coord_br = new_coord_br;
	this->ti_ul = new_ti_ul;

	this->request = new_request;
}




/**
 * Common render function which can run in separate thread.
 */
void LayerMapnik::render(const TileInfo & ti_ul, const Coord & coord_ul, const Coord & coord_br)
{
	int64_t tt1 = g_get_real_time();
	QPixmap pixmap = this->mi->render_map(coord_ul.ll.lat, coord_ul.ll.lon, coord_br.ll.lat, coord_br.ll.lon);
	int64_t tt2 = g_get_real_time();
	double tt = (double)(tt2-tt1)/1000000;
	fprintf(stderr, "DEBUG: Mapnik rendering completed in %.3f seconds\n", tt);
	if (pixmap.isNull()) {
		/* A pixmap to stick into cache incase of an unrenderable area - otherwise will get continually re-requested. */
		const QPixmap substitute = QPixmap(":/icons/layer/mapnik.png");
		pixmap = substitute.scaled(this->tile_size_x, this->tile_size_x, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
	this->possibly_save_pixmap(pixmap, ti_ul);

	/* TODO_MAYBE: Mapnik can apply alpha, but use our own function for now. */
	if (scale_alpha.is_in_range(this->alpha)) {
		ui_pixmap_scale_alpha(pixmap, this->alpha);
	}

	MapCache::add_tile_pixmap(pixmap, MapCacheItemProperties(tt), ti_ul, MapTypeID::MapnikRender, this->alpha, 0.0, 0.0, this->filename_xml);
}




void RenderInfo::run(void)
{
	const bool end_job = this->set_progress_state(0);
	if (!end_job) {
		this->lmk->render(this->ti_ul, this->coord_ul, this->coord_br);
	}

	tp_mutex.lock();
	mapnik_requests.remove(this->request);
	tp_mutex.unlock();

	if (!end_job) {
		this->lmk->emit_tree_item_changed("Mapnik - render info"); /* NB update display from background. */
	}
	return;
}




#define REQUEST_HASHKEY_FORMAT "%1-%2-%3-%4-%5"




/**
 * Thread.
 */
void LayerMapnik::thread_add(const TileInfo & ti_ul, const Coord & coord_ul, const Coord & coord_br, const QString & file_name)
{
	/* Create request. */
	const unsigned int nn = file_name.isEmpty() ? 0 : qHash(file_name, 0);
	const QString request = QString(REQUEST_HASHKEY_FORMAT).arg(ti_ul.x).arg(ti_ul.y).arg(ti_ul.z).arg(ti_ul.scale.get_scale_value()).arg(nn);

	tp_mutex.lock();

	if (mapnik_requests.contains(request)) {
		tp_mutex.unlock();
		return;
	}

	RenderInfo * ri = new RenderInfo(this, coord_ul, coord_br, ti_ul, request);
	mapnik_requests.insert(request, true);

	tp_mutex.unlock();

	const QString base_name = FileUtils::get_base_name(file_name);
	const QString job_description = QObject::tr("Mapnik Render %1:%2:%3 %4").arg(ti_ul.scale.get_scale_value()).arg(ti_ul.x).arg(ti_ul.y).arg(base_name);
	ri->set_description(job_description);
	ri->run_in_background(ThreadPoolType::LocalMapnik);
}




/**
 * If function returns QPixmap properly, reference counter to this
 * buffer has to be decreased, when buffer is no longer needed.
 */
QPixmap LayerMapnik::load_pixmap(const TileInfo & ti_ul, const TileInfo & ti_br, bool * rerender_) const
{
	*rerender_ = false;
	QPixmap pixmap;
	const QString filename = get_filename(this->file_cache_dir, ti_ul.x, ti_ul.y, ti_ul.scale);

	struct stat stat_buf;
	if (stat(filename.toUtf8().constData(), &stat_buf) == 0) {
		/* Get from disk. */
		if (!pixmap.load(filename)) {
			qDebug() << "WW: Layer Mapnik: failed to load pixmap from" << filename;
		} else {
			if (scale_alpha.is_in_range(this->alpha)) {
				ui_pixmap_set_alpha(pixmap, this->alpha);
			}

			MapCache::add_tile_pixmap(pixmap, MapCacheItemProperties(-1.0), ti_ul, MapTypeID::MapnikRender, this->alpha, 0.0, 0.0, this->filename_xml);
		}
		/* If file is too old mark for rerendering. */
		if (g_planet_import_time < stat_buf.st_mtime) {
			*rerender_ = true;
		}
	}

	return pixmap;
}




/**
 * Caller has to decrease reference counter of returned QPixmap,
 * when buffer is no longer needed.
 */
QPixmap LayerMapnik::get_pixmap(const TileInfo & ti_ul, const TileInfo & ti_br)
{
	const Coord coord_ul = Coord(MapUtils::iTMS_to_lat_lon(ti_ul), CoordMode::LatLon);
	const Coord coord_br = Coord(MapUtils::iTMS_to_lat_lon(ti_br), CoordMode::LatLon);

	QPixmap pixmap = MapCache::get_tile_pixmap(ti_ul, MapTypeID::MapnikRender, this->alpha, 0.0, 0.0, this->filename_xml);
	if (!pixmap.isNull()) {
		qDebug() << SG_PREFIX_I << "MAP CACHE HIT";
	} else {
		qDebug() << SG_PREFIX_I << "MAP CACHE MISS";

		bool rerender_ = false;
		if (this->use_file_cache && !this->file_cache_dir.isEmpty()) {
			pixmap = this->load_pixmap(ti_ul, ti_br, &rerender_);
		}

		if (pixmap.isNull() || rerender_) {
			if (true) {
				this->thread_add(ti_ul, coord_ul, coord_br, this->filename_xml);
			} else {
				/* Run in the foreground. */
				this->render(ti_ul, coord_ul, coord_br);
				this->emit_tree_item_changed("Mapnik - get pixmap");
			}
		}
	}

	return pixmap;
}




void LayerMapnik::draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected)
{
	if (!this->loaded) {
		return;
	}

	if (viewport->get_drawmode() != ViewportDrawMode::Mercator) {
		this->get_window()->get_statusbar()->set_message(StatusBarField::Info, tr("Mapnik Rendering must be in Mercator mode"));
		return;
	}

	if (this->mi) {
		const QString copyright = this->mi->get_copyright();
		if (!copyright.isEmpty()) {
			viewport->add_copyright(copyright);
		}
	}

	const Coord coord_ul = viewport->screen_pos_to_coord(0, 0);
	const Coord coord_br = viewport->screen_pos_to_coord(viewport->get_width(), viewport->get_height());

	const VikingZoomLevel viking_zoom_level = viewport->get_viking_zoom_level();



	if (coord_ul.mode != CoordMode::LatLon) {
		qDebug() << SG_PREFIX_E << "Invalid coord mode of ul coord:" << (int) coord_ul.mode;
		return;
	}
	if (coord_br.mode != CoordMode::LatLon) {
		qDebug() << SG_PREFIX_E << "Invalid coord mode of br coord:" << (int) coord_br.mode;
		return;
	}


	TileInfo ti_ul, ti_br;
	if (sg_ret::ok != MapUtils::lat_lon_to_iTMS(coord_ul.ll, viking_zoom_level, ti_ul)) {
		qDebug() << SG_PREFIX_E << "Failed to convert ul";
		return;
	}
	if (sg_ret::ok != MapUtils::lat_lon_to_iTMS(coord_br.ll, viking_zoom_level, ti_br)) {
		qDebug() << SG_PREFIX_E << "Failed to convert br";
		return;
	}


	/* TODO_LATER: Understand if tilesize != 256 does this need to use shrinkfactors? */

	const int xmin = std::min(ti_ul.x, ti_br.x);
	const int xmax = std::max(ti_ul.x, ti_br.x);
	const int ymin = std::min(ti_ul.y, ti_br.y);
	const int ymax = std::max(ti_ul.y, ti_br.y);

	/* Split rendering into a grid for the current viewport
	   thus each individual 'tile' can then be stored in the map cache. */
	for (int x = xmin; x <= xmax; x++) {
		for (int y = ymin; y <= ymax; y++) {
			ti_ul.x = x;
			ti_ul.y = y;
			ti_br.x = x+1;
			ti_br.y = y+1;

			const QPixmap pixmap = this->get_pixmap(ti_ul, ti_br);
			if (!pixmap.isNull()) {
				const LatLon lat_lon = MapUtils::iTMS_to_lat_lon(ti_ul);
				int xx, yy;
				viewport->lat_lon_to_screen_pos(lat_lon, &xx, &yy);
				viewport->draw_pixmap(pixmap, 0, 0, xx, yy, this->tile_size_x, this->tile_size_x);
			}
		}
	}

	/* Done after so drawn on top.
	   Just a handy guide to tile blocks. */
	if (vik_debug && vik_verbose) {
#ifdef K_FIXME_RESTORE
		QPen * black_pen = viewport->get_widget()->style->black_pen;
		int width = viewport->get_width();
		int height = viewport->get_height();
		int xx, yy;
		ti_ul.x = xmin; ti_ul.y = ymin;

		const LatLon lat_lon = MapUtils::iTMS_to_center_lat_lon(&ti_ul);
		viewport->lat_lon_to_screen_pos(lat_lon, &xx, &yy);
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




LayerMapnik::~LayerMapnik()
{
	delete this->mi;
}




void LayerMapnik::flush_memory_cb(void)
{
	MapCache::flush_type(MapTypeID::MapnikRender);
}




void LayerMapnik::reload_cb(void)
{
	Viewport * viewport = ThisApp::get_main_viewport();

	this->post_read(viewport, false);
	this->draw_tree_item(viewport, false, false);
}




/**
   Force carto run

   Most carto projects will consist of many files.
   ATM don't have a way of detecting when any of the included files have changed.
   Thus allow a manual method to force re-running carto.
*/
void LayerMapnik::run_carto_cb(void)
{
	Viewport * viewport = ThisApp::get_main_viewport();

	/* Don't load the XML config if carto load fails. */
	if (!this->carto_load()) {
		return;
	}
	const QString ans = this->mi->load_map_file(this->filename_xml, this->tile_size_x, this->tile_size_x);
	if (ans.isEmpty()) {
		this->draw_tree_item(viewport, false, false);
	} else {
		Dialog::error(QObject::tr("Mapnik error loading configuration file:\n%1").arg(ans), this->get_window());
	}
}




/**
   Show Mapnik configuration parameters
*/
void LayerMapnik::information_cb(void)
{
	if (!this->mi) {
		return;
	}
	QStringList params = this->mi->get_parameters();
	if (params.size()) {
		a_dialog_list(QObject::tr("Mapnik Information"), params, 1, this->get_window());
	}
}




void LayerMapnik::about_cb(void)
{
	Dialog::info(MapnikInterface::about(), this->get_window());
}




void LayerMapnik::add_menu_items(QMenu & menu)
{
	QAction * action = NULL;

	/* Typical users shouldn't need to use this functionality - so debug only ATM. */
	if (vik_debug) {
		action = new QAction(QObject::tr("&Flush Memory Cache"), this);
		action->setIcon(QIcon::fromTheme("GTK_STOCK_REMOVE"));
		QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (flush_memory_cb()));
		menu.addAction(action);
	}

	action = new QAction(QObject::tr("Re&fresh"), this);
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (reload_cb()));
	menu.addAction(action);


	if ("" != this->filename_css) {
		action = new QAction(QObject::tr("&Run Carto Command"), this);
		action->setIcon(QIcon::fromTheme("GTK_STOCK_EXECUTE"));
		QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (run_carto_cb()));
		menu.addAction(action);
	}

	action = new QAction(QObject::tr("&Info"), this);
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (information_cb()));
	menu.addAction(action);


	action = new QAction(QObject::tr("&About"), this);
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (about_cb()));
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
	if (this->rerender_ul.mode != CoordMode::LatLon) {
		qDebug() << SG_PREFIX_E << "Invalid coord mode of ul:" << (int) this->rerender_ul.mode;
		return;
	}

	TileInfo ti_ul;
	/* Requested position to map coord. */
	if (sg_ret::ok != MapUtils::lat_lon_to_iTMS(this->rerender_ul.ll, this->rerender_viking_zoom_level, ti_ul)) {
		qDebug() << SG_PREFIX_E << "Failed to convert ul";
		return;
	}

	/* Reconvert back - thus getting the coordinate at the tile *ul corner*. */
	this->rerender_ul = Coord(MapUtils::iTMS_to_lat_lon(ti_ul), CoordMode::LatLon);

	/* Bottom right bound is simply +1 in TMS coords. */
	TileInfo ti_br = ti_ul;
	ti_br.x = ti_br.x+1;
	ti_br.y = ti_br.y+1;
	this->rerender_br = Coord(MapUtils::iTMS_to_lat_lon(ti_br), CoordMode::LatLon);
	this->thread_add(ti_ul, this->rerender_ul, this->rerender_br, this->filename_xml);
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
	if (this->rerender_ul.mode != CoordMode::LatLon) {
		qDebug() << SG_PREFIX_E << "Invalid coord mode of ul:" << (int) this->rerender_ul.mode;
		return;
	}

	TileInfo ti_ul;
	/* Requested position to map coord. */
	if (sg_ret::ok != MapUtils::lat_lon_to_iTMS(this->rerender_ul.ll, this->rerender_viking_zoom_level, ti_ul)) {
		qDebug() << SG_PREFIX_E << "Failed to convert ul";
		return;
	}

	MapCacheItemProperties properties = MapCache::get_properties(ti_ul, MapTypeID::MapnikRender, this->alpha, 0.0, 0.0, this->filename_xml);

	const QString tile_file_full_path = get_filename(this->file_cache_dir, ti_ul.x, ti_ul.y, ti_ul.scale);

	QStringList tile_info_strings;
	tile_info_add_file_info_strings(tile_info_strings, tile_file_full_path);

	/* Show the info. */
	if (properties.duration > 0.0) {
		QString render_message = QObject::tr("Rendering time %1 seconds").arg(properties.duration, 0, 'f', 2);
		tile_info_strings.push_back(render_message);
	}

	a_dialog_list(tr("Tile Information"), tile_info_strings, 5, this->get_window());
}




LayerToolMapnikFeature::LayerToolMapnikFeature(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::Mapnik)
{
	this->id_string = "sg.tool.layer_mapnik.feature";

	this->action_icon_path   = ":/icons/layer_tool/mapnik_feature.png";
	this->action_label       = QObject::tr("&Mapnik Features");
	this->action_tooltip     = QObject::tr("Mapnik Features");
	// this->action_accelerator = ...; /* Empty accelerator. */
}




ToolStatus LayerToolMapnikFeature::handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	if (!layer) {
		return ToolStatus::Ignored;
	}

	return ((LayerMapnik *) layer)->feature_release(ev, this);
}




ToolStatus LayerMapnik::feature_release(QMouseEvent * ev, LayerTool * tool)
{
	if (ev->button() == Qt::RightButton) {
		this->rerender_ul = tool->viewport->screen_pos_to_coord(MAX(0, ev->x()), MAX(0, ev->y()));
		this->rerender_viking_zoom_level = tool->viewport->get_viking_zoom_level();

		if (!this->right_click_menu) {
			QAction * action = NULL;
			this->right_click_menu = new QMenu();

			action = new QAction(QObject::tr("&Rerender Tile"), this);
			action->setIcon(QIcon::fromTheme("GTK_STOCK_REFRESH"));
			QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (mapnik_layer_rerender_cb));
			this->right_click_menu->addAction(action);

			action = new QAction(QObject::tr("&Info"), this);
			action->setIcon(QIcon::fromTheme("dialog-information"));
			QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (mapnik_layer_tile_info_cb));
			this->right_click_menu->addAction(action);
		}

		this->right_click_menu->exec(QCursor::pos());

		/* FIXME: Where do we return Ack? */
	}

	return ToolStatus::Ignored;
}




LayerMapnik::LayerMapnik()
{
	this->type = LayerType::Mapnik;
	strcpy(this->debug_string, "MAPNIK");
	this->interface = &vik_mapnik_layer_interface;

	this->set_initial_parameter_values();
	this->set_name(Layer::get_type_ui_label(this->type));

	this->tile_size_x = size_default().u.val_int; /* FUTURE: Is there any use in this being configurable? */
	this->loaded = false;
	this->mi = new MapnikInterface();

	/* TODO_LATER: initialize this? */
	//this->rerender_ul;
	//this->rerender_br;
}
