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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <cstdlib>

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "background.h"
//#include "viking.h"
#include "vikmapslayer.h"
#include "vikdemlayer.h"
#include "dem.h"
#include "dems.h"
#include "icons/icons.h"
#include "file.h"
#include "dialog.h"
#include "globals.h"




using namespace SlavGPS;




#define MAPS_CACHE_DIR maps_layer_default_dir()
#define MAPS_CACHE_DIR_2 maps_layer_default_dir_2()

#define SRTM_CACHE_TEMPLATE "%ssrtm3-%s%s%c%02d%c%03d.hgt.zip"
#define SRTM_HTTP_SITE "dds.cr.usgs.gov"
#define SRTM_HTTP_URI  "/srtm/version2_1/SRTM3/"

#ifdef VIK_CONFIG_DEM24K
#define DEM24K_DOWNLOAD_SCRIPT "dem24k.pl"
#endif

#define UNUSED_LINE_THICKNESS 3

static Layer * dem_layer_unmarshall(uint8_t * data, int len, Viewport * viewport);
static void srtm_draw_existence(Viewport * viewport);

#ifdef VIK_CONFIG_DEM24K
static void dem24k_draw_existence(Viewport * viewport);
#endif


/* Upped upper limit incase units are feet */
static VikLayerParamScale param_scales[] = {
	{ 0, 30000, 10, 1 },
	{ 1, 30000, 10, 1 },
};

static char *params_source[] = {
	(char *) "SRTM Global 90m (3 arcsec)",
#ifdef VIK_CONFIG_DEM24K
	(char *) "USA 10m (USGS 24k)",
#endif
	NULL
};

static char *params_type[] = {
	(char *) N_("Absolute height"),
	(char *) N_("Height gradient"),
	NULL
};

enum { DEM_SOURCE_SRTM,
#ifdef VIK_CONFIG_DEM24K
       DEM_SOURCE_DEM24K,
#endif
};

enum { DEM_TYPE_HEIGHT = 0,
       DEM_TYPE_GRADIENT,
       DEM_TYPE_NONE,
};




static VikLayerParamData color_default(void)
{
	VikLayerParamData data;
	gdk_color_parse("blue", &data.c);
	return data;
}




static VikLayerParamData source_default(void)
{
	return VIK_LPD_UINT (DEM_SOURCE_SRTM);
}




static VikLayerParamData type_default(void)
{
	return VIK_LPD_UINT (DEM_TYPE_HEIGHT);
}




static VikLayerParamData min_elev_default(void)
{
	return VIK_LPD_DOUBLE (0.0);
}




static VikLayerParamData max_elev_default(void)
{
	return VIK_LPD_DOUBLE (1000.0);
}




static VikLayerParam dem_layer_params[] = {
	{ LayerType::DEM, "files",    VIK_LAYER_PARAM_STRING_LIST, VIK_LAYER_GROUP_NONE, N_("DEM Files:"),       VIK_LAYER_WIDGET_FILELIST,          NULL,             NULL, NULL, NULL, NULL, NULL },
	{ LayerType::DEM, "source",   VIK_LAYER_PARAM_UINT,        VIK_LAYER_GROUP_NONE, N_("Download Source:"), VIK_LAYER_WIDGET_RADIOGROUP_STATIC, params_source,    NULL, NULL, source_default, NULL, NULL },
	{ LayerType::DEM, "color",    VIK_LAYER_PARAM_COLOR,       VIK_LAYER_GROUP_NONE, N_("Min Elev Color:"),  VIK_LAYER_WIDGET_COLOR,             NULL,             NULL, NULL, color_default, NULL, NULL },
	{ LayerType::DEM, "type",     VIK_LAYER_PARAM_UINT,        VIK_LAYER_GROUP_NONE, N_("Type:"),            VIK_LAYER_WIDGET_RADIOGROUP_STATIC, params_type,      NULL, NULL, type_default, NULL, NULL },
	{ LayerType::DEM, "min_elev", VIK_LAYER_PARAM_DOUBLE,      VIK_LAYER_GROUP_NONE, N_("Min Elev:"),        VIK_LAYER_WIDGET_SPINBUTTON,        param_scales + 0, NULL, NULL, min_elev_default, NULL, NULL },
	{ LayerType::DEM, "max_elev", VIK_LAYER_PARAM_DOUBLE,      VIK_LAYER_GROUP_NONE, N_("Max Elev:"),        VIK_LAYER_WIDGET_SPINBUTTON,        param_scales + 0, NULL, NULL, max_elev_default, NULL, NULL },
};




enum {
	PARAM_FILES = 0,
	PARAM_SOURCE,
	PARAM_COLOR,
	PARAM_TYPE,
	PARAM_MIN_ELEV,
	PARAM_MAX_ELEV,
	NUM_PARAMS
};




static LayerTool * dem_layer_download_create(Window * window, Viewport * viewport);
static bool dem_layer_download_release(Layer * vdl, GdkEventButton *event, LayerTool * tool);
static bool dem_layer_download_click(Layer * vdl, GdkEventButton *event, LayerTool * tool);

static LayerTool * dem_tools[] = { NULL };




