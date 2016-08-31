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

#include <glib.h>
#ifndef SLAVGPS_QT
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#endif

#ifndef SLAVGPS_QT
#include "uibuilder.h"
#include "viktreeview.h"
#include "slav_qt.h"
#endif
#include "vikviewport.h"
#include "globals.h"



typedef GObject VikLayer;




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




typedef struct _VikLayerInterface VikLayerInterface;




namespace SlavGPS {




	class Window;
	class LayerTRW;
	class LayerTool;




	typedef struct {
		bool has_oldcoord;
		VikCoord oldcoord;
	} ruler_tool_state_t;




	typedef struct {
		LayerTRW * trw; /* LayerTRW. */
		bool holding;
		bool moving;
		bool is_waypoint; /* Otherwise a track. */
		GdkGC * gc;
		int oldx, oldy;
	} tool_ed_t;



	typedef struct {
		GdkPixmap *pixmap;
		/* Track zoom bounds for zoom tool with shift modifier: */
		bool bounds_active;
		int start_x;
		int start_y;
	} zoom_tool_state_t;




	typedef void (* LayerRefCB) (void * ptr, GObject * dead_vml);




	class Layer {
	public:

		Layer();
		Layer(VikLayer * vl);
		~Layer();

		static void    marshall(Layer * layer, uint8_t ** data, int * len);
		void           marshall_params(uint8_t ** data, int * datalen);
		static Layer * unmarshall(uint8_t * data, int len, Viewport * viewport);
		void           unmarshall_params(uint8_t * data, int len, Viewport * viewport);

		static Layer * new_(LayerType layer_type, Viewport * viewport, bool interactive);

		void emit_update();


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

		virtual bool show_selected_viewport_menu(GdkEventButton * event, Viewport * viewport);

		virtual bool select_click(GdkEventButton * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_move(GdkEventMotion * event, Viewport * viewport, LayerTool * tool);
		virtual bool select_release(GdkEventButton * event, Viewport * viewport, LayerTool * tool);

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

		virtual void add_menu_items(GtkMenu * menu, void * panel);
		virtual bool sublayer_add_menu_items(GtkMenu * menu, void * panel, SublayerType sublayer_type, sg_uid_t sublayer_uid, GtkTreeIter * iter, Viewport * viewport);
		virtual char const * sublayer_rename_request(const char * newname, void * panel, SublayerType sublayer_type, sg_uid_t sublayer_uid, GtkTreeIter * iter);
		virtual bool sublayer_toggle_visible(SublayerType sublayer_type, sg_uid_t sublayer_uid);

		/* Do _not_ use this unless absolutely neccesary. Use the dynamic properties (see coordlayer for example).
		   Returns true if OK was pressed. */
		virtual bool properties(void * viewport);

		/* Normally only needed for layers with sublayers. This is called when they
		   are added to the treeview so they can add sublayers to the treeview. */
		virtual void realize(TreeView * tree_view, GtkTreeIter * layer_iter);

		virtual VikLayerParamData get_param(uint16_t id, bool is_file_operation) const;
		virtual bool set_param(uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation);


		static LayerType type_from_string(char const * str);



		static bool compare_timestamp_descending(Layer * first, Layer * second); /* kamilTODO: make arguments const. */
		static bool compare_timestamp_ascending(Layer * first, Layer * second);

		static bool compare_name_descending(Layer * first, Layer * second); /* kamilTODO: make arguments const. */
		static bool compare_name_ascending(Layer * first, Layer * second);




		char const * get_name();
		void rename(char const * new_name);
		void rename_no_copy(char * new_name);
		void draw_visible(Viewport * viewport);

		void set_defaults(Viewport * viewport);

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




		char * name;
		bool visible;
		bool realized;
		Viewport * viewport;  /* Simply a reference. */
		TreeView * tree_view; /* Simply a reference. */
		GtkTreeIter iter;

		/* For explicit "polymorphism" (function type switching). */
		LayerType type;


		GObject * vl;

		char type_string[10];

	protected:
		virtual void marshall(uint8_t ** data, int * len);
	};




	/* void * is tool-specific state created in the constructor. */
	typedef LayerTool * (*VikToolConstructorFunc) (Window *, Viewport *);
	typedef void (*VikToolDestructorFunc) (LayerTool *);
	typedef VikLayerToolFuncStatus (*VikToolMouseFunc) (Layer *, GdkEventButton *, LayerTool *);
	typedef VikLayerToolFuncStatus (*VikToolMouseMoveFunc) (Layer *, GdkEventMotion *, LayerTool *);
	typedef void (*VikToolActivationFunc) (Layer *, LayerTool *);
	typedef bool (*VikToolKeyFunc) (Layer *, GdkEventKey *, LayerTool *);




