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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <mutex>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <QMenu>
#include <QDebug>
#include <QHash>
#include <QDir>

#include "background.h"
#include "layer_map.h"
#include "layer_dem.h"
#include "dem.h"
#include "dem_cache.h"
#include "map_cache.h"
typedef int GdkPixdata; /* TODO: remove sooner or later. */
#include "icons/icons.h"
#include "file.h"
#include "dialog.h"
#include "globals.h"
#include "download.h"
#include "preferences.h"
#include "util.h"




using namespace SlavGPS;




/* Structure for DEM data used in background thread. */
class DemLoadJob : public BackgroundJob {
public:
	DemLoadJob(LayerDEM * layer);

	void cleanup_on_cancel(void);

	LayerDEM * layer = NULL;
};




class DEMDownloadJob : public BackgroundJob {
public:
	DEMDownloadJob(const QString & dest_file_path, const struct LatLon & ll, LayerDEM * layer);
	~DEMDownloadJob();

	QString dest_file_path;
	double lat, lon;

	std::mutex mutex;
	LayerDEM * layer; /* NULL if not alive. */

	unsigned int source;
};




#define SRTM_HTTP_SITE "dds.cr.usgs.gov"
#define SRTM_HTTP_URI_PREFIX  "/srtm/version2_1/SRTM3/"

#ifdef VIK_CONFIG_DEM24K
#define DEM24K_DOWNLOAD_SCRIPT "dem24k.pl"
#endif




/* Upper limit is that high in case if units are feet. */
static ParameterScale scale_min_elev = { 0, 30000,    SGVariant(0.0), 10, 1 };
static ParameterScale scale_max_elev = { 1, 30000, SGVariant(1000.0), 10, 1 };





static std::vector<SGLabelID> params_source = {
	SGLabelID(QObject::tr("SRTM Global 90m (3 arcsec)"), 0),
#ifdef VIK_CONFIG_DEM24K
	SGLabelID(QObject::tr("USA 10m (USGS 24k)"), 1),
#endif
};


static std::vector<SGLabelID> params_type = {
	SGLabelID(QObject::tr("Absolute height"), 0),
	SGLabelID(QObject::tr("Height gradient"), 1),
};



static SGVariant color_default(void)
{
	return SGVariant(0, 0, 255, 255);
}




static SGVariant source_default(void)
{
	return SGVariant((int32_t) DEM_SOURCE_SRTM);
}




static SGVariant type_default(void)
{
	return SGVariant((int32_t) DEM_TYPE_HEIGHT);
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




static Parameter dem_layer_params[] = {
	{ PARAM_FILES,      "files",    SGVariantType::STRING_LIST, PARAMETER_GROUP_GENERIC, N_("DEM Files:"),       WidgetType::FILELIST,        NULL,             NULL,             NULL, NULL },
	{ PARAM_SOURCE,     "source",   SGVariantType::INT,         PARAMETER_GROUP_GENERIC, N_("Download Source:"), WidgetType::RADIOGROUP,      &params_source,   source_default,   NULL, NULL },
	{ PARAM_COLOR,      "color",    SGVariantType::COLOR,       PARAMETER_GROUP_GENERIC, N_("Min Elev Color:"),  WidgetType::COLOR,           NULL,             color_default,    NULL, NULL },
	{ PARAM_TYPE,       "type",     SGVariantType::INT,         PARAMETER_GROUP_GENERIC, N_("Type:"),            WidgetType::RADIOGROUP,      &params_type,     type_default,     NULL, NULL },
	{ PARAM_MIN_ELEV,   "min_elev", SGVariantType::DOUBLE,      PARAMETER_GROUP_GENERIC, N_("Min Elev:"),        WidgetType::SPINBOX_DOUBLE,  &scale_min_elev,  NULL,             NULL, NULL },
	{ PARAM_MAX_ELEV,   "max_elev", SGVariantType::DOUBLE,      PARAMETER_GROUP_GENERIC, N_("Max Elev:"),        WidgetType::SPINBOX_DOUBLE,  &scale_max_elev,  NULL,             NULL, NULL },

	{ NUM_PARAMS,       NULL,       SGVariantType::EMPTY,       PARAMETER_GROUP_GENERIC, NULL,                   WidgetType::NONE,            NULL,             NULL,             NULL, NULL }, /* Guard. */
};




static LayerTool * dem_layer_download_create(Window * window, Viewport * viewport);
static bool dem_layer_download_release(Layer * vdl, QMouseEvent * ev, LayerTool * tool);
static bool dem_layer_download_click(Layer * vdl, QMouseEvent * ev, LayerTool * tool);
static void srtm_draw_existence(Viewport * viewport);
static int dem_load_list_thread(BackgroundJob * bg_job);
static int dem_download_thread(BackgroundJob * bg_job);
#ifdef VIK_CONFIG_DEM24K
static void dem24k_draw_existence(Viewport * viewport);
#endif




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

static const unsigned int DEM_N_HEIGHT_COLORS = sizeof(dem_height_colors)/sizeof(dem_height_colors[0]);




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

static const unsigned int DEM_N_GRADIENT_COLORS = sizeof(dem_gradient_colors)/sizeof(dem_gradient_colors[0]);




LayerDEMInterface vik_dem_layer_interface;




LayerDEMInterface::LayerDEMInterface()
{
	this->parameters_c = dem_layer_params;

	strncpy(this->layer_type_string, "DEM", sizeof (this->layer_type_string) - 1); /* Non-translatable. */
	this->layer_type_string[sizeof (this->layer_type_string) - 1] = '\0';

	this->layer_name = QObject::tr("DEM");
	this->action_accelerator = Qt::CTRL + Qt::SHIFT + Qt::Key_D;
	// this->action_icon = ...; /* Set elsewhere. */

	this->layer_tool_constructors.insert({{ 0, dem_layer_download_create }});

	this->menu_items_selection = LayerMenuItem::ALL;

	this->ui_labels.new_layer = QObject::tr("New DEM Layer");
}




QString LayerDEM::tooltip()
{
	return QString(tr("Number of files: %1")).arg(this->files.size());
}




Layer * LayerDEMInterface::unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	LayerDEM * layer = new LayerDEM();

	/* TODO: share ->colors[] between layers. */
	layer->colors[0] = new QColor(layer->base_color);
	for (unsigned int i = 1; i < DEM_N_HEIGHT_COLORS; i++) {
		layer->colors[i] = new QColor(dem_height_colors[i]);
	}

	for (unsigned int i = 0; i < DEM_N_GRADIENT_COLORS; i++) {
		layer->gradients[i] = new QColor(dem_gradient_colors[i]);
	}

	layer->unmarshall_params(data, len);
	return layer;
}




