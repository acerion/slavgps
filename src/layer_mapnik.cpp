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
#include <ctime>

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
#include "layer_mapnik_wrapper.h"
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
#define LAYER_MAPNIK_GRID_COLOR "black"

/* x, y, z, scale, xml map file path's hash. */
#define REQUEST_HASHKEY_FORMAT "%1-%2-%3-%4-%5"




extern bool vik_debug;
extern bool vik_verbose;




/* Wrapper around two consecutive calls to
   clock_gettime(CLOCK_MONOTONIC, ...). Used to measure duration of
   any action. Call ::start() before the action starts and ::stop()
   after the action completes. It provides nanosecond-resolution for
   very fast actions, e.g. rendering of a single Map tile. */
class DurationMeter {
public:
	void start(void);
	void stop(void);
	double get_seconds(void) const;
	long get_nanoseconds(void) const;
	QString get_seconds_string(int precision) const;
	bool is_valid(void) const;
private:
	struct timespec before = { 0, 0 };
	struct timespec after = { 0, 0 };

	int before_rv = -1;
	int after_rv = -1;
};




static QString g_mapnik_xml_mime_type("text/xml");

static SGVariant file_default(void)      { return SGVariant(""); }
static SGVariant size_default(void)      { return SGVariant(256, SGVariantType::Int); } /* TODO_MAYBE: Is there any use in this being configurable? */
static SGVariant cache_dir_default(void) { return SGVariant(MapCache::get_default_maps_dir() + "MapnikRendering"); }


static ParameterScale<int> scale_alpha(0,  255, SGVariant(255, SGVariantType::Int), 5, 0);
static MeasurementScale<Duration, Duration_ll, DurationUnit> scale_renderer_timeout(0, 1024, 7 * 24, 12, DurationUnit::Hours, 0);
static ParameterScale<int> scale_threads(1, 64, SGVariant(1, SGVariantType::Int), 1, 0); /* 64 threads should be enough for anyone... */


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

	this->fixed_layer_kind_string = "Mapnik Rendering"; /* Non-translatable. */

	// this->action_accelerator =  ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->ui_labels.new_layer = QObject::tr("New Mapnik Rendering Layer");
	this->ui_labels.translated_layer_kind = QObject::tr("Mapnik Rendering");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of Mapnik Rendering Layer");
}




LayerToolContainer * LayerMapnikInterface::create_tools(Window * window, GisViewport * gisview)
{
	/* This method should be called only once. */
	static bool created = false;
	if (created) {
		return NULL;
	}

	auto tools = new LayerToolContainer;

	LayerTool * tool = new LayerToolMapnikFeature(window, gisview);
	tools->insert({ tool->get_tool_id(), tool });

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
	{ 0, PREFERENCES_NAMESPACE_MAPNIK "plugins_directory",                   SGVariantType::String,       PARAMETER_GROUP_GENERIC,  QObject::tr("Plugins Directory:"),        WidgetType::FolderEntry,   NULL,                    plugins_default,  QObject::tr("You need to restart Viking for a change to this value to be used") },
	{ 1, PREFERENCES_NAMESPACE_MAPNIK "fonts_directory",                     SGVariantType::String,       PARAMETER_GROUP_GENERIC,  QObject::tr("Fonts Directory:"),          WidgetType::FolderEntry,   NULL,                    fonts_default,    QObject::tr("You need to restart Viking for a change to this value to be used") },
	{ 2, PREFERENCES_NAMESPACE_MAPNIK "recurse_fonts_directory",             SGVariantType::Boolean,      PARAMETER_GROUP_GENERIC,  QObject::tr("Recurse Fonts Directory:"),  WidgetType::CheckButton,   NULL,                    sg_variant_true,  QObject::tr("You need to restart Viking for a change to this value to be used") },
	{ 3, PREFERENCES_NAMESPACE_MAPNIK "rerender_after",                      SGVariantType::DurationType, PARAMETER_GROUP_GENERIC,  QObject::tr("Rerender Timeout:"),         WidgetType::DurationType,  &scale_renderer_timeout, NULL,             QObject::tr("You need to restart Viking for a change to this value to be used") }, // KKAMIL
	/* Changeable any time. */
	{ 4, PREFERENCES_NAMESPACE_MAPNIK "carto",                               SGVariantType::String,       PARAMETER_GROUP_GENERIC,  QObject::tr("CartoCSS:"),                 WidgetType::FileSelector,  NULL,                    NULL,             QObject::tr("The program to convert CartoCSS files into Mapnik XML") },
	{ 5, PREFERENCES_NAMESPACE_MAPNIK "background_max_threads_local_mapnik", SGVariantType::Int,          PARAMETER_GROUP_GENERIC,  QObject::tr("Threads:"),                  WidgetType::SpinBoxInt,    &scale_threads,          NULL,             QObject::tr("Number of threads to use for Mapnik tasks. You need to restart Viking for a change to this value to be used") },
	{ 6,                              "",                                    SGVariantType::Empty,        PARAMETER_GROUP_GENERIC,  "",                                       WidgetType::None,          NULL,                    NULL,             "" } /* Guard. */
};




