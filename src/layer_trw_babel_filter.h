/*
 * SlavGPS -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_LAYER_TRW_BABEL_FILTER_H_
#define _SG_LAYER_TRW_BABEL_FILTER_H_




#include <QObject>
#include <QMenu>




#include "datasource_bfilter.h"
#include "layer_trw_import.h"




namespace SlavGPS {




	class GisViewport;
	class Window;
	class LayerTRW;
	class Track;
	class DataSource;




	class LayerTRWBabelFilter : public QObject {
		Q_OBJECT
	public:
		static void init(void);
		static void uninit(void);

		/*
		  Notice that we don't pass a 'filter with this track'
		  track through this function. Such track should be
		  registered with ::set_babel_filter_track().
		*/
		sg_ret set_main_fields(Window * window, GisViewport * gisview, Layer * parent_layer);
		void set_trw_field(LayerTRW * trw);
		void clear_all(void);

		/* Add 'filter' entries to context menu for TRW layer. */
		sg_ret add_babel_filters_for_layer_submenu(QMenu & submenu);
		/* Add 'filter' entries to context menu for TRW track. */
		sg_ret add_babel_filters_for_track_submenu(QMenu & submenu);

		sg_ret add_babel_filters_to_submenu(QMenu & menu, DataSourceBabelFilter::Type filter_type);

		/**
		   Sets application-wide track to use with gpsbabel
		   filter. @param trk can be used with a TRW layer
		   other than parent of this @param trk.
		*/
		static void set_babel_filter_track(Track * trk);

		AcquireContext ctx;

	public slots:
		void apply_babel_filter_cb(void);

	private:
		static sg_ret register_babel_filter(DataSourceBabelFilter * bfilter);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_BABEL_FILTER_H_ */
