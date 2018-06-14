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




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




#include <cstdint>

#include <QObject>
#include <QIcon>
#include <QMouseEvent>
#include <QMenu>
#include <QString>
#include <QPen>
#include <QDebug>

#include "tree_view.h"
#include "globals.h"
#include "clipboard.h"




namespace SlavGPS {




	class Viewport;
	class Window;
	class LayerTRW;
	class LayerTool;
	class LayerInterface;
	class LayersPanel;
	class SGVariant;
	class ParameterSpecification;
	enum class LayerDataReadStatus;
	enum class CoordMode;




	enum class SublayerType {
		NONE,
		TRACKS,
		WAYPOINTS,
		TRACK,
		WAYPOINT,
		ROUTES,
		ROUTE
	};




	enum class LayerType {
		AGGREGATE = 0,
		TRW,
		COORD,
		GEOREF,
		GPS,
		MAP,
		DEM,
#ifdef HAVE_LIBMAPNIK
		MAPNIK,
#endif
		NUM_TYPES // Also use this value to indicate no layer association
	};

	LayerType& operator++(LayerType& layer_type);
 	QDebug operator<<(QDebug debug, const LayerType & layer_type);




	typedef void (* LayerRefCB) (void * ptr, void * dead_vml);




	class Layer : public TreeItem {
		Q_OBJECT

	public:

		Layer();
		~Layer();

		static void    marshall(Layer * layer, Pickle & pickle);
		void           marshall_params(Pickle & pickle);
		static Layer * unmarshall(Pickle & pickle, Viewport * viewport);
		void           unmarshall_params(Pickle & pickle);

		static Layer * construct_layer(LayerType layer_type, Viewport * viewport, bool interactive = false);

		void emit_layer_changed(const QString & where);
		void emit_layer_changed_although_invisible(const QString & where);

		const LayerInterface & get_interface(void) const;
		static LayerInterface * get_interface(LayerType layer_type);
		void configure_interface(LayerInterface * intf, ParameterSpecification * param_specs);
		static void preconfigure_interfaces(void);

		/* Rarely used, this is called after a read operation
		   or properties box is run.  usually used to create
		   GC's that depend on params, but GC's can also be
		   created from create() or set_param(). */
		virtual void post_read(Viewport * viewport, bool from_file);

		virtual void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected) { return; };

		virtual QString get_tooltip(void);

		bool handle_selection_in_tree(void);

		/* Methods for generic "Select" tool. */
		virtual bool handle_select_tool_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool)         { return false; };
		virtual bool handle_select_tool_double_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool)  { return false; };
		virtual bool handle_select_tool_move(QMouseEvent * event, Viewport * viewport, LayerTool * tool)          { return false; };
		virtual bool handle_select_tool_release(QMouseEvent * event, Viewport * viewport, LayerTool * tool)       { return false; };
		virtual bool handle_select_tool_context_menu(QMouseEvent * event, Viewport * viewport)                    { return false; };

		/* kamilTODO: consider removing them from Layer. They are overriden only in LayerTRW. */
		virtual void set_menu_selection(TreeItemOperation selection) { return; };
		virtual TreeItemOperation get_menu_selection(void) const { return TreeItemOperation::None; };

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
		virtual LayerDataReadStatus read_layer_data(FILE * file, char const * dirpath);
		/* Write layer-specific data to Vik file. */
		virtual void write_layer_data(FILE * file) const;

		virtual void add_menu_items(QMenu & menu);

		virtual bool properties_dialog(Viewport * viewport);
		virtual bool properties_dialog();

		/* Get current, per-instance-of-layer, value of a layer parameter. The parameter is specified by its id.
		   @is_file_operation denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual SGVariant get_param_value(param_id_t id, bool is_file_operation) const;

		/* Returns true if needs to redraw due to changed param. */
		/* bool denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);


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


		static bool compare_timestamp_descending(const Layer * first, const Layer * second);
		static bool compare_timestamp_ascending(const Layer * first, const Layer * second);

		static bool compare_name_descending(const Layer * first, const Layer * second);
		static bool compare_name_ascending(const Layer * first, const Layer * second);



		const QString get_name(void) const;
		void set_name(const QString & new_name);


		void set_initial_parameter_values(void);

		Window * get_window(void);

		void ref();
		void unref();
		void weak_ref(LayerRefCB cb, void * obj);
		void weak_unref(LayerRefCB cb, void * obj);

		bool the_same_object(const Layer * layer) const;

		/* If there are any highlighted/selected elements in
		   the layer, unhighlight them.  Return true if any
		   element in layer has been un-highlighted and the
		   layer needs to be redrawn. */
		virtual bool clear_highlight(void) { return false; }

		/* GUI. */
		QIcon get_icon(void);
		TreeItemOperation get_menu_items_selection(void);

		/* QString name; */ /* Inherited from TreeItem. */

		LayerType type;

	protected:
		virtual void marshall(Pickle & pickle);

		LayerInterface * interface = NULL;
		QMenu * right_click_menu = NULL;

	protected slots:
		virtual void location_info_cb(void);

	signals:
		void layer_changed(const QString & layer_name);

	private:
		sg_uid_t layer_instance_uid = SG_UID_INITIAL;
	};




	void layer_init(void);




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Layer*)




#endif /* #ifndef _SG_LAYER_H_ */