static time_t g_planet_import_time;
static std::mutex tp_mutex;
static QHash<QString, bool> mapnik_job_requests; /* Just for storing of QStrings. */




/**
 * Just initialize preferences.
 */
void LayerMapnik::init(void)
{
#ifdef HAVE_LIBMAPNIK
	Preferences::register_parameter_group(PREFERENCES_NAMESPACE_MAPNIK, tr("Mapnik"));

	unsigned int i = 0;

	Preferences::register_parameter_instance(prefs[i], plugins_default());
	i++;
	Preferences::register_parameter_instance(prefs[i], fonts_default());
	i++;
	Preferences::register_parameter_instance(prefs[i], SGVariant(true, prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(prefs[i], SGVariant(scale_renderer_timeout.m_initial, prefs[i].type_id));
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
	const Duration seconds = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK "rerender_after").get_duration().convert_to_unit(DurationUnit::Seconds);
	g_planet_import_time = QDateTime::currentDateTime().addSecs(-1 * seconds.get_ll_value()).toTime_t(); /* In local time zone. */

	/* Similar to mod_tile method to mark DB has been imported/significantly changed to cause a rerendering of all tiles. */
	const QString import_time_full_path = SlavGPSLocations::get_file_full_path("planet-import-complete");
	struct stat stat_buf;
	if (stat(import_time_full_path.toUtf8().constData(), &stat_buf) == 0) {
		/* Only update if newer. */
		if (g_planet_import_time > stat_buf.st_mtime) {
			g_planet_import_time = stat_buf.st_mtime;
		}
	}

	LayerMapnik::init_wrapper();
}




void LayerMapnik::uninit(void)
{
}




/* Only performed once per program run.
   TODO_LATER: run on every change of preferences? */
void LayerMapnik::init_wrapper(void)
{
#ifdef HAVE_LIBMAPNIK
	const SGVariant plugins_dir = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK "plugins_directory");
	const SGVariant fonts_dir = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK "fonts_directory");
	const SGVariant recurse = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK "recurse_fonts_directory");

	if (plugins_dir.type_id != SGVariantType::Empty
	    && fonts_dir.type_id != SGVariantType::Empty
	    && recurse.type_id != SGVariantType::Empty) {

		MapnikWrapper::initialize(plugins_dir.val_string, fonts_dir.val_string, recurse.u.val_bool);
	} else {
		qDebug() << SG_PREFIX_E << "Unable to initialize Mapnik interface from preferences";
	}
#endif
}




QString LayerMapnik::get_tooltip(void) const
{
	return this->xml_map_file_full_path;
}




void LayerMapnik::set_file_xml(const QString & file_full_path)
{
	/* Mapnik doesn't seem to cope with relative filenames. */
	if (file_full_path.isEmpty()) {
		this->xml_map_file_full_path = file_full_path;
	} else {
		this->xml_map_file_full_path = vu_get_canonical_filename(this, file_full_path, this->get_window()->get_current_document_full_path());
	}
}




void LayerMapnik::set_file_css(const QString & file_full_path)
{
	this->css_file_full_path = file_full_path;
}




void LayerMapnik::set_cache_dir(const QString & name_)
{
	this->file_cache_dir = name_;
}