	class LayerTool {

	public:
		LayerTool(Window * window, Viewport * viewport, LayerType layer_type);
		~LayerTool();

		VikToolActivationFunc activate = NULL;
		VikToolActivationFunc deactivate = NULL;
		VikToolMouseFunc click = NULL;
		VikToolMouseMoveFunc move = NULL;
		VikToolMouseFunc release = NULL;
		VikToolKeyFunc key_press = NULL; /* Return false if we don't use the key press -- should return false most of the time if we want any shortcuts / UI keybindings to work! use sparingly. */

		GtkRadioActionEntry radioActionEntry = { NULL, NULL, NULL, NULL, NULL, 0 };

		bool pan_handler = false; /* Call click & release funtions even when 'Pan Mode' is on. */
		GdkCursorType cursor_type = GDK_CURSOR_IS_PIXMAP;
		GdkPixdata const * cursor_data = NULL;
		GdkCursor const * cursor = NULL;

		Window * window = NULL;
		Viewport * viewport = NULL;

		tool_ed_t * ed = NULL;
		ruler_tool_state_t * ruler = NULL;
		zoom_tool_state_t * zoom = NULL;

		LayerType layer_type;
	};




	void layer_init(void);




}




#ifdef __cplusplus
extern "C" {
#endif



/* Layer interface functions. */

typedef SlavGPS::Layer *         (* VikLayerFuncUnmarshall)    (uint8_t *, int, SlavGPS::Viewport *);
/* Returns true if needs to redraw due to changed param. */
/* In parameter bool denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
typedef bool                     (* VikLayerFuncSetParam)      (SlavGPS::Layer *, uint16_t, VikLayerParamData, SlavGPS::Viewport *, bool);
/* In parameter bool denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
typedef VikLayerParamData        (* VikLayerFuncGetParam)      (SlavGPS::Layer const *, uint16_t, bool);
typedef void                     (* VikLayerFuncChangeParam)   (GtkWidget *, ui_change_values *);



/* See vik_layer_* for function parameter names. */
struct _VikLayerInterface {
	const char *                      fixed_layer_name; /* Used in .vik files - this should never change to maintain file compatibility. */
	const char *                      name;             /* Translate-able name used for display purposes. */
	const char *                      accelerator;
	const GdkPixdata *                icon;

	SlavGPS::VikToolConstructorFunc layer_tool_constructors[7];
	SlavGPS::LayerTool **              layer_tools;

	uint16_t                           tools_count;

	/* For I/O reading to and from .vik files -- params like coordline width, color, etc. */
	VikLayerParam *                   params;
	uint16_t                           params_count;
	char **                          params_groups;
	uint8_t                            params_groups_count;

	/* Menu items to be created. */
	VikStdLayerMenuItem               menu_items_selection;

	VikLayerFuncUnmarshall            unmarshall;

	/* For I/O. */
	VikLayerFuncSetParam              set_param;
	VikLayerFuncGetParam              get_param;
	VikLayerFuncChangeParam           change_param;
};

VikLayerInterface * vik_layer_get_interface(SlavGPS::LayerType layer_type);
bool layer_set_param(SlavGPS::Layer * layer, uint16_t id, VikLayerParamData data, SlavGPS::Viewport * viewport, bool is_file_operation);
VikLayerParamData layer_get_param(SlavGPS::Layer const * layer, uint16_t id, bool is_file_operation);


/* GUI. */
uint16_t vik_layer_get_menu_items_selection(SlavGPS::Layer * layer);
bool vik_layer_properties(SlavGPS::Layer * layer, SlavGPS::Viewport * viewport);

bool vik_layer_selected(SlavGPS::Layer * layer, SlavGPS::SublayerType sublayer_type, sg_uid_t sublayer_uid, SlavGPS::TreeItemType type);

/* TODO: put in layerspanel. */
GdkPixbuf * vik_layer_load_icon(SlavGPS::LayerType layer_type);

void vik_layer_emit_update_secondary(SlavGPS::Layer * layer); /* To be called by aggregate layer only. Doesn't set the trigger. */
void vik_layer_emit_update_although_invisible(SlavGPS::Layer * layer);


typedef struct {
	VikLayerParamData data;
	VikLayerParamType type;
} VikLayerTypedParamData;

void vik_layer_typed_param_data_free(void * gp);
VikLayerTypedParamData * vik_layer_typed_param_data_copy_from_data(VikLayerParamType type, VikLayerParamData val);
VikLayerTypedParamData * vik_layer_data_typed_param_copy_from_string(VikLayerParamType type, const char * str);




#ifdef __cplusplus
}
#endif




#endif /* #ifndef _SG_LAYER_H_ */