DemLoadJob::DemLoadJob(LayerDEM * layer_)
{
	this->thread_fn = dem_load_list_thread;
	this->n_items = layer_->files.size(); /* Number of DEM files. */

	this->layer = layer_;
}




/*
 * Function for starting the DEM file loading as a background thread/
 */
static int dem_load_list_thread(BackgroundJob * bg_job)
{
	DemLoadJob * load_job = (DemLoadJob *) bg_job;

	int result = 0; /* Default to good. */
	/* Actual Load. */

	QStringList dem_filenames = load_job->layer->files; /* kamilTODO: do we really need to make the copy? */

	if (DEMCache::load_files_into_cache(dem_filenames, load_job)) {
		/* Thread cancelled. */
		result = -1;
	}

	/* ATM as each file is processed the screen is not updated (no mechanism exposed to DEMCache::load_files_into_cache()).
	   Thus force draw only at the end, as loading is complete/aborted. */
	//gdk_threads_enter();
	/* Test is helpful to prevent Gtk-CRITICAL warnings if the program is exitted whilst loading. */
	if (load_job->layer) {
		qDebug() << "SIGNAL: Layer DEM: will emit 'layer changed' B";
		load_job->layer->emit_changed(); /* NB update from background thread. */
	}
	//gdk_threads_leave();

	return result;
}




void DemLoadJob::cleanup_on_cancel(void)
{
	/* Abort loading.
	   Instead of freeing the list, leave it as partially processed.
	   Thus we can see/use what was done. */
}




/**
 * Process the list of DEM files and convert each one to a relative path.
 */
static void dem_layer_convert_to_relative_filenaming(QStringList & relfiles, const QStringList & files)
{
	const QString cwd = QDir::currentPath();
	if (cwd.isEmpty()) {
		relfiles = files;
		return;
	}

	for (auto iter = files.begin(); iter != files.end(); iter++) {
		const QString file = file_GetRelativeFilename(cwd, *iter);
		relfiles.push_front(file);
	}

	if (relfiles.empty()) {
		/* TODO: is this correct? Should we return original files? Is this condition ever true? */
		relfiles = files;
	}

	return;
}




bool LayerDEM::set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation)
{
	switch (id) {
	case PARAM_COLOR:
		this->base_color.setRed(param_value.c.r);
		this->base_color.setGreen(param_value.c.g);
		this->base_color.setBlue(param_value.c.b);
		this->base_color.setAlpha(127);

		*this->colors[0] = this->base_color;

		break;

	case PARAM_SOURCE:
		this->source = param_value.i;
		break;

	case PARAM_TYPE:
		this->dem_type = param_value.i;
		break;

	case PARAM_MIN_ELEV:
		/* Convert to store internally.
		   NB file operation always in internal units (metres). */
		if (!is_file_operation && Preferences::get_unit_height() == HeightUnit::FEET) {
			this->min_elev = VIK_FEET_TO_METERS(param_value.d);
		} else {
			this->min_elev = param_value.d;
		}
		break;
	case PARAM_MAX_ELEV:
		/* Convert to store internally.
		   NB file operation always in internal units (metres). */
		if (!is_file_operation && Preferences::get_unit_height() == HeightUnit::FEET) {
			this->max_elev = VIK_FEET_TO_METERS(param_value.d);
		} else {
			this->max_elev = param_value.d;
		}
		break;
	case PARAM_FILES: {
		/* Clear out old settings - if any commonalities with new settings they will have to be read again. */
		DEMCache::unload_from_cache(this->files);

		/* Set file list so any other intermediate screen drawing updates will show currently loaded DEMs by the working thread. */
		this->files = param_value.sl;

		qDebug() << "DD: Layer DEM: set param value: list of files:";
		if (!this->files.empty()) {
			for (auto iter = this->files.begin(); iter != this->files.end(); ++iter) {
				qDebug() << "DD: Layer DEM: set param value: file:" << *iter;
			}
		} else {
			qDebug() << "DD: Layer DEM: set param value: no files";
		}
		/* No need for thread if no files. */
		if (!this->files.empty()) {
			/* Thread Load. */
			DemLoadJob * load_job = new DemLoadJob(this);
			a_background_thread(load_job, ThreadPoolType::LOCAL, _("DEM Loading"));
		}

		break;
	}

	default:
		break;
	}
	return true;
}