Layer * LayerMapnikInterface::unmarshall(Pickle & pickle, GisViewport * gisview)
{
	LayerMapnik * layer = new LayerMapnik();

	layer->set_tile_size(size_default().u.val_int);
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
		param_value = SGVariant(this->css_file_full_path);
		bool set = false;
		if (is_file_operation) {
			if (Preferences::get_file_path_format() == FilePathFormat::Relative) {
				const QString cwd = QDir::currentPath();
				if (!cwd.isEmpty()) {
					param_value = SGVariant(file_GetRelativeFilename(cwd, this->css_file_full_path));
					set = true;
				}
			}
		}
		if (!set) {
			param_value = SGVariant(this->css_file_full_path);
		}
		break;
	}
	case PARAM_CONFIG_XML: {
		param_value = SGVariant(this->xml_map_file_full_path);
		bool set = false;
		if (is_file_operation) {
			if (Preferences::get_file_path_format() == FilePathFormat::Relative) {
				const QString cwd = QDir::currentPath();
				if (!cwd.isEmpty()) {
					param_value = SGVariant(file_GetRelativeFilename(cwd, this->xml_map_file_full_path));
					set = true;
				}
			}
		}
		if (!set) {
			param_value = SGVariant(this->xml_map_file_full_path);
		}
		break;
	}
	case PARAM_ALPHA:
		param_value = SGVariant((int32_t) this->alpha, mapnik_layer_param_specs[PARAM_ALPHA].type_id);
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
   Run carto command.
   ATM don't have any version issues AFAIK.
   Tested with carto 0.14.0.
