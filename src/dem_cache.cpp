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
#include "dem_srtm.h"
#include "background.h"




using namespace SlavGPS;




#define SG_MODULE "DEM Cache"




class LoadedDEM { /* TODO_LATER: to be replaced with smart pointer. */
public:
	LoadedDEM(DEM * dem);
	~LoadedDEM();

	DEM * dem = NULL;
	unsigned int ref_count = 0;
};




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
		DEM * dem = nullptr;

		const DEMSource source = DEM::recognize_source_type(file_full_path);
		switch (source) {
		case DEMSource::SRTM:
			dem = new DEMSRTM();
			break;
#ifdef VIK_CONFIG_DEM24K
		case DEMSource::DEM24k:
			dem = new DEM24K();
			break;
#endif
		default:
			dem = nullptr;
			break;
		};

		if (nullptr == dem) {
			return nullptr;
		}

		if (sg_ret::ok != dem->read_from_file(file_full_path)) {
			delete dem;
			return nullptr;
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




/* TODO_MAYBE: keep a (sorted) linked list of DEMs and select the best resolution one. */
Altitude DEMCache::get_elev_by_coord(const Coord & coord, DEMInterpolation method)
{
	Altitude result; /* Invalid by default. */

	if (loaded_dems.empty()) {
		return result;
	}

	for (auto iter = loaded_dems.begin(); iter != loaded_dems.end(); ++iter) {
		int16_t elev = DEM::invalid_elevation;

		if (sg_ret::ok != (*iter).second->dem->get_elev_by_coord(coord, method, elev)) {
			/* Some logic error that is certain to repeat
			   in next iteration. */
			qDebug() << SG_PREFIX_E << "Can't find elevation by coordinates";
			break;
		}
		if (DEM::invalid_elevation == elev) {
			/* These coordinates aren't covered by this
			   DEM, try next DEM. */
			continue;
		}

		result = Altitude(elev, AltitudeType::Unit::E::Metres); /* This is DEM, so meters. */
		break;
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
	auto iter = file_paths->begin();

	while (iter != file_paths->end()) {
		DEM * dem = DEMCache::get(*iter);
		if (dem) {
			if (dem->horiz_units == DEMHorizontalUnit::LatLonArcSeconds) {
				LatLon lat_lon = coord->get_latlon();
				lat_lon.lat *= 3600;
				lat_lon.lon *= 3600;
				int16_t elev = dem->get_elev_at_east_north_no_interpolation(lat_lon.lon, lat_lon.lat);
				if (elev != DEM::invalid_elevation) {
					return elev;
				}
			} else if (dem->horiz_units == DEMHorizontalUnit::UTMMeters) {
				int16_t elev = 0;
				utm_tmp = coord->get_utm();
				if (utm_tmp.zone == dem->utm.zone
				    && (elev = dem->get_elev_at_east_north_no_interpolation(utm_tmp.get_easting(), utm_tmp.get_northing())) != DEM::invalid_elevation) {

					return elev;
				}
			}
		}
		iter++;
	}
	return DEM::invalid_elevation;
}




#endif
