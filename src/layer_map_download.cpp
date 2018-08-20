/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Evan Battaglia <viking@greentorch.org>
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (c) 2013, Rob Norris <rw_norris@hotmail.com>
 * UTM multi-zone stuff by Kit Transue <notlostyet@didactek.com>
 * Dynamic map type by Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#include <unistd.h>




#include <QDebug>
#include <QDir>




#include "layer_map_download.h"
#include "window.h"
#include "map_source.h"
#include "layer_map.h"
#include "globals.h"
#include "statusbar.h"




using namespace SlavGPS;




#define SG_MODULE "Map Download Job"




/*
  Download tiles between @ulm and @brm
*/
MapDownloadJob::MapDownloadJob(LayerMap * new_layer, const MapSource * new_map_source, const TileInfo & ulm, const TileInfo & brm, bool new_refresh_display, MapDownloadMode new_map_download_mode)
{
	this->layer = new_layer;
	this->refresh_display = new_refresh_display;

	/* TODO: add assignment operator for MapCacheObj class. */
	this->map_cache.layout = new_layer->cache_layout;
	this->map_cache.dir_full_path = new_layer->cache_dir;

	this->map_type_id = layer->map_type_id;

	this->tile_info = ulm;
	this->map_download_mode = new_map_download_mode;

	this->map_source = new_map_source;

	this->x_begin = std::min(ulm.x, brm.x);
	this->x_end   = std::max(ulm.x, brm.x);
	this->y_begin = std::min(ulm.y, brm.y);
	this->y_end   = std::max(ulm.y, brm.y);
}




MapDownloadJob::~MapDownloadJob()
{
}




static bool is_in_area(const MapSource * map_source, const TileInfo & tile_info)
{
	Coord center_coord;
	map_source->tile_to_center_coord(tile_info, center_coord);

	const Coord coord_tl(LatLon(map_source->get_lat_max(), map_source->get_lon_min()), CoordMode::LATLON);
	const Coord coord_br(LatLon(map_source->get_lat_min(), map_source->get_lon_max()), CoordMode::LATLON);

	return center_coord.is_inside(&coord_tl, &coord_br);
}