*/
sg_ret LayerMapnik::carto_load(void)
{
	char *mystdout = NULL;
	char *mystderr = NULL;
	GError *error = NULL;

	const SGVariant pref_value = Preferences::get_param_value(PREFERENCES_NAMESPACE_MAPNIK "carto");
	const QString command = QString("%1 %2").arg(pref_value.val_string).arg(this->css_file_full_path);

	sg_ret result = sg_ret::ok;

	//char *args[2]; args[0] = pref_value.s; args[1] = this->css_file_full_path;
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

	struct timespec before = { 0, 0 };
	struct timespec after = { 0, 0 };

	DurationMeter dmeter;
	dmeter.start();
	const bool spawn_result = g_spawn_command_line_sync(command.toUtf8().constData(), &mystdout, &mystderr, NULL, &error);
	dmeter.stop();

	if (spawn_result) {
		if (mystderr)
			if (strlen(mystderr) > 1) {
				Dialog::error(tr("Error running carto command:\n%1").arg(QString(mystderr)), this->get_window());
				result = sg_ret::err;
			}
		if (mystdout) {
			/* NB This will overwrite the specified XML file. */
			if (! (this->xml_map_file_full_path.length() > 1)) {
				/* XML Not specified so try to create based on CSS file name. */
				GRegex *regex = g_regex_new("\\.mml$|\\.mss|\\.css$", G_REGEX_CASELESS, (GRegexMatchFlags) 0, &error);
				if (error) {
					fprintf(stderr, "CRITICAL: %s: %s\n", __FUNCTION__, error->message);
				}
				this->xml_map_file_full_path = QString(g_regex_replace_literal(regex, this->css_file_full_path.toUtf8().constData(), -1, 0, ".xml", (GRegexMatchFlags) 0, &error)) ;
				if (error) {
					fprintf(stderr, "WARNING: %s: %s\n", __FUNCTION__, error->message);
				}
				/* Prevent overwriting self. */
				if (this->xml_map_file_full_path == this->css_file_full_path) {
					this->xml_map_file_full_path = this->css_file_full_path + ".xml";
				}
			}
			if (!g_file_set_contents(this->xml_map_file_full_path.toUtf8().constData(), mystdout, -1, &error) ) {
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
		QString msg;
		if (dmeter.is_valid()) {
			msg = tr("%1 completed in %2 seconds").arg(pref_value.val_string).arg(dmeter.get_seconds_string(1));
		} else {
			msg = tr("%1 completed").arg(pref_value.val_string);
			qDebug() << SG_PREFIX_E << "Failed to get duration of 'carto' execution";
		}
		window_->statusbar_update(StatusBarField::Info, msg);
		window_->clear_busy_cursor();
	}

	return result;
}




void LayerMapnik::post_read(GisViewport * gisview, bool from_file)
{
	if (this->should_run_carto()) {
		/* Don't load the XML config if carto load fails. */
		if (sg_ret::ok != this->carto_load()) {
			return;
		}
	}

	QString msg;
	if (sg_ret::ok == this->mw.load_map_file(this->xml_map_file_full_path, this->tile_size_x, this->tile_size_x, msg)) {
		this->xml_map_file_loaded = true;
		if (!from_file) {
			/* Mapnik xml file is not a file type that the
			   program knows to handle.  If we registered
			   the xml file with
			   Window::update_recent_files(), the program
			   wouldn't know how to open an xml file (we
			   would have to add logic that checks the
			   contents of xml file, and we don't have
			   that yet). */
			update_desktop_recent_documents(this->get_window(), this->xml_map_file_full_path, g_mapnik_xml_mime_type);
		}
	} else {
		Dialog::error(tr("Mapnik error during loading configuration file:\n%1").arg(msg), this->get_window());
	}
}




static QString get_pixmap_full_path(const QString dir, int x, int y, const TileScale & scale)
{
	return QDir::toNativeSeparators(QString("%1/%2/%3/%4.png").arg(dir).arg(scale.get_tile_zoom_level()).arg(x).arg(y));
}




void LayerMapnik::possibly_save_pixmap(QPixmap & pixmap, const TileInfo & tile_info) const
{
	if (!this->use_file_cache) {
		return;
	}

	if (this->file_cache_dir.isEmpty()) {
		return;
	}

	const QString file_full_path = get_pixmap_full_path(this->file_cache_dir, tile_info.x, tile_info.y, tile_info.scale);
	if (sg_ret::ok == FileUtils::create_directory_for_file(file_full_path)) {
		qDebug() << SG_PREFIX_I << "Directory for pixmap" << file_full_path;
		if (!pixmap.save(file_full_path, "png")) {
			qDebug() << SG_PREFIX_W << "Failed to save pixmap to" << file_full_path;
		}
	} else {
		qDebug() << SG_PREFIX_E << "No directory for pixmap" << file_full_path;
	}
}




class RenderJob : public BackgroundJob {
public:
	RenderJob(LayerMapnik * layer, const TileInfo & tile_info, const QString & job_request_key);

	void run(void);

	LayerMapnik * layer = NULL;
	TileInfo tile_info;
	QString job_request_key;
};




RenderJob::RenderJob(LayerMapnik * new_layer, const TileInfo & new_tile_info, const QString & new_job_request_key)
{
	this->n_items = 1; /* Render one tile at a time (one tile per background job). */
	this->layer = new_layer;
	this->tile_info = new_tile_info;
	this->job_request_key = new_job_request_key;
}




void LayerMapnik::render_tile_now(const TileInfo & tile_info)
{
	LatLon lat_lon_ul;
	LatLon lat_lon_br;
	tile_info.get_itms_lat_lon_ul_br(lat_lon_ul, lat_lon_br);

	DurationMeter dmeter;
	dmeter.start();
	QPixmap pixmap = this->mw.render_map(lat_lon_ul.lat, lat_lon_ul.lon, lat_lon_br.lat, lat_lon_br.lon);
	dmeter.stop();

	MapCacheItemProperties properties;
	if (dmeter.is_valid()) {
		properties.rendering_duration_ns = dmeter.get_nanoseconds();
		const QString seconds_string = dmeter.get_seconds_string(SG_RENDER_TIME_PRECISION);
		qDebug() << SG_PREFIX_D << "Mapnik rendering completed in" << seconds_string << "seconds";
	} else {
		properties.rendering_duration_ns = SG_RENDER_TIME_NO_RENDER;
		qDebug() << SG_PREFIX_E << "Failed to get duration of Mapnik rendering";
	}

	if (pixmap.isNull()) {
		qDebug() << SG_PREFIX_N << "Rendered pixmap is empty";
		/* A pixmap to stick into cache in case of an unrenderable area - otherwise will get continually re-requested. */
		const QPixmap substitute = QPixmap(":/icons/layer/mapnik.png");
		pixmap = substitute.scaled(this->tile_size_x, this->tile_size_x, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
	this->possibly_save_pixmap(pixmap, tile_info);

	if (scale_alpha.is_in_range(this->alpha)) {
		ui_pixmap_scale_alpha(pixmap, this->alpha);
	}

	MapCache::add_tile_pixmap(pixmap, properties, tile_info, MapTypeID::MapnikRender, this->alpha, PixmapScale(0.0, 0.0), this->xml_map_file_full_path);
}




void RenderJob::run(void)
{
	const bool terminate_job = this->set_progress_state(0);
	if (!terminate_job) {
		/* We weren't told to terminate this background task,
		   so we can proceed to render the tile. */
		this->layer->render_tile_now(this->tile_info);
	}

	tp_mutex.lock();
	mapnik_job_requests.remove(this->job_request_key);
	tp_mutex.unlock();

	if (!terminate_job) {
		/* We weren't told to terminate this background task,
		   so we can proceed to ask for displaying of rendered
		   tile. */
		this->layer->emit_tree_item_changed("Indicating ending of rendering of Mapnik tile from background job");
	}
	return;
}




void LayerMapnik::queue_rendering_in_background(const TileInfo & tile_info, const QString & file_full_path)
{
	/* Create request. */
	const unsigned int nn = file_full_path.isEmpty() ? 0 : qHash(file_full_path, 0);
	const QString job_request_key = QString(REQUEST_HASHKEY_FORMAT).arg(tile_info.x).arg(tile_info.y).arg(tile_info.z).arg(tile_info.scale.get_scale_value()).arg(nn);

	tp_mutex.lock();

	if (mapnik_job_requests.contains(job_request_key)) {
		/* This tile is already being rendered, no need to create a duplicate. */
		qDebug() << SG_PREFIX_N << "Skipping duplicate job with request key" << job_request_key;
		tp_mutex.unlock();
		return;
	}

	RenderJob * job = new RenderJob(this, tile_info, job_request_key);
	mapnik_job_requests.insert(job_request_key, true);

	tp_mutex.unlock();

	const QString base_name = FileUtils::get_base_name(file_full_path);
	const QString job_description = tr("Mapnik Render %1:%2:%3 %4").arg(tile_info.scale.get_scale_value()).arg(tile_info.x).arg(tile_info.y).arg(base_name);
	job->set_description(job_description);
	job->run_in_background(ThreadPoolType::LocalMapnik);
}




QPixmap LayerMapnik::load_pixmap(const TileInfo & tile_info, bool & rerender) const
{
	rerender = false;

	QPixmap pixmap;
	const QString file_full_path = get_pixmap_full_path(this->file_cache_dir, tile_info.x, tile_info.y, tile_info.scale);

	struct stat stat_buf;
	if (stat(file_full_path.toUtf8().constData(), &stat_buf) == 0) {
		/* Get from disk. */
		if (!pixmap.load(file_full_path)) {
			qDebug() << "WW: Layer Mapnik: failed to load pixmap from" << file_full_path;
		} else {
			if (scale_alpha.is_in_range(this->alpha)) {
				ui_pixmap_set_alpha(pixmap, this->alpha);
			}

			MapCache::add_tile_pixmap(pixmap, MapCacheItemProperties(SG_RENDER_TIME_NO_RENDER), tile_info, MapTypeID::MapnikRender, this->alpha, PixmapScale(0.0, 0.0), this->xml_map_file_full_path);
		}
		/* If file is too old mark for rerendering. */
		if (g_planet_import_time < stat_buf.st_mtime) {
			rerender = true;
		}
	}

	return pixmap;
}




QPixmap LayerMapnik::get_pixmap(const TileInfo & tile_info)
{
	QPixmap pixmap = MapCache::get_tile_pixmap(tile_info, MapTypeID::MapnikRender, this->alpha, PixmapScale(0.0, 0.0), this->xml_map_file_full_path);
	if (!pixmap.isNull()) {
		qDebug() << SG_PREFIX_I << "MAP CACHE HIT";
		return pixmap;
	}

	qDebug() << SG_PREFIX_I << "MAP CACHE MISS";

	bool rerender = false;
	if (this->use_file_cache && !this->file_cache_dir.isEmpty()) {
		pixmap = this->load_pixmap(tile_info, rerender);
	}

	if (pixmap.isNull() || rerender) {

		/* Ask Mapnik to render a tile, and put results of the
		   rendering in some known location. */

		const bool run_in_background = true;
		if (run_in_background) {
			this->queue_rendering_in_background(tile_info, this->xml_map_file_full_path);
		} else {
			/* TODO_MAYBE: maybe we could return pixmap
			   here from render_tile_now() and pass it to
			   caller of get_pixmap(), without the need to
			   emit signal? */
			this->render_tile_now(tile_info);
			this->emit_tree_item_changed("Indicating ending of rendering of Mapnik tile from foreground job");
		}
	}

	return pixmap;
}




void LayerMapnik::draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected)
{
	if (!this->xml_map_file_loaded) {
		return;
	}

	if (gisview->get_draw_mode() != GisViewportDrawMode::Mercator) {
		this->get_window()->get_statusbar()->set_message(StatusBarField::Info, tr("Mapnik Rendering must be in Mercator mode"));
		return;
	}

	const QString copyright = this->mw.get_copyright();
	if (!copyright.isEmpty()) {
		gisview->add_attribution(copyright);
	}

	/* Split rendering of a map into a grid for the current
	   viewport thus each individual 'tile' can then be stored in
	   the map cache. */

	/* TODO_LATER: Understand if tilesize != 256 does this need to use shrinkfactors? */

	TilesRange range;
	TileInfo tile_iter; /* Will be set by get_tiles_range() to first tile in range (to upper-left tile). */
	if (sg_ret::ok != this->get_tiles_range(gisview, range, tile_iter)) {
		qDebug() << SG_PREFIX_E << "Failed to get tiles range for current viewport";
		return;
	}

	for (tile_iter.x = range.x_first; tile_iter.x <= range.x_last; tile_iter.x++) {
		for (tile_iter.y = range.y_first; tile_iter.y <= range.y_last; tile_iter.y++) {
			this->draw_tile(gisview, tile_iter);
		}
	}


	if (true || (vik_debug && vik_verbose)) {
		/* This drawing below is done after main map is
		   rendered, so it is done on top of the map. Just a
		   handy guide to tile blocks. */

		/* Reset tile iter to beginning of tiles range. */
		tile_iter.x = range.x_first;
		tile_iter.y = range.y_first;

		this->draw_grid(gisview, range, tile_iter);
	}
}




LayerMapnik::~LayerMapnik()
{
}




void LayerMapnik::flush_map_cache_cb(void)
{
	MapCache::flush_type(MapTypeID::MapnikRender);
}




void LayerMapnik::reload_map_cb(void)
{
	GisViewport * gisview = ThisApp::get_main_gis_view();

	this->post_read(gisview, false);
	this->draw_tree_item(gisview, false, false);
}




/**
   Force carto run

   Most carto projects will consist of many files.
   ATM don't have a way of detecting when any of the included files have changed.
   Thus allow a manual method to force re-running carto.
*/
void LayerMapnik::run_carto_cb(void)
{
	/* Don't load the XML config if carto load fails. */
	if (sg_ret::ok != this->carto_load()) {
		return;
	}

	QString msg;
	if (sg_ret::ok == this->mw.load_map_file(this->xml_map_file_full_path, this->tile_size_x, this->tile_size_x, msg)) {
		this->draw_tree_item(ThisApp::get_main_gis_view(), false, false);
	} else {
		Dialog::error(tr("Mapnik error loading configuration file:\n%1").arg(msg), this->get_window());
	}
}




/**
   Show Mapnik configuration parameters
*/
void LayerMapnik::mapnik_layer_information_cb(void)
{
	const QStringList params = this->mw.get_parameters();
	Dialog::info(tr("Mapnik Layer Information"), params, this->get_window());
}




void LayerMapnik::about_mapnik_cb(void)
{
	Dialog::info(tr("About Mapnik"), MapnikWrapper::about_mapnik(), this->get_window());
}




void LayerMapnik::add_menu_items(QMenu & menu)
{
	QAction * action = NULL;

	/* Typical users shouldn't need to use this functionality - so debug only ATM. */
	if (vik_debug) {
		action = new QAction(tr("&Flush Map Cache"), this);
		action->setIcon(QIcon::fromTheme("edit-clear"));
		QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (flush_map_cache_cb()));
		menu.addAction(action);
	}

	action = new QAction(tr("Re&fresh Map"), this);
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (reload_map_cb()));
	menu.addAction(action);


	if (!this->css_file_full_path.isEmpty()) {
		action = new QAction(tr("&Run Carto Command"), this);
		QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (run_carto_cb()));
		menu.addAction(action);
	}

	action = new QAction(tr("&Information about this layer"), this);
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (mapnik_layer_information_cb()));
	menu.addAction(action);


	action = new QAction(tr("&About Mapnik"), this);
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (about_mapnik_cb()));
	menu.addAction(action);
}




