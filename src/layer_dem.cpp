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
 */




#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cassert>

#include <sys/types.h>
#include <sys/stat.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QMenu>
#include <QDebug>
#include <QHash>
#include <QDir>




#include "window.h"
#include "layer_map.h"
#include "layer_dem.h"
#include "dem.h"
#include "dem_cache.h"
#include "map_cache.h"
#include "file.h"
#include "dialog.h"
#include "download.h"
#include "preferences.h"
#include "util.h"
#include "vikutils.h"
#include "statusbar.h"
#include "viewport_internal.h"




using namespace SlavGPS;




#define SG_MODULE "Layer DEM"




void draw_loaded_dem_box(Viewport * viewport);




#define SRTM_HTTP_SITE "dds.cr.usgs.gov"
#define SRTM_HTTP_URI_PREFIX  "/srtm/version2_1/SRTM3/"

#ifdef VIK_CONFIG_DEM24K
#define DEM24K_DOWNLOAD_SCRIPT "dem24k.pl"
#endif




/* Internal units - Metres. */
static SGVariant scale_min_elev_initial_iu(Altitude(0.0, HeightUnit::Metres));
static SGVariant scale_max_elev_initial_iu(Altitude(1000.0, HeightUnit::Metres));

/* Upper limit is that high in case if units are feet. */
static ParameterScale<double> scale_min_elev_iu(0.0, 30000.0, scale_min_elev_initial_iu, 10, 1);
static ParameterScale<double> scale_max_elev_iu(1.0, 30000.0, scale_max_elev_initial_iu, 10, 1);




static WidgetEnumerationData dem_source_enum = {
	{
		SGLabelID(QObject::tr("SRTM Global 90m (3 arcsec)"), DEM_SOURCE_SRTM),
#ifdef VIK_CONFIG_DEM24K
		SGLabelID(QObject::tr("USA 10m (USGS 24k)"),         DEM_SOURCE_DEM24K),
#endif
	},
	DEM_SOURCE_SRTM
};
static SGVariant dem_source_default(void) { return SGVariant(dem_source_enum.default_value, SGVariantType::Enumeration); }




static WidgetEnumerationData dem_type_enum = {
	{
		SGLabelID(QObject::tr("Absolute height"), DEM_TYPE_HEIGHT),
		SGLabelID(QObject::tr("Height gradient"), DEM_TYPE_GRADIENT),
	},
	DEM_TYPE_HEIGHT
};
static SGVariant dem_type_default(void) { return SGVariant(dem_type_enum.default_value, SGVariantType::Enumeration); }




static SGVariant color_default(void)
{
	return SGVariant(0, 0, 255, 255);
}




enum {
	PARAM_FILES = 0,
	PARAM_SOURCE,
	PARAM_COLOR,
	PARAM_TYPE,
	PARAM_MIN_ELEV,
	PARAM_MAX_ELEV,
	NUM_PARAMS
};




static ParameterSpecification dem_layer_param_specs[] = {
	{ PARAM_FILES,      "files",    SGVariantType::StringList,  PARAMETER_GROUP_GENERIC, QObject::tr("DEM Files:"),       WidgetType::FileList,        NULL,                NULL,                "" },
	{ PARAM_SOURCE,     "source",   SGVariantType::Enumeration, PARAMETER_GROUP_GENERIC, QObject::tr("Download Source:"), WidgetType::Enumeration,     &dem_source_enum,    dem_source_default,  QObject::tr("Source of DEM data") },
	{ PARAM_COLOR,      "color",    SGVariantType::Color,       PARAMETER_GROUP_GENERIC, QObject::tr("Min Elev Color:"),  WidgetType::Color,           NULL,                color_default,       "" },
	{ PARAM_TYPE,       "type",     SGVariantType::Enumeration, PARAMETER_GROUP_GENERIC, QObject::tr("Type:"),            WidgetType::Enumeration,     &dem_type_enum,      dem_type_default,    "" },
	{ PARAM_MIN_ELEV,   "min_elev", SGVariantType::Altitude,    PARAMETER_GROUP_GENERIC, QObject::tr("Min Elev:"),        WidgetType::Altitude,        &scale_min_elev_iu,  NULL,                "" },
	{ PARAM_MAX_ELEV,   "max_elev", SGVariantType::Altitude,    PARAMETER_GROUP_GENERIC, QObject::tr("Max Elev:"),        WidgetType::Altitude,        &scale_max_elev_iu,  NULL,                "" },
	{ NUM_PARAMS,       "",         SGVariantType::Empty,       PARAMETER_GROUP_GENERIC, "",                              WidgetType::None,            NULL,                NULL,                "" }, /* Guard. */
};




static bool dem_layer_download_release(Layer * vdl, QMouseEvent * ev, LayerTool * tool);
static bool dem_layer_download_click(Layer * vdl, QMouseEvent * ev, LayerTool * tool);
static void srtm_draw_existence(Viewport * viewport);
#ifdef VIK_CONFIG_DEM24K
static void dem24k_draw_existence(Viewport * viewport);
#endif
static void draw_existence_common(Viewport * viewport, const QPen & pen, const Coord & coord_ne, const Coord & coord_sw, const QString & cache_file_path);



/* Height colors.
   The first entry is blue for a default 'sea' color,
   however the value used by the corresponding gc can be configured as part of the DEM layer properties.
   The other colors, shaded from brown to white are used to give an indication of height.
*/
static char const * dem_height_colors[] = {
	"#0000FF",
	"#9b793c", "#9c7d40", "#9d8144", "#9e8549", "#9f894d", "#a08d51", "#a29156", "#a3955a", "#a4995e", "#a69d63",
	"#a89f65", "#aaa267", "#ada569", "#afa76b", "#b1aa6d", "#b4ad6f", "#b6b071", "#b9b373", "#bcb676", "#beb978",
	"#c0bc7a", "#c2c07d", "#c4c37f", "#c6c681", "#c8ca84", "#cacd86", "#ccd188", "#cfd58b", "#c2ce84", "#b5c87e",
	"#a9c278", "#9cbb71", "#8fb56b", "#83af65", "#76a95e", "#6aa358", "#5e9d52", "#63a055", "#69a458", "#6fa85c",
	"#74ac5f", "#7ab063", "#80b467", "#86b86a", "#8cbc6e", "#92c072", "#94c175", "#97c278", "#9ac47c", "#9cc57f",
	"#9fc682", "#a2c886", "#a4c989", "#a7cb8d", "#aacd91", "#afce99", "#b5d0a1", "#bbd2aa", "#c0d3b2", "#c6d5ba",
	"#ccd7c3", "#d1d9cb", "#d7dbd4", "#DDDDDD", "#e0e0e0", "#e4e4e4", "#e8e8e8", "#ebebeb", "#efefef", "#f3f3f3",
	"#f7f7f7", "#fbfbfb", "#ffffff"
};