SGVariant LayerDEM::get_param_value(param_id_t id, bool is_file_operation) const
{
	SGVariant rv;

	switch (id) {

	case PARAM_FILES:
		qDebug() << "II: Layer DEM: get param value: string list (" << this->files.size() << " elements):";
		qDebug() << this->files;

		/* Save in relative format if necessary. */
		if (is_file_operation && Preferences::get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
			dem_layer_convert_to_relative_filenaming(rv.sl, this->files);
		} else {
			rv.sl = this->files;
		}
		rv.type_id = SGVariantType::STRING_LIST; /* TODO: direct assignment of variant type - fix (hide) this. */

		break;

	case PARAM_SOURCE:
		rv.i = this->source;
		break;

	case PARAM_TYPE:
		rv.i = this->dem_type;
		break;

	case PARAM_COLOR:
		rv.c.r = this->base_color.red();
		rv.c.g = this->base_color.green();
		rv.c.b = this->base_color.blue();
		rv.c.a = this->base_color.alpha();
		break;

	case PARAM_MIN_ELEV:
		/* Convert for display in desired units.
		   NB file operation always in internal units (metres). */
		if (!is_file_operation && Preferences::get_unit_height() == HeightUnit::FEET) {
			rv.d = VIK_METERS_TO_FEET(this->min_elev);
		} else {
			rv.d = this->min_elev;
		}
		break;
	case PARAM_MAX_ELEV:
		/* Convert for display in desired units.
		   NB file operation always in internal units (metres). */
		if (!is_file_operation && Preferences::get_unit_height() == HeightUnit::FEET) {
			rv.d = VIK_METERS_TO_FEET(this->max_elev);
		} else {
			rv.d = this->max_elev;
		}
		break;
	default: break;
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
	double max_lat, max_lon, min_lat, min_lon;
	viewport->get_min_max_lat_lon(&min_lat, &max_lat, &min_lon, &max_lon);

	/* If given DEM is loaded into application, we want to know whether the DEM and
	   current viewport overlap, so that we know whether we should draw it in
	   viewport or not. We do this check every time a viewport has been changed
	   (moved or re-zoomed). */
	LatLonBBox viewport_bbox;
	viewport->get_bbox(&viewport_bbox);
	if (!dem->overlap(&viewport_bbox)) {
		qDebug() << "Dem: no overlap, skipping";
		return;
	}

	/* Boxes to show where we have DEM instead of actually drawing the DEM.
	 * useful if we want to see what areas we have coverage for (if we want
	 * to get elevation data for a track) but don't want to cover the map.
	 */

	/* Draw a box if a DEM is loaded. in future I'd like to add an option for this
	 * this is useful if we want to see what areas we have dem for but don't want to
	 * cover the map (or maybe we just need translucent DEM?). */
#if 0
	draw_loaded_dem_box(viewport);
#endif

	if (dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS) {
		Coord tmp; /* TODO: don't use Coord(ll, mode), especially if in latlon drawing mode. */

		unsigned int skip_factor = ceil(viewport->get_xmpp() / 80); /* TODO: smarter calculation. */

		double nscale_deg = dem->north_scale / ((double) 3600);
		double escale_deg = dem->east_scale / ((double) 3600);

		double max_lat_as = max_lat * 3600;
		double min_lat_as = min_lat * 3600;
		double max_lon_as = max_lon * 3600;
		double min_lon_as = min_lon * 3600;

		double start_lat_as = MAX(min_lat_as, dem->min_north);
		double end_lat_as   = MIN(max_lat_as, dem->max_north);
		double start_lon_as = MAX(min_lon_as, dem->min_east);
		double end_lon_as   = MIN(max_lon_as, dem->max_east);

		double start_lat = floor(start_lat_as / dem->north_scale) * nscale_deg;
		double end_lat   = ceil(end_lat_as / dem->north_scale) * nscale_deg;
		double start_lon = floor(start_lon_as / dem->east_scale) * escale_deg;
		double end_lon   = ceil(end_lon_as / dem->east_scale) * escale_deg;

		unsigned int start_x, start_y;
		dem->east_north_to_xy(start_lon_as, start_lat_as, &start_x, &start_y);
		unsigned int gradient_skip_factor = 1;
		if (this->dem_type == DEM_TYPE_GRADIENT) {
			gradient_skip_factor = skip_factor;
		}

		/* Verify sane elev interval. */
		if (this->max_elev <= this->min_elev) {
			this->max_elev = this->min_elev + 1;
		}

		struct LatLon counter;
		unsigned int x;
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

			unsigned int y;
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
				struct LatLon box_c;
				box_c = counter;
				box_c.lat += (nscale_deg * skip_factor)/2;
				box_c.lon -= (escale_deg * skip_factor)/2;
				tmp = Coord(box_c, viewport->get_coord_mode());
				viewport->coord_to_screen(&tmp, &box_x, &box_y);
				/* Catch box at borders. */
				if (box_x < 0) {
					box_x = 0;
				}

				if (box_y < 0) {
					box_y = 0;
				}

				box_c.lat -= nscale_deg * skip_factor;
				box_c.lon += escale_deg * skip_factor;
				tmp = Coord(box_c, viewport->get_coord_mode());
				viewport->coord_to_screen(&tmp, &box_width, &box_height);
				box_width -= box_x;
				box_height -= box_y;
				/* Catch box at borders. */
				if (box_width < 0 || box_height < 0) {
					/* Skip this as is out of the viewport (e.g. zoomed in so this point is way off screen). */
					continue;
				}

				bool below_minimum = false;
				if (this->dem_type == DEM_TYPE_HEIGHT) {
					if (elev < this->min_elev) {
						/* Prevent 'elev - this->min_elev' from being negative so can safely use as array index. */
						elev = ceil(this->min_elev);
						below_minimum = true;
					}
					if (elev > this->max_elev) {
						elev = this->max_elev;
					}
				}

				if (this->dem_type == DEM_TYPE_GRADIENT) {
					/* Calculate and sum gradient in all directions. */
					int16_t change = 0;
					int32_t new_y;

					/* Calculate gradient from height points all around the current one. */
					new_y = y - gradient_skip_factor;
					if (new_y < 0) {
						new_y = y;
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

					if (change < this->min_elev) {
						/* Prevent 'change - this->min_elev' from being negative so can safely use as array index. */
						change = ceil(this->min_elev);
					}

					if (change > this->max_elev) {
						change = this->max_elev;
					}

					int idx = (int)floor(((change - this->min_elev)/(this->max_elev - this->min_elev))*(DEM_N_GRADIENT_COLORS-2))+1;
					//fprintf(stderr, "VIEWPORT: filling rectangle with gradient (%s:%d)\n", __FUNCTION__, __LINE__);
					viewport->fill_rectangle(*this->gradients[idx], box_x, box_y, box_width, box_height);

				} else if (this->dem_type == DEM_TYPE_HEIGHT) {
					int idx = 0; /* Default index for color of 'sea' or for places below the defined mininum. */
					if (elev > 0 && !below_minimum) {
						idx = (int)floor(((elev - this->min_elev)/(this->max_elev - this->min_elev))*(DEM_N_HEIGHT_COLORS-2))+1;
					}
					//fprintf(stderr, "VIEWPORT: filling rectangle with color (%s:%d)\n", __FUNCTION__, __LINE__);
					viewport->fill_rectangle(*this->colors[idx], box_x, box_y, box_width, box_height);
				} else {
					; /* No other dem type to process. */
				}
			} /* for y= */
		} /* for x= */
	} else if (dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {

		Coord tmp; /* TODO: don't use Coord(ll, mode), especially if in latlon drawing mode. */

		unsigned int skip_factor = ceil(viewport->get_xmpp() / 10); /* TODO: smarter calculation. */

		Coord tleft = viewport->screen_to_coord(0,                      0);
		Coord tright = viewport->screen_to_coord(viewport->get_width(), 0);
		Coord bleft = viewport->screen_to_coord(0,                      viewport->get_height());
		Coord bright = viewport->screen_to_coord(viewport->get_width(), viewport->get_height());

		tleft.change_mode(CoordMode::UTM);
		tright.change_mode(CoordMode::UTM);
		bleft.change_mode(CoordMode::UTM);
		bright.change_mode(CoordMode::UTM);

		double max_nor = MAX(tleft.utm.northing, tright.utm.northing);
		double min_nor = MIN(bleft.utm.northing, bright.utm.northing);
		double max_eas = MAX(bright.utm.easting, tright.utm.easting);
		double min_eas = MIN(bleft.utm.easting, tleft.utm.easting);

		double start_eas, end_eas;
		double start_nor = MAX(min_nor, dem->min_north);
		double end_nor   = MIN(max_nor, dem->max_north);
		if (tleft.utm.zone == dem->utm_zone && bleft.utm.zone == dem->utm_zone
		    && (tleft.utm.letter >= 'N') == (dem->utm_letter >= 'N')
		    && (bleft.utm.letter >= 'N') == (dem->utm_letter >= 'N')) { /* If the utm zones/hemispheres are different, min_eas will be bogus. */

			start_eas = MAX(min_eas, dem->min_east);
		} else {
			start_eas = dem->min_east;
		}

		if (tright.utm.zone == dem->utm_zone && bright.utm.zone == dem->utm_zone
		    && (tright.utm.letter >= 'N') == (dem->utm_letter >= 'N')
		    && (bright.utm.letter >= 'N') == (dem->utm_letter >= 'N')) { /* If the utm zones/hemispheres are different, min_eas will be bogus. */

			end_eas = MIN(max_eas, dem->max_east);
		} else {
			end_eas = dem->max_east;
		}

		start_nor = floor(start_nor / dem->north_scale) * dem->north_scale;
		end_nor   = ceil(end_nor / dem->north_scale) * dem->north_scale;
		start_eas = floor(start_eas / dem->east_scale) * dem->east_scale;
		end_eas   = ceil(end_eas / dem->east_scale) * dem->east_scale;

		unsigned int start_x, start_y;
		dem->east_north_to_xy(start_eas, start_nor, &start_x, &start_y);

		/* TODO: why start_x and start_y are -1 -- rounding error from above? */

		struct UTM counter;
		counter.zone = dem->utm_zone;
		counter.letter = dem->utm_letter;

		unsigned int x;
		for (x = start_x, counter.easting = start_eas; counter.easting <= end_eas; counter.easting += dem->east_scale * skip_factor, x += skip_factor) {
			if (x <= 0 || x >= dem->n_columns) { /* kamilTODO: verify this condition, shouldn't it be "if (x < 0 || x >= dem->n_columns)"? */
				continue;
			}

			DEMColumn * column = dem->columns[x];
			unsigned int y;
			for (y = start_y, counter.northing = start_nor; counter.northing <= end_nor; counter.northing += dem->north_scale * skip_factor, y += skip_factor) {
				if (y > column->n_points) {
					continue;
				}

				int16_t elev = column->points[y];
				if (elev == DEM_INVALID_ELEVATION) {
					continue; /* don't draw it */
				}


				if (elev < this->min_elev) {
					elev = this->min_elev;
				}
				if (elev > this->max_elev) {
					elev = this->max_elev;
				}


				{
					int a, b;
					tmp = Coord(counter, viewport->get_coord_mode());
					viewport->coord_to_screen(&tmp, &a, &b);

					int idx = 0; /* Default index for color of 'sea'. */
					if (elev > 0) {
						idx = (int)floor((elev - this->min_elev)/(this->max_elev - this->min_elev)*(DEM_N_HEIGHT_COLORS-2))+1;
					}
					//fprintf(stderr, "VIEWPORT: filling rectangle with color (%s:%d)\n", __FUNCTION__, __LINE__);
					viewport->fill_rectangle(*this->colors[idx], a - 1, b - 1, 2, 2);
				}
			} /* for y= */
		} /* for x= */
	}
}




void draw_loaded_dem_box(Viewport * viewport)
{
#if 0
	/* For getting values of dem_northeast and dem_southwest see vik_dem_overlap(). */
	int x1, y1, x2, y2;
	const Coord demne(dem_northeast, viewport->get_coord_mode());
	const Coord demsw(dem_southwest, viewport->get_coord_mode());

	viewport->coord_to_screen(&demne, &x1, &y1);
	viewport->coord_to_screen(&demsw, &x2, &y2);

	if (x1 > viewport->get_width()) {
		x1 = viewport->get_width();
	}

	if (y2 > viewport->get_height()) {
		y2 = viewport->get_height();
	}

	if (x2 < 0) {
		x2 = 0;
	}

	if (y1 < 0) {
		y1 = 0;
	}

	qDebug() << "II: Layer DEM: drawing loaded DEM box";
#if 0
	viewport->draw_rectangle(gtk_widget_get_style(GTK_WIDGET(viewport->vvp))->black_gc, x2, y1, x1-x2, y2-y1);
#endif
#endif
	return;
}



/* Get name of hgt.zip file specified by lat/lon location. */
const QString srtm_file_name(int lat, int lon)
{
	return QString("%1%2%3%4.hgt.zip")
		.arg((lat >= 0) ? 'N' : 'S')
		.arg(ABS (lat), 2, 10, QChar('0'))
		.arg((lon >= 0) ? 'E' : 'W')
		.arg(ABS (lon), 3, 10, QChar('0'));
}



/* Return the continent for the specified lat, lon. */
/* TODO */
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




void LayerDEM::draw(Viewport * viewport)
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
		const QString dem_file_path = *iter;
		DEM * dem = DEMCache::get(dem_file_path);
		if (dem) {
			qDebug() << "II: Layer DEM: got file" << dem_file_path << "from cache, will now draw it";
			this->draw_dem(viewport, dem);
		} else {
			qDebug() << "EE: Layer DEM: failed to get file" << dem_file_path << "from cache, not drawing";
		}
	}
}