void LayerMapnik::rerender_tile_cb(void)
{
	TileInfo tile_info;
	if (sg_ret::ok != MapUtils::lat_lon_to_iTMS(this->clicked_lat_lon, this->clicked_viking_scale, tile_info)) {
		qDebug() << SG_PREFIX_E << "Failed to convert clicked coordinate to tile info";
		return;
	}

	this->queue_rendering_in_background(tile_info, this->xml_map_file_full_path);
}




void LayerMapnik::tile_info_cb(void)
{
	TileInfo tile_info;
	/* Requested position to map coord. */
	if (sg_ret::ok != MapUtils::lat_lon_to_iTMS(this->clicked_lat_lon, this->clicked_viking_scale, tile_info)) {
		qDebug() << SG_PREFIX_E << "Failed to convert clicked coordinate to tile info";
		return;
	}

	MapCacheItemProperties properties = MapCache::get_properties(tile_info, MapTypeID::MapnikRender, this->alpha, PixmapScale(0.0, 0.0), this->xml_map_file_full_path);

	const QString file_full_path = get_pixmap_full_path(this->file_cache_dir, tile_info.x, tile_info.y, tile_info.scale);

	QStringList tile_info_strings;
	tile_info_add_file_info_strings(tile_info_strings, file_full_path);

	/* Show the info. */
	if (properties.rendering_duration_ns != SG_RENDER_TIME_NO_RENDER) {
		const double seconds = (1.0 * properties.rendering_duration_ns) / NANOSECS_PER_SEC;
		const QString render_message = tr("Rendering time: %1 seconds").arg(seconds, 0, 'f', SG_RENDER_TIME_PRECISION);
		tile_info_strings.push_back(render_message);
	} else {
		/* File was read from disc, and render duration is
		   lost, and is not available now. */
	}

	Dialog::info(tr("Tile Information"), tile_info_strings, this->get_window());
}