static const int32_t DEM_N_HEIGHT_COLORS = sizeof(dem_height_colors)/sizeof(dem_height_colors[0]);




/* Gradient colors. */
static char const * dem_gradient_colors[] = {
	"#AAAAAA",
	"#000000", "#000011", "#000022", "#000033", "#000044", "#00004c", "#000055", "#00005d", "#000066", "#00006e",
	"#000077", "#00007f", "#000088", "#000090", "#000099", "#0000a1", "#0000aa", "#0000b2", "#0000bb", "#0000c3",
	"#0000cc", "#0000d4", "#0000dd", "#0000e5", "#0000ee", "#0000f6", "#0000ff", "#0008f7", "#0011ee", "#0019e6",
	"#0022dd", "#002ad5", "#0033cc", "#003bc4", "#0044bb", "#004cb3", "#0055aa", "#005da2", "#006699", "#006e91",
	"#007788", "#007f80", "#008877", "#00906f", "#009966", "#00a15e", "#00aa55", "#00b24d", "#00bb44", "#00c33c",
	"#00cc33", "#00d42b", "#00dd22", "#00e51a", "#00ee11", "#00f609", "#00ff00", "#08f700", "#11ee00", "#19e600",
	"#22dd00", "#2ad500", "#33cc00", "#3bc400", "#44bb00", "#4cb300", "#55aa00", "#5da200", "#669900", "#6e9100",
	"#778800", "#7f8000", "#887700", "#906f00", "#996600", "#a15e00", "#aa5500", "#b24d00", "#bb4400", "#c33c00",
	"#cc3300", "#d42b00", "#dd2200", "#e51a00", "#ee1100", "#f60900", "#ff0000",
	"#FFFFFF"
};

static const int32_t DEM_N_GRADIENT_COLORS = sizeof(dem_gradient_colors)/sizeof(dem_gradient_colors[0]);




LayerDEMInterface vik_dem_layer_interface;




LayerDEMInterface::LayerDEMInterface()
{
	this->parameters_c = dem_layer_param_specs;

	this->fixed_layer_type_string = "DEM"; /* Non-translatable. */

	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_D;
	// this->action_icon = ...; /* Set elsewhere. */

	this->ui_labels.new_layer = QObject::tr("New DEM Layer");
	this->ui_labels.layer_type = QObject::tr("DEM");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of DEM Layer");
}




LayerToolContainer * LayerDEMInterface::create_tools(Window * window, Viewport * viewport)
{
	/* This method should be called only once. */
	static bool created = false;
	if (created) {
		return NULL;
	}
	auto tools = new LayerToolContainer;

	LayerTool * tool = new LayerToolDEMDownload(window, viewport);
	tools->insert({{ tool->id_string, tool }});

	created = true;

	return tools;
}




QString LayerDEM::get_tooltip(void) const
{
	return tr("Number of files: %1").arg(this->files.size());
}






Layer * LayerDEMInterface::unmarshall(Pickle & pickle, Viewport * viewport)
{
	LayerDEM * layer = new LayerDEM();

	/* Overwrite base color configured in constructor. */
	layer->colors[0] = QColor(layer->base_color);

	layer->unmarshall_params(pickle);
	return layer;
}




DEMLoadJob::DEMLoadJob(const QStringList & new_file_paths)
{
	this->file_paths = new_file_paths;
	this->n_items = this->file_paths.size(); /* Number of DEM files. */
}




/**
   Function for starting the DEM file loading as a background thread
*/
void DEMLoadJob::run(void)
{
	const bool load_status = this->load_files_into_cache();
	if (!load_status) {
		; /* Thread cancelled. */
	}

	/* Signal completion of work only after all files from the
	   list have been loaded (or abortion of task has been
	   detected). Don't signal the layer on each file. */
	emit this->loading_to_cache_completed();

	return;
}




/**
   \brief Load a group of DEM tiles into program's cache

   \return true when function processed all files (successfully or unsuccessfully)
   \return false if function was interrupted after detecting request for end of processing
*/
bool DEMLoadJob::load_files_into_cache(void)
{
	size_t dem_count = 0;
	const size_t dem_total = this->file_paths.size();

	for (auto iter = this->file_paths.begin(); iter != this->file_paths.end(); iter++) {
		if (!DEMCache::load_file_into_cache(*iter)) {
			qDebug() << SG_PREFIX_E << "Failed to load into cache file" << *iter;
		}

		dem_count++;
		/* Progress also detects abort request via the returned value. */
		const bool end_job = this->set_progress_state(((double) dem_count) / dem_total);
		if (end_job) {
			return false; /* Abort thread. */
		}
	}
	return true;
}




void DEMLoadJob::cleanup_on_cancel(void)
{
	/* Abort loading.
	   Instead of freeing the list, leave it as partially processed.
	   Thus we can see/use what was done. */
}




/**
 * Process the list of DEM files and convert each one to a relative path.
 */
static QStringList dem_layer_convert_to_relative_filenaming(const QStringList & input_files)
{
	QStringList result;

	const QString cwd = QDir::currentPath();
	if (cwd.isEmpty()) {
		result = input_files;
		return result;
	}

	for (auto iter = input_files.begin(); iter != input_files.end(); iter++) {
		const QString file = file_GetRelativeFilename(cwd, *iter);
		result.push_front(file);
	}

	if (result.empty()) {
		/* TODO_LATER: is this correct? Should we return original files? Is this condition ever true? */
		result = input_files;
	}

	return result;
}




