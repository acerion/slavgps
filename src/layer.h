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




#include <cstdio>
#include <cstdint>
#include <map>

#include <QObject>
#include <QTreeWidgetItem>
#include <QPersistentModelIndex>
#include <QIcon>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCursor>
#include <QMenu>
#include <QString>
#include <QPixmap>
#include <QPen>

#include "ui_builder.h"
#include "tree_view.h"
#include "viewport.h"
#include "globals.h"




/* I think most of these are ignored, returning GRAB_FOCUS grabs the
 * focus for mouse move, mouse click, release always grabs
 * focus. Focus allows key presses to be handled.
 *
 * It used to be that, if ignored, Viking could look for other layers.
 * this was useful for clicking a way/trackpoint in any layer, if no
 * layer was selected (find way/trackpoint).
 */
enum class ToolStatus {
	IGNORED = 0,
	ACK,
	ACK_REDRAW_ABOVE,
	ACK_REDRAW_ALL,
	ACK_REDRAW_IF_VISIBLE,
	ACK_GRAB_FOCUS, /* Only for move. */
};




namespace SlavGPS {




	class Window;
	class LayerTRW;
	class LayerTool;
	class LayerInterface;
	class trw_menu_sublayer_t;
	class LayersPanel;




	typedef int GtkWidget; /* TODO: remove sooner or later. */




	/* Which items shall be present in context menu for a layer? */
	enum class LayerMenuItem {
		NONE       = 0x0000,
		PROPERTIES = 0x0001,
		CUT        = 0x0002,
		COPY       = 0x0004,
		PASTE      = 0x0008,
		DELETE     = 0x0010,
		NEW        = 0x0020,
		ALL        = 0xffff,
	};
#if 0
	LayerMenuItem operator&(const LayerMenuItem& arg1, const LayerMenuItem& arg2);
	LayerMenuItem operator|(const LayerMenuItem& arg1, const LayerMenuItem& arg2);
	LayerMenuItem operator~(const LayerMenuItem& arg);
#endif



	/*
	  Stuff related to editing TRW's sublayers.
	  To be more precise: to moving points constituting TRW's sublayers: waypoints or trackpoint.
	  The points can be selected by either TRW-specific edit tools, or by generic select tool.
	*/
	class SublayerEdit {
	public:
		SublayerEdit();

		LayerTRW * trw = NULL;
		bool holding = false;
		bool moving = false;
		SublayerType sublayer_type = SublayerType::NONE; /* WAYPOINT or TRACK or ROUTE. */
		QPen pen;
		int oldx = 0;
		int oldy = 0;
	};




	typedef void (* LayerRefCB) (void * ptr, void * dead_vml);




	class Sublayer : public TreeItem {
		Q_OBJECT
	public:
		Sublayer() {}
		Sublayer(SublayerType t) { sublayer_type = t; tree_item_type = TreeItemType::SUBLAYER; }
		~Sublayer() {};

		sg_uid_t get_uid(void);

	//protected:
		SublayerType sublayer_type;
		sg_uid_t uid = SG_UID_INITIAL;
	};




	class Layer : public TreeItem {
		Q_OBJECT

	public:

		Layer();
		~Layer();

		static void    marshall(Layer * layer, uint8_t ** data, int * len);
		void           marshall_params(uint8_t ** data, int * datalen);
		static Layer * unmarshall(uint8_t * data, int len, Viewport * viewport);
		void           unmarshall_params(uint8_t * data, int len);

		static Layer * construct_layer(LayerType layer_type, Viewport * viewport);

		void emit_changed(void);
		void emit_changed_although_invisible(void);

		LayerInterface * get_interface(void);
		static LayerInterface * get_interface(LayerType layer_type);
		void configure_interface(LayerInterface * intf, Parameter * parameters);
		static void preconfigure_interfaces(void);

		/* Layer interface methods. */

		/* Rarely used, this is called after a read operation
		   or properties box is run.  usually used to create
		   GC's that depend on params, but GC's can also be
		   created from create() or set_param(). */
		virtual void post_read(Viewport * viewport, bool from_file);

		virtual void draw(Viewport * viewport);
		virtual QString tooltip();
		virtual QString sublayer_tooltip(Sublayer * sublayer);

