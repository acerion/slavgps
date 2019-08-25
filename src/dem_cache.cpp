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
 */




#include <unordered_map>
#include <cstdlib>




#include "dem_cache.h"
#include "background.h"




using namespace SlavGPS;




class LoadedDEM {
public:
	LoadedDEM(DEM * dem);
	~LoadedDEM();

	DEM * dem = NULL;
	unsigned int ref_count = 0;
};





typedef struct {
	const Coord * coord;
	DemInterpolation method;
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




/* File path -> DEM. */
static std::unordered_map<QString, LoadedDEM *, MyQHasher> loaded_dems;




static void dem_cache_unref(const QString & file_path);
static bool calculate_elev_by_coord(LoadedDEM * ldem, CoordElev * ce);




LoadedDEM::LoadedDEM(DEM * new_dem)
{
	this->dem = new_dem;
	this->ref_count++;
}




LoadedDEM::~LoadedDEM()
{
	delete this->dem;
}




void DEMCache::uninit(void)
{
	for (auto iter = loaded_dems.begin(); iter != loaded_dems.end(); iter++) {
		delete (*iter).second;
	}
	loaded_dems.clear();
}




/**
   \brief Load a DEM tile from given file, return it

   Read a DEM tile into DEM object, add the object to cache, return
   the object.

   If the object has already been loaded before, reading the tile and
   adding the object to cache is skipped.

   Called when DEM tile clicked in DEM layer is available on disc.
   The tile may been sitting on disc before, or may have been just
   downloaded - the function gets called just the same.

   \param file_full_path: path to data file with tile data

   \return DEM object representing a tile
*/
DEM * DEMCache::load_file_into_cache(const QString & file_full_path)
{
	auto iter = loaded_dems.find(file_full_path);
	if (iter != loaded_dems.end()) { /* Found. */
		(*iter).second->ref_count++;
		return (*iter).second->dem;
	} else {
		DEM * dem = new DEM();
		if (!dem->read_from_file(file_full_path)) {
			delete dem;
			return NULL;
		}
		LoadedDEM * ldem = new LoadedDEM(dem);
		loaded_dems[file_full_path] = ldem;
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
		delete (*iter).second;
	}
}




/* Probably gets called whenever DEM layer is moved in viewport.
   Probably called with tile names that are - or can be - in current viewport. */

/* To get a DEM that was already loaded.
 * Assumes that its in there already,
 * although it could not be if earlier load failed.
 */
DEM * DEMCache::get(const QString & file_path)
{
	auto iter = loaded_dems.find(file_path);
	if (iter != loaded_dems.end()) {
		return (*iter).second->dem;
	}
	return NULL;
}




/**
   \brief Unload DEMs specified by given file paths

   The list itself is not modified.

   \param file_paths: list of DEMs (specified by paths to DEM data files) to be unloaded from cache
*/
void DEMCache::unload_from_cache(QStringList & file_paths)
{
	for (int i = 0; i < file_paths.size(); i++) {
		dem_cache_unref(file_paths.at(i));
	}
}




static bool calculate_elev_by_coord(LoadedDEM * ldem, CoordElev * ce)
{
	DEM * dem = ldem->dem;
	double lat, lon;

	if (dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS) {
		const LatLon ll_tmp = ce->coord->get_lat_lon();
		lat = ll_tmp.lat * 3600;
		lon = ll_tmp.lon * 3600;
	} else if (dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {
		static UTM utm_tmp;
		if (!UTM::is_the_same_zone(utm_tmp, dem->utm)) {
			return false;
		}
		utm_tmp = ce->coord->get_utm();
		lat = utm_tmp.get_northing();
		lon = utm_tmp.get_easting();
	} else {
		return false;
	}

	switch (ce->method) {
	case DemInterpolation::None:
		ce->elev = dem->get_east_north_no_interpolation(lon, lat);
		break;
	case DemInterpolation::Simple:
		ce->elev = dem->get_east_north_simple_interpolation(lon, lat);
		break;
	case DemInterpolation::Best:
		ce->elev = dem->get_east_north_shepard_interpolation(lon, lat);
		break;
	default: break;
	}
	return (ce->elev != DEM_INVALID_ELEVATION);
}




/* TODO_2_LATER: keep a (sorted) linked list of DEMs and select the best resolution one. */
Altitude DEMCache::get_elev_by_coord(const Coord & coord, DemInterpolation method)
{
	Altitude result; /* Invalid by default. */

	if (loaded_dems.empty()) {
		result;
	}

	CoordElev ce;
	ce.coord = &coord;
	ce.method = method;
	ce.elev = DEM_INVALID_ELEVATION;

	for (auto iter = loaded_dems.begin(); iter != loaded_dems.end(); ++iter) {
		if (calculate_elev_by_coord((*iter).second, &ce)) {
			result = Altitude(ce.elev, HeightUnit::Metres); /* This is DEM, so meters. */
			break;
		}
	}

	return result;
}




#ifdef K_OLD_IMPLEMENTATION




static GList * a_dems_list_copy(GList * dems);
static int16_t a_dems_list_get_elev_by_coord(GList * dems, const Coord * coord);




GList * a_dems_list_copy(GList * dems)
{
	GList * rv = g_list_copy(dems);
	GList * iter = rv;
	while (iter) {
		std::string dem_file_full_path = std::string((const char *) (iter->data));
		if (!DEMCache::load_file_into_cache(dem_file_full_path)) {
			GList * iter_temp = iter->next; /* Delete link, don't bother strdup'ing and free'ing string. */
			rv = g_list_remove_link(rv, iter);
			iter = iter_temp;
		} else {
			iter->data = g_strdup((char *)iter->data); /* Copy the string too. */
			iter = iter->next;
		}
	}
	return rv;
}




int16_t a_dems_list_get_elev_by_coord(std::list<QString> & file_paths, const Coord * coord)
{
	static UTM utm_tmp;
	LatLon ll_tmp;
	auto iter = file_paths->begin();
	DEM * dem;
	int elev;

	while (iter != file_paths->end()) {
		dem = DEMCache::get(*iter);
		if (dem) {
			if (dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS) {
				ll_tmp = coord->get_latlon();
				ll_tmp.lat *= 3600;
				ll_tmp.lon *= 3600;
				elev = dem->get_east_north_no_interpolation(ll_tmp.lon, ll_tmp.lat);
				if (elev != DEM_INVALID_ELEVATION) {
					return elev;
				}
			} else if (dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {
				utm_tmp = coord->get_utm();
				if (utm_tmp.zone == dem->utm.zone
				    && (elev = dem->get_east_north_no_interpolation(utm_tmp.get_easting(), utm_tmp.get_northing())) != DEM_INVALID_ELEVATION) {

					return elev;
				}
			}
		}
		iter++;
	}
	return DEM_INVALID_ELEVATION;
}




#endif