LayerToolMapnikFeature::LayerToolMapnikFeature(Window * window_, GisViewport * gisview_) : LayerTool(window_, gisview_, LayerKind::Mapnik)
{
	this->action_icon_path   = ":/icons/layer_tool/mapnik_feature.png";
	this->action_label       = QObject::tr("&Mapnik Features");
	this->action_tooltip     = QObject::tr("Mapnik Features");
	// this->action_accelerator = ...; /* Empty accelerator. */
}




SGObjectTypeID LayerToolMapnikFeature::get_tool_id(void) const
{
	return LayerToolMapnikFeature::tool_id();
}
SGObjectTypeID LayerToolMapnikFeature::tool_id(void)
{
	return SGObjectTypeID("sg.tool.layer_mapnik.feature");
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
		const Coord coord = tool->gisview->screen_pos_to_coord(std::max(0, ev->x()), std::max(0, ev->y()));
		this->clicked_lat_lon = coord.get_lat_lon();

		this->clicked_viking_scale = tool->gisview->get_viking_scale();

		if (!this->right_click_menu) {
			QAction * action = NULL;
			this->right_click_menu = new QMenu();

			action = new QAction(tr("&Rerender Tile"), this);
			action->setIcon(QIcon::fromTheme("view-refresh"));
			QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (rerender_tile_cb()));
			this->right_click_menu->addAction(action);

			action = new QAction(tr("Tile &Info"), this);
			action->setIcon(QIcon::fromTheme("dialog-information"));
			QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (tile_info_cb()));
			this->right_click_menu->addAction(action);
		}

		this->right_click_menu->exec(QCursor::pos());

		return ToolStatus::Ack;
	}

	return ToolStatus::Ignored;
}




