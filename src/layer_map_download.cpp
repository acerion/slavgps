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




#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QDebug>
#include <QDir>




#include "layer_map_download.h"
#include "window.h"
#include "layer_map_source.h"
#include "layer_map.h"
#include "globals.h"
#include "statusbar.h"




using namespace SlavGPS;




#define SG_MODULE "Map Download Job"




/*
  Download tiles between @ulm and @brm
*/
MapDownloadJob::MapDownloadJob(LayerMap * layer, const TileInfo & ulm, const TileInfo & brm, bool refresh_display, MapDownloadMode map_download_mode)
{
	/* TODO_LATER: What happens if layer and its associated source are removed before the download job is completed? */
	this->m_layer = layer;
	this->m_refresh_display = refresh_display;

	this->m_map_cache_path = MapCachePath(this->m_layer->cache_layout, this->m_layer->cache_dir);

	/* We only need to store tile parameters other than x and y,
	   that are common for all tiles downloaded by this job. */
	this->common_tile_info = ulm;

	this->m_map_download_mode = map_download_mode;

	this->range = TileInfo::get_tiles_range(ulm, brm);

	connect(this, SIGNAL (download_job_completed(void)), this->m_layer, SLOT (handle_downloaded_tile_cb(void)));
}




MapDownloadJob::~MapDownloadJob()
{
}




/* Map download function. */
void MapDownloadJob::run(void)
{
	DownloadHandle * dl_handle = this->m_layer->map_source()->download_handle_init();
	unsigned int donemaps = 0;

	/* The purpose of this assignment is to set fields in tile_iter
	   other than x and y.  x and y will be set and incremented in
	   loops below, but other fields of the iter also need to have
	   some valid values. These valid values are set here. */
	TileInfo tile_iter = this->common_tile_info;

	qDebug() << SG_PREFIX_I << "Called";

	for (tile_iter.x = this->range.horiz_first_idx; tile_iter.x <= this->range.horiz_last_idx; tile_iter.x++) {
		for (tile_iter.y = this->range.vert_first_idx; tile_iter.y <= this->range.vert_last_idx; tile_iter.y++) {

			/* Only attempt to download a tile from areas supported by current map source. */
			if (!this->m_layer->map_source()->includes_tile(tile_iter)) {
				qDebug() << SG_PREFIX_I << "Tile" << tile_iter.x << tile_iter.y << "is not in area of map id" << (int) this->m_layer->map_source()->map_type_id() << ", skipping";
				continue;
			}

			bool remove_mem_cache = false;
			bool need_download = false;

			this->file_full_path = this->m_map_cache_path.get_cache_file_full_path(tile_iter,
											       this->m_layer->map_source()->map_type_id(),
											       this->m_layer->map_source()->map_type_string(),
											       this->m_layer->map_source()->get_file_extension());

			donemaps++;

			const bool end_job = this->set_progress_state(((double) donemaps) / this->n_items); /* this also calls testcancel */
			if (end_job) {
				qDebug() << SG_PREFIX_I << "Background module informs this thread to end its job";
				this->m_layer->map_source()->download_handle_cleanup(dl_handle);
				return;
			}

			if (0 != access(this->file_full_path.toUtf8().constData(), F_OK)) {
				need_download = true;
				remove_mem_cache = true;

			} else {  /* In case map file already exists. */
				switch (this->m_map_download_mode) {
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
					/* TODO_LATER: need a better way than to erase file in case of server/network problem. */
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
					qDebug() << SG_PREFIX_W << "Redownload mode unknown:" << (int) this->m_map_download_mode;
				}
			}

			this->tile_info_in_download = tile_iter;
			this->download_in_progress = true;

			if (need_download) {
				/* tile_iter has obviously x and y fields, but also all other fields
				   set, thanks to assignment made where tile_iter has been defined. */
				const DownloadStatus dr = this->m_layer->map_source()->download_tile(tile_iter, this->file_full_path, dl_handle);
				switch (dr) {
				case DownloadStatus::HTTPError:
				case DownloadStatus::ContentError: {
					this->failed_downloads++;
					const QString msg = tr("%1: Failed to download map tile (%2 failed in total)")
						.arg(this->m_layer->get_map_type_ui_label())
						.arg(this->failed_downloads);
					ThisApp::main_window()->statusbar()->set_message(StatusBarField::Info, msg);
					break;
				}
				case DownloadStatus::FileWriteError: {
					this->failed_saves++;
					const QString msg = tr("%1: Failed to save map tile (%2 failed in total)")
						.arg(this->m_layer->get_map_type_ui_label())
						.arg(this->failed_saves);
					ThisApp::main_window()->statusbar()->set_message(StatusBarField::Info, msg);
					break;
				}
				case DownloadStatus::Success:
				case DownloadStatus::DownloadNotRequired:
				default:
					break;
				}
			} else {
				qDebug() << SG_PREFIX_I << "This tile doesn't need download";
			}

			if (remove_mem_cache) {
				MapCache::remove_all_shrinkfactors(tile_iter, this->m_layer->map_source()->map_type_id(), this->m_layer->file_full_path);
			}

			if (this->m_refresh_display && this->m_layer->is_tile_visible(tile_iter)) {
				qDebug() << SG_PREFIX_SIGNAL << "Will emit 'download job completed' signal to indicate completion of tile download job";
				emit this->download_job_completed();
			}

			/* We're temporarily between downloads. */
			this->download_in_progress = false;
		}
	}
	this->m_layer->map_source()->download_handle_cleanup(dl_handle);

	return;
}




