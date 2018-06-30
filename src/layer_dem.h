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




#include <QPen>
#include <QColor>
#include <QObject>

#include "layer.h"
#include "layer_interface.h"
#include "layer_tool.h"
#include "dem.h"
#include "background.h"




namespace SlavGPS {




	enum { DEM_SOURCE_SRTM,
#ifdef VIK_CONFIG_DEM24K
	       DEM_SOURCE_DEM24K,
#endif
	};

	enum { DEM_TYPE_HEIGHT = 0,
	       DEM_TYPE_GRADIENT,
	       DEM_TYPE_NONE,
	};




	class LayerDEMInterface : public LayerInterface {
	public:
		LayerDEMInterface();
		Layer * unmarshall(Pickle & pickle, Viewport * viewport);
		LayerToolContainer * create_tools(Window * window, Viewport * viewport);
	};




	class LayerDEM : public Layer {
		Q_OBJECT
	public:
		LayerDEM();
		~LayerDEM();

		/* Layer interface methods. */
		void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected);
		QString get_tooltip(void) const;
		bool download_release(QMouseEvent * event, LayerTool * tool);
		bool add_file(const QString & dem_file_path);
		void draw_dem(Viewport * viewport, DEM * dem);
		void draw_dem_ll(Viewport * viewport, DEM * dem, const LatLonMinMax & min_max);
		void draw_dem_utm(Viewport * viewport, DEM * dem);
		bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t id, bool is_file_operation) const;

		static void weak_ref_cb(void * ptr, void * dead_vdl);

		/* Table of colors. */
		QColor * colors = NULL;
		/* Table of gradients. */
		QColor * gradients = NULL;

		QStringList files;
		double min_elev = 0;
		double max_elev = 0;
		QColor base_color; /* Minimum elevation color, selected in layer's properties window. */
		int source = DEM_SOURCE_SRTM;    /* Signed int because this is a generic enum ID. */
		int dem_type = DEM_TYPE_HEIGHT;  /* Signed int because this is a generic enum ID. */


	private slots:
		void location_info_cb(void);
		void on_loading_to_cache_completed_cb(void);
	};




	class LayerToolDEMDownload : public LayerTool {
	public:
		LayerToolDEMDownload(Window * window, Viewport * viewport);

		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
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




}




#endif /* #ifndef _SG_LAYER_DEM_H_ */
