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

#ifndef _SG_LAYER_DEM_H_
#define _SG_LAYER_DEM_H_




#include <vector>




#include <QPen>
#include <QColor>
#include <QObject>




#include "layer.h"
#include "layer_interface.h"
#include "layer_tool.h"
#include "dem.h"
#include "background.h"
#include "measurements.h"




namespace SlavGPS {




	enum {
		DEM_SOURCE_SRTM,
#ifdef VIK_CONFIG_DEM24K
		DEM_SOURCE_DEM24K,
#endif
	};

	enum {
		DEM_TYPE_HEIGHT = 0,
		DEM_TYPE_GRADIENT,
	};




	class LayerDEMInterface : public LayerInterface {
	public:
		LayerDEMInterface();
		Layer * unmarshall(Pickle & pickle, GisViewport * gisview);
		LayerToolContainer create_tools(Window * window, GisViewport * gisview);
	};




	class LayerDEM : public Layer {
		Q_OBJECT
	public:
		LayerDEM();
		~LayerDEM();

		/* Layer interface methods. */
		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);
		QString get_tooltip(void) const;
		bool download_release(QMouseEvent * event, LayerTool * tool);
		bool add_file(const QString & dem_file_path);
		void draw_dem(GisViewport * gisview, DEM * dem);
		void draw_dem_ll(GisViewport * gisview, DEM * dem);
		void draw_dem_utm(GisViewport * gisview, DEM * dem);
		bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const override;

		std::vector<QColor> colors;
		std::vector<QColor> gradients;

		QStringList files;

		/* Always in meters, even if units selected by user are different. */
		Altitude min_elev{0.0, HeightUnit::Metres};
		Altitude max_elev{0.0, HeightUnit::Metres};

		QColor base_color; /* Minimum elevation color, selected in layer's properties window. */
		int source = DEM_SOURCE_SRTM;    /* Signed int because this is a generic enum ID. */
		int dem_type = DEM_TYPE_HEIGHT;  /* Signed int because this is a generic enum ID. */


	public slots:
		sg_ret handle_downloaded_file_cb(const QString & file_full_path);

	private slots:
		void location_info_cb(void);
		void on_loading_to_cache_completed_cb(void);
	};




	class LayerToolDEMDownload : public LayerTool {
	public:
		LayerToolDEMDownload(Window * window, GisViewport * gisview);

		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);

		SGObjectTypeID get_tool_id(void) const override;
		static SGObjectTypeID tool_id(void);
	};




	class DEMLoadJob : public BackgroundJob {
		Q_OBJECT
	public:
		DEMLoadJob(const QStringList & file_paths);

		void run(void);
		void cleanup_on_cancel(void);
		bool load_files_into_cache(void);

		QStringList file_paths;
	signals:
		void loading_to_cache_completed();
	};




	class DEMDownloadJob : public BackgroundJob {
		Q_OBJECT
	public:
		DEMDownloadJob(const QString & dest_file_path, const LatLon & lat_lon, LayerDEM * layer);
		~DEMDownloadJob();

		void run(void);

		QString dest_file_path;
		LatLon lat_lon;

		unsigned int source;

	signals:
		void download_job_completed(const QString & file_full_path);
	};




}




#endif /* #ifndef _SG_LAYER_DEM_H_ */
