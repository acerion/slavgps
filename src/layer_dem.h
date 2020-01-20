/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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
#include "layer_dem_dem.h"
#include "background.h"
#include "measurements.h"




namespace SlavGPS {




	enum class DEMDrawingType {
		Elevation = 0,
		Gradient,
	};




	class LayerDEMInterface : public LayerInterface {
	public:
		LayerDEMInterface();
		Layer * unmarshall(Pickle & pickle, GisViewport * gisview);
		LayerToolContainer create_tools(Window * window, GisViewport * gisview);
	};




	class DEMPalette {
	public:
		DEMPalette() {};
		DEMPalette(const std::vector<QColor> & values) : m_values(values) {}
		int size(void) const { return this->m_values.size(); };
		std::vector<QColor> m_values;
	};




	class LayerDEM : public Layer {
		Q_OBJECT
	public:
		LayerDEM();
		~LayerDEM();

		/* Layer interface methods. */
		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);
		QString get_tooltip(void) const;
		bool add_file(const QString & dem_file_path);
		void draw_dem(GisViewport * gisview, const DEM & dem);
		bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const override;

		void draw_dem_lat_lon(GisViewport * gisview, const DEM & dem);
		void draw_dem_utm(GisViewport * gisview, const DEM & dem);
		bool download_selected_tile(const QMouseEvent * event, const LayerTool * tool);

		DEMPalette colors;
		DEMPalette gradients;

		QStringList files;

		/* Always in meters, even if units selected by user are different. */
		Altitude min_elev{0.0, AltitudeType::Unit::E::Metres};
		Altitude max_elev{0.0, AltitudeType::Unit::E::Metres};

		QColor base_color; /* Minimum elevation color, selected in layer's properties window. */
		DEMSource dem_source = DEMSource::SRTM;
		DEMDrawingType dem_drawing_type = DEMDrawingType::Elevation;


	public slots:
		sg_ret handle_downloaded_file_cb(const QString & file_full_path);

	private slots:
		void location_info_cb(void);
		void on_loading_to_cache_completed_cb(void);
	};




	class LayerToolDEMDownload : public LayerTool {
	public:
		LayerToolDEMDownload(Window * window, GisViewport * gisview);

		LayerTool::Status handle_mouse_release(Layer * layer, QMouseEvent * event);

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

		DEMSource dem_source;

	signals:
		void download_job_completed(const QString & file_full_path);
	};




}




#endif /* #ifndef _SG_LAYER_DEM_H_ */
