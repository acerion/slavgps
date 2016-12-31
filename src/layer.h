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
 *
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

#include "uibuilder.h"
#include "tree_view.h"
#include "slav_qt.h"
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
enum class LayerToolFuncStatus {
	IGNORE = 0,
	ACK,
	ACK_REDRAW_ABOVE,
	ACK_REDRAW_ALL,
	ACK_REDRAW_IF_VISIBLE,
	ACK_GRAB_FOCUS, /* Only for move. */
};




typedef enum {
	VIK_MENU_ITEM_PROPERTY =    1,
	VIK_MENU_ITEM_CUT      =    2,
	VIK_MENU_ITEM_COPY     =    4,
	VIK_MENU_ITEM_PASTE    =    8,
	VIK_MENU_ITEM_DELETE   =   16,
	VIK_MENU_ITEM_ALL      = 0xff
} LayerMenuItem;




namespace SlavGPS {




	class Window;
	class LayerTRW;
	class LayerTool;
	class LayerInterface;
	class trw_menu_sublayer_t;
	class LayersPanel;




	typedef struct {
		LayerTRW * trw = NULL; /* LayerTRW. */
		bool holding = false;
		bool moving = false;
		bool is_waypoint = false; /* Otherwise a track. */
		QPen * ed_pen = NULL;
		int oldx = 0;
		int oldy = 0;
	} tool_ed_t;




	typedef void (* LayerRefCB) (void * ptr, GObject * dead_vml);




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

		static Layer * new_(LayerType layer_type, Viewport * viewport);

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

		virtual bool selected(TreeItemType type, Sublayer * sublayer);
		bool layer_selected(TreeItemType type, Sublayer * sublayer);