void MapDownloadJob::cleanup_on_cancel(void)
{
	if (!this->download_in_progress) {
		return;
	}


	/* Remove file that is being / has been now downloaded. */
	const QString full_path = this->m_map_cache_path.get_cache_file_full_path(this->tile_info_in_download,
										  this->m_layer->map_source()->map_type_id(),
										  this->m_layer->map_source()->map_type_string(),
										  this->m_layer->map_source()->get_file_extension());
	if (0 == access(full_path.toUtf8().constData(), F_OK)) {
		qDebug() << SG_PREFIX_D << "Removing file" << full_path << "(cleanup on cancel)";
		if (!QDir::root().remove(full_path)) {
			qDebug() << SG_PREFIX_W << "Cleanup failed to remove file" << full_path;
		}
	}
}




int MapDownloadJob::calculate_tile_count_to_download(void) const
{
	/* Defined on top, to avoid creating this object multiple times in the loop. */
	QPixmap pixmap;
	QString tile_file_full_path;

	/* The two loops below will iterate over x and y, but the tile
	   iterator also needs to have other tile info parameters
	   set. These "other tile parameters" have been saved in
	   constructor in ::common_tile_info. */
	TileInfo tile_iter = this->common_tile_info;

	int n_maps = 0;

	for (tile_iter.x = this->range.horiz_first_idx; tile_iter.x <= this->range.horiz_last_idx; tile_iter.x++) {
		for (tile_iter.y = this->range.vert_first_idx; tile_iter.y <= this->range.vert_last_idx; tile_iter.y++) {
			/* Only count tiles from supported areas. */
			if (!this->m_layer->map_source()->includes_tile(tile_iter)) {
				continue;
			}


			switch (this->m_map_download_mode) {
			case MapDownloadMode::MissingOnly:
				/* Download only missing tiles.
				   Checking which tile is missing is easy. */
				tile_file_full_path = this->m_map_cache_path.get_cache_file_full_path(tile_iter,
												      this->m_layer->map_source()->map_type_id(),
												      this->m_layer->map_source()->map_type_string(),
												      this->m_layer->map_source()->get_file_extension());
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

				   TODO_MAYBE: perhaps we could somehow
				   implement the comparison of dates
				   of local and remote tiles, even if
				   it's slow.
				*/
				n_maps++;
				break;

			case MapDownloadMode::MissingAndBad:
				/* Download missing and bad tiles. */
				tile_file_full_path = this->m_map_cache_path.get_cache_file_full_path(tile_iter,
												      this->m_layer->map_source()->map_type_id(),
												      this->m_layer->map_source()->map_type_string(),
												      this->m_layer->map_source()->get_file_extension());
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
				/* TODO_LATER: unhandled download mode. */
				break;

			default:
				qDebug() << SG_PREFIX_E << "Unexpected download mode" << (int) this->m_map_download_mode;
				break;
			}
		}
	}

	return n_maps;
}




int MapDownloadJob::calculate_total_tile_count_to_download(void) const
{
	const int count = this->range.get_tiles_count();
	qDebug() << SG_PREFIX_I << "Number of maps to download:" << count;
	return count;
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
