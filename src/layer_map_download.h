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

#ifndef _SG_LAYER_MAP_DOWNLOAD_H_
#define _SG_LAYER_MAP_DOWNLOAD_H_




#include <QString>




#include "background.h"
#include "map_cache.h"




namespace SlavGPS {




	enum class MapDownloadMode;
	enum class MapTypeID;
	class LayerMap;
	class MapSource;




	class MapDownloadJob : public BackgroundJob {
		Q_OBJECT
	public:
		MapDownloadJob() {};
		MapDownloadJob(LayerMap * layer, const TileInfo & ulm, const TileInfo & brm, bool refresh_display, MapDownloadMode map_download_mode);
		~MapDownloadJob();

		void cleanup_on_cancel(void);

		int calculate_tile_count_to_download(void) const;
		int calculate_total_tile_count_to_download(void) const;

		void run(void); /* Re-implementation of QRunnable::run(). */

		void set_description(MapDownloadMode map_download_mode, int maps_to_get, const QString & label);

		QString file_full_path;


		MapDownloadMode m_map_download_mode;

	signals:
		void download_job_completed(void);

	private:
		bool m_refresh_display = false;
		LayerMap * m_layer = nullptr;

		MapCachePath m_map_cache_path;

		/* Tile info variable holding information common for
		   all tiles downloaded by this job, across all x/y
		   values. The variable stores values of fields other
		   than x/y. */
		TileInfo common_tile_info;

		/* Tile that is currently being downloaded. */
		TileInfo tile_info_in_download;

		/* Downloading process (through download module) is
		   taking place right now. */
		bool download_in_progress = false;

		TilesRange range;

		/* How many tiles did we fail to download? */
		unsigned int failed_downloads = 0;

		/* How many tiles did we fail to save to disc? */
		unsigned int failed_saves = 0;
	};




} /* namespace SlavGPS */





#endif /* #ifndef _SG_LAYER_MAP_DOWNLOAD_H_ */