LayerDEM::LayerDEM()
{
	qDebug() << "II: LayerDEM::LayerDEM()";

	this->type = LayerType::DEM;
	strcpy(this->debug_string, "LayerType::DEM");
	this->interface = &vik_dem_layer_interface;

	this->dem_type = 0;

	this->colors = (QColor **) malloc(sizeof(QColor *) * DEM_N_HEIGHT_COLORS);
	this->gradients = (QColor **) malloc(sizeof(QColor *) * DEM_N_GRADIENT_COLORS);

	/* Ensure the base color is available so the default color can be applied. */
	this->colors[0] = new QColor("#0000FF");

	this->set_initial_parameter_values();

	/* TODO: share ->colors[] between layers. */
	for (unsigned int i = 0; i < DEM_N_HEIGHT_COLORS; i++) {
		if (i > 0) {
			this->colors[i] = new QColor(dem_height_colors[i]);
		}
	}

	for (unsigned int i = 0; i < DEM_N_GRADIENT_COLORS; i++) {
		this->gradients[i] = new QColor(dem_gradient_colors[i]);
	}
}




LayerDEM::~LayerDEM()
{
	if (this->colors) {
		for (unsigned int i = 0; i < DEM_N_HEIGHT_COLORS; i++) {
			delete this->colors[i];
		}
	}
	free(this->colors);


	if (this->gradients) {
		for (unsigned int i = 0; i < DEM_N_GRADIENT_COLORS; i++) {
			delete this->gradients[i];
		}
	}
	free(this->gradients);


#ifdef K
	DEMCache::unload_from_cache(this->files);
	this->files.clear();
#endif

}




