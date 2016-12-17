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
	IGNORED = 0,
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
		bool has_start_coord;
		VikCoord start_coord;
		bool invalidate_start_coord; /* Discard/invalidate ->start_coord on release of left mouse button? */
	} ruler_tool_state_t;




	typedef struct {
		LayerTRW * trw = NULL; /* LayerTRW. */
		bool holding = false;
		bool moving = false;
		bool is_waypoint = false; /* Otherwise a track. */
		QPen * ed_pen = NULL;
		int oldx = 0;
		int oldy = 0;
	} tool_ed_t;



	typedef struct {
		GdkPixmap *pixmap;
		/* Track zoom bounds for zoom tool with shift modifier: */
		bool bounds_active;
		int start_x;
		int start_y;
	} zoom_tool_state_t;




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
		void           unmarshall_params(uint8_t * data, int len, Viewport * viewport);

		static Layer * new_(LayerType layer_type, Viewport * viewport, bool interactive);

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

		virtual bool show_selected_viewport_menu(QMouseEvent * event, Viewport * viewport);

		virtual bool select_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_move(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_release(QMouseEvent * event, Viewport * viewport, LayerTool * tool);

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
		virtual void realize(TreeView * tree_view, TreeIndex const & layer_index);

		/* bool denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual ParameterValue get_param_value(param_id_t id, bool is_file_operation) const;

		/* Returns true if needs to redraw due to changed param. */
		/* bool denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual bool set_param_value(uint16_t id, ParameterValue param_value, Viewport * viewport, bool is_file_operation);


		static LayerType type_from_string(char const * str);



		static bool compare_timestamp_descending(Layer * first, Layer * second); /* kamilTODO: make arguments const. */
		static bool compare_timestamp_ascending(Layer * first, Layer * second);

		static bool compare_name_descending(Layer * first, Layer * second); /* kamilTODO: make arguments const. */
		static bool compare_name_ascending(Layer * first, Layer * second);




		char const * get_name();
		void rename(char const * new_name);
		void rename_no_copy(char * new_name);
		void draw_visible(Viewport * viewport);

		void set_initial_parameter_values(Viewport * viewport);

		Window * get_window(void);

		void ref();
		void unref();
		void weak_ref(LayerRefCB cb, void * obj);
		void weak_unref(LayerRefCB cb, void * obj);

		bool the_same_object(Layer const * layer);
		void disconnect_layer_signal(Layer * layer);



		static bool properties_dialog(Layer * layer, Viewport * viewport);


		char * name = NULL;
		bool visible = true;
		bool realized = false;
		Viewport * viewport = NULL;  /* Simply a reference. */

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




	/* void * is tool-specific state created in the constructor. */
	typedef LayerTool * (*ToolConstructorFunc) (Window *, Viewport *);
	typedef LayerToolFuncStatus (*ToolMouseFunc) (Layer *, QMouseEvent *, LayerTool *);
	typedef LayerToolFuncStatus (*ToolMouseMoveFunc) (Layer *, QMouseEvent *, LayerTool *);
	typedef void (*ToolActivationFunc) (Layer *, LayerTool *);
	typedef bool (*ToolKeyFunc) (Layer *, GdkEventKey *, LayerTool *);



	struct ActionEntry {
		char const * stock_id;
		char const * label;
		char const * accelerator;
		char const * tooltip;
		int value;
	};



	class LayerTool {

	public:
		LayerTool(Window * window, Viewport * viewport, LayerType layer_type);
		~LayerTool();

		QString get_description() const;

		ToolActivationFunc activate = NULL;
		ToolActivationFunc deactivate = NULL;
		ToolMouseFunc click = NULL;
		ToolMouseFunc double_click = NULL;
		ToolMouseMoveFunc move = NULL;
		ToolMouseFunc release = NULL;
		ToolKeyFunc key_press = NULL; /* Return false if we don't use the key press -- should return false most of the time if we want any shortcuts / UI keybindings to work! use sparingly. */

		ActionEntry radioActionEntry = { NULL, NULL, NULL, NULL, 0 };

		bool pan_handler = false; /* Call click & release funtions even when 'Pan Mode' is on. */

		QCursor const * cursor_click = NULL;
		QCursor const * cursor_release = NULL;

		Window * window = NULL;
		Viewport * viewport = NULL;

		tool_ed_t * ed = NULL;
		ruler_tool_state_t * ruler = NULL;
		zoom_tool_state_t * zoom = NULL;

		LayerType layer_type; /* Can be set to LayerType::NUM_TYPES to indicate "generic" (non-layer-specific) tool (zoom, select, etc.). */
		QString id_string;    /* E.g. "generic.zoom", or "dem.download". For internal use, not visible to user. */

		char layer_type_string[30]; /* For debug purposes only. */
	};




	void layer_init(void);




	/* Layer interface functions. */
	typedef Layer * (* LayerFuncUnmarshall)  (uint8_t *, int, Viewport *);
	typedef void    (* LayerFuncChangeParam) (GtkWidget *, ui_change_values *);




	class LayerInterface {
	public:
		const char  * layer_type_string; /* Used in .vik files - this should never change to maintain file compatibility. */
		const char  * name;              /* Translate-able name used for display purposes. */
		const char  * accelerator;
		const QIcon * icon;

		ToolConstructorFunc layer_tool_constructors[7];
		LayerTool           ** layer_tools;
		uint16_t               tools_count;


		/* For I/O reading to and from .vik files -- params like coordline width, color, etc. */
		Parameter * params;
		uint16_t    params_count;
		char     ** params_groups;
		uint8_t     params_groups_count;

		/* Menu items to be created. */
		LayerMenuItem    menu_items_selection;

		LayerFuncUnmarshall unmarshall;

		/* For I/O. */
		LayerFuncChangeParam           change_param;

		std::map<param_id_t, Parameter *> * layer_parameters;
		std::map<param_id_t, ParameterValue> * parameter_value_defaults;
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
