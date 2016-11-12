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
typedef enum {
	VIK_LAYER_TOOL_IGNORED=0,
	VIK_LAYER_TOOL_ACK,
	VIK_LAYER_TOOL_ACK_REDRAW_ABOVE,
	VIK_LAYER_TOOL_ACK_REDRAW_ALL,
	VIK_LAYER_TOOL_ACK_REDRAW_IF_VISIBLE,
	VIK_LAYER_TOOL_ACK_GRAB_FOCUS, /* only for move */
} VikLayerToolFuncStatus;




typedef enum {
	VIK_MENU_ITEM_PROPERTY =    1,
	VIK_MENU_ITEM_CUT      =    2,
	VIK_MENU_ITEM_COPY     =    4,
	VIK_MENU_ITEM_PASTE    =    8,
	VIK_MENU_ITEM_DELETE   =   16,
	VIK_MENU_ITEM_ALL      = 0xff
} VikStdLayerMenuItem;




typedef struct _LayerInterface LayerInterface;
struct _trw_menu_sublayer_t;



namespace SlavGPS {




	class Window;
	class LayerTRW;
	class LayerTool;




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




	class Layer : public QObject {
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
		virtual char const * tooltip();
		virtual char const * sublayer_tooltip(SublayerType sublayer_type, sg_uid_t sublayer_uid);

		virtual bool selected(SublayerType sublayer_type, sg_uid_t sublayer_uid, TreeItemType type);
		bool layer_selected(SublayerType sublayer_type, sg_uid_t sublayer_uid, TreeItemType type);

		virtual bool show_selected_viewport_menu(QMouseEvent * event, Viewport * viewport);

		virtual bool select_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_move(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_release(QMouseEvent * event, Viewport * viewport, LayerTool * tool);

		/* kamilTODO: consider removing them from Layer. They are overriden only in LayerTRW. */
		virtual void set_menu_selection(uint16_t selection);
		virtual uint16_t get_menu_selection();

		virtual void cut_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid);
		virtual void copy_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid, uint8_t ** item, unsigned int * len);
		virtual bool paste_sublayer(SublayerType sublayer_type, uint8_t * item, size_t len);
		virtual void delete_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid);

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
		virtual char const * sublayer_rename_request(const char * newname, void * panel, SublayerType sublayer_type, sg_uid_t sublayer_uid, TreeIndex * parent_index);
		virtual bool sublayer_toggle_visible(SublayerType sublayer_type, sg_uid_t sublayer_uid);

		virtual bool properties_dialog(Viewport * viewport);

		/* Normally only needed for layers with sublayers. This is called when they
		   are added to the treeview so they can add sublayers to the treeview. */
		virtual void realize(TreeView * tree_view, TreeIndex * layer_index);

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

		GtkWindow * get_toolkit_window(void);
		Window * get_window(void);
		void * get_toolkit_object(void);
		static Layer * get_layer(VikLayer * vl);

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
		TreeView * tree_view = NULL; /* Simply a reference. */
		TreeIndex * index = NULL;

		/* For explicit "polymorphism" (function type switching). */
		LayerType type;

		char type_string[30] = { 0 };

		struct _trw_menu_sublayer_t * menu_data = NULL;

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
	typedef LayerTool * (*VikToolConstructorFunc) (Window *, Viewport *);
	typedef void (*VikToolDestructorFunc) (LayerTool *);
	typedef VikLayerToolFuncStatus (*VikToolMouseFunc) (Layer *, QMouseEvent *, LayerTool *);
	typedef VikLayerToolFuncStatus (*VikToolMouseMoveFunc) (Layer *, QMouseEvent *, LayerTool *);
	typedef void (*VikToolActivationFunc) (Layer *, LayerTool *);
	typedef bool (*VikToolKeyFunc) (Layer *, GdkEventKey *, LayerTool *);



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

		VikToolActivationFunc activate = NULL;
		VikToolActivationFunc deactivate = NULL;
		VikToolMouseFunc click = NULL;
		VikToolMouseFunc double_click = NULL;
		VikToolMouseMoveFunc move = NULL;
		VikToolMouseFunc release = NULL;
		VikToolKeyFunc key_press = NULL; /* Return false if we don't use the key press -- should return false most of the time if we want any shortcuts / UI keybindings to work! use sparingly. */

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




}




Q_DECLARE_METATYPE(SlavGPS::Layer*);




#ifdef __cplusplus
extern "C" {
#endif



/* Layer interface functions. */

typedef SlavGPS::Layer *         (* VikLayerFuncUnmarshall)    (uint8_t *, int, SlavGPS::Viewport *);
typedef void                     (* VikLayerFuncChangeParam)   (GtkWidget *, ui_change_values *);





/* See vik_layer_* for function parameter names. */
struct _LayerInterface {
	const char *                      layer_type_string; /* Used in .vik files - this should never change to maintain file compatibility. */
	const char *                      name;             /* Translate-able name used for display purposes. */
	const char *                      accelerator;
	const QIcon * icon;

	SlavGPS::VikToolConstructorFunc layer_tool_constructors[7];
	SlavGPS::LayerTool **              layer_tools;

	uint16_t                           tools_count;


	/* For I/O reading to and from .vik files -- params like coordline width, color, etc. */
	Parameter *                       params;
	uint16_t                          params_count;
	char **                           params_groups;
	uint8_t                           params_groups_count;

	/* Menu items to be created. */
	VikStdLayerMenuItem               menu_items_selection;

	VikLayerFuncUnmarshall            unmarshall;

	/* For I/O. */
	VikLayerFuncChangeParam           change_param;

	std::map<param_id_t, Parameter *> * layer_parameters;
	std::map<param_id_t, ParameterValue> * parameter_value_defaults;
};



/* GUI. */
uint16_t vik_layer_get_menu_items_selection(SlavGPS::Layer * layer);


/* TODO: put in layerspanel. */
GdkPixbuf * vik_layer_load_icon(SlavGPS::LayerType layer_type);



typedef struct {
	ParameterValue data;
	ParameterType type;
} ParameterValueTyped;


void vik_layer_typed_param_data_free(void * gp);
ParameterValueTyped * vik_layer_typed_param_data_copy_from_data(ParameterType type, ParameterValue val);
ParameterValueTyped * vik_layer_data_typed_param_copy_from_string(ParameterType type, const char * str);



typedef struct _trw_menu_sublayer_t {
	void * layers_panel;
	SlavGPS::SublayerType sublayer_type;
	sg_uid_t sublayer_uid;
	bool confirm;
	SlavGPS::Viewport * viewport;
	SlavGPS::TreeIndex * index;
	void * misc;
} trw_menu_sublayer_t;



#ifdef __cplusplus
}
#endif




#endif /* #ifndef _SG_LAYER_H_ */