/**************************************************************
 **** SOURCES & DOWNLOADING
 **************************************************************/




DEMDownloadJob::DEMDownloadJob(const QString & dest_file_path_, const struct LatLon & ll, LayerDEM * layer_dem)
{
	this->thread_fn = dem_download_thread;
	this->n_items = 1; /* We are downloading one DEM tile at a time. */

	this->dest_file_path = dest_file_path_;
	this->lat = ll.lat;
	this->lon = ll.lon;
	this->layer = layer_dem;
	this->source = layer_dem->source;
	this->layer->weak_ref(LayerDEM::weak_ref_cb, this);
}




DEMDownloadJob::~DEMDownloadJob()
{
}




/**************************************************
 *  SOURCE: SRTM                                  *
 **************************************************/

/* Function for downloading single SRTM data tile in background (in
   background thread). */
static void srtm_dem_download_thread(DEMDownloadJob * dl_job)
{
	int intlat = (int) floor(dl_job->lat);
	int intlon = (int) floor(dl_job->lon);

	QString continent_dir;
	if (!srtm_get_continent_dir(continent_dir, intlat, intlon)) {
		if (dl_job->layer) {
			dl_job->layer->get_window()->statusbar_update(StatusBarField::INFO, QString("No SRTM data available for %1, %2").arg(dl_job->lat).arg(dl_job->lon)); /* Float + float */
		}
		return;
	}

	/* This is a file path on server. */
	const QString source_file = QString("%1%2/").arg(SRTM_HTTP_URI_PREFIX).arg(continent_dir) + srtm_file_name(intlat, intlon);

	static DownloadOptions dl_options(1); /* Follow redirect from http to https. */
	dl_options.check_file = a_check_map_file;

	DownloadResult result = Download::get_url_http(SRTM_HTTP_SITE, source_file, dl_job->dest_file_path, &dl_options, NULL);
	switch (result) {
	case DownloadResult::CONTENT_ERROR:
	case DownloadResult::HTTP_ERROR: {
		dl_job->layer->get_window()->statusbar_update(StatusBarField::INFO, QString("DEM download failure for %1, %2").arg(dl_job->lat).arg(dl_job->lon)); /* Float + float. */
		break;
	}
	case DownloadResult::FILE_WRITE_ERROR: {
		dl_job->layer->get_window()->statusbar_update(StatusBarField::INFO, QString("DEM write failure for %s").arg(dl_job->dest_file_path));
		break;
	}
	case DownloadResult::SUCCESS:
	case DownloadResult::NOT_REQUIRED:
	default:
		qDebug() << "II: Layer DEM: layer download progress = 100";
		dl_job->progress = 100;
		break;
	}
}