bool LayerDEM::set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation)
{
	switch (param_id) {
	case PARAM_COLOR:
		this->base_color = param_value.val_color;
		this->colors[0] = this->base_color;
		break;

	case PARAM_SOURCE:
		this->source = param_value.u.val_int;
		break;

	case PARAM_TYPE:
		this->dem_type = param_value.u.val_int;
		break;

	case PARAM_MIN_ELEV:
		if (is_file_operation) {
			/* Value stored in .vik file is always in
			   meters, and class member always stores
			   value in meters, so this branch is simple.

			   At this point I think that code reading
			   value from file doesn't know that it's an
			   altitude, and has sent us a simple double.
			   Therefore use param_value.u.val_double.
			*/
			this->min_elev = Altitude(param_value.u.val_double, HeightUnit::Metres);
		} else {
			/* This value should have been converted to
			   internal units right after getting it from
			   user interface. So it should be in meters
			   here. */
			this->min_elev = param_value.get_altitude();
		}
		qDebug() << SG_PREFIX_I << "Saved min elev to layer:" << this->min_elev << ", file operation =" << is_file_operation;
		break;

	case PARAM_MAX_ELEV:
		if (is_file_operation) {
			/* Value stored in .vik file is always in
			   meters, and class member always stores
			   value in meters, so this branch is simple.

			   At this point I think that code reading
			   value from file doesn't know that it's an
			   altitude, and has sent us a simple double.
			   Therefore use param_value.u.val_double. */
			this->max_elev = Altitude(param_value.u.val_double, HeightUnit::Metres);
		} else {
			/* Convert from value that was presented in
			   user interface with user units into value
			   in internal units (meters) */
			this->max_elev = param_value.get_altitude().convert_to_unit(HeightUnit::Metres);
		}
		qDebug() << SG_PREFIX_I << "Saved max elev to layer:" << this->max_elev << ", file operation =" << is_file_operation;
		break;

	case PARAM_FILES: {
		/* Clear out old settings - if any commonalities with new settings they will have to be read again. */
		DEMCache::unload_from_cache(this->files);

		/* Set file list so any other intermediate screen drawing updates will show currently loaded DEMs by the working thread. */
		this->files = param_value.val_string_list;

		qDebug() << SG_PREFIX_D << "List of files:";
		if (!this->files.empty()) {
			for (auto iter = this->files.begin(); iter != this->files.end(); ++iter) {
				qDebug() << SG_PREFIX_D << "File:" << *iter;
			}
		} else {
			qDebug() << SG_PREFIX_D << "No files";
		}
		/* No need for thread if no files. */
		if (!this->files.empty()) {
			/* Thread Load. */
			DEMLoadJob * load_job = new DEMLoadJob(this->files);
			load_job->set_description(QObject::tr("DEM Loading"));
			QObject::connect(load_job, SIGNAL (loading_to_cache_completed(void)), this, SLOT (on_loading_to_cache_completed_cb(void)));

			load_job->run_in_background(ThreadPoolType::Local);
		}

		break;
	}

	default:
		break;
	}
	return true;
}




SGVariant LayerDEM::get_param_value(param_id_t param_id, bool is_file_operation) const
{
	SGVariant rv;

	switch (param_id) {

	case PARAM_FILES:
		qDebug() << SG_PREFIX_I << "string list (" << this->files.size() << " elements):";
		qDebug() << this->files;

		/* Save in relative format if necessary. */
		if (is_file_operation && Preferences::get_file_path_format() == FilePathFormat::Relative) {
			rv = SGVariant(dem_layer_convert_to_relative_filenaming(this->files));
		} else {
			rv = SGVariant(this->files);
		}

		break;

	case PARAM_SOURCE:
		rv = SGVariant((int32_t) this->source);
		break;

	case PARAM_TYPE:
		rv = SGVariant((int32_t) this->dem_type);
		break;

	case PARAM_COLOR:
		rv = SGVariant(this->base_color);
		break;

	case PARAM_MIN_ELEV:
		if (is_file_operation) {
			/* Value stored in .vik file is always in
			   meters, and class member always stores
			   value in meters, so this branch is simple.

			   Code saving value in file only knows how to
			   save a double, so pass a variant/double to
			   the code. */
			rv = SGVariant(this->min_elev.get_value(), SGVariantType::Double);
		} else {
			/* Keep in internal units until the last moment before presenting to user. */
			rv = this->min_elev;
		}
		qDebug() << SG_PREFIX_I << "Read min elev from layer:" << this->min_elev;
		break;

	case PARAM_MAX_ELEV:
		if (is_file_operation) {
			/* Value stored in .vik file is always in
			   meters, and class member always stores
			   value in meters, so this branch is simple.

			   Code saving value in file only knows how to
			   save a double, so pass a variant/double to
			   the code. */
			rv = SGVariant(this->max_elev.get_value(), SGVariantType::Double);
		} else {
			/* Build value for presentation in user
			   interface - convert from internal unit
			   (meters) into current user's unit. */
			rv = this->max_elev.convert_to_unit(Preferences::get_unit_height());
		}
		qDebug() << SG_PREFIX_I << "Read max elev from layer:" << this->max_elev;
		break;

	default:
		break;
	}
	return rv;
}




static inline uint16_t get_height_difference(int16_t elev, int16_t new_elev)
{
	if (new_elev == DEM_INVALID_ELEVATION) {
		return 0;
	} else {
		return abs(new_elev - elev);
	}
}




void LayerDEM::draw_dem(Viewport * viewport, DEM * dem)
{
	/* If given DEM is loaded into application, we want to know whether the DEM and
	   current viewport overlap, so that we know whether we should draw it in
	   viewport or not. We do this check every time a viewport has been changed
	   (moved or re-zoomed). */
	const LatLonBBox viewport_bbox = viewport->get_bbox();
	if (!dem->intersect(viewport_bbox)) {
		qDebug() << SG_PREFIX_I << "DEM does not overlap with viewport, not drawing the DEM";
		return;
	}

	/*
	  Boxes to show where we have DEM instead of actually drawing
	  the DEM.  useful if we want to see what areas we have
	  coverage for (if we want to get elevation data for a track)
	  but don't want to cover the map.
	 */

	/*
	  Draw a box if a DEM is loaded. in future I'd like to add an
	  option for this this is useful if we want to see what areas
	  we have dem for but don't want to cover the map (or maybe
	  we just need translucent DEM?).
	*/
	draw_loaded_dem_box(viewport);

	switch (dem->horiz_units) {
	case VIK_DEM_HORIZ_LL_ARCSECONDS:
		this->draw_dem_ll(viewport, dem);
		break;
	case VIK_DEM_HORIZ_UTM_METERS:
		this->draw_dem_utm(viewport, dem);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected DEM horiz units" << (int) dem->horiz_units;
		break;
	}

	return;
}




/* Get index to array of colors or gradients for given value 'm_value' of elevation or gradient. */
#define GET_INDEX(m_value, m_min_elev, m_max_elev, m_palette_size) \
	(1 + ((int) floor(((m_value - m_min_elev)/(m_max_elev - m_min_elev)) * (m_palette_size - 2))))