/* Map download function. */
void MapDownloadJob::run(void)
{
	DownloadHandle * dl_handle = this->map_source->download_handle_init();
	unsigned int donemaps = 0;

	/* The purpose of this call is to set fields in tile_iter
	   other than x and y.  x and y will be set and incremented in
	   loops below, but other fields of the iter also need to have
	   some valid values. These valid values are set here. */
	TileInfo tile_iter = this->tile_info;

	qDebug() << SG_PREFIX_I << "Called";

	for (tile_iter.x = this->x_begin; tile_iter.x <= this->x_end; tile_iter.x++) {
		for (tile_iter.y = this->y_begin; tile_iter.y <= this->y_end; tile_iter.y++) {

			/* Only attempt to download a tile from areas supported by current map source. */
			if (!is_in_area(this->map_source, tile_iter)) {
				qDebug() << SG_PREFIX_I << "Tile" << tile_iter.x << tile_iter.y << "is not in area of map id" << (int) this->map_source->map_type_id << ", skipping";
				continue;
			}

			bool remove_mem_cache = false;
			bool need_download = false;

			this->file_full_path = this->map_cache.get_cache_file_full_path(tile_iter,
											this->map_source->map_type_id,
											this->map_source->get_map_type_string(),
											this->map_source->get_file_extension());

			donemaps++;

			const bool end_job = this->set_progress_state(((double) donemaps) / this->n_items); /* this also calls testcancel */
			if (end_job) {
				qDebug() << SG_PREFIX_I << "Background module informs this thread to end its job";
				this->map_source->download_handle_cleanup(dl_handle);
				return;
			}

			if (0 != access(this->file_full_path.toUtf8().constData(), F_OK)) {
				need_download = true;
				remove_mem_cache = true;

			} else {  /* In case map file already exists. */
				switch (this->map_download_mode) {
				case MapDownloadMode::MissingOnly:
					qDebug() << SG_PREFIX_I << "Continue";
					continue;

				case MapDownloadMode::MissingAndBad: {
					/* See if this one is bad or what. */
					QPixmap tmp_pixmap; /* Apparently this will pixmap is only for test of some kind. */
					if (!tmp_pixmap.load(this->file_full_path)) {
						qDebug() << SG_PREFIX_D << "Removing file" << this->file_full_path << "(redownload bad)";
						if (!QDir::root().remove(this->file_full_path)) {
							qDebug() << SG_PREFIX_W << "Redownload Bad failed to remove" << this->file_full_path;
						}
						need_download = true;
						remove_mem_cache = true;
					}
					break;
				}

				case MapDownloadMode::New:
					need_download = true;
					remove_mem_cache = true;
					break;

				case MapDownloadMode::All:
					/* FIXME: need a better way than to erase file in case of server/network problem. */
					qDebug() << SG_PREFIX_D << "Removing file" << this->file_full_path << "(redownload all)";
					if (!QDir::root().remove(this->file_full_path)) {
						qDebug() << SG_PREFIX_W << "Redownload All failed to remove" << this->file_full_path;
					}
					need_download = true;
					remove_mem_cache = true;
					break;

				case MapDownloadMode::DownloadAndRefresh:
					remove_mem_cache = true;
					break;

				default:
					qDebug() << SG_PREFIX_W << "Redownload mode unknown:" << (int) this->map_download_mode;
				}
			}

			this->tile_info.x = tile_iter.x;
			this->tile_info.y = tile_iter.y;

			if (need_download) {
				/* tile_iter has obviously x and y fields, but also all other fields
				   set, thanks to assignment made where tile_iter has been defined. */
				const DownloadResult dr = this->map_source->download_tile(tile_iter, this->file_full_path, dl_handle);
				switch (dr) {
				case DownloadResult::HTTPError:
				case DownloadResult::ContentError: {
					/* TODO: ?? count up the number of download errors somehow... */
					QString msg = tr("%1: %2").arg(this->layer->get_map_label()).arg("Failed to download map tile");
					this->layer->get_window()->statusbar_update(StatusBarField::INFO, msg);
					break;
				}
				case DownloadResult::FileWriteError: {
					QString msg = tr("%1: %2").arg(this->layer->get_map_label()).arg("Unable to save map tile");
					this->layer->get_window()->statusbar_update(StatusBarField::INFO, msg);
					break;
				}
				case DownloadResult::Success:
				case DownloadResult::DownloadNotRequired:
				default:
					break;
				}
			} else {
				qDebug() << SG_PREFIX_I << "This tile doesn't need download";
			}

			this->mutex.lock();
			if (remove_mem_cache) {
				MapCache::remove_all_shrinkfactors(tile_iter, this->map_source->map_type_id, this->layer->file_full_path);
			}

			if (this->refresh_display && this->map_layer_alive) {
				/* TODO: check if downloaded tile is visible in viewport.
				   Otherwise redraw of viewport is not needed. */
				this->layer->emit_layer_changed("Set of tiles for Map Layer has been updated after tile download");
			}
			this->mutex.unlock();

			/* We're temporarily between downloads. */
			this->tile_info.x = 0;
			this->tile_info.y = 0;
		}
	}
	this->map_source->download_handle_cleanup(dl_handle);
	this->mutex.lock();
	if (this->map_layer_alive) {
		this->layer->weak_unref(LayerMap::weak_ref_cb, this);
	}
	this->mutex.unlock();
	return;
}




void MapDownloadJob::cleanup_on_cancel(void)
{
	if (this->tile_info.x || this->tile_info.y) {
		this->file_full_path = this->map_cache.get_cache_file_full_path(this->tile_info,
										this->map_source->map_type_id,
										this->map_source->get_map_type_string(),
										this->map_source->get_file_extension());
		if (0 == access(this->file_full_path.toUtf8().constData(), F_OK)) {
			qDebug() << SG_PREFIX_D << "Removing file" << this->file_full_path << "(cleanup on cancel)";
			if (!QDir::root().remove(this->file_full_path)) {
				qDebug() << SG_PREFIX_W << "Cleanup failed to remove file" << this->file_full_path;
			}
		}
	}
}