/* HEIGHT COLORS
   The first entry is blue for a default 'sea' colour,
   however the value used by the corresponding gc can be configured as part of the DEM layer properties.
   The other colours, shaded from brown to white are used to give an indication of height.
*/
static char *dem_height_colors[] = {
	(char *) "#0000FF",
	(char *) "#9b793c", (char *) "#9c7d40", (char *) "#9d8144", (char *) "#9e8549", (char *) "#9f894d", (char *) "#a08d51", (char *) "#a29156", (char *) "#a3955a", (char *) "#a4995e", (char *) "#a69d63",
	(char *) "#a89f65", (char *) "#aaa267", (char *) "#ada569", (char *) "#afa76b", (char *) "#b1aa6d", (char *) "#b4ad6f", (char *) "#b6b071", (char *) "#b9b373", (char *) "#bcb676", (char *) "#beb978",
	(char *) "#c0bc7a", (char *) "#c2c07d", (char *) "#c4c37f", (char *) "#c6c681", (char *) "#c8ca84", (char *) "#cacd86", (char *) "#ccd188", (char *) "#cfd58b", (char *) "#c2ce84", (char *) "#b5c87e",
	(char *) "#a9c278", (char *) "#9cbb71", (char *) "#8fb56b", (char *) "#83af65", (char *) "#76a95e", (char *) "#6aa358", (char *) "#5e9d52", (char *) "#63a055", (char *) "#69a458", (char *) "#6fa85c",
	(char *) "#74ac5f", (char *) "#7ab063", (char *) "#80b467", (char *) "#86b86a", (char *) "#8cbc6e", (char *) "#92c072", (char *) "#94c175", (char *) "#97c278", (char *) "#9ac47c", (char *) "#9cc57f",
	(char *) "#9fc682", (char *) "#a2c886", (char *) "#a4c989", (char *) "#a7cb8d", (char *) "#aacd91", (char *) "#afce99", (char *) "#b5d0a1", (char *) "#bbd2aa", (char *) "#c0d3b2", (char *) "#c6d5ba",
	(char *) "#ccd7c3", (char *) "#d1d9cb", (char *) "#d7dbd4", (char *) "#DDDDDD", (char *) "#e0e0e0", (char *) "#e4e4e4", (char *) "#e8e8e8", (char *) "#ebebeb", (char *) "#efefef", (char *) "#f3f3f3",
	(char *) "#f7f7f7", (char *) "#fbfbfb", (char *) "#ffffff"
};

static const unsigned int DEM_N_HEIGHT_COLORS = sizeof(dem_height_colors)/sizeof(dem_height_colors[0]);

/*
"#9b793c", "#9e8549", "#a29156", "#a69d63", "#ada569", "#b4ad6f", "#bcb676", "#c2c07d", "#c8ca84", "#cfd58b",
"#a9c278", "#83af65", "#5e9d52", "#6fa85c", "#80b467", "#92c072", "#9ac47c", "#a2c886", "#aacd91", "#bbd2aa",
"#ccd7c3", "#DDDDDD", "#e8e8e8", "#f3f3f3", "#FFFFFF"
};
*/

static char *dem_gradient_colors[] = {
	(char *) "#AAAAAA",
	(char *) "#000000", (char *) "#000011", (char *) "#000022", (char *) "#000033", (char *) "#000044", (char *) "#00004c", (char *) "#000055", (char *) "#00005d", (char *) "#000066", (char *) "#00006e",
	(char *) "#000077", (char *) "#00007f", (char *) "#000088", (char *) "#000090", (char *) "#000099", (char *) "#0000a1", (char *) "#0000aa", (char *) "#0000b2", (char *) "#0000bb", (char *) "#0000c3",
	(char *) "#0000cc", (char *) "#0000d4", (char *) "#0000dd", (char *) "#0000e5", (char *) "#0000ee", (char *) "#0000f6", (char *) "#0000ff", (char *) "#0008f7", (char *) "#0011ee", (char *) "#0019e6",
	(char *) "#0022dd", (char *) "#002ad5", (char *) "#0033cc", (char *) "#003bc4", (char *) "#0044bb", (char *) "#004cb3", (char *) "#0055aa", (char *) "#005da2", (char *) "#006699", (char *) "#006e91",
	(char *) "#007788", (char *) "#007f80", (char *) "#008877", (char *) "#00906f", (char *) "#009966", (char *) "#00a15e", (char *) "#00aa55", (char *) "#00b24d", (char *) "#00bb44", (char *) "#00c33c",
	(char *) "#00cc33", (char *) "#00d42b", (char *) "#00dd22", (char *) "#00e51a", (char *) "#00ee11", (char *) "#00f609", (char *) "#00ff00", (char *) "#08f700", (char *) "#11ee00", (char *) "#19e600",
	(char *) "#22dd00", (char *) "#2ad500", (char *) "#33cc00", (char *) "#3bc400", (char *) "#44bb00", (char *) "#4cb300", (char *) "#55aa00", (char *) "#5da200", (char *) "#669900", (char *) "#6e9100",
	(char *) "#778800", (char *) "#7f8000", (char *) "#887700", (char *) "#906f00", (char *) "#996600", (char *) "#a15e00", (char *) "#aa5500", (char *) "#b24d00", (char *) "#bb4400", (char *) "#c33c00",
	(char *) "#cc3300", (char *) "#d42b00", (char *) "#dd2200", (char *) "#e51a00", (char *) "#ee1100", (char *) "#f60900", (char *) "#ff0000",
	(char *) "#FFFFFF"
};

static const unsigned int DEM_N_GRADIENT_COLORS = sizeof(dem_gradient_colors)/sizeof(dem_gradient_colors[0]);