		virtual bool kamil_selected(TreeItemType type, Sublayer * sublayer);
		bool layer_selected(TreeItemType type, Sublayer * sublayer);

		/* Methods for generic "Select" tool. */
		virtual bool select_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_move(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_release(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_tool_context_menu(QMouseEvent * event, Viewport * viewport);

		/* kamilTODO: consider removing them from Layer. They are overriden only in LayerTRW. */
		virtual void set_menu_selection(LayerMenuItem selection);
		virtual LayerMenuItem get_menu_selection();

		virtual void cut_sublayer(Sublayer * sublayer);
		virtual void copy_sublayer(Sublayer * sublayer, uint8_t ** item, unsigned int * len);
		virtual bool paste_sublayer(Sublayer * sublayer, uint8_t * item, size_t len);
		virtual void delete_sublayer(Sublayer * sublayer);

		virtual void change_coord_mode(CoordMode dest_mode);

		virtual time_t get_timestamp();

		/* Treeview drag and drop method. called on the
		   destination layer. it is given a source and
		   destination layer, and the source and destination
		   iters in the treeview. */
		virtual void drag_drop_request(Layer * src, TreeIndex * src_item_iter, void * GtkTreePath_dest_path);

		virtual int read_file(FILE * f, char const * dirpath);
		virtual void write_file(FILE * f) const;

		virtual void add_menu_items(QMenu & menu);
		virtual bool sublayer_add_menu_items(QMenu & menu);
		virtual QString sublayer_rename_request(Sublayer * sublayer, const QString & new_name, LayersPanel * panel);
		virtual bool sublayer_toggle_visible(Sublayer * sublayer);

		virtual bool properties_dialog(Viewport * viewport);

		/* Normally only needed for layers with sublayers. This is called when they
		   are added to the treeview so they can add sublayers to the treeview. */
		virtual void connect_to_tree(TreeView * tree_view, TreeIndex const & layer_index);

		/* Get current, per-instance-of-layer, value of a layer parameter. The parameter is specified by its id.
		   @is_file_operation denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual SGVariant get_param_value(param_id_t id, bool is_file_operation) const;

		/* Returns true if needs to redraw due to changed param. */
		/* bool denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);


		/* "type string" means Layer's internal, fixed string
		   that can be used in .vik file operations and to
		   create internal IDs of objects. */
		static LayerType type_from_type_string(const QString & type_string);
		static QString get_type_string(LayerType type);
		QString get_type_string(void) const;

		/* "type ui label" means human-readable label that is
		   suitable for using in UI or in debug messages. */
		static QString get_type_ui_label(LayerType type);
		QString get_type_ui_label(void) const;


		static bool compare_timestamp_descending(Layer * first, Layer * second); /* kamilTODO: make arguments const. */
		static bool compare_timestamp_ascending(Layer * first, Layer * second);

		static bool compare_name_descending(Layer * first, Layer * second); /* kamilTODO: make arguments const. */
		static bool compare_name_ascending(Layer * first, Layer * second);




		const QString & get_name();
		void rename(const QString & new_name);
		void draw_visible(Viewport * viewport);

		void set_initial_parameter_values(void);

		Window * get_window(void);

		void ref();
		void unref();
		void weak_ref(LayerRefCB cb, void * obj);
		void weak_unref(LayerRefCB cb, void * obj);

		bool the_same_object(Layer const * layer);
		void disconnect_layer_signal(Layer * layer);


		/* GUI. */
		QIcon get_icon(void);
		LayerMenuItem get_menu_items_selection(void);



		QString name;
		bool visible = true;
		bool connected_to_tree = false; /* A layer cannot be totally stand-alone, it has to be a part of layers tree. */

		/* For explicit "polymorphism" (function type switching). */
		LayerType type;

		trw_menu_sublayer_t * menu_data = NULL;

	protected:
		virtual void marshall(uint8_t ** data, int * len);

		LayerInterface * interface = NULL;
		QMenu * right_click_menu = NULL;

	public slots:
		void visibility_toggled_cb(QStandardItem * item);
		void child_layer_changed_cb(void);


	protected slots:
		virtual void location_info_cb(void);

	signals:
		void changed(void);
	};




	typedef LayerTool * (*ToolConstructorFunc) (Window *, Viewport *);




	class LayerTool {

	public:
		LayerTool(Window * window, Viewport * viewport, LayerType layer_type);
		~LayerTool();

		QString get_description(void) const;

		virtual ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event)        { return ToolStatus::IGNORED; }
		virtual ToolStatus handle_mouse_double_click(Layer * layer, QMouseEvent * event) { return ToolStatus::IGNORED; }
		virtual ToolStatus handle_mouse_move(Layer * layer, QMouseEvent * event)         { return ToolStatus::IGNORED; }
		virtual ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event)      { return ToolStatus::IGNORED; }
		virtual ToolStatus handle_key_press(Layer * layer, QKeyEvent * event)            { return ToolStatus::IGNORED; }; /* TODO: where do we call this function? */
		virtual void activate_tool(Layer * layer) { return; };
		virtual void deactivate_tool(Layer * layer) { return; };