void LayerDEM::draw_dem_ll(Viewport * viewport, DEM * dem)
{
	unsigned int skip_factor = ceil(viewport->get_viking_scale().get_x() / 80); /* TODO_2_LATER: smarter calculation. */

	double nscale_deg = dem->north_scale / ((double) 3600);
	double escale_deg = dem->east_scale / ((double) 3600);

	const LatLonBBox viewport_bbox = viewport->get_bbox();
	double start_lat_as = std::max(viewport_bbox.south.get_value() * 3600.0, dem->min_north_seconds);
	double end_lat_as   = std::min(viewport_bbox.north.get_value() * 3600.0, dem->max_north_seconds);
	double start_lon_as = std::max(viewport_bbox.west.get_value() * 3600.0, dem->min_east_seconds);
	double end_lon_as   = std::min(viewport_bbox.east.get_value() * 3600.0, dem->max_east_seconds);

	double start_lat = floor(start_lat_as / dem->north_scale) * nscale_deg;
	double end_lat   = ceil(end_lat_as / dem->north_scale) * nscale_deg;
	double start_lon = floor(start_lon_as / dem->east_scale) * escale_deg;
	double end_lon   = ceil(end_lon_as / dem->east_scale) * escale_deg;

	int32_t start_x;
        int32_t start_y;
	dem->east_north_to_xy(start_lon_as, start_lat_as, &start_x, &start_y);
	int32_t gradient_skip_factor = 1;
	if (this->dem_type == DEM_TYPE_GRADIENT) {
		gradient_skip_factor = skip_factor;
	}

	/* Verify sane elevation range. */
	if (this->max_elev <= this->min_elev) {
		this->max_elev = this->min_elev + 1;
	}
	const double min_elevation = this->min_elev.get_value();
	const double max_elevation = this->max_elev.get_value();

	Coord tmp; /* TODO_2_LATER: don't use Coord(ll, mode), especially if in latlon drawing mode. */
	const CoordMode viewport_coord_mode = viewport->get_coord_mode();
	LatLon counter;
	int32_t x;
	for (x = start_x, counter.lon = start_lon; counter.lon <= end_lon+escale_deg*skip_factor; counter.lon += escale_deg * skip_factor, x += skip_factor) {
		/* NOTE: (counter.lon <= end_lon + ESCALE_DEG*SKIP_FACTOR) is neccessary so in high zoom modes,
		   the leftmost column does also get drawn, if the center point is out of viewport. */
		if (x >= dem->n_columns) {
			break;
		}

		/* Get previous and next column. Catch out-of-bound. */
		DEMColumn *column, *prevcolumn, *nextcolumn;
		{
			column = dem->columns[x];

			int32_t new_x = x - gradient_skip_factor;
			if (new_x < 1) {
				new_x = x + 1;
			}
			prevcolumn = dem->columns[new_x];

			new_x = x + gradient_skip_factor;
			if (new_x >= dem->n_columns) {
				new_x = x - 1;
			}
			nextcolumn = dem->columns[new_x];
		}

		int32_t y;
		for (y = start_y, counter.lat = start_lat; counter.lat <= end_lat; counter.lat += nscale_deg * skip_factor, y += skip_factor) {
			if (y > column->n_points) {
				break;
			}

			int16_t elev = column->points[y];
			if (elev == DEM_INVALID_ELEVATION) {
				continue; /* Don't draw it. */
			}

			/* Calculate bounding box for drawing. */
			int box_x, box_y, box_width, box_height;
			LatLon box_c;
			box_c = counter;
			box_c.lat += (nscale_deg * skip_factor)/2;
			box_c.lon -= (escale_deg * skip_factor)/2;
			tmp = Coord(box_c, viewport_coord_mode);
			if (sg_ret::ok != viewport->coord_to_screen_pos(tmp, &box_x, &box_y)) {
				continue;
			}
			/* Catch box at borders. */
			if (box_x < 0) {
				box_x = 0;
			}

			if (box_y < 0) {
				box_y = 0;
			}

			box_c.lat -= nscale_deg * skip_factor;
			box_c.lon += escale_deg * skip_factor;
			tmp = Coord(box_c, viewport_coord_mode);
			if (sg_ret::ok != viewport->coord_to_screen_pos(tmp, &box_width, &box_height)) {
				continue;
			}
			box_width -= box_x;
			box_height -= box_y;
			/* Catch box at borders. */
			if (box_width < 0 || box_height < 0) {
				/* Skip this as is out of the viewport (e.g. zoomed in so this point is way off screen). */
				continue;
			}

			bool below_minimum = false;
			if (this->dem_type == DEM_TYPE_HEIGHT) {
				if (elev < min_elevation) {
					/* Prevent 'elev - this->min_elev' from being negative so can safely use as array index. */
					elev = ceil(min_elevation);
					below_minimum = true;
				}
				if (elev > max_elevation) {
					elev = max_elevation;
				}
			}

			if (this->dem_type == DEM_TYPE_GRADIENT) {
				/* Calculate and sum gradient in all directions. */
				int16_t change = 0;

				/* Calculate gradient from height points all around the current one. */
				int32_t new_y;
				if (y < gradient_skip_factor) {
					new_y = y;
				} else {
					new_y = y - gradient_skip_factor;
				}
				change += get_height_difference(elev, prevcolumn->points[new_y]);
				change += get_height_difference(elev, column->points[new_y]);
				change += get_height_difference(elev, nextcolumn->points[new_y]);

				change += get_height_difference(elev, prevcolumn->points[y]);
				change += get_height_difference(elev, nextcolumn->points[y]);

				new_y = y + gradient_skip_factor;
				if (new_y >= column->n_points) {
					new_y = y;
				}
				change += get_height_difference(elev, prevcolumn->points[new_y]);
				change += get_height_difference(elev, column->points[new_y]);
				change += get_height_difference(elev, nextcolumn->points[new_y]);

				change = change / ((skip_factor > 1) ? log(skip_factor) : 0.55); /* FIXME: better calc. */

				if (change < min_elevation) {
					/* Prevent 'change - this->min_elev' from being negative so can safely use as array index. */
					change = ceil(min_elevation);
				}

				if (change > max_elevation) {
					change = max_elevation;
				}

				int idx = GET_INDEX(change, min_elevation, max_elevation, DEM_N_GRADIENT_COLORS);
				viewport->fill_rectangle(this->gradients[idx], box_x, box_y, box_width, box_height);

			} else if (this->dem_type == DEM_TYPE_HEIGHT) {
				int idx = 0; /* Default index for color of 'sea' or for places below the defined mininum. */
				if (elev > 0 && !below_minimum) {
					idx = GET_INDEX(elev, min_elevation, max_elevation, DEM_N_HEIGHT_COLORS);
				}
				viewport->fill_rectangle(this->colors[idx], box_x, box_y, box_width, box_height);
			} else {
				; /* No other dem type to process. */
			}
		} /* for y= */
	} /* for x= */

	return;
}