VikLayerInterface vik_dem_layer_interface = {
	"DEM",
	N_("DEM"),
	"<control><shift>D",
	&vikdemlayer_pixbuf,

	{ dem_layer_download_create, NULL, NULL, NULL, NULL, NULL, NULL }, /* (VikToolConstructorFunc)  */
	dem_tools,
	1,

	dem_layer_params,
	NUM_PARAMS,
	NULL,
	0,

	VIK_MENU_ITEM_ALL,

	/* (VikLayerFuncUnmarshall) */   dem_layer_unmarshall,

	/* (VikLayerFuncSetParam) */     layer_set_param,
	/* (VikLayerFuncGetParam) */     layer_get_param,
	/* (VikLayerFuncChangeParam) */  NULL,
};




char const * LayerDEM::tooltip()
{
	static char tmp_buf[100];

	snprintf(tmp_buf, sizeof(tmp_buf), _("Number of files: %ld"), this->files->size());
	return tmp_buf;
}

static Layer * dem_layer_unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	LayerDEM * layer = new LayerDEM(viewport);

	/* TODO: share GCS between layers. */
	for (unsigned int i = 0; i < DEM_N_HEIGHT_COLORS; i++) {
		if (i == 0) {
			layer->gcs[i] = viewport->new_gc_from_color(&layer->color, UNUSED_LINE_THICKNESS);
		} else {
			layer->gcs[i] = viewport->new_gc(dem_height_colors[i], UNUSED_LINE_THICKNESS);
		}
	}
	for (unsigned int i = 0; i < DEM_N_GRADIENT_COLORS; i++) {
		layer->gcsgradient[i] = viewport->new_gc(dem_gradient_colors[i], UNUSED_LINE_THICKNESS);
	}

	layer->unmarshall_params(data, len, viewport);
	return layer;
}




/* Structure for DEM data used in background thread. */
typedef struct {
	LayerDEM * layer;
} dem_load_thread_data;




/*
 * Function for starting the DEM file loading as a background thread/
 */
static int dem_layer_load_list_thread(dem_load_thread_data * dltd, void * threaddata)
{
	int result = 0; /* Default to good. */
	/* Actual Load. */

	std::list<std::string> dem_filenames;
	for (auto iter = dltd->layer->files->begin(); iter != dltd->layer->files->end(); iter++) {
		std::string dem_filename = std::string(*iter);
		dem_filenames.push_front(dem_filename);
	}

	if (dem_cache_load_list(dem_filenames, threaddata)) {
		/* Thread cancelled. */
		result = -1;
	}

	/* ATM as each file is processed the screen is not updated (no mechanism exposed to dem_cache_load_list).
	   Thus force draw only at the end, as loading is complete/aborted. */
	//gdk_threads_enter();
	/* Test is helpful to prevent Gtk-CRITICAL warnings if the program is exitted whilst loading. */
	if (dltd->layer) {
		dltd->layer->emit_update(); /* NB update from background thread. */
	}
	//gdk_threads_leave();

	return result;
}




static void dem_layer_thread_data_free(dem_load_thread_data * data)
{
	/* Simple release. */
	free(data);
}




static void dem_layer_thread_cancel(dem_load_thread_data * data)
{
	/* Abort loading.
	   Instead of freeing the list, leave it as partially processed.
	   Thus we can see/use what was done. */
}




/**
 * Process the list of DEM files and convert each one to a relative path.
 */
static std::list<char *> * dem_layer_convert_to_relative_filenaming(std::list<char *> * files)
{
	char *cwd = g_get_current_dir();
	if (!cwd) {
		return files;
	}

	std::list<char *> * relfiles = new std::list<char *>;

	for (auto iter = files->begin(); iter != files->end(); iter++) {
		char * file = (char *) g_strdup(file_GetRelativeFilename(cwd, *iter));
		relfiles->push_front(file);
	}

	free(cwd);

	if (relfiles->empty()) {
		return files;
	} else {
		/* Replacing current list, so delete old values first. */
		for (auto iter = files->begin(); iter != files->end(); iter++) {
			free(*iter);
		}
		delete files;
		return relfiles;
	}
}




bool LayerDEM::set_param(uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation)
{
	switch (id) {
	case PARAM_COLOR:
		this->color = data.c;
		gdk_gc_set_rgb_fg_color(this->gcs[0], &(this->color));
		break;
	case PARAM_SOURCE:
		this->source = data.u;
		break;
	case PARAM_TYPE:
		this->dem_type = data.u;
		break;
	case PARAM_MIN_ELEV:
		/* Convert to store internally.
		   NB file operation always in internal units (metres). */
		if (!is_file_operation && a_vik_get_units_height() == HeightUnit::FEET) {
			this->min_elev = VIK_FEET_TO_METERS(data.d);
		} else {
			this->min_elev = data.d;
		}
		break;
	case PARAM_MAX_ELEV:
		/* Convert to store internally.
		   NB file operation always in internal units (metres). */
		if (!is_file_operation && a_vik_get_units_height() == HeightUnit::FEET) {
			this->max_elev = VIK_FEET_TO_METERS(data.d);
		} else {
			this->max_elev = data.d;
		}
		break;
	case PARAM_FILES:
		{
			/* Clear out old settings - if any commonalities with new settings they will have to be read again. */
			// dem_cache_list_free (this->files); // kamilFIXME: re-enable this line in future.
			/* Set file list so any other intermediate screen drawing updates will show currently loaded DEMs by the working thread. */
			this->files = data.sl;
			/* No need for thread if no files. */
			if (this->files && !this->files->empty()) {
				/* Thread Load. */
				dem_load_thread_data * dltd = (dem_load_thread_data *) malloc(sizeof(dem_load_thread_data));
				dltd->layer = this;
				dltd->layer->files = data.sl;

				a_background_thread(BACKGROUND_POOL_LOCAL,
						    viewport->get_toolkit_window(),
						    _("DEM Loading"),
						    (vik_thr_func) dem_layer_load_list_thread,
						    dltd,
						    (vik_thr_free_func) dem_layer_thread_data_free,
						    (vik_thr_free_func) dem_layer_thread_cancel,
						    data.sl->size()); /* Number of DEM files. */
			}
			break;
		}
	default: break;
	}
	return true;
}