LayerMapnik::LayerMapnik()
{
	this->m_kind = LayerKind::Mapnik;
	strcpy(this->debug_string, "Mapnik");
	this->interface = &vik_mapnik_layer_interface;

	this->set_initial_parameter_values();
	this->set_name(Layer::get_translated_layer_kind_string(this->m_kind));

	this->tile_size_x = size_default().u.val_int;
}




bool LayerMapnik::should_run_carto(void) const
{
	if (this->css_file_full_path.isEmpty()) {
		return false;
	}

	if (this->xml_map_file_full_path.isEmpty()) {
		/* No XML specified thus need to generate. */
		return true;
	}

	/* Compare timestamps. */
	bool result = false;
	struct stat stat_buf1;
	if (stat(this->xml_map_file_full_path.toUtf8().constData(), &stat_buf1) == 0) {
		struct stat stat_buf2;
		if (stat(this->css_file_full_path.toUtf8().constData(), &stat_buf2) == 0) {
			/* Is CSS file newer than the XML file. */
			if (stat_buf2.st_mtime > stat_buf1.st_mtime) {
				result = true;
			} else {
				fprintf(stderr, "DEBUG: No need to run carto\n");
			}
		}
	} else {
		/* XML file doesn't exist. */
		result = true;
	}

	return result;
}




