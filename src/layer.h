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




	class Viewport;
	class Window;
	class LayerTRW;
	class LayerInterface;
	class LayersPanel;
	class SGVariant;
	class ParameterSpecification;

	class LayerTool;
	class LayerToolSelect;

	enum class LayerDataReadStatus;
	enum class CoordMode;




	enum class SublayerType {
		None,
		Tracks,
		Waypoints,
		Track,
		Waypoint,
		Routes,
		Route
	};




	enum class LayerType {
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

	LayerType& operator++(LayerType & layer_type);
 	QDebug operator<<(QDebug debug, const LayerType & layer_type);




	typedef void (* LayerRefCB) (void * ptr, void * dead_vml);




	class Layer : public TreeItem {
		Q_OBJECT

	public:

		Layer();
		~Layer();

		virtual void marshall(Pickle & pickle);
		virtual void marshall_params(Pickle & pickle);
		virtual void unmarshall_params(Pickle & pickle);
		static Layer * unmarshall(Pickle & pickle, Viewport * viewport);


		static Layer * construct_layer(LayerType layer_type, Viewport * viewport, bool interactive = false);

		void emit_layer_changed(const QString & where);
		void emit_layer_changed_although_invisible(const QString & where);

		const LayerInterface & get_interface(void) const;
		static LayerInterface * get_interface(LayerType layer_type);
		void configure_interface(LayerInterface * intf, ParameterSpecification * param_specs);

		/* Call before LayerDefaults::init() */
		static void preconfigure_interfaces(void);
		/* Call after LayerDefaults::init() */
		static void postconfigure_interfaces(void);

		/* Rarely used, this is called after a read operation
		   or properties box is run.  usually used to create
		   GC's that depend on params, but GC's can also be
		   created from create() or set_param(). */
		virtual void post_read(Viewport * viewport, bool from_file);

		virtual void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected) { return; };

		virtual QString get_tooltip(void) const;

		bool handle_selection_in_tree(void);

		virtual void set_coord_mode(CoordMode mode) { return; };

		/* Methods for generic "Select" tool. */
		virtual bool handle_select_tool_click(QMouseEvent * event, Viewport * viewport, LayerToolSelect * select_tool)         { return false; };
		virtual bool handle_select_tool_double_click(QMouseEvent * event, Viewport * viewport, LayerToolSelect * select_tool)  { return false; };
		virtual bool handle_select_tool_move(QMouseEvent * event, Viewport * viewport, LayerToolSelect * select_tool)          { return false; };
		virtual bool handle_select_tool_release(QMouseEvent * event, Viewport * viewport, LayerToolSelect * select_tool)       { return false; };
		virtual bool handle_select_tool_context_menu(QMouseEvent * event, Viewport * viewport)                                 { return false; };

		virtual void cut_sublayer(TreeItem * item) { return; };
		virtual void copy_sublayer(TreeItem * item, uint8_t ** data, unsigned int * len) { return; };
		virtual bool paste_sublayer(TreeItem * item, Pickle & pickle) { return false; };
		virtual void delete_sublayer(TreeItem * item) { return; };

		virtual void change_coord_mode(CoordMode dest_mode) { return; };

		virtual time_t get_timestamp(void) const { return 0; };

		/* Treeview drag and drop method. called on the
		   destination layer. it is given a source and
		   destination layer, and the source and destination
		   iters in the tree view. */
		virtual void drag_drop_request(Layer * src, TreeIndex & src_item_index, void * GtkTreePath_dest_path);

		/* Read layer-specific data from Vik file. */
		virtual LayerDataReadStatus read_layer_data(QFile & file, const QString & dirpath);
		/* Write layer-specific data to Vik file. */
		virtual sg_ret write_layer_data(QFile & file) const;

		virtual void add_menu_items(QMenu & menu);

		virtual bool properties_dialog(Viewport * viewport);
		virtual bool properties_dialog();

		/* Get current, per-instance-of-layer, value of a layer parameter. The parameter is specified by its id.
		   @is_file_operation denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual SGVariant get_param_value(param_id_t param_id, bool is_file_operation) const;

		/* Returns true if needs to redraw due to changed param. */
		/* bool denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual bool set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation);

		/* Most of layer types aren't able to store child layers.
		   Those that do, may have zero child layers at the moment. */
		virtual int get_child_layers_count(void) const { return 0; };

		/* Return list of children layers. Most of layer types won't have child layers. */
		virtual std::list<Layer const *> get_child_layers(void) const { std::list<Layer const *> a_list; return a_list; };

		/* "type id string" means Layer's internal, fixed string
		   that can be used in .vik file operations and to
		   create internal IDs of objects. */
		static LayerType type_from_type_id_string(const QString & type_id_string);
		static QString get_type_id_string(LayerType type);
		QString get_type_id_string(void) const;

		/* "type ui label" means human-readable label that is
		   suitable for using in UI or in debug messages. */
		static QString get_type_ui_label(LayerType type);
		QString get_type_ui_label(void) const;


		static bool compare_timestamp_ascending(const Layer * first, const Layer * second);  /* Ascending: 1 -> 10 */
		static bool compare_timestamp_descending(const Layer * first, const Layer * second); /* Descending: 10 -> 1 */


		virtual std::list<TreeItem *> get_items_by_date(const QDate & search_date) const;


		const QString get_name(void) const;
		void set_name(const QString & new_name);


		void set_initial_parameter_values(void);

		Window * get_window(void);

		void ref();
		void unref();
		void weak_ref(LayerRefCB cb, void * obj);
		void weak_unref(LayerRefCB cb, void * obj);


		/* GUI. */
		QIcon get_icon(void);

		/* QString name; */ /* Inherited from TreeItem. */

		LayerType type;

	protected:

		LayerInterface * interface = NULL;
		QMenu * right_click_menu = NULL;

	protected slots:
		virtual void location_info_cb(void);

	signals:
		void layer_changed(const QString & layer_name);

	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Layer*)




#endif /* #ifndef _SG_LAYER_H_ */