VikLayerParamData LayerDEM::get_param(uint16_t id, bool is_file_operation) const
{
	VikLayerParamData rv;

	switch (id) {
	case PARAM_FILES:
		rv.sl = this->files;
		if (is_file_operation) {
			/* Save in relative format if necessary. */
			if (a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE) {
				rv.sl = dem_layer_convert_to_relative_filenaming(rv.sl);
			}
		}
		break;
	case PARAM_SOURCE:
		rv.u = this->source;
		break;
	case PARAM_TYPE:
		rv.u = this->dem_type;
		break;
	case PARAM_COLOR:
		rv.c = this->color;
		break;
	case PARAM_MIN_ELEV:
		/* Convert for display in desired units.
		   NB file operation always in internal units (metres). */
		if (!is_file_operation && a_vik_get_units_height() == HeightUnit::FEET) {
			rv.d = VIK_METERS_TO_FEET(this->min_elev);
		} else {
			rv.d = this->min_elev;
		}
		break;
	case PARAM_MAX_ELEV:
		/* Convert for display in desired units.
		   NB file operation always in internal units (metres). */
		if (!is_file_operation && a_vik_get_units_height() == HeightUnit::FEET) {
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
	if (new_elev == VIK_DEM_INVALID_ELEVATION) {
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
		return;
	}

#if 0
	/* Boxes to show where we have DEM instead of actually drawing the DEM.
	 * useful if we want to see what areas we have coverage for (if we want
	 * to get elevation data for a track) but don't want to cover the map.
	 */

	/* Draw a box if a DEM is loaded. in future I'd like to add an option for this
	 * this is useful if we want to see what areas we have dem for but don't want to
	 * cover the map (or maybe we just need translucent DEM?). */
	draw_loaded_dem_box(viewport);
#endif

	if (dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS) {
		Coord tmp; /* TODO: don't use coord_load_from_latlon, especially if in latlon drawing mode. */

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
				if (elev == VIK_DEM_INVALID_ELEVATION) {
					continue; /* Don't draw it. */
				}

				/* Calculate bounding box for drawing. */
				int box_x, box_y, box_width, box_height;
				struct LatLon box_c;
				box_c = counter;
				box_c.lat += (nscale_deg * skip_factor)/2;
				box_c.lon -= (escale_deg * skip_factor)/2;
				vik_coord_load_from_latlon(&tmp, viewport->get_coord_mode(), &box_c);
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
				vik_coord_load_from_latlon(&tmp, viewport->get_coord_mode(), &box_c);
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

					// void Viewport::draw_rectangle(Viewport * viewport, GdkGC *gc, bool filled, int x1, int y1, int x2, int y2);
					int idx = (int)floor(((change - this->min_elev)/(this->max_elev - this->min_elev))*(DEM_N_GRADIENT_COLORS-2))+1;
					viewport->draw_rectangle(this->gcsgradient[idx], true, box_x, box_y, box_width, box_height);

				} else if (this->dem_type == DEM_TYPE_HEIGHT) {
					int idx = 0; /* Default index for colour of 'sea' or for places below the defined mininum. */
					if (elev > 0 && !below_minimum) {
						idx = (int)floor(((elev - this->min_elev)/(this->max_elev - this->min_elev))*(DEM_N_HEIGHT_COLORS-2))+1;
					}
					viewport->draw_rectangle(this->gcs[idx], true, box_x, box_y, box_width, box_height);
				} else {
					; /* No other dem type to process. */
				}
			} /* for y= */
		} /* for x= */
	} else if (dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {

		Coord tmp; /* TODO: don't use coord_load_from_latlon, especially if in latlon drawing mode. */

		unsigned int skip_factor = ceil(viewport->get_xmpp() / 10); /* TODO: smarter calculation. */

		Coord tleft, tright, bleft, bright;

		viewport->screen_to_coord(0,                     0,                      &tleft);
		viewport->screen_to_coord(viewport->get_width(), 0,                      &tright);
		viewport->screen_to_coord(0,                     viewport->get_height(), &bleft);
		viewport->screen_to_coord(viewport->get_width(), viewport->get_height(), &bright);

		vik_coord_convert(&tleft, VIK_COORD_UTM);
		vik_coord_convert(&tright, VIK_COORD_UTM);
		vik_coord_convert(&bleft, VIK_COORD_UTM);
		vik_coord_convert(&bright, VIK_COORD_UTM);

		double max_nor = MAX(tleft.north_south, tright.north_south);
		double min_nor = MIN(bleft.north_south, bright.north_south);
		double max_eas = MAX(bright.east_west, tright.east_west);
		double min_eas = MIN(bleft.east_west, tleft.east_west);

		double start_eas, end_eas;
		double start_nor = MAX(min_nor, dem->min_north);
		double end_nor   = MIN(max_nor, dem->max_north);
		if (tleft.utm_zone == dem->utm_zone && bleft.utm_zone == dem->utm_zone
		    && (tleft.utm_letter >= 'N') == (dem->utm_letter >= 'N')
		    && (bleft.utm_letter >= 'N') == (dem->utm_letter >= 'N')) { /* If the utm zones/hemispheres are different, min_eas will be bogus. */

			start_eas = MAX(min_eas, dem->min_east);
		} else {
			start_eas = dem->min_east;
		}

		if (tright.utm_zone == dem->utm_zone && bright.utm_zone == dem->utm_zone
		    && (tright.utm_letter >= 'N') == (dem->utm_letter >= 'N')
		    && (bright.utm_letter >= 'N') == (dem->utm_letter >= 'N')) { /* If the utm zones/hemispheres are different, min_eas will be bogus. */

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
				if (elev == VIK_DEM_INVALID_ELEVATION) {
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
					vik_coord_load_from_utm(&tmp, viewport->get_coord_mode(), &counter);
					viewport->coord_to_screen(&tmp, &a, &b);

					int idx = 0; /* Default index for colour of 'sea'. */
					if (elev > 0) {
						idx = (int)floor((elev - this->min_elev)/(this->max_elev - this->min_elev)*(DEM_N_HEIGHT_COLORS-2))+1;
					}
					viewport->draw_rectangle(this->gcs[idx], true, a-1, b-1, 2, 2);
				}
			} /* for y= */
		} /* for x= */
	}
}




