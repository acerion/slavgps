/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2008, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
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

#include <unordered_map>
#include <string>

#include <stdlib.h>

#include "dems.h"
#include "background.h"

typedef struct {
	VikDEM * dem;
	unsigned int ref_count;
} LoadedDEM;

/* filename -> DEM */
static std::unordered_map<std::string, LoadedDEM *> loaded_dems;


static void loaded_dem_free(LoadedDEM *ldem)
{
	vik_dem_free(ldem->dem);
	free(ldem);
}

void a_dems_uninit()
{
	if (loaded_dems.size()) {
		loaded_dems.clear();
	}
}


/* Called when DEM tile clicked in DEM layer is available on disc.
   The time may been sitting on disc before, or may have been just
   downloaded - the function gets called just the same.

   . area clicking in DEM layer on area already downloaded to disc. */
/* To load a dem. if it was already loaded, will simply
 * reference the one already loaded and return it.
 */
VikDEM * a_dems_load(std::string& filename)
{
	LoadedDEM *ldem;

#if 0
	/* dems init hash table */
	if (!loaded_dems) {
		loaded_dems = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) loaded_dem_free);
	}
#endif

	auto iter = loaded_dems.find(filename);
	if (iter != loaded_dems.end()) { /* Found. */
		(*iter).second->ref_count++;
		return (*iter).second->dem;
	} else {
		VikDEM * dem = vik_dem_new_from_file(filename.c_str());
		if (!dem) {
			return NULL;
		}
		ldem = (LoadedDEM *) malloc(sizeof (LoadedDEM));
		ldem->ref_count = 1;
		ldem->dem = dem;
		loaded_dems[filename] = ldem;
		return dem;
	}
}

void a_dems_unref(std::string& filename)
{
	auto iter = loaded_dems.find(filename);
	if (iter == loaded_dems.end()) {
		/* This is fine - probably means the loaded list was aborted / not completed for some reason */
		return;
	}
	(*iter).second->ref_count--;
	if ((*iter).second->ref_count == 0) {
		loaded_dems.erase(iter);
	}
}

/* Probably gets called whenever DEM layer is moved in viewport.
   Probably called with tile names that are - or can be - in current viewport.

/* to get a DEM that was already loaded.
 * assumes that its in there already,
 * although it could not be if earlier load failed.
 */
VikDEM *a_dems_get(std::string& filename)
{
	auto iter = loaded_dems.find(filename);
	if (iter != loaded_dems.end()) {
		return (*iter).second->dem;
	}
	return NULL;
}


/* Load a string list (GList of strings) of dems. You have to use get to at them later.
 * When updating a list as a parameter, this should be bfore freeing the list so
 * the same DEMs won't be loaded & unloaded.
 * Modifies the list to remove DEMs which did not load.
 */

/* TODO: don't delete them when they don't exist.
 * we need to warn the user, but we should keep them in the GList.
 * we need to know that they weren't referenced though when we
 * do the a_dems_list_free().
 */
int a_dems_load_list(std::list<std::string>& filenames, void * threaddata)
{
	auto iter = filenames.begin();
	unsigned int dem_count = 0;
	const unsigned int dem_total = filenames.size();
	while (iter != filenames.end()) {
		std::string dem_filename = *iter;
		if (!a_dems_load(dem_filename)) {
			iter = filenames.erase(iter);
		} else {
			iter++;
		}
		/* When running a thread - inform of progress. */
		if (threaddata) {
			dem_count++;
			/* NB Progress also detects abort request via the returned value */
			int result = a_background_thread_progress(threaddata, ((double) dem_count) / dem_total);
			if (result != 0) {
				return -1; /* Abort thread */
			}
		}
	}
	return 0;
}

/* Takes a string list of dem filenames .
 * Unrefs all the dems (i.e. "unloads" them), then frees the
 * strings, the frees the list.
 */
void a_dems_list_free(std::list<std::string>& filenames)
{
	for (auto iter = filenames.begin(); iter != filenames.end(); iter++) {
		a_dems_unref(*iter);
		/* kamilTODO: "delete (*iter)" ? */
	}

	filenames.clear();
}