void LayerDEM::draw_dem_utm(Viewport * viewport, DEM * dem)
{
	unsigned int skip_factor = ceil(viewport->get_viking_scale().get_x() / 10); /* TODO_2_LATER: smarter calculation. */

	Coord tleft =  viewport->screen_pos_to_coord(0,                     0);
	Coord tright = viewport->screen_pos_to_coord(viewport->get_width(), 0);
	Coord bleft =  viewport->screen_pos_to_coord(0,                     viewport->get_height());
	Coord bright = viewport->screen_pos_to_coord(viewport->get_width(), viewport->get_height());

	tleft.recalculate_to_mode(CoordMode::UTM);
	tright.recalculate_to_mode(CoordMode::UTM);
	bleft.recalculate_to_mode(CoordMode::UTM);
	bright.recalculate_to_mode(CoordMode::UTM);

	double max_nor = std::max(tleft.utm.get_northing(), tright.utm.get_northing());
	double min_nor = std::min(bleft.utm.get_northing(), bright.utm.get_northing());
	double max_eas = std::max(bright.utm.get_easting(), tright.utm.get_easting());
	double min_eas = std::min(bleft.utm.get_easting(), tleft.utm.get_easting());

	double start_eas, end_eas;
	double start_nor = std::max(min_nor, dem->min_north_seconds);
	double end_nor   = std::min(max_nor, dem->max_north_seconds);
	if (UTM::is_the_same_zone(tleft.utm, dem->utm) && UTM::is_the_same_zone(bleft.utm, dem->utm)
	    && UTM::is_northern_hemisphere(tleft.utm) == UTM::is_northern_hemisphere(dem->utm)
	    && UTM::is_northern_hemisphere(bleft.utm) == UTM::is_northern_hemisphere(dem->utm)) { /* If the utm zones/hemispheres are different, min_eas will be bogus. */

		start_eas = std::max(min_eas, dem->min_east_seconds);
	} else {
		start_eas = dem->min_east_seconds;
	}

	if (UTM::is_the_same_zone(tright.utm, dem->utm) && UTM::is_the_same_zone(bright.utm, dem->utm)
	    && UTM::is_northern_hemisphere(tright.utm) == UTM::is_northern_hemisphere(dem->utm)
	    && UTM::is_northern_hemisphere(bright.utm) == UTM::is_northern_hemisphere(dem->utm)) { /* If the utm zones/hemispheres are different, min_eas will be bogus. */

		end_eas = std::min(max_eas, dem->max_east_seconds);
	} else {
		end_eas = dem->max_east_seconds;
	}

	start_nor = floor(start_nor / dem->north_scale) * dem->north_scale;
	end_nor   = ceil(end_nor / dem->north_scale) * dem->north_scale;
	start_eas = floor(start_eas / dem->east_scale) * dem->east_scale;
	end_eas   = ceil(end_eas / dem->east_scale) * dem->east_scale;

	int32_t start_x;
	int32_t start_y;
	dem->east_north_to_xy(start_eas, start_nor, &start_x, &start_y);

	/* TODO_LATER: why start_x and start_y are -1 -- rounding error from above? */

	const CoordMode viewport_coord_mode = viewport->get_coord_mode();

	/* TODO_2_LATER: smarter handling of invalid band letter
	   value. In theory the source object should be valid and for
	   sure contain valid band letter. */
	UTM counter(NAN, NAN, dem->utm.get_zone(), dem->utm.get_band_letter());

	const double min_elevation = this->min_elev.get_value();
	const double max_elevation = this->max_elev.get_value();

	int32_t x;
	for (x = start_x, counter.easting = start_eas; counter.easting <= end_eas; counter.easting += dem->east_scale * skip_factor, x += skip_factor) {
		if (x <= 0 || x >= dem->n_columns) { /* TODO_LATER: verify this condition, shouldn't it be "if (x < 0 || x >= dem->n_columns)"? */
			continue;
		}

		const DEMColumn * column = dem->columns[x];
	        int32_t y;
		for (y = start_y, counter.northing = start_nor; counter.northing <= end_nor; counter.northing += dem->north_scale * skip_factor, y += skip_factor) {
			if (y > column->n_points) {
				continue;
			}

			int16_t elev = column->points[y];
			if (elev == DEM_INVALID_ELEVATION) {
				continue; /* don't draw it */
			}


			if (elev < min_elevation) {
				elev = min_elevation;
			}
			if (elev > max_elevation) {
				elev = max_elevation;
			}


			{
				/* TODO_2_LATER: don't use Coord(ll, mode), especially if in latlon drawing mode. */
				const ScreenPos pos = viewport->coord_to_screen_pos(Coord(counter, viewport_coord_mode));

				int idx = 0; /* Default index for color of 'sea'. */
				if (elev > 0) {
					idx = GET_INDEX(elev, min_elevation, max_elevation, DEM_N_HEIGHT_COLORS);
				}
				//fprintf(stderr, "VIEWPORT: filling rectangle with color (%s:%d)\n", __FUNCTION__, __LINE__);
				viewport->fill_rectangle(this->colors[idx], pos.x - 1, pos.y - 1, 2, 2);
			}
		} /* for y= */
	} /* for x= */

	return;
}




void draw_loaded_dem_box(Viewport * viewport)
{
#ifdef TODO_LATER
	/* For getting values of dem_northeast and dem_southwest see DEM::intersect(). */
	const Coord demne(dem_northeast, viewport->get_coord_mode());
	const Coord demsw(dem_southwest, viewport->get_coord_mode());

	ScreenPos sp_ne = viewport->coord_to_screen_pos(demne);
	ScreenPos sp_sw = viewport->coord_to_screen_pos(demsw);

	if (sp_ne.x > viewport->get_width()) {
		sp_ne.x = viewport->get_width();
	}

	if (sp_sw.y > viewport->get_height()) {
		sp_sw.y = viewport->get_height();
	}

	if (sp_sw.x < 0) {
		sp_sw.x = 0;
	}

	if (sp_ne.y < 0) {
		sp_ne.y = 0;
	}

	qDebug() << SG_PREFIX_I << "drawing loaded DEM box";

	viewport->draw_rectangle(black_gc, sp_sw.x, sp_ne.y, sp_ne.x - sp_sw.x, sp_sw.y - sp_ne.y);
#endif

	return;
}



/* Get name of hgt.zip file specified by lat/lon location. */
const QString srtm_file_name(int lat, int lon)
{
	return QString("%1%2%3%4.hgt.zip")
		.arg((lat >= 0) ? 'N' : 'S')
		.arg(std::abs(lat), 2, 10, QChar('0'))
		.arg((lon >= 0) ? 'E' : 'W')
		.arg(std::abs(lon), 3, 10, QChar('0'));
}



/* Return the continent for the specified lat, lon. */
static bool srtm_get_continent_dir(QString & continent_dir, int lat, int lon)
{
	static QHash<QString, QString> continents; /* Coords -> continent. */

	extern const char *_srtm_continent_data[];

	if (continents.isEmpty()) {

		const char ** s = _srtm_continent_data;
		while (*s != (char *)-1) {
			const char * continent = *s++;
			while (*s) {
				continents.insert(QString(*s), QString(continent));
				s++;
			}
			s++;
		}
	}

	QString coords = srtm_file_name(lat, lon);
	coords.remove(".hgt.zip");

	auto iter = continents.find(coords);
	if (iter == continents.end()) {
		return false;
	} else {
		continent_dir = *iter;
		return true;
	}
}