#if 0
void draw_loaded_dem_box(Viewport * viewport)
{
	/* For getting values of dem_northeast and dem_southwest see vik_dem_overlap(). */
	Coord demne, demsw;
	int x1, y1, x2, y2;
	vik_coord_load_from_latlon(&demne, viewport->get_coord_mode(), &dem_northeast);
	vik_coord_load_from_latlon(&demsw, viewport->get_coord_mode(), &dem_southwest);

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

	viewport->draw_rectangle(gtk_widget_get_style(GTK_WIDGET(viewport->vvp))->black_gc,
					 false, x2, y1, x1-x2, y2-y1);
	return;
}
#endif




/* Return the continent for the specified lat, lon. */
/* TODO */
static const char *srtm_continent_dir(int lat, int lon)
{
	extern const char *_srtm_continent_data[];
	static GHashTable *srtm_continent = NULL;
	const char *continent;
	char name[16];

	if (!srtm_continent) {
		const char **s;

		srtm_continent = g_hash_table_new(g_str_hash, g_str_equal);
		s = _srtm_continent_data;
		while (*s != (char *)-1) {
			continent = *s++;
			while (*s) {
				g_hash_table_insert(srtm_continent, (void *) *s, (void *) continent);
				s++;
			}
			s++;
		}
	}
	snprintf(name, sizeof(name), "%c%02d%c%03d",
		 (lat >= 0) ? 'N' : 'S', ABS(lat),
		 (lon >= 0) ? 'E' : 'W', ABS(lon));

	return ((const char *) g_hash_table_lookup(srtm_continent, name));
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

	for (auto iter = this->files->begin(); iter != this->files->end(); iter++) {
		std::string dem_filename = std::string(*iter);
		DEM * dem = dem_cache_get(dem_filename);
		if (dem) {
			this->draw_dem(viewport, dem);
		}
	}
}




LayerDEM::~LayerDEM()
{
	if (this->gcs) {
		for (unsigned int i = 0; i < DEM_N_HEIGHT_COLORS; i++) {
			g_object_unref(this->gcs[i]);
		}
	}

	free(this->gcs);

	if (this->gcsgradient) {
		for (unsigned int i = 0; i < DEM_N_GRADIENT_COLORS; i++) {
			g_object_unref(this->gcsgradient[i]);
		}
	}
	free(this->gcsgradient);

	// dem_cache_list_free(this->files); // kamilFIXME: re-enable this line in future
}




/**************************************************************
 **** SOURCES & DOWNLOADING
 **************************************************************/
class DEMDownloadParams {
public:
	DEMDownloadParams(std::string& full_path, struct LatLon * ll, LayerDEM * layer);
	~DEMDownloadParams();

	std::string dest;
	double lat, lon;

	GMutex * mutex;
	LayerDEM * layer; /* NULL if not alive. */

	unsigned int source;
};





DEMDownloadParams::DEMDownloadParams(std::string& full_path, struct LatLon * ll, LayerDEM * layer)
{
	this->dest = std::string(full_path);
	this->lat = ll->lat;
	this->lon = ll->lon;
	this->layer = layer;
	this->mutex = vik_mutex_new();
	this->source = layer->source;
	this->layer->weak_ref(LayerDEM::weak_ref_cb, this);
}




DEMDownloadParams::~DEMDownloadParams()
{
	vik_mutex_free(this->mutex);
}




/**************************************************
 *  SOURCE: SRTM                                  *
 **************************************************/