#if 0
GList * a_dems_list_copy(GList * dems)
{
	GList * rv = g_list_copy(dems);
	GList * iter = rv;
	while (iter) {
		std::string dem_filename = std::string((const char *) (iter->data));
		if (! a_dems_load(dem_filename)) {
			GList *iter_temp = iter->next; /* delete link, don't bother strdup'ing and free'ing string */
			rv = g_list_remove_link(rv, iter);
			iter = iter_temp;
		} else {
			iter->data = g_strdup((char *)iter->data); /* copy the string too. */
			iter = iter->next;
		}
	}
	return rv;
}

int16_t a_dems_list_get_elev_by_coord(std::list<std::string> * filenames, const VikCoord * coord)
{
	static struct UTM utm_tmp;
	static struct LatLon ll_tmp;
	auto iter = filenames->begin();
	VikDEM * dem;
	int elev;

	while (iter != filenames->end()) {
		dem = a_dems_get(*iter);
		if (dem) {
			if (dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS) {
				vik_coord_to_latlon(coord, &ll_tmp);
				ll_tmp.lat *= 3600;
				ll_tmp.lon *= 3600;
				elev = vik_dem_get_east_north(dem, ll_tmp.lon, ll_tmp.lat);
				if (elev != VIK_DEM_INVALID_ELEVATION) {
					return elev;
				}
			} else if (dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {
				vik_coord_to_utm(coord, &utm_tmp);
				if (utm_tmp.zone == dem->utm_zone
				    && (elev = vik_dem_get_east_north(dem, utm_tmp.easting, utm_tmp.northing)) != VIK_DEM_INVALID_ELEVATION) {

					return elev;
				}
			}
		}
		iter++;
	}
	return VIK_DEM_INVALID_ELEVATION;
}
#endif

typedef struct {
	const VikCoord * coord;
	VikDemInterpol method;
	int elev;
} CoordElev;

static bool get_elev_by_coord(std::string key, LoadedDEM * ldem, CoordElev * ce)
{
	VikDEM * dem = ldem->dem;
	double lat, lon;

	if (dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS) {
		struct LatLon ll_tmp;
		vik_coord_to_latlon(ce->coord, &ll_tmp);
		lat = ll_tmp.lat * 3600;
		lon = ll_tmp.lon * 3600;
	} else if (dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {
		static struct UTM utm_tmp;
		if (utm_tmp.zone != dem->utm_zone) {
			return false;
		}
		vik_coord_to_utm(ce->coord, &utm_tmp);
		lat = utm_tmp.northing;
		lon = utm_tmp.easting;
	} else {
		return false;
	}

	switch (ce->method) {
	case VIK_DEM_INTERPOL_NONE:
		ce->elev = vik_dem_get_east_north(dem, lon, lat);
		break;
	case VIK_DEM_INTERPOL_SIMPLE:
		ce->elev = vik_dem_get_simple_interpol(dem, lon, lat);
		break;
	case VIK_DEM_INTERPOL_BEST:
		ce->elev = vik_dem_get_shepard_interpol(dem, lon, lat);
		break;
	default: break;
	}
	return (ce->elev != VIK_DEM_INVALID_ELEVATION);
}

/* TODO: keep a (sorted) linked list of DEMs and select the best resolution one */
int16_t a_dems_get_elev_by_coord(const VikCoord *coord, VikDemInterpol method)
{
	CoordElev ce;

	if (loaded_dems.empty()) {
		return VIK_DEM_INVALID_ELEVATION;
	}

	ce.coord = coord;
	ce.method = method;
	ce.elev = VIK_DEM_INVALID_ELEVATION;

	for (auto iter = loaded_dems.begin(); iter != loaded_dems.end(); ++iter) {
		if (get_elev_by_coord((*iter).first, (*iter).second, &ce)) {
			return ce.elev;
		}
	}

	return VIK_DEM_INVALID_ELEVATION;
}