void LayerDEM::draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected)
{
	/* Draw rectangles around areas, for which DEM tiles are already downloaded. */
	if (this->source == DEM_SOURCE_SRTM) {
		srtm_draw_existence(viewport);
#ifdef VIK_CONFIG_DEM24K
	} else if (this->source == DEM_SOURCE_DEM24K) {
		dem24k_draw_existence(viewport);
#endif
	}

	for (auto iter = this->files.begin(); iter != this->files.end(); iter++) {

		/* FIXME: dereferencing this iterator may fail when two things happen at the same time:
		   - layer is drawn,
		   - new DEM tiles are being downloaded.
		   Try this scenario:
		   1. create new DEM Layer,
		   2. open list of background jobs to observe downloads,
		   3. select "DEM download" tool from toolbar,
		   4. click with the tool in viewport to start downloading new DEM tile,
		   5. quickly switch to Pan tool, start moving viewport around to cause its redraws.

		   When new tile is downloaded and added to
		   this->files while this loop in ::draw_tree_item()
		   is executed, the iter becomes invalid and
		   dereferencing it crashes the program. */
		const QString dem_file_path = *iter;
		DEM * dem = DEMCache::get(dem_file_path);
		if (dem) {
			qDebug() << SG_PREFIX_I << "Got file" << dem_file_path << "from cache, will now draw it";
			this->draw_dem(viewport, dem);
		} else {
			qDebug() << SG_PREFIX_E << "Failed to get file" << dem_file_path << "from cache, not drawing";
		}
	}
}




LayerDEM::LayerDEM()
{
	qDebug() << SG_PREFIX_I << "LayerDEM::LayerDEM()";

	this->type = LayerType::DEM;
	strcpy(this->debug_string, "LayerType::DEM");
	this->interface = &vik_dem_layer_interface;

	this->dem_type = 0;

	this->colors.reserve(DEM_N_HEIGHT_COLORS);
	this->gradients.reserve(DEM_N_GRADIENT_COLORS);


	/* TODO_MAYBE: share ->colors[] between layers. */
	this->colors[0] = QColor("#0000FF"); /* Ensure the base color is available as soon as possible. */
	for (int32_t i = 1; i < DEM_N_HEIGHT_COLORS; i++) {
		this->colors[i] = QColor(dem_height_colors[i]);
	}

	for (size_t i = 0; i < DEM_N_GRADIENT_COLORS; i++) {
		this->gradients[i] = QColor(dem_gradient_colors[i]);
	}

	this->set_initial_parameter_values();
	this->set_name(Layer::get_type_ui_label(this->type));
}




LayerDEM::~LayerDEM()
{
	DEMCache::unload_from_cache(this->files);
	this->files.clear();
}




/**************************************************************
 **** SOURCES & DOWNLOADING
 **************************************************************/




DEMDownloadJob::DEMDownloadJob(const QString & new_dest_file_path, const LatLon & new_lat_lon, LayerDEM * layer_dem)
{
	this->n_items = 1; /* We are downloading one DEM tile at a time. */

	this->dest_file_path = new_dest_file_path;
	this->lat_lon = new_lat_lon;
	this->source = layer_dem->source;

	connect(this, SIGNAL (download_job_completed(const QString &)), layer_dem, SLOT (handle_downloaded_file_cb(const QString &)));
}




DEMDownloadJob::~DEMDownloadJob()
{
	/* TODO: make DEMDownloadJob a QT child of a DEM layer, so
	   that the job is destroyed when a layer is destroyed. */
}




/**************************************************
 *  SOURCE: SRTM                                  *
 **************************************************/

/* Function for downloading single SRTM data tile in background (in
   background thread). */
static void srtm_dem_download_thread(DEMDownloadJob * dl_job)
{
	const int intlat = (int) floor(dl_job->lat_lon.lat);
	const int intlon = (int) floor(dl_job->lat_lon.lon);

	QString continent_dir;
	if (!srtm_get_continent_dir(continent_dir, intlat, intlon)) {
		ThisApp::get_main_window()->statusbar_update(StatusBarField::Info, QObject::tr("No SRTM data available for %1").arg(dl_job->lat_lon.to_string()));
		return;
	}

	/* This is a file path on server. */
	const QString source_file = QString("%1%2/").arg(SRTM_HTTP_URI_PREFIX).arg(continent_dir) + srtm_file_name(intlat, intlon);

	static DownloadOptions dl_options(1); /* Follow redirect from http to https. */
	dl_options.file_validator_fn = map_file_validator_fn;

	DownloadHandle dl_handle(&dl_options);

	const DownloadStatus result = dl_handle.perform_download(SRTM_HTTP_SITE, source_file, dl_job->dest_file_path, DownloadProtocol::HTTP);
	switch (result) {
	case DownloadStatus::ContentError:
	case DownloadStatus::HTTPError:
		ThisApp::get_main_window()->statusbar_update(StatusBarField::Info, QObject::tr("DEM download failure for %1").arg(dl_job->lat_lon.to_string()));
		break;
	case DownloadStatus::FileWriteError:
		ThisApp::get_main_window()->statusbar_update(StatusBarField::Info, QObject::tr("DEM write failure for %s").arg(dl_job->dest_file_path));
		break;
	case DownloadStatus::Success:
	case DownloadStatus::DownloadNotRequired:
	default:
		qDebug() << SG_PREFIX_I << "layer download progress = 100";
		dl_job->progress = 100;
		break;
	}
}




static const QString srtm_lat_lon_to_cache_file_name(const LatLon & lat_lon)
{
	const int intlat = (int) floor(lat_lon.lat);
	const int intlon = (int) floor(lat_lon.lon);
	QString continent_dir;

	if (!srtm_get_continent_dir(continent_dir, intlat, intlon)) {
		qDebug() << SG_PREFIX_N << "Didn't hit any continent and coordinates" << lat_lon;
		continent_dir = "nowhere";
	}

	/* This is file in local maps cache dir. */
	const QString file = QString("srtm3-%1%2").arg(continent_dir).arg(QDir::separator()) + srtm_file_name(intlat, intlon);

	return file;
}