		/* Start holding a TRW point. */
		void sublayer_edit_click(int x, int y);
		/* A TRW point that is being held is now also moved around. */
		void sublayer_edit_move(int x, int y);
		/* Stop holding (i.e. release) a TRW point. */
		void sublayer_edit_release(void);

		QString action_icon_path;
		QString action_label;
		QString action_tooltip;
		QKeySequence action_accelerator;
		QAction * qa = NULL;

		bool pan_handler = false; /* Call click & release funtions even when 'Pan Mode' is on. */

		QCursor const * cursor_click = NULL;
		QCursor const * cursor_release = NULL;

		Window * window = NULL;
		Viewport * viewport = NULL;

		/* This should be moved to class LayerToolTRW. */
		SublayerEdit * sublayer_edit = NULL;

		LayerType layer_type; /* Can be set to LayerType::NUM_TYPES to indicate "generic" (non-layer-specific) tool (zoom, select, etc.). */
		int id;
		QString id_string;    /* E.g. "generic.zoom", or "dem.download". For internal use, not visible to user. */

		char debug_string[100]; /* For debug purposes only. */
	};




	void layer_init(void);




	class LayerInterface {

	friend class Layer;

	public:
		LayerInterface();

		virtual Layer * unmarshall(uint8_t * data, int len, Viewport * viewport);
		virtual void change_param(GtkWidget *, ui_change_values *);

		QKeySequence action_accelerator;
		QIcon action_icon;

		std::map<int, ToolConstructorFunc> layer_tool_constructors;  /* Tool index -> Layer Tool constructor function. */
		std::map<int, LayerTool *>         layer_tools;              /* Tool index -> Layer Tool. */

		/* Menu items (actions) to be created and put into a
		   context menu for given layer type. */
		LayerMenuItem menu_items_selection = LayerMenuItem::NONE;


		/* Specification of parameters in each layer type is
		   stored in 'parameters_c' C array.  During
		   application startup, in Layer::preconfigure_interfaces(),
		   pointers to these parameters in C array are stored
		   in 'parameters' container. The parameters can be later
		   accessed in C++-like fashion.

		   Each layer type stores (here, in layer interface) a
		   set of default values of parameters, to be used
		   when user creates a new instance of layer of type X.

		   Parameters can be combined into groups, they names
		   of the groups are in parameter_groups. */
		Parameter * parameters_c = NULL;
		std::map<param_id_t, Parameter *> parameters;
		std::map<param_id_t, SGVariant>  parameter_default_values;
		const char ** parameter_groups = NULL;

		struct {
			QString new_layer;      /* Menu "Layers" -> "New type-X Layer". */
			QString layer_type;     /* Stand-alone label for layer's type. Not to be concatenated with other string to form longer labels. */
			QString layer_defaults; /* Title of "Default settings of layer type X" dialog window. */
		} ui_labels;

	protected:
		QString fixed_layer_type_string; /* Used in .vik files - this should never change to maintain file compatibility. */
	};




	class trw_menu_sublayer_t {
	public:
		Sublayer * sublayer = NULL;
		Viewport * viewport = NULL;
		LayersPanel * layers_panel = NULL;

		bool confirm = false;
		void * misc = NULL;
		QString string;
	};




}




Q_DECLARE_METATYPE(SlavGPS::Layer*)




#endif /* #ifndef _SG_LAYER_H_ */
