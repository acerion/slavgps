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




#include <cstdint>




#include <time.h>




#include <QPixmap>




#include "coord.h"
#include "measurements.h"
#include "tree_view.h"




namespace SlavGPS {




	class LayerTRW;




	class Waypoint : public TreeItem {
		Q_OBJECT
	public:
		Waypoint();
		Waypoint(const Coord & coord);
		Waypoint(const Waypoint& other);
		~Waypoint();

		virtual TreeItemType get_tree_item_type(void) const override { return TreeItemType::Sublayer; }


		void set_name(const QString & new_name);
		void set_comment(const QString & new_comment);
		void set_description(const QString & new_description);
		void set_source(const QString & new_source);
		void set_type(const QString & new_type);
		void set_url(const QString & new_url);
		void set_image_full_path(const QString & new_image_full_path);
		void set_symbol_name(const QString & new_symbol_name);
		sg_ret set_coord(const Coord & new_coord, bool do_recalculate_bbox, bool only_set_value = false);

		const Coord & get_coord(void) const;

		bool apply_dem_data(bool skip_existing_elevations);
		void apply_dem_data_common(bool skip_existing_elevations);

		QString get_tooltip(void) const;

		void marshall(Pickle & pickle);
		static Waypoint * unmarshall(Pickle & pickle);

		/**
		   @brief Common method for showing a list of waypoints with extended information

		   @title: the title for the dialog
		   @layer: the layer, from which a list of waypoints should be extracted

		   @param layer can be also Aggregate layer - the
		   function then goes through all child layers of the
		   Aggregate layer in search of Waypoints.
		*/
		static void list_dialog(QString const & title, Layer * layer);

		QList<QStandardItem *> get_list_representation(const TreeItemViewFormat & view_format) override;

		void convert(CoordMode dest_mode);

		/* Does ::url, ::comment or ::description field contain an url? */
		bool has_any_url(void) const;
		/* Get url from first of these fields that does contain url, starting with ::url. */
		QString get_any_url(void) const;


		bool add_context_menu_items(QMenu & menu, bool tree_view_context_menu);
		void sublayer_menu_waypoint_misc(LayerTRW * parent_layer_, QMenu & menu, bool tree_view_context_menu);

		bool handle_selection_in_tree(void);

		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);

		QString sublayer_rename_request(const QString & new_name);

		sg_ret update_tree_item_properties(void) override;

		void self_assign_icon(void);
		sg_ret set_new_waypoint_icon(void); /* Wrapper. */

		sg_ret propagate_new_waypoint_name(void);


		bool show_properties_dialog(void);

		LayerTRW * get_parent_layer_trw() const;

		void display_debug_info(const QString & reference) const;

		sg_ret properties_dialog_set(void);
		static sg_ret properties_dialog_reset(void);

		SGObjectTypeID get_type_id(void) const override;
		static SGObjectTypeID type_id(void);

		static TreeItemViewFormat get_view_format_header(bool include_parent_layer);


		/* bool visible = true; */ /* Inherited from TreeItem. */
		Altitude altitude;

		/* QString name; */ /* Inherited from TreeItem. */
		QString comment;
		QString description;
		QString source;
		QString type;
		QString url;
		QString image_full_path;
		QString symbol_name;

		/* Rectangle in viewport, in which waypoints' image is
		   painted.  Needed by "select waypoint by clicking on
		   it in viewport" code.

		   If the ::isNull() returns true, waypoint's image
		   was not drawn. */
		QRect drawn_image_rect;


		/* For display in viewport. This is only reference. */
		QPixmap * symbol_pixmap = NULL;


	public slots:
		bool show_properties_dialog_cb(void);
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

	private:
		Coord m_coord;
	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Waypoint*)




#endif /* #ifndef _SG_WAYPOINT_H_ */