static void srtm_dem_download_thread(DEMDownloadParams * p, void * threaddata)
{
	int intlat, intlon;
	const char *continent_dir;

	intlat = (int)floor(p->lat);
	intlon = (int)floor(p->lon);
	continent_dir = srtm_continent_dir(intlat, intlon);

	if (!continent_dir) {
		if (p->layer) {
			char *msg = g_strdup_printf(_("No SRTM data available for %f, %f"), p->lat, p->lon);
			p->layer->get_window()->statusbar_update(msg, VIK_STATUSBAR_INFO);
			free(msg);
		}
		return;
	}

	char *src_fn = g_strdup_printf("%s%s/%c%02d%c%03d.hgt.zip",
				       SRTM_HTTP_URI,
				       continent_dir,
				       (intlat >= 0) ? 'N' : 'S',
				       ABS(intlat),
				       (intlon >= 0) ? 'E' : 'W',
				       ABS(intlon));

	static DownloadFileOptions options = { false, false, NULL, 0, a_check_map_file, NULL, NULL };
	DownloadResult_t result = a_http_download_get_url(SRTM_HTTP_SITE, src_fn, p->dest.c_str(), &options, NULL);
	switch (result) {
	case DOWNLOAD_CONTENT_ERROR:
	case DOWNLOAD_HTTP_ERROR: {
		char *msg = g_strdup_printf(_("DEM download failure for %f, %f"), p->lat, p->lon);
		p->layer->get_window()->statusbar_update(msg, VIK_STATUSBAR_INFO);
		free(msg);
		break;
	}
	case DOWNLOAD_FILE_WRITE_ERROR: {
		char *msg = g_strdup_printf(_("DEM write failure for %s"), p->dest.c_str());
		p->layer->get_window()->statusbar_update(msg, VIK_STATUSBAR_INFO);
		free(msg);
		break;
	}
	case DOWNLOAD_SUCCESS:
	case DOWNLOAD_NOT_REQUIRED:
	default:
		break;
	}
	free(src_fn);
}




static char *srtm_lat_lon_to_dest_fn(double lat, double lon)
{
	int intlat, intlon;
	const char *continent_dir;

	intlat = (int)floor(lat);
	intlon = (int)floor(lon);
	continent_dir = srtm_continent_dir(intlat, intlon);

	if (!continent_dir) {
		continent_dir = "nowhere";
	}

	return g_strdup_printf("srtm3-%s%s%c%02d%c%03d.hgt.zip",
			       continent_dir,
			       G_DIR_SEPARATOR_S,
			       (intlat >= 0) ? 'N' : 'S',
			       ABS(intlat),
			       (intlon >= 0) ? 'E' : 'W',
			       ABS(intlon));

}




/* TODO: generalize. */
static void srtm_draw_existence(Viewport * viewport)
{
	char buf[strlen(MAPS_CACHE_DIR)+strlen(SRTM_CACHE_TEMPLATE)+30];

	LatLonBBox bbox;
	viewport->get_bbox(&bbox);

	for (int i = floor(bbox.south); i <= floor(bbox.north); i++) {
		for (int j = floor(bbox.west); j <= floor(bbox.east); j++) {
			const char *continent_dir;
			if ((continent_dir = srtm_continent_dir(i, j)) == NULL) {
				continue;
			}

			snprintf(buf, sizeof(buf), SRTM_CACHE_TEMPLATE,
				 MAPS_CACHE_DIR,
				 continent_dir,
				 G_DIR_SEPARATOR_S,
				 (i >= 0) ? 'N' : 'S',
				 ABS(i),
				 (j >= 0) ? 'E' : 'W',
				 ABS(j));
			if (0 == access(buf, F_OK)) {
				Coord ne, sw;
				int x1, y1, x2, y2;
				sw.north_south = i;
				sw.east_west = j;
				sw.mode = VIK_COORD_LATLON;
				ne.north_south = i+1;
				ne.east_west = j+1;
				ne.mode = VIK_COORD_LATLON;
				viewport->coord_to_screen(&sw, &x1, &y1);
				viewport->coord_to_screen(&ne, &x2, &y2);

				if (x1 < 0) {
					x1 = 0;
				}

				if (y2 < 0) {
					y2 = 0;
				}

				viewport->draw_rectangle(gtk_widget_get_style(viewport->get_toolkit_widget())->black_gc,
							 false, x1, y2, x2-x1, y1-y2);
			}
		}
	}
}




/**************************************************
 *  SOURCE: USGS 24K                              *
 **************************************************/

#ifdef VIK_CONFIG_DEM24K

static void dem24k_dem_download_thread(DEMDownloadParams * p, void * threaddata)
{
	/* TODO: dest dir. */
	char *cmdline = g_strdup_printf("%s %.03f %.03f",
					DEM24K_DOWNLOAD_SCRIPT,
					floor(p->lat*8)/8,
					ceil(p->lon*8)/8);
	/* FIXME: don't use system, use execv or something. check for existence. */
	system(cmdline);
	free(cmdline);
}




