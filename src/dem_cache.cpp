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
#include <cstdlib>

#include "dem_cache.h"
#include "background.h"




using namespace SlavGPS;




typedef struct {
	DEM * dem;
	unsigned int ref_count;
} LoadedDEM;

typedef struct {
	const Coord * coord;
	VikDemInterpol method;
	int elev;
} CoordElev;




struct MyQHasher
{
	std::size_t operator()(const QString & s) const {
		using std::hash;
		using std::string;

		return hash<string>()(s.toUtf8().constData());
	}
};


/* Filename -> DEM. */
static std::unordered_map<QString, LoadedDEM *, MyQHasher> loaded_dems;




static void dem_cache_unref(const QString & file_path);
static bool get_elev_by_coord(LoadedDEM * ldem, CoordElev * ce);
//static GList * a_dems_list_copy(GList * dems);
//static int16_t a_dems_list_get_elev_by_coord(GList * dems, const Coord * coord);




static void loaded_dem_free(LoadedDEM *ldem)
{
	delete ldem->dem;
	free(ldem);
}




void SlavGPS::dem_cache_uninit()
{
	if (loaded_dems.size()) {
		loaded_dems.clear();
	}
}





/* Called when DEM tile clicked in DEM layer is available on disc.
   The timl may been sitting on disc before, or may have been just
   downloaded - the function gets called just the same. */
/* To load a dem. if it was already loaded, will simply
 * reference the one already loaded and return it.
 */
DEM * SlavGPS::dem_cache_load(const QString & file_path)
{
	auto iter = loaded_dems.find(file_path);
	if (iter != loaded_dems.end()) { /* Found. */
		(*iter).second->ref_count++;
		return (*iter).second->dem;
	} else {
		DEM * dem = new DEM();
		if (!dem->read(file_path)) {
			delete dem;
			return NULL;
		}
		LoadedDEM * ldem = (LoadedDEM *) malloc(sizeof (LoadedDEM));
		ldem->ref_count = 1;
		ldem->dem = dem;
		loaded_dems[file_path] = ldem;
		return dem;
	}
}




static void dem_cache_unref(const QString & file_path)
{
	auto iter = loaded_dems.find(file_path);
	if (iter == loaded_dems.end()) {
		/* This is fine - probably means the loaded list was aborted / not completed for some reason. */
		return;
	}
	(*iter).second->ref_count--;
	if ((*iter).second->ref_count == 0) {
		loaded_dems.erase(iter);
	}
}





/* Probably gets called whenever DEM layer is moved in viewport.
   Probably called with tile names that are - or can be - in current viewport. */

/* To get a DEM that was already loaded.
 * Assumes that its in there already,
 * although it could not be if earlier load failed.
 */
DEM * SlavGPS::dem_cache_get(const QString & file_path)
{
	auto iter = loaded_dems.find(file_path);
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
 * We need to warn the user, but we should keep them in the GList.
 * We need to know that they weren't referenced though when we
 * do the dem_cache_list_free().
 */
int SlavGPS::dem_cache_load_list(std::list<QString> & filenames, BackgroundJob * bg_job)
{
	auto iter = filenames.begin();
	unsigned int dem_count = 0;
	const unsigned int dem_total = filenames.size();
	while (iter != filenames.end()) {
		QString dem_filename = *iter;
		if (!dem_cache_load(dem_filename)) {
			iter = filenames.erase(iter);
		} else {
			iter++;
		}
		/* When running a thread - inform of progress. */
		if (bg_job) {
#if 1
			dem_count++;
			/* NB Progress also detects abort request via the returned value. */
			int result = a_background_thread_progress(bg_job, ((double) dem_count) / dem_total);
			if (result != 0) {
				return -1; /* Abort thread. */
			}
#endif
		}
	}
	return 0;
}




/* Takes a string list of dem filenames.
 * Unrefs all the dems (i.e. "unloads" them), then frees the
 * strings, the frees the list.
 */
void SlavGPS::dem_cache_list_free(std::list<QString>& filenames)
{
	for (auto iter = filenames.begin(); iter != filenames.end(); iter++) {
		dem_cache_unref(*iter);
		/* kamilTODO: "delete (*iter)" ? */
	}

	filenames.clear();
}




static bool get_elev_by_coord(LoadedDEM * ldem, CoordElev * ce)
{
	DEM * dem = ldem->dem;
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
		ce->elev = dem->get_east_north(lon, lat);
		break;
	case VIK_DEM_INTERPOL_SIMPLE:
		ce->elev = dem->get_simple_interpol(lon, lat);
		break;
	case VIK_DEM_INTERPOL_BEST:
		ce->elev = dem->get_shepard_interpol(lon, lat);
		break;
	default: break;
	}
	return (ce->elev != VIK_DEM_INVALID_ELEVATION);
}




/* TODO: keep a (sorted) linked list of DEMs and select the best resolution one. */
int16_t SlavGPS::dem_cache_get_elev_by_coord(const Coord * coord, VikDemInterpol method)
{
	if (loaded_dems.empty()) {
		return VIK_DEM_INVALID_ELEVATION;
	}

	CoordElev ce;
	ce.coord = coord;
	ce.method = method;
	ce.elev = VIK_DEM_INVALID_ELEVATION;

	for (auto iter = loaded_dems.begin(); iter != loaded_dems.end(); ++iter) {
		if (get_elev_by_coord((*iter).second, &ce)) {
			return ce.elev;
		}
	}

	return VIK_DEM_INVALID_ELEVATION;
}





#if 0
GList * a_dems_list_copy(GList * dems)
{
	GList * rv = g_list_copy(dems);
	GList * iter = rv;
	while (iter) {
		std::string dem_filename = std::string((const char *) (iter->data));
		if (!dem_cache_load(dem_filename)) {
			GList *iter_temp = iter->next; /* Delete link, don't bother strdup'ing and free'ing string. */
			rv = g_list_remove_link(rv, iter);
			iter = iter_temp;
		} else {
			iter->data = g_strdup((char *)iter->data); /* Copy the string too. */
			iter = iter->next;
		}
	}
	return rv;
}




int16_t a_dems_list_get_elev_by_coord(std::list<std::string> * filenames, const Coord * coord)
{
	static struct UTM utm_tmp;
	static struct LatLon ll_tmp;
	auto iter = filenames->begin();
	DEM * dem;
	int elev;

	while (iter != filenames->end()) {
		dem = dem_cache_get(*iter);
		if (dem) {
			if (dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS) {
				vik_coord_to_latlon(coord, &ll_tmp);
				ll_tmp.lat *= 3600;
				ll_tmp.lon *= 3600;
				elev = dem->get_east_north(ll_tmp.lon, ll_tmp.lat);
				if (elev != VIK_DEM_INVALID_ELEVATION) {
					return elev;
				}
			} else if (dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {
				vik_coord_to_utm(coord, &utm_tmp);
				if (utm_tmp.zone == dem->utm_zone
				    && (elev = dem->get_east_north(utm_tmp.easting, utm_tmp.northing)) != VIK_DEM_INVALID_ELEVATION) {

					return elev;
				}
			}
		}
		iter++;
	}
	return VIK_DEM_INVALID_ELEVATION;
}





#endif
