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

#ifndef _SG_WAYPOINT_H_
#define _SG_WAYPOINT_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdint>

#include <time.h>

#include <QPixmap>

#include "coord.h"
#include "globals.h"
#include "tree_view.h"
#include "layer.h"
#include "file.h"




namespace SlavGPS {




	class Waypoint : public TreeItem {
		Q_OBJECT
	public:
		Waypoint();
		Waypoint(const Waypoint& other);
		~Waypoint();


		void set_name(const QString & new_name);
		void set_comment(const QString & new_comment);
		void set_description(const QString & new_description);
		void set_source(const QString & new_source);
		void set_type(const QString & new_type);
		void set_url(const QString & new_url);
		void set_image_full_path(const QString & new_image_full_path);
		void set_symbol_name(const QString & new_symbol_name);

		bool apply_dem_data(bool skip_existing_elevations);
		void apply_dem_data_common(bool skip_existing_elevations);

		QString get_tooltip(void);

		void marshall(uint8_t ** data, size_t * data_len);
		static Waypoint * unmarshall(uint8_t * data, size_t data_len);

		void convert(CoordMode dest_mode);

		/* Does ::url, ::comment or ::description field contain an url? */
		bool has_any_url(void) const;
		/* Get url from first of these fields that does contain url, starting with ::url. */
		QString get_any_url(void) const;


		bool add_context_menu_items(QMenu & menu, bool tree_view_context_menu);
		void sublayer_menu_waypoint_misc(LayerTRW * parent_layer_, QMenu & menu, bool tree_view_context_menu);

		bool handle_selection_in_tree(void);

		void draw_tree_item(Viewport * viewport, bool hl_is_allowed, bool hl_is_required);

		QString sublayer_rename_request(const QString & new_name);

		void delete_sublayer(bool confirm);

		bool properties_dialog();

		LayerTRW * get_parent_layer_trw() const;

		Coord coord;
		//bool visible = true;
		bool has_timestamp = false;
		time_t timestamp = 0;
		double altitude = VIK_DEFAULT_ALTITUDE;

		/* QString name; */ /* Inherited from TreeItem. */
		QString comment;
		QString description;
		QString source;
		QString type;
		QString url;
		QString image_full_path;
		QString symbol_name;

		/* A rather misleading, ugly hack needed for trwlayer's click image.
		   These are the height at which the thumbnail is being drawn, not the
		   dimensions of the original image. */
		uint8_t image_width;
		uint8_t image_height;


		/* Only for GUI display. */
		QPixmap * symbol_pixmap = NULL;


	public slots:
		void properties_dialog_cb(void);
		void apply_dem_data_all_cb(void);
		void apply_dem_data_only_missing_cb(void);

		void open_diary_cb(void);
		void open_astro_cb(void);
		void open_geocache_webpage_cb(void);
		void open_waypoint_webpage_cb(void);

		void show_in_viewport_cb(void);

#ifdef VIK_CONFIG_GEOTAG
		void geotagging_waypoint_cb(void);
		void geotagging_waypoint_mtime_keep_cb(void);
		void geotagging_waypoint_mtime_update_cb(void);
#endif

		void cut_sublayer_cb(void);
		void copy_sublayer_cb(void);
		void delete_sublayer_cb(void);
	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Waypoint*)




#endif /* #ifndef _SG_WAYPOINT_H_ */