		/* Methods for generic "Select" tool. */
		virtual bool select_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_move(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_release(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_tool_context_menu(QMouseEvent * event, Viewport * viewport);

		/* kamilTODO: consider removing them from Layer. They are overriden only in LayerTRW. */
		virtual void set_menu_selection(uint16_t selection);
		virtual uint16_t get_menu_selection();

		virtual void cut_sublayer(Sublayer * sublayer);
		virtual void copy_sublayer(Sublayer * sublayer, uint8_t ** item, unsigned int * len);
		virtual bool paste_sublayer(Sublayer * sublayer, uint8_t * item, size_t len);
		virtual void delete_sublayer(Sublayer * sublayer);

		virtual void change_coord_mode(VikCoordMode dest_mode);

		virtual time_t get_timestamp();

		/* Treeview drag and drop method. called on the
		   destination layer. it is given a source and
		   destination layer, and the source and destination
		   iters in the treeview. */
		virtual void drag_drop_request(Layer * src, GtkTreeIter * src_item_iter, GtkTreePath * dest_path);

		virtual int read_file(FILE * f, char const * dirpath);
		virtual void write_file(FILE * f) const;

		virtual void add_menu_items(QMenu & menu);
		virtual bool sublayer_add_menu_items(QMenu & menu);
		virtual char const * sublayer_rename_request(Sublayer * sublayer, const char * newname, LayersPanel * panel);
		virtual bool sublayer_toggle_visible(Sublayer * sublayer);

		virtual bool properties_dialog(Viewport * viewport);

		/* Normally only needed for layers with sublayers. This is called when they
		   are added to the treeview so they can add sublayers to the treeview. */
		virtual void connect_to_tree(TreeView * tree_view, TreeIndex const & layer_index);

		/* bool denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual ParameterValue get_param_value(param_id_t id, bool is_file_operation) const;

		/* Returns true if needs to redraw due to changed param. */
		/* bool denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual bool set_param_value(uint16_t id, ParameterValue param_value, bool is_file_operation);


		static LayerType type_from_string(char const * str);



		static bool compare_timestamp_descending(Layer * first, Layer * second); /* kamilTODO: make arguments const. */
		static bool compare_timestamp_ascending(Layer * first, Layer * second);

		static bool compare_name_descending(Layer * first, Layer * second); /* kamilTODO: make arguments const. */
		static bool compare_name_ascending(Layer * first, Layer * second);




		char const * get_name();
		void rename(char const * new_name);
		void rename_no_copy(char * new_name);
		void draw_visible(Viewport * viewport);

		void set_initial_parameter_values(void);

		Window * get_window(void);

		void ref();
		void unref();
		void weak_ref(LayerRefCB cb, void * obj);
		void weak_unref(LayerRefCB cb, void * obj);

		bool the_same_object(Layer const * layer);
		void disconnect_layer_signal(Layer * layer);



		char * name = NULL;
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

		QString get_description() const;

		virtual LayerToolFuncStatus click_(Layer * layer, QMouseEvent * event) { return LayerToolFuncStatus::IGNORE; }
		virtual LayerToolFuncStatus double_click_(Layer * layer, QMouseEvent * event) { return LayerToolFuncStatus::IGNORE; }
		virtual LayerToolFuncStatus move_(Layer * layer, QMouseEvent * event) { return LayerToolFuncStatus::IGNORE; }
		virtual LayerToolFuncStatus release_(Layer * layer, QMouseEvent * event) { return LayerToolFuncStatus::IGNORE; }
		virtual bool key_press_(Layer * layer, QKeyEvent * event) { return false; }; /* Return false if we don't use the key press -- should return false most of the time if we want any shortcuts / UI keybindings to work! use sparingly. */
		virtual void activate_(Layer * layer) { return; };
		virtual void deactivate_(Layer * layer) { return; };

		void marker_begin_move(int x, int y);
		void marker_moveto(int x, int y);
		void marker_end_move(void);

		QString action_icon_path;
		QString action_label;
		QString action_tooltip;
		QKeySequence action_accelerator;

		bool pan_handler = false; /* Call click & release funtions even when 'Pan Mode' is on. */

		QCursor const * cursor_click = NULL;
		QCursor const * cursor_release = NULL;

		Window * window = NULL;
		Viewport * viewport = NULL;

		tool_ed_t * ed = NULL;

		LayerType layer_type; /* Can be set to LayerType::NUM_TYPES to indicate "generic" (non-layer-specific) tool (zoom, select, etc.). */
		QString id_string;    /* E.g. "generic.zoom", or "dem.download". For internal use, not visible to user. */

		char debug_string[100]; /* For debug purposes only. */
	};




	void layer_init(void);




	class LayerInterface {
	public:
		LayerInterface();

		virtual Layer * unmarshall(uint8_t * data, int len, Viewport * viewport);
		virtual void change_param(GtkWidget *, ui_change_values *);

		/* For I/O reading to and from .vik files -- params like coordline width, color, etc. */
		Parameter * params = NULL;
		uint16_t    params_count = 0;
		char     ** params_groups = NULL;

		char    layer_type_string[30]; /* Used in .vik files - this should never change to maintain file compatibility. */
		QString layer_name;            /* Translate-able name used for display purposes. */

		QKeySequence action_accelerator;
		QIcon action_icon;

		std::map<int, ToolConstructorFunc> layer_tool_constructors;  /* Tool index -> Layer Tool constructor function. */
		std::map<int, LayerTool *>         layer_tools;              /* Tool index -> Layer Tool. */


		/* Menu items to be created. */
		LayerMenuItem menu_items_selection;

		std::map<param_id_t, Parameter *> * layer_parameters = NULL;
		std::map<param_id_t, ParameterValue> * parameter_value_defaults = NULL;
	};




	/* GUI. */
	uint16_t vik_layer_get_menu_items_selection(Layer * layer);


	/* TODO: put in layerspanel. */
	GdkPixbuf * vik_layer_load_icon(LayerType layer_type);



	typedef struct {
		ParameterValue data;
		ParameterType type;
	} ParameterValueTyped;


	void vik_layer_typed_param_data_free(void * gp);
	ParameterValueTyped * vik_layer_typed_param_data_copy_from_data(ParameterType type, ParameterValue val);
	ParameterValueTyped * vik_layer_data_typed_param_copy_from_string(ParameterType type, const char * str);




	class trw_menu_sublayer_t {
	public:
		Sublayer * sublayer = NULL;
		Viewport * viewport = NULL;
		LayersPanel * layers_panel = NULL;

		bool confirm = false;
		void * misc = NULL;
	};




}




Q_DECLARE_METATYPE(SlavGPS::Layer*);




#endif /* #ifndef _SG_LAYER_H_ */