static const QString srtm_lat_lon_to_cache_file_name(double lat, double lon)
{
	int intlat = (int) floor(lat);
	int intlon = (int) floor(lon);
	QString continent_dir;

	if (!srtm_get_continent_dir(continent_dir, intlat, intlon)) {
		qDebug() << "NN: Layer DEM: didn't hit any continent and coordinates" << lat << lon;
		continent_dir = "nowhere";
	}

	/* This is file in local maps cache dir. */
	const QString file = QString("srtm3-%1%2").arg(continent_dir).arg(G_DIR_SEPARATOR_S) + srtm_file_name(intlat, intlon);

	return file;
}




/* TODO: generalize. */
static void srtm_draw_existence(Viewport * viewport)
{
	QString cache_file_path;

	LatLonBBox bbox;
	viewport->get_bbox(&bbox);
	QPen pen("black");


	qDebug() << "DD: Layer DEM: Existence: viewport bounding box: north:" << (int) bbox.north << "south:" << (int) bbox.south << "east:" << (int) bbox.east << "west:" << (int) bbox.west;

	for (int lat = floor(bbox.south); lat <= floor(bbox.north); lat++) {
		for (int lon = floor(bbox.west); lon <= floor(bbox.east); lon++) {
			QString continent_dir;
			if (!srtm_get_continent_dir(continent_dir, lat, lon)) {
				continue;
			}

			cache_file_path = QString("%1srtm3-%2%3").arg(map_cache_dir()).arg(continent_dir).arg(G_DIR_SEPARATOR_S) + srtm_file_name(lat, lon);
			if (0 != access(cache_file_path.toUtf8().constData(), F_OK)) {
				continue;
			}

			Coord ne, sw;
			int x1, y1, x2, y2;

			sw.ll.lat = lat;
			sw.ll.lon = lon;
			sw.mode = CoordMode::LATLON;

			ne.ll.lat = lat + 1;
			ne.ll.lon = lon + 1;
			ne.mode = CoordMode::LATLON;

			viewport->coord_to_screen(&sw, &x1, &y1);
			viewport->coord_to_screen(&ne, &x2, &y2);

			if (x1 < 0) {
				x1 = 0;
			}

			if (y2 < 0) {
				y2 = 0;
			}

			//qDebug() << "II: Layer DEM: drawing existence rectangle for" << buf;
			viewport->draw_rectangle(pen, x1, y2, x2 - x1, y1 - y2);
		}
	}
}




/**************************************************
 *  SOURCE: USGS 24K                              *
 **************************************************/

#ifdef VIK_CONFIG_DEM24K

static void dem24k_dem_download_thread(DemDownloadJob * dl_job)
{
	/* TODO: dest dir. */
	char *cmdline = g_strdup_printf("%s %.03f %.03f",
					DEM24K_DOWNLOAD_SCRIPT,
					floor(dl_job->lat * 8) / 8,
					ceil(dl_job->lon * 8) / 8);
	/* FIXME: don't use system, use execv or something. check for existence. */
	system(cmdline);
	free(cmdline);
}