static char *dem24k_lat_lon_to_dest_fn(double lat, double lon)
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
	char buf[strlen(MAPS_CACHE_DIR)+40];
	double i, j;

	viewport->get_min_max_lat_lon(&min_lat, &max_lat, &min_lon, &max_lon);

	for (i = floor(min_lat*8)/8; i <= floor(max_lat*8)/8; i+=0.125) {
		/* Check lat dir first -- faster. */
		snprintf(buf, sizeof(buf), "%sdem24k/%d/", MAPS_CACHE_DIR, (int) i);

		if (0 != access(buf, F_OK)) {
			continue;
		}

		for (j = floor(min_lon*8)/8; j <= floor(max_lon*8)/8; j+=0.125) {
			/* Check lon dir first -- faster. */
			snprintf(buf, sizeof(buf), "%sdem24k/%d/%d/", MAPS_CACHE_DIR, (int) i, (int) j);
			if (0 != access(buf, F_OK)) {
				continue;
			}

			snprintf(buf, sizeof(buf), "%sdem24k/%d/%d/%.03f,%.03f.dem",
				 MAPS_CACHE_DIR,
				 (int) i,
				 (int) j,
				 floor(i*8)/8,
				 floor(j*8)/8);

			if (0 == access(buf, F_OK)) {
				Coord ne, sw;
				int x1, y1, x2, y2;
				sw.north_south = i;
				sw.east_west = j-0.125;
				sw.mode = VIK_COORD_LATLON;
				ne.north_south = i+0.125;
				ne.east_west = j;
				ne.mode = VIK_COORD_LATLON;
				viewport->coord_to_screen(&sw, &x1, &y1);
				viewport->coord_to_screen(&ne, &x2, &y2);

				if (x1 < 0) {
					x1 = 0;
				}

				if (y2 < 0) {
					y2 = 0;
				}

				viewport->draw_rectangle(gtk_widget_get_style(GTK_WIDGET(viewport->vvp))->black_gc,
							 false, x1, y2, x2-x1, y1-y2);
			}
		}
	}
}
#endif




/**************************************************
 *   SOURCES -- DOWNLOADING & IMPORTING TOOL      *
 ***************************************************/




void LayerDEM::weak_ref_cb(void * ptr, GObject * dead_vdl)
{
	DEMDownloadParams * p = (DEMDownloadParams *) ptr;
	g_mutex_lock(p->mutex);
	p->layer = NULL;
	g_mutex_unlock(p->mutex);
}

/* Try to add file full_path.
 * filename will be copied.
 * Returns false if file does not exists, true otherwise.
 */
bool LayerDEM::add_file(std::string& dem_filename)
{
	if (0 == access(dem_filename.c_str(), F_OK)) {
		/* Only load if file size is not 0 (not in progress). */
		GStatBuf sb;
		(void) stat(dem_filename.c_str(), &sb);
		if (sb.st_size) {
			std::string * duped_path = new std::string(dem_filename);
			this->files->push_front((char *) duped_path->c_str());
			std::string dem_fullpath = std::string(*duped_path);
			dem_cache_load(dem_fullpath);
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, duped_path->c_str());
		}
		return true;
	} else {
		return false;
	}
}




static void dem_download_thread(DEMDownloadParams * p, void * threaddata)
{
	if (p->source == DEM_SOURCE_SRTM) {
		srtm_dem_download_thread(p, threaddata);
#ifdef VIK_CONFIG_DEM24K
	} else if (p->source == DEM_SOURCE_DEM24K) {
		dem24k_dem_download_thread(p, threaddata);
#endif
	} else {
		return;
	}

	//gdk_threads_enter();
	g_mutex_lock(p->mutex);
	if (p->layer) {
		p->layer->weak_unref(LayerDEM::weak_ref_cb, p);

		if (p->layer->add_file(p->dest)) {
			p->layer->emit_update(); /* NB update from background thread. */
		}
	}
	g_mutex_unlock(p->mutex);
	//gdk_threads_leave();
}




static void free_dem_download_params(DEMDownloadParams * p)
{
	delete p;
}




static LayerTool * dem_layer_download_create(Window * window, Viewport * viewport)
{
	LayerTool * layer_tool = new LayerTool(window, viewport, LayerType::DEM);

	dem_tools[0] = layer_tool;

	layer_tool->layer_type = LayerType::DEM;

	layer_tool->radioActionEntry.name        = strdup("DEMDownload");
	layer_tool->radioActionEntry.stock_id    = strdup("vik-icon-DEM Download");
	layer_tool->radioActionEntry.label       = strdup(N_("_DEM Download"));
	layer_tool->radioActionEntry.accelerator = NULL;
	layer_tool->radioActionEntry.tooltip     = strdup(N_("DEM Download"));
	layer_tool->radioActionEntry.value       = 0;

	layer_tool->click = (VikToolMouseFunc) dem_layer_download_click;
	layer_tool->release = (VikToolMouseFunc) dem_layer_download_release;

	layer_tool->cursor_type = GDK_CURSOR_IS_PIXMAP;
	layer_tool->cursor_data = &cursor_demdl_pixbuf;

	return layer_tool;
}




/**
 * Display a simple dialog with information about the DEM file at this location.
 */