sg_ret LayerMapnik::draw_tile(GisViewport * gisview, const TileInfo & tile_info)
{
	const QPixmap pixmap = this->get_pixmap(tile_info);
	if (!pixmap.isNull()) {
		/* Lat/lon coordinate of u-l corner of a pixmap. */
		const LatLon pixmap_lat_lon_ul = MapUtils::iTMS_to_lat_lon(tile_info);

		/* x/y coordinate of u-l corner of a pixmap in viewport's x/y coordinates. */
		fpixel viewport_x;
		fpixel viewport_y;
		gisview->coord_to_screen_pos(Coord(pixmap_lat_lon_ul, CoordMode::LatLon), &viewport_x, &viewport_y);

		const fpixel pixmap_x = 0;
		const fpixel pixmap_y = 0;
		gisview->draw_pixmap(pixmap, viewport_x, viewport_y, pixmap_x, pixmap_y, this->tile_size_x, this->tile_size_x);
	}

	return sg_ret::ok;
}




sg_ret LayerMapnik::get_tiles_range(const GisViewport * gisview, TilesRange & range, TileInfo & tile_info_ul)
{
	const Coord coord_ul = gisview->screen_pos_to_coord(ScreenPosition::UpperLeft);
	const Coord coord_br = gisview->screen_pos_to_coord(ScreenPosition::BottomRight);
	const LatLon lat_lon_ul = coord_ul.get_lat_lon();
	const LatLon lat_lon_br = coord_br.get_lat_lon();

	const VikingScale viking_scale = gisview->get_viking_scale();

	if (sg_ret::ok != MapUtils::lat_lon_to_iTMS(lat_lon_ul, viking_scale, tile_info_ul)) {
		qDebug() << SG_PREFIX_E << "Failed to convert ul";
		return sg_ret::err;
	}
	TileInfo tile_info_br;
	if (sg_ret::ok != MapUtils::lat_lon_to_iTMS(lat_lon_br, viking_scale, tile_info_br)) {
		qDebug() << SG_PREFIX_E << "Failed to convert br";
		return sg_ret::err;
	}

	range = TileInfo::get_tiles_range(tile_info_ul, tile_info_br);

	return sg_ret::ok;
}




void LayerMapnik::draw_grid(GisViewport * gisview, const TilesRange & range, const TileInfo & tile_info_ul) const
{
	fpixel viewport_x;
	fpixel viewport_y;
	const LatLon lat_lon = MapUtils::iTMS_to_center_lat_lon(tile_info_ul);
	gisview->coord_to_screen_pos(Coord(lat_lon, CoordMode::LatLon), &viewport_x, &viewport_y);

	const int delta_x = 1;
	const int delta_y = 1;
	const int tile_width = this->tile_size_x;
	const int tile_height = this->tile_size_x; /* Using LayerMapnik::tile_size_x because we don't have LayerMapnik::tile_size_y. */

	const QPen pen(QColor(LAYER_MAPNIK_GRID_COLOR));
	LayerMap::draw_grid(gisview, pen, viewport_x, viewport_y, range.x_first, delta_x, range.x_last + 1, range.y_first, delta_y, range.y_last + 1, tile_width, tile_height);

	return;
}




sg_ret LayerMapnik::set_tile_size(int size)
{
	if (size > 0) {
		this->tile_size_x = size;
		return sg_ret::ok;
	} else {
		qDebug() << SG_PREFIX_E << "Invalid value of tile size:" << size;
		return sg_ret::err;
	}
}




void DurationMeter::start(void)
{

	this->before_rv = clock_gettime(CLOCK_MONOTONIC, &this->before);
}




void DurationMeter::stop(void)
{
	this->after_rv = clock_gettime(CLOCK_MONOTONIC, &this->after);
}




double DurationMeter::get_seconds(void) const
{
	long ns = this->get_nanoseconds();
	return (1.0 * ns) / NANOSECS_PER_SEC;
}




long DurationMeter::get_nanoseconds(void) const
{
	const long ns = (this->after.tv_nsec - this->before.tv_nsec) + ((this->after.tv_sec - this->before.tv_sec) * NANOSECS_PER_SEC);
	return ns;
}




QString DurationMeter::get_seconds_string(int precision) const
{
	const double seconds = this->get_seconds();
	return QString("%1").arg(seconds, 0, 'f', precision);
}




bool DurationMeter::is_valid(void) const
{
	if (this->before_rv == 0 && this->after_rv == 0) {
		return true;
	} else {
		qDebug() << SG_PREFIX_E << "One or both calls to clock_gettime() has failed:" << this->before_rv << this->after_rv;
		return false;
	}
}