static char * dem24k_lat_lon_to_cache_file_name(double lat, double lon)
{
	return g_strdup_printf("dem24k/%d/%d/%.03f,%.03f.dem",
			       (int) lat,
			       (int) lon,
			       floor(lat*8)/8,
			       ceil(lon*8)/8);
}




/* TODO: generalize. */
static void dem24k_draw_existence(Viewport * viewport)
{
	double max_lat, max_lon, min_lat, min_lon;
	QString buf;
	double i, j;

	viewport->get_min_max_lat_lon(&min_lat, &max_lat, &min_lon, &max_lon);

	for (i = floor(min_lat*8)/8; i <= floor(max_lat*8)/8; i+=0.125) {
		/* Check lat dir first -- faster. */
		buf = QString("%1dem24k/%2/").arg(map_cache_dir()).arg((int) i);

		if (0 != access(buf.toUtf8().constData(), F_OK)) {
			continue;
		}

		for (j = floor(min_lon*8)/8; j <= floor(max_lon*8)/8; j+=0.125) {
			/* Check lon dir first -- faster. */
			buf = QString("%1dem24k/%2/%3/").arg(map_cache_dir()).arg((int) i).arg((int) j);
			if (0 != access(buf.toUtf8().constData(), F_OK)) {
				continue;
			}

			buf = QString("%1dem24k/%2/%3/%4,%5.dem")
				.arg(map_cache_dir())
				.arg((int) i)
				.arg((int) j)
				.arg(floor(i*8)/8, 0, 'f', 3, '0')  /* "%.03f" */
				.arg(floor(j*8)/8, 0, 'f', 3, '0'); /* "%.03f" */

			if (0 == access(buf.toUtf8().constData(), F_OK)) {
				Coord ne, sw;
				int x1, y1, x2, y2;

				sw.ll.lat = i;
				sw.ll.lon = j-0.125;
				sw.mode = CoordMode::LATLON;

				ne.ll.lat = i+0.125;
				ne.ll.lon = j;
				ne.mode = CoordMode::LATLON;

				viewport->coord_to_screen(&sw, &x1, &y1);
				viewport->coord_to_screen(&ne, &x2, &y2);

				if (x1 < 0) {
					x1 = 0;
				}

				if (y2 < 0) {
					y2 = 0;
				}

				qDebug() << "DD: Layer DEM: drawing rectangle";
				viewport->draw_rectangle(viewport->black_gc, x1, y2, x2 - x1, y1 - y2);
			}
		}
	}
}
#endif




/**************************************************
 *   SOURCES -- DOWNLOADING & IMPORTING TOOL      *
 ***************************************************/




void LayerDEM::weak_ref_cb(void * ptr, void * dead_vdl)
{
	DEMDownloadJob * p = (DEMDownloadJob *) ptr;
	p->mutex.lock();
	p->layer = NULL;
	p->mutex.unlock();
}

/* Try to add file full_path.
 * Returns false if file does not exists, true otherwise.
 */
bool LayerDEM::add_file(const QString & dem_file_path)
{
	if (0 == access(dem_file_path.toUtf8().constData(), F_OK)) {
		/* Only load if file size is not 0 (not in progress). */
		struct stat sb;
		stat(dem_file_path.toUtf8().constData(), &sb);
		if (sb.st_size) {
			this->files.push_front(dem_file_path);
			qDebug () << "II: Layer DEM: Add file: will now load file" << dem_file_path << "from cache";
			DEMCache::load_file_into_cache(dem_file_path);
		} else {
			qDebug() << "II: Layer DEM: Add file:" << dem_file_path << ": file size is zero";
		}
		return true;
	} else {
		qDebug() << "II: Layer DEM: Add file:" << dem_file_path << ": file does not exist";
		return false;
	}
}




static int dem_download_thread(BackgroundJob * bg_job)
{
	DEMDownloadJob * dl_job = (DEMDownloadJob *) bg_job;

	qDebug() << "II: Layer DEM: download thread --------------------";

	if (dl_job->source == DEM_SOURCE_SRTM) {
		srtm_dem_download_thread(dl_job);
#ifdef VIK_CONFIG_DEM24K
	} else if (p->source == DEM_SOURCE_DEM24K) {
		dem24k_dem_download_thread(dl_job);
#endif
	} else {
		return 0;
	}

	//gdk_threads_enter();

	dl_job->mutex.lock();
	if (dl_job->layer) {
		dl_job->layer->weak_unref(LayerDEM::weak_ref_cb, dl_job);

		if (dl_job->layer->add_file(dl_job->dest_file_path)) {
			qDebug() << "SIGNAL: Layer DEM: will emit 'layer changed' A";
			dl_job->layer->emit_changed(); /* NB update from background thread. */
		}
	}
	dl_job->mutex.unlock();

	//gdk_threads_leave();

	return 0;
}




static void free_dem_download_params(DEMDownloadJob * p)
{
	delete p;
}




static LayerTool * dem_layer_download_create(Window * window, Viewport * viewport)
{
	return new LayerToolDEMDownload(window, viewport);
}