int MapDownloadJob::calculate_tile_count_to_download(const TileInfo & ulm, bool simple) const
{
	QPixmap pixmap; /* Defined on top, to avoid creating this object multiple times in the loop. */

	TileInfo tile_iter = this->tile_info;
	tile_iter.z = ulm.z;
	tile_iter.scale = ulm.scale;

	QString tile_file_full_path;

	int n_maps = 0;

	for (tile_iter.x = this->x_begin; tile_iter.x <= this->x_end; tile_iter.x++) {
		for (tile_iter.y = this->y_begin; tile_iter.y <= this->y_end; tile_iter.y++) {
			/* Only count tiles from supported areas. */
			if (!is_in_area(this->map_source, tile_iter)) {
				continue;
			}


			switch (this->map_download_mode) {
			case MapDownloadMode::MissingOnly:
				/* Download only missing tiles.
				   Checking which tile is missing is easy. */
				tile_file_full_path = this->map_cache.get_cache_file_full_path(tile_iter,
											       this->map_source->map_type_id,
											       this->map_source->get_map_type_string(),
											       this->map_source->get_file_extension());
				if (0 != access(tile_file_full_path.toUtf8().constData(), F_OK)) {
					n_maps++;
				}
				break;
			case MapDownloadMode::All:
				/* Download all tiles.
				   Deciding which tiles to download is easy: all of them. */
				n_maps++;
				break;

			case MapDownloadMode::New:
				/* Download missing tile that are newer on server only.

				   This case is harder. For now assume
				   that tiles in local cache (if they
				   exist at all) are older than tiles
				   on server, and download them.

				   Comparing dates of local tiles and
				   tiles on server would require a
				   lookup on server and that would be
				   slow.

				   TODO: perhaps we could somehow
				   implement the comparison of dates
				   of local and remote tiles, even if
				   it's slow.
				*/
				n_maps++;
				break;

			case MapDownloadMode::MissingAndBad:
				/* Download missing and bad tiles. */
				tile_file_full_path = this->map_cache.get_cache_file_full_path(tile_iter,
											       this->map_source->map_type_id,
											       this->map_source->get_map_type_string(),
											       this->map_source->get_file_extension());
				if (0 != access(tile_file_full_path.toUtf8().constData(), F_OK)) {
					/* Missing. */
					n_maps++;
				} else {
					if (!pixmap.load(tile_file_full_path)) {
						/* Bad. */
						n_maps++;
					}
				}
				break;

			case MapDownloadMode::DownloadAndRefresh:
				/* TODO: unhandled download mode. */
				break;

			default:
				qDebug() << SG_PREFIX_E << "Unexpected download mode" << (int) this->map_download_mode;
				break;
			}
		}
	}

	return n_maps;
}




void MapDownloadJob::set_description(MapDownloadMode a_map_download_mode, int maps_to_get, const QString & label)
{
	QString fmt;

	switch (a_map_download_mode) {
	case MapDownloadMode::MissingOnly:
		fmt = QObject::tr("Downloading %n %1 maps...", "", maps_to_get);
		break;
	case MapDownloadMode::MissingAndBad:
		fmt = QObject::tr("Redownloading up to %n %1 maps...", "", maps_to_get);
		break;
	default:
		fmt = QObject::tr("Redownloading %n %1 maps...", "", maps_to_get);
		break;
	};

	BackgroundJob::set_description(QString(fmt).arg(label));
}




void MapDownloadJob::reset_internal(void)
{
	this->tile_info.x = 0;
	this->tile_info.y = 0;
}




int MapDownloadJob::calculate_total_tile_count_to_download(void) const
{
	return (this->x_end - this->x_begin + 1) * (this->y_end - this->y_begin + 1);
}