static void srtm_draw_existence(Viewport * viewport)
{
	QString cache_file_path;

	const LatLonBBox bbox = viewport->get_bbox();
	QPen pen("black");

	qDebug() << SG_PREFIX_D << "Viewport bounding box:" << bbox;

	for (int lat = floor(bbox.south.get_value()); lat <= floor(bbox.north.get_value()); lat++) {
		for (int lon = floor(bbox.west.get_value()); lon <= floor(bbox.east.get_value()); lon++) {
			QString continent_dir;
			if (!srtm_get_continent_dir(continent_dir, lat, lon)) {
				continue;
			}

			cache_file_path = QString("%1srtm3-%2%3").arg(MapCache::get_dir()).arg(continent_dir).arg(QDir::separator()) + srtm_file_name(lat, lon);
			if (0 != access(cache_file_path.toUtf8().constData(), F_OK)) {
				continue;
			}

			const LatLon lat_lon_sw(lat, lon);
			const LatLon lat_lon_ne(lat + 1, lon + 1);
			const LatLonBBox dem_bbox(lat_lon_sw, lat_lon_ne);

			viewport->draw_bbox(dem_bbox, pen);

#if 0
			Coord coord_ne;
			Coord coord_sw;

			coord_sw.ll.lat = lat;
			coord_sw.ll.lon = lon;
			coord_sw.mode = CoordMode::LatLon;

			coord_ne.ll.lat = lat + 1;
			coord_ne.ll.lon = lon + 1;
			coord_ne.mode = CoordMode::LatLon;

			draw_existence_common(viewport, pen, coord_sw, coord_ne, cache_file_path);
#endif
		}
	}
}


void draw_existence_common(Viewport * viewport, const QPen & pen, const Coord & coord_ne, const Coord & coord_sw, const QString & cache_file_path)
{
	ScreenPos sp_sw = viewport->coord_to_screen_pos(coord_sw);
	ScreenPos sp_ne = viewport->coord_to_screen_pos(coord_ne);

	if (sp_sw.x < 0) {
		sp_sw.x = 0;
	}

	if (sp_ne.y < 0) {
		sp_ne.y = 0;
	}

	qDebug() << SG_PREFIX_D << "Drawing existence rectangle for" << cache_file_path;
	viewport->draw_rectangle(pen, sp_sw.x, sp_ne.y, sp_ne.x - sp_sw.x, sp_sw.y - sp_ne.y);
}


/**************************************************
 *  SOURCE: USGS 24K                              *
 **************************************************/

#ifdef VIK_CONFIG_DEM24K

static void dem24k_dem_download_thread(DEMDownloadJob * dl_job)
{
	/* TODO_2_LATER: dest dir. */
	const QString cmdline = QString("%1 %2 %3")
		.arg(DEM24K_DOWNLOAD_SCRIPT)
		.arg(floor(dl_job->lat * 8) / 8, 0, 'f', 3, '0')  /* "%.03f" */
		.arg(ceil(dl_job->lon * 8) / 8, 0, 'f', 3, '0');  /* "%.03f" */

	/* FIXME: don't use system, use execv or something. check for existence. */
	system(cmdline.toUtf8().constData());
}




static QString dem24k_lat_lon_to_cache_file_name(const LatLon & lat_lon)
{
	return QString("dem24k/%1/%2/%3,%4.dem")
		.arg((int) lat_lon.lat)
		.arg((int) lat_lon.lon)
		.arg(floor(lat * 8) / 8, 0, 'f', 3, '0')
		.arg(ceil(lon * 8) / 8, 0, 'f', 3, '0');
}




static void dem24k_draw_existence(Viewport * viewport)
{
	const LatLonBBox viewport_bbox = viewport->get_bbox();
	QPen pen("black");

	for (double lat = floor(viewport_bbox.south * 8) / 8; lat <= floor(viewport_bbox.north * 8) / 8; lat += 0.125) {
		/* Check lat dir first -- faster. */
		QString cache_file_path = QString("%1dem24k/%2/").arg(MapCache::get_dir()).arg((int) lat);

		if (0 != access(cache_file_path.toUtf8().constData(), F_OK)) {
			continue;
		}

		for (double lon = floor(viewport_bbox.west * 8) / 8; lon <= floor(viewport_bbox.east * 8) / 8; lon += 0.125) {
			/* Check lon dir first -- faster. */
			cache_file_path = QString("%1dem24k/%2/%3/").arg(MapCache::get_dir()).arg((int) lat).arg((int) lon);
			if (0 != access(cache_file_path.toUtf8().constData(), F_OK)) {
				continue;
			}

			cache_file_path = QString("%1dem24k/%2/%3/%4,%5.dem")
				.arg(MapCache::get_dir())
				.arg((int) lat)
				.arg((int) lon)
				.arg(floor(lat * 8) / 8, 0, 'f', 3, '0')  /* "%.03f" */
				.arg(floor(lon * 8) / 8, 0, 'f', 3, '0'); /* "%.03f" */

			if (0 != access(cache_file_path.toUtf8().constData(), F_OK)) {
				continue;
			}

			Coord coord_ne;
			Coord coord_sw;

			coord_sw.ll.lat = lat;
			coord_sw.ll.lon = lon - 0.125;
			coord_sw.mode = CoordMode::LatLon;

			coord_ne.ll.lat = lat + 0.125;
			coord_ne.ll.lon = lon;
			coord_ne.mode = CoordMode::LatLon;

			draw_existence_common(viewport, pen, coord_sw, coord_ne, cache_file_path);
		}
	}
}
#endif


/**************************************************
 *   SOURCES -- DOWNLOADING & IMPORTING TOOL      *
 ***************************************************/




/* Try to add file full_path.
 * Returns false if file does not exists, true otherwise.
 */
bool LayerDEM::add_file(const QString & dem_file_full_path)
{
	if (0 == access(dem_file_full_path.toUtf8().constData(), F_OK)) {
		/* Only load if file size is not 0 (not in progress). */
		struct stat sb;
		stat(dem_file_full_path.toUtf8().constData(), &sb);
		if (sb.st_size) {
			this->files.push_front(dem_file_full_path);
			qDebug () << SG_PREFIX_I << "Will now load file" << dem_file_full_path << "from cache";
			DEMCache::load_file_into_cache(dem_file_full_path);
		} else {
			qDebug() << SG_PREFIX_I << dem_file_full_path << ": file size is zero";
		}
		return true;
	} else {
		qDebug() << SG_PREFIX_I << dem_file_full_path << ": file does not exist";
		return false;
	}
}




void DEMDownloadJob::run(void)
{
	qDebug() << SG_PREFIX_I << "download thread";

	if (this->source == DEM_SOURCE_SRTM) {
		srtm_dem_download_thread(this);
#ifdef VIK_CONFIG_DEM24K
	} else if (this->source == DEM_SOURCE_DEM24K) {
		dem24k_dem_download_thread(this);
#endif
	} else {
		return;
	}

	emit this->download_job_completed(this->dest_file_path);

	return;
}




static void free_dem_download_params(DEMDownloadJob * p)
{
	delete p;
}