static void dem_layer_file_info(GtkWidget *widget, struct LatLon *ll)
{
	int intlat = (int)floor(ll->lat);
	int intlon = (int)floor(ll->lon);
	const char * continent_dir = srtm_continent_dir(intlat, intlon);

	char * source = NULL;
	if (continent_dir) {
		source = g_strdup_printf("http://%s%s%s/%c%02d%c%03d.hgt.zip",
					   SRTM_HTTP_SITE,
					   SRTM_HTTP_URI,
					   continent_dir,
					   (intlat >= 0) ? 'N' : 'S',
					   ABS(intlat),
					   (intlon >= 0) ? 'E' : 'W',
					   ABS(intlon));
	} else {
		/* Probably not over any land... */
		source = strdup(_("No DEM File Available"));
	}

#ifdef VIK_CONFIG_DEM24K
	char * dem_file = dem24k_lat_lon_to_dest_fn(ll->lat, ll->lon);
#else
	char * dem_file = srtm_lat_lon_to_dest_fn(ll->lat, ll->lon);
#endif
	char * message = NULL;
	char * filename = g_strdup_printf("%s%s", MAPS_CACHE_DIR, dem_file);

	if (0 == access(filename, F_OK)) {
		/* Get some timestamp information of the file. */
		GStatBuf stat_buf;
		if (g_stat(filename, &stat_buf) == 0) {
			char time_buf[64];
			strftime(time_buf, sizeof(time_buf), "%c", gmtime((const time_t *)&stat_buf.st_mtime));
			message = g_strdup_printf(_("\nSource: %s\n\nDEM File: %s\nDEM File Timestamp: %s"), source, filename, time_buf);
		}
	} else {
		message = g_strdup_printf(_("Source: %s\n\nNo DEM File!"), source);
	}

	/* Show the info. */
	a_dialog_info_msg(GTK_WINDOW(gtk_widget_get_toplevel(widget)), message);

	free(message);
	free(source);
	free(dem_file);
	free(filename);
}




static bool dem_layer_download_release(Layer * vdl, GdkEventButton * event, LayerTool * tool)
{
	return ((LayerDEM *) vdl)->download_release(event, tool);
}




bool LayerDEM::download_release(GdkEventButton * event, LayerTool * tool)
{
	Coord coord;
	static struct LatLon ll;

	tool->viewport->screen_to_coord(event->x, event->y, &coord);
	vik_coord_to_latlon(&coord, &ll);


	char * dem_file = NULL;
	if (this->source == DEM_SOURCE_SRTM) {
		dem_file = srtm_lat_lon_to_dest_fn(ll.lat, ll.lon);
#ifdef VIK_CONFIG_DEM24K
	} else if (this->source == DEM_SOURCE_DEM24K) {
		dem_file = dem24k_lat_lon_to_dest_fn(ll.lat, ll.lon);
#endif
	}

	if (!dem_file) {
		return true;
	}

	if (event->button == MouseButton::LEFT) {
		std::string dem_full_path = std::string(MAPS_CACHE_DIR_2 + std::string(dem_file));
		fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, dem_full_path.c_str());

		/* TODO: check if already in filelist. */
		if (!this->add_file(dem_full_path)) {
			char * tmp = g_strdup_printf(_("Downloading DEM %s"), dem_file);
			DEMDownloadParams * p = new DEMDownloadParams(dem_full_path, &ll, this);

			a_background_thread(BACKGROUND_POOL_REMOTE,
					    this->get_toolkit_window(), tmp,
					    (vik_thr_func) dem_download_thread, p,
					    (vik_thr_free_func) free_dem_download_params, NULL, 1);

			free(tmp);
		} else {
			this->emit_update();
		}
	} else {
		if (!this->right_click_menu) {
			this->right_click_menu = GTK_MENU (gtk_menu_new());

			GtkWidget * item = gtk_image_menu_item_new_with_mnemonic(_("_Show DEM File Information"));
			gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INFO, GTK_ICON_SIZE_MENU));
			g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(dem_layer_file_info), &ll);
			gtk_menu_shell_append(GTK_MENU_SHELL(this->right_click_menu), item);
		}

		gtk_menu_popup(this->right_click_menu, NULL, NULL, NULL, NULL, event->button, event->time);
		gtk_widget_show_all(GTK_WIDGET(this->right_click_menu));
	}

	free(dem_file);

	return true;
}




static bool dem_layer_download_click(Layer * vdl, GdkEventButton * event, LayerTool * tool)
{
	/* Choose & keep track of cache dir.
	 * Download in background thread.
	 * Download over area. */
	return true;
}




LayerDEM::LayerDEM()
{
	this->files = new std::list<char *>;
	this->type = LayerType::DEM;
	this->dem_type = 0;
	strcpy(this->type_string, "DEM");
}




LayerDEM::LayerDEM(Viewport * viewport) : LayerDEM()
{
	this->gcs = (GdkGC **) malloc(sizeof(GdkGC *) * DEM_N_HEIGHT_COLORS);
	this->gcsgradient = (GdkGC **) malloc(sizeof(GdkGC *) * DEM_N_GRADIENT_COLORS);
	/* Make new gcs only if we need it (copy layer -> use old). */

	/* Ensure the base GC is available so the default colour can be applied. */
	if (viewport) {
		this->gcs[0] = viewport->new_gc("#0000FF", 1);
	}

	this->set_defaults(viewport);


	if (viewport) {
		/* TODO: share GCS between layers. */
		for (unsigned int i = 0; i < DEM_N_HEIGHT_COLORS; i++) {
			if (i > 0) {
				this->gcs[i] = viewport->new_gc(dem_height_colors[i], UNUSED_LINE_THICKNESS);
			}
		}

		for (unsigned int i = 0; i < DEM_N_GRADIENT_COLORS; i++) {
				this->gcsgradient[i] = viewport->new_gc(dem_gradient_colors[i], UNUSED_LINE_THICKNESS);
		}
	}
}