LayerToolDEMDownload::LayerToolDEMDownload(Window * window_, Viewport * viewport_) : LayerTool(window_, viewport_, LayerType::DEM)
{
	this->id_string = QString("dem.download");

	this->action_icon_path   = ":/icons/layer_tool/dem_download_18.png";
	this->action_label       = QObject::tr("&DEM Download");
	this->action_tooltip     = QObject::tr("DEM Download");
	// this->action_accelerator = ...; /* Empty accelerator. */

	this->cursor_click = new QCursor(Qt::ArrowCursor);
	this->cursor_release = new QCursor(Qt::ArrowCursor);

	Layer::get_interface(LayerType::DEM)->layer_tools.insert({{ 0, this }} ); /* There is only one tool, so this magic number is not that bad. */
}




LayerToolFuncStatus LayerToolDEMDownload::handle_mouse_release(Layer * layer, QMouseEvent * ev)
{
	if (layer->type != LayerType::DEM) {
		return LayerToolFuncStatus::IGNORE;
	}

	/* Left button: download. Right button: context menu. */
	if (ev->button() != Qt::LeftButton && ev->button() != Qt::RightButton) {
		return LayerToolFuncStatus::IGNORE;
	}

	if (((LayerDEM *) layer)->download_release(ev, this)) {
		return LayerToolFuncStatus::ACK;
	} else {
		return LayerToolFuncStatus::IGNORE;
	}
}




/**
 * Display a simple dialog with information about the DEM file at this location.
 */
void LayerDEM::location_info_cb(void) /* Slot. */
{
	QAction * qa = (QAction *) QObject::sender();
	QMenu * menu = (QMenu *) qa->parentWidget();

	struct LatLon ll;

	QVariant variant;
	variant = menu->property("lat");
	ll.lat = variant.toDouble();
	variant = menu->property("lon");
	ll.lon = variant.toDouble();

	qDebug() << "II: Layer DEM: will display file info for coordinates" << ll.lat << ll.lon;



	int intlat = (int) floor(ll.lat);
	int intlon = (int) floor(ll.lon);

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
	QString cache_file_name(dem24k_lat_lon_to_cache_file_name(ll.lat, ll.lon));
#else
	QString cache_file_name = srtm_lat_lon_to_cache_file_name(ll.lat, ll.lon);
#endif
	QString cache_file_path = map_cache_dir() + cache_file_name;

	QString message;
	if (0 == access(cache_file_path.toUtf8().constData(), F_OK)) {
		/* Get some timestamp information of the file. */
		struct stat stat_buf;
		if (stat(cache_file_path.toUtf8().constData(), &stat_buf) == 0) {
			char time_buf[64];
			strftime(time_buf, sizeof(time_buf), "%c", gmtime((const time_t *)&stat_buf.st_mtime));
			message = QString("\nSource: %1\n\nDEM File: %2\nDEM File Timestamp: %3").arg(remote_location).arg(cache_file_path).arg(time_buf);
		} else {
			message = QString("\nSource: %1\n\nDEM File: %2\nDEM File Timestamp: unavailable").arg(source).arg(cache_file_path);
		}
	} else {
		message = QString("Source: %1\n\nNo local DEM File!").arg(QString(remote_location));
	}

	/* Show the info. */
	Dialog::info(message, this->get_window());
}




/* Mouse button released when "Download Tool" for DEM layer is active. */
bool LayerDEM::download_release(QMouseEvent * ev, LayerTool * tool)
{
	const Coord coord = tool->viewport->screen_to_coord(ev->x(), ev->y());
	const struct LatLon ll = coord.get_latlon();

	qDebug() << "II: Layer DEM: Download Tool: Release: received event, processing (coord" << ll.lat << ll.lon << ")";

	QString cache_file_name;
	if (this->source == DEM_SOURCE_SRTM) {
		cache_file_name = srtm_lat_lon_to_cache_file_name(ll.lat, ll.lon);
		qDebug() << "II: Layer DEM: Download Tool: Release: cache file name" << cache_file_name;
#ifdef VIK_CONFIG_DEM24K
	} else if (this->source == DEM_SOURCE_DEM24K) {
		cache_file_name = dem24k_lat_lon_to_cache_file_name(ll.lat, ll.lon);
#endif
	}

	if (!cache_file_name.length()) {
		qDebug() << "NN: Layer DEM: Download Tool: Release: received click, but no dem file";
		return true;
	}

	if (ev->button() == Qt::LeftButton) {
		const QString dem_full_path = map_cache_dir() + cache_file_name;
		qDebug() << "II: Layer DEM: Download Tool: Release: released left button, path is" << dem_full_path;

		/* TODO: check if already in filelist. */
		if (!this->add_file(dem_full_path)) {
			qDebug() << "II: Layer DEM: Download Tool: Release: released left button, failed to add the file, downloading it";
			const QString job_description = QString(tr("Downloading DEM  %1")).arg(cache_file_name);
			DEMDownloadJob * job = new DEMDownloadJob(dem_full_path, ll, this);
			a_background_thread(job, ThreadPoolType::REMOTE, job_description);
		} else {
			qDebug() << "II: Layer DEM: Download Tool: Release: released left button, successfully added the file, emitting 'changed'";
			this->emit_changed();
		}

	} else if (ev->button() == Qt::RightButton) {
		qDebug() << "II: Layer DEM: release right button";
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
	qDebug() << "II: Layer DEM: received click event, ignoring";
	return true;
}