LayerToolDEMDownload::LayerToolDEMDownload(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::DEM)
{
	this->id_string = "sg.tool.layer_dem.dem_download";

	this->action_icon_path   = ":/icons/layer_tool/dem_download_18.png";
	this->action_label       = QObject::tr("&DEM Download");
	this->action_tooltip     = QObject::tr("DEM Download");
	// this->action_accelerator = ...; /* Empty accelerator. */
}




ToolStatus LayerToolDEMDownload::handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	if (layer->type != LayerType::DEM) {
		return ToolStatus::Ignored;
	}

	/* Left button: download. Right button: context menu. */
	if (ev->button() != Qt::LeftButton && ev->button() != Qt::RightButton) {
		return ToolStatus::Ignored;
	}

	if (((LayerDEM *) layer)->download_release(ev, this)) {
		return ToolStatus::Ack;
	} else {
		return ToolStatus::Ignored;
	}
}




/**
 * Display a simple dialog with information about the DEM file at this location.
 */
void LayerDEM::location_info_cb(void) /* Slot. */
{
	QAction * qa = (QAction *) QObject::sender();
	QMenu * menu = (QMenu *) qa->parentWidget();

	const LatLon ll(menu->property("lat").toDouble(), menu->property("lon").toDouble());
	qDebug() << SG_PREFIX_I << "will display file info for coordinates" << ll;

	const int intlat = (int) floor(ll.lat);
	const int intlon = (int) floor(ll.lon);

	QString remote_location; /* Remote address where this file has been downloaded from. */
	QString continent_dir;
	if (srtm_get_continent_dir(continent_dir, intlat, intlon)) {
		remote_location = QString("http://%1%2%3/").arg(SRTM_HTTP_SITE).arg(SRTM_HTTP_URI_PREFIX).arg(continent_dir);
		remote_location += srtm_file_name(intlat, intlon);
	} else {
		/* Probably not over any land... */
		remote_location = tr("No DEM File Available");
	}

#ifdef VIK_CONFIG_DEM24K
	QString cache_file_name = dem24k_lat_lon_to_cache_file_name(ll);
#else
	QString cache_file_name = srtm_lat_lon_to_cache_file_name(ll);
#endif
	QString cache_file_path = MapCache::get_dir() + cache_file_name;

	QString message;
	if (0 == access(cache_file_path.toUtf8().constData(), F_OK)) {
		/* Get some timestamp information of the file. */
		struct stat stat_buf;
		if (stat(cache_file_path.toUtf8().constData(), &stat_buf) == 0) {
			const Time ts(stat_buf.st_mtime);
			message = tr("\nSource: %1\n\nDEM File: %2\nDEM File Timestamp: %3").arg(remote_location).arg(cache_file_path).arg(ts.strftime_utc("%c"));
		} else {
			message = tr("\nSource: %1\n\nDEM File: %2\nDEM File Timestamp: unavailable").arg(source).arg(cache_file_path);
		}
	} else {
		message = tr("Source: %1\n\nNo local DEM File!").arg(remote_location);
	}

	/* Show the info. */
	Dialog::info(message, this->get_window());
}




/* Mouse button released when "Download Tool" for DEM layer is active. */
bool LayerDEM::download_release(QMouseEvent * ev, LayerTool * tool)
{
	const Coord coord = tool->viewport->screen_pos_to_coord(ev->x(), ev->y());
	const LatLon ll = coord.get_latlon();

	qDebug() << SG_PREFIX_I << "received event, processing, coord =" << ll;

	QString cache_file_name;
	if (this->source == DEM_SOURCE_SRTM) {
		cache_file_name = srtm_lat_lon_to_cache_file_name(ll);
		qDebug() << SG_PREFIX_I << "cache file name" << cache_file_name;
#ifdef VIK_CONFIG_DEM24K
	} else if (this->source == DEM_SOURCE_DEM24K) {
		cache_file_name = dem24k_lat_lon_to_cache_file_name(ll);
#endif
	}

	if (!cache_file_name.length()) {
		qDebug() << SG_PREFIX_W << "Received click, but no DEM file";
		return true;
	}

	if (ev->button() == Qt::LeftButton) {
		const QString dem_full_path = MapCache::get_dir() + cache_file_name;
		qDebug() << SG_PREFIX_I << "released left button, path is" << dem_full_path;

		if (this->files.contains(dem_full_path)) {
			qDebug() << SG_PREFIX_I << "path already on list of files:" << dem_full_path;

		} else if (!this->add_file(dem_full_path)) {
			qDebug() << SG_PREFIX_I << "released left button, failed to add the file, downloading it";
			const QString job_description = QObject::tr("Downloading DEM %1").arg(cache_file_name);
			DEMDownloadJob * job = new DEMDownloadJob(dem_full_path, ll, this);
			job->set_description(job_description);
			job->run_in_background(ThreadPoolType::Remote);
		} else {
			qDebug() << SG_PREFIX_I << "released left button, successfully added the file, emitting 'changed'";
			this->emit_tree_item_changed("DEM - released left button");
		}

	} else if (ev->button() == Qt::RightButton) {
		qDebug() << SG_PREFIX_I << "release right button";
		if (!this->right_click_menu) {

			this->right_click_menu = new QMenu();
			QAction * qa = this->right_click_menu->addAction(QIcon::fromTheme("dialog-information"), "&Show DEM File Information");

			connect(qa, SIGNAL(triggered(bool)), this, SLOT(location_info_cb(void)));
		}

		/* What a hack... */
		QVariant variant;
		variant = QVariant::fromValue((double) ll.lat);
		this->right_click_menu->setProperty("lat", variant);
		variant = QVariant::fromValue((double) ll.lon);
		this->right_click_menu->setProperty("lon", variant);

		this->right_click_menu->exec(QCursor::pos());
	} else {
		;
	}

	return true;
}




static bool dem_layer_download_click(Layer * vdl, QMouseEvent * ev, LayerTool * tool)
{
	/* Choose & keep track of cache dir.
	 * Download in background thread.
	 * Download over area. */
	qDebug() << SG_PREFIX_I << "received click event, ignoring";
	return true;
}




void LayerDEM::on_loading_to_cache_completed_cb(void)
{
	qDebug() << SG_PREFIX_SIGNAL << "Will emit 'layer changed' after loading list of files";
	this->emit_tree_item_changed("DEM - loading to cache completed");
}




sg_ret LayerDEM::handle_downloaded_file_cb(const QString & file_full_path)
{
	qDebug() << SG_PREFIX_SLOT << "Received notification about downloaded file" << file_full_path;

	if (this->add_file(file_full_path)) {
		qDebug() << SG_PREFIX_SIGNAL << "Will emit 'layer changed' after downloading file";
		this->emit_tree_item_changed("Indicating change to DEM Layer after downloading a file");
	}

	return sg_ret::ok;
}
