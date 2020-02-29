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

#ifndef _SG_LAYER_H_
#define _SG_LAYER_H_




#include <cstdint>




#include <QObject>
#include <QIcon>
#include <QMouseEvent>
#include <QMenu>
#include <QString>
#include <QPen>
#include <QDebug>
#include <QFile>




#include "tree_view.h"
#include "globals.h"
#include "clipboard.h"




namespace SlavGPS {




	class GisViewport;
	class Window;
	class LayerTRW;
	class LayerInterface;
	class SGVariant;
	class ParameterSpecification;

	class LayerTool;
	class LayerToolSelect;

	enum class LayerDataReadStatus;
	enum class CoordMode;




	enum class LayerKind {
		Aggregate = 0,
		TRW,
		Coordinates,
		Georef,
		GPS,
		Map,
		DEM,
#ifdef HAVE_LIBMAPNIK
		Mapnik,
#endif
		Max /* Also use this value to indicate no layer association. */
	};

	LayerKind& operator++(LayerKind & layer_kind);
 	QDebug operator<<(QDebug debug, const LayerKind & layer_kind);




	class Layer : public TreeItem {
		Q_OBJECT

	public:

		Layer();
		~Layer();

		virtual void marshall(Pickle & pickle);
		virtual void marshall_params(Pickle & pickle);
		virtual void unmarshall_params(Pickle & pickle);
		static Layer * unmarshall(Pickle & pickle, GisViewport * gisview);

		static Layer * construct_layer(LayerKind layer_kind, GisViewport * gisview, bool interactive = false);

		const LayerInterface & get_interface(void) const;
		static LayerInterface * get_interface(LayerKind layer_kind);
		void configure_interface(LayerInterface * intf, ParameterSpecification * param_specs);

		/* Call before LayerDefaults::init() */
		static void preconfigure_interfaces(void);
		/* Call after LayerDefaults::init() */
		static void postconfigure_interfaces(void);

		/* Rarely used, this is called after a read operation
		   or properties box is run.  usually used to create
		   GC's that depend on params, but GC's can also be
		   created from create() or set_param(). */
		virtual sg_ret post_read(GisViewport * gisview, bool from_file);

		void draw_tree_item(__attribute__((unused)) GisViewport * gisview, __attribute__((unused)) bool highlight_selected, __attribute__((unused)) bool parent_is_selected) override { return; };

		virtual QString get_tooltip(void) const;

		bool handle_selection_in_tree(void);

		virtual void set_coord_mode(__attribute__((unused)) CoordMode mode) { return; };

		/* Methods for generic "Select" tool. */
		virtual bool handle_select_tool_click(__attribute__((unused)) QMouseEvent * event, __attribute__((unused)) GisViewport * gisview, __attribute__((unused)) LayerToolSelect * select_tool)         { return false; };
		virtual bool handle_select_tool_double_click(__attribute__((unused)) QMouseEvent * event, __attribute__((unused)) GisViewport * gisview, __attribute__((unused)) LayerToolSelect * select_tool)  { return false; };
		virtual bool handle_select_tool_move(__attribute__((unused)) QMouseEvent * event, __attribute__((unused)) GisViewport * gisview, __attribute__((unused)) LayerToolSelect * select_tool)          { return false; };
		virtual bool handle_select_tool_release(__attribute__((unused)) QMouseEvent * event, __attribute__((unused)) GisViewport * gisview, __attribute__((unused)) LayerToolSelect * select_tool)       { return false; };
		virtual bool handle_select_tool_context_menu(__attribute__((unused)) QMouseEvent * event, __attribute__((unused)) GisViewport * gisview)                                 { return false; };


		virtual sg_ret copy_child_item2(__attribute__((unused)) TreeItem * item, __attribute__((unused)) uint8_t ** data, __attribute__((unused)) unsigned int * len) { return sg_ret::err; }
		virtual sg_ret unpickle_child_item(__attribute__((unused)) TreeItem * item, __attribute__((unused)) Pickle & pickle) { return sg_ret::err; }



		virtual void change_coord_mode(__attribute__((unused)) CoordMode dest_mode) { return; };

		void request_new_viewport_center(GisViewport * gisview, const Coord & coord);

		virtual Time get_timestamp(void) const override;

		sg_ret accept_dropped_child(TreeItem * tree_item, int row, int col) override;

		/* Read layer-specific data from Vik file. */
		virtual LayerDataReadStatus read_layer_data(QFile & file, const QString & dirpath);
		/* Write layer-specific data to Vik file. */
		virtual SaveStatus write_layer_data(FILE * file) const;

		sg_ret menu_add_type_specific_operations(QMenu & menu, bool in_tree_view) override;


		virtual bool show_properties_dialog(GisViewport * gisview);
		virtual bool show_properties_dialog(void) override;

		/* Get current, per-instance-of-layer, value of a layer parameter. The parameter is specified by its id.
		   @is_file_operation denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const;

		/* Returns true if needs to redraw due to changed param. */
		/* bool denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);

		/* Most of layer kinds aren't able to store child layers.
		   Those that do, may have zero child layers at the moment. */
		virtual int get_child_layers_count(void) const { return 0; };

		/* Return list of children layers. Most of layer kinds won't have child layers. */
		virtual std::list<Layer const *> get_child_layers(void) const { std::list<Layer const *> a_list; return a_list; };

		/* "type id string" means Layer's internal, fixed string
		   that can be used in .vik file operations and to
		   create internal IDs of objects. */
		static LayerKind kind_from_layer_kind_string(const QString & layer_kind_string);
		static QString get_fixed_layer_kind_string(LayerKind kind);
		QString get_fixed_layer_kind_string(void) const;

		/* Get human-readable label that is suitable for using
		   in UI or in debug messages. */
		static QString get_translated_layer_kind_string(LayerKind kind);
		QString get_translated_layer_kind_string(void) const;


		static bool compare_timestamp_ascending(const Layer * first, const Layer * second);  /* Ascending: 1 -> 10 */
		static bool compare_timestamp_descending(const Layer * first, const Layer * second); /* Descending: 10 -> 1 */


		virtual std::list<TreeItem *> get_items_by_date(const QDate & search_date) const;


		void set_initial_parameter_values(void);

		Window * get_window(void) const;

		void ref_layer(void);
		void unref_layer(void);

		Layer * immediate_layer(void) override;

		bool is_layer(void) const override;


		/* GUI. */
		QIcon get_icon(void);

		/* QString name; */ /* Inherited from TreeItem. */

		LayerKind m_kind;

	protected:

		LayerInterface * interface = NULL;
		QMenu * right_click_menu = NULL;

	protected slots:
		virtual void location_info_cb(void);

	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Layer*)




#endif /* #ifndef _SG_LAYER_H_ */
