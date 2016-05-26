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
#ifndef _VIKING_LAYER_H
#define _VIKING_LAYER_H

#include <stdio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <stdbool.h>
#include <stdint.h>


#include "uibuilder.h"
#include "vikwindow.h"
#include "viktreeview.h"
#include "vikviewport.h"

#ifdef __cplusplus
extern "C" {
#endif


#define VIK_LAYER_TYPE            (vik_layer_get_type ())
#define VIK_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_LAYER_TYPE, VikLayer))
#define VIK_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_LAYER_TYPE, VikLayerClass))
#define IS_VIK_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_LAYER_TYPE))
#define IS_VIK_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_LAYER_TYPE))

typedef struct _VikLayer VikLayer;
typedef struct _VikLayerClass VikLayerClass;

struct _VikLayerClass
{
  GObjectClass object_class;
  void (* update) (VikLayer *vl);
};

GType vik_layer_get_type ();

struct _VikLayer {
	GObject obj;
	char *name;
	bool visible;

	bool realized;
	Viewport * viewport;/* simply a reference */
	VikTreeview *vt; /* simply a reference */
	GtkTreeIter iter;

	/* for explicit "polymorphism" (function type switching) */
	VikLayerTypeEnum type;

	void * layer; /* class Layer */
};

/* I think most of these are ignored,
 * returning GRAB_FOCUS grabs the focus for mouse move,
 * mouse click, release always grabs focus. Focus allows key presses
 * to be handled.
 * It used to be that, if ignored, Viking could look for other layers.
 * this was useful for clicking a way/trackpoint in any layer,
 * if no layer was selected (find way/trackpoint)
 */
typedef enum {
  VIK_LAYER_TOOL_IGNORED=0,
  VIK_LAYER_TOOL_ACK,
  VIK_LAYER_TOOL_ACK_REDRAW_ABOVE,
  VIK_LAYER_TOOL_ACK_REDRAW_ALL,
  VIK_LAYER_TOOL_ACK_REDRAW_IF_VISIBLE,
  VIK_LAYER_TOOL_ACK_GRAB_FOCUS, /* only for move */
} VikLayerToolFuncStatus;

/* void * is tool-specific state created in the constructor */
typedef void * (*VikToolConstructorFunc) (VikWindow *, Viewport *);
typedef void (*VikToolDestructorFunc) (void *);
typedef VikLayerToolFuncStatus (*VikToolMouseFunc) (VikLayer *, GdkEventButton *, void *);
typedef VikLayerToolFuncStatus (*VikToolMouseMoveFunc) (VikLayer *, GdkEventMotion *, void *);
typedef void (*VikToolActivationFunc) (VikLayer *, void *);
typedef bool (*VikToolKeyFunc) (VikLayer *, GdkEventKey *, void *);

typedef struct _VikToolInterface VikToolInterface;
struct _VikToolInterface {
  GtkRadioActionEntry radioActionEntry;
  VikToolConstructorFunc create;
  VikToolDestructorFunc destroy;
  VikToolActivationFunc activate;
  VikToolActivationFunc deactivate;
  VikToolMouseFunc click;
  VikToolMouseMoveFunc move;
  VikToolMouseFunc release;
  VikToolKeyFunc key_press; /* return false if we don't use the key press -- should return false most of the time if we want any shortcuts / UI keybindings to work! use sparingly. */
  bool pan_handler; // Call click & release funtions even when 'Pan Mode' is on
  GdkCursorType cursor_type;
  const GdkPixdata *cursor_data;
  const GdkCursor *cursor;
};

/* Parameters (for I/O and Properties) */
/* --> moved to uibuilder.h */


/* layer interface functions */

/* Create a new layer of a certain type. Should be filled with defaults */
typedef VikLayer *    (*VikLayerFuncCreate)                (Viewport *);

typedef VikLayer *    (*VikLayerFuncUnmarshall)            (uint8_t *, int, Viewport *);

/* returns true if needs to redraw due to changed param */
/* in parameter bool denotes if for file I/O, as opposed to display/cut/copy etc... operations */
typedef bool      (*VikLayerFuncSetParam)              (VikLayer *, uint16_t, VikLayerParamData, Viewport *, bool);

/* in parameter bool denotes if for file I/O, as opposed to display/cut/copy etc... operations */
typedef VikLayerParamData
                      (*VikLayerFuncGetParam)              (VikLayer *, uint16_t, bool);

typedef void          (*VikLayerFuncChangeParam)           (GtkWidget *, ui_change_values );



typedef enum {
  VIK_MENU_ITEM_PROPERTY=1,
  VIK_MENU_ITEM_CUT=2,
  VIK_MENU_ITEM_COPY=4,
  VIK_MENU_ITEM_PASTE=8,
  VIK_MENU_ITEM_DELETE=16,
  VIK_MENU_ITEM_ALL=0xff
} VikStdLayerMenuItem;

typedef struct _VikLayerInterface VikLayerInterface;

/* See vik_layer_* for function parameter names */
struct _VikLayerInterface {
  const char *                     fixed_layer_name; // Used in .vik files - this should never change to maintain file compatibility
  const char *                     name;             // Translate-able name used for display purposes
  const char *                     accelerator;
  const GdkPixdata *                icon;

  VikToolInterface *                tools;
  uint16_t                           tools_count;

  /* for I/O reading to and from .vik files -- params like coordline width, color, etc. */
  VikLayerParam *                   params;
  uint16_t                           params_count;
  char **                          params_groups;
  uint8_t                            params_groups_count;

  /* menu items to be created */
  VikStdLayerMenuItem               menu_items_selection;

  VikLayerFuncCreate                create;

  VikLayerFuncUnmarshall            unmarshall;

  /* for I/O */
  VikLayerFuncSetParam              set_param;
  VikLayerFuncGetParam              get_param;
  VikLayerFuncChangeParam           change_param;
};

VikLayerInterface *vik_layer_get_interface ( VikLayerTypeEnum type );


void vik_layer_set_type ( VikLayer *vl, VikLayerTypeEnum type );
void vik_layer_draw(VikLayer *l, Viewport * viewport);
void vik_layer_change_coord_mode ( VikLayer *l, VikCoordMode mode );
void vik_layer_rename ( VikLayer *l, const char *new_name );
void vik_layer_rename_no_copy ( VikLayer *l, char *new_name );
const char *vik_layer_get_name ( VikLayer *l );

time_t vik_layer_get_timestamp ( VikLayer *vl );

bool vik_layer_set_param (VikLayer *layer, uint16_t id, VikLayerParamData data, void * viewport, bool is_file_operation);

void vik_layer_set_defaults ( VikLayer *vl, Viewport * viewport);

void vik_layer_emit_update ( VikLayer *vl );

/* GUI */
void vik_layer_set_menu_items_selection(VikLayer *l, uint16_t selection);
uint16_t vik_layer_get_menu_items_selection(VikLayer *l);
void vik_layer_add_menu_items ( VikLayer *l, GtkMenu *menu, void * vlp );
VikLayer *vik_layer_create ( VikLayerTypeEnum type, Viewport * viewport, bool interactive );
bool vik_layer_properties ( VikLayer *layer, Viewport * viewport);

void vik_layer_post_read ( VikLayer *layer, Viewport * viewport, bool from_file );

bool vik_layer_sublayer_add_menu_items ( VikLayer *l, GtkMenu *menu, void * vlp, int subtype, void * sublayer, GtkTreeIter *iter, Viewport * viewport);

VikLayer *vik_layer_copy(VikLayer *vl, void * viewport);
void      vik_layer_marshall ( VikLayer *vl, uint8_t **data, int *len );
VikLayer *vik_layer_unmarshall ( uint8_t *data, int len, Viewport * viewport);
void      vik_layer_marshall_params ( VikLayer *vl, uint8_t **data, int *len );
void      vik_layer_unmarshall_params ( VikLayer *vl, uint8_t *data, int len, Viewport * viewport);

const char *vik_layer_sublayer_rename_request ( VikLayer *l, const char *newname, void * vlp, int subtype, void * sublayer, GtkTreeIter *iter );

bool vik_layer_sublayer_toggle_visible ( VikLayer *l, int subtype, void * sublayer );


const char* vik_layer_layer_tooltip ( VikLayer *l );

bool vik_layer_selected ( VikLayer *l, int subtype, void * sublayer, int type, void * vlp );

/* TODO: put in layerspanel */
GdkPixbuf *vik_layer_load_icon ( VikLayerTypeEnum type );

VikLayer *vik_layer_get_and_reset_trigger();
void vik_layer_emit_update_secondary ( VikLayer *vl ); /* to be called by aggregate layer only. doesn't set the trigger */
void vik_layer_emit_update_although_invisible ( VikLayer *vl );

VikLayerTypeEnum vik_layer_type_from_string ( const char *str );

typedef struct {
  VikLayerParamData data;
  VikLayerParamType type;
} VikLayerTypedParamData;

void vik_layer_typed_param_data_free ( void * gp );
VikLayerTypedParamData *vik_layer_typed_param_data_copy_from_data ( VikLayerParamType type, VikLayerParamData val );
VikLayerTypedParamData *vik_layer_data_typed_param_copy_from_string ( VikLayerParamType type, const char *str );

#ifdef __cplusplus
}
#endif


namespace SlavGPS {

	class Layer {
	public:

		Layer(VikLayer * vl);
		~Layer() {};


		/* Layer interface methods. */

		/* Rarely used, this is called after a read operation
		   or properties box is run.  usually used to create
		   GC's that depend on params, but GC's can also be
		   created from create() or set_param() */
		virtual void post_read(Viewport * viewport, bool from_file);

		virtual void draw(Viewport * viewport);
		virtual char const * tooltip();
		virtual char const * sublayer_tooltip(int subtype, void * sublayer);

		virtual bool selected(int subtype, void * sublayer, int type, void * vlp);

		virtual bool show_selected_viewport_menu(GdkEventButton * event, Viewport * viewport);

		virtual bool select_click(GdkEventButton * event, Viewport * viewport, tool_ed_t * tet);
		virtual bool select_move(GdkEventMotion * event, Viewport * viewport, tool_ed_t * t);
		virtual bool select_release(GdkEventButton * event, Viewport * viewport, tool_ed_t * t);

		/* kamilTODO: consider removing them from Layer. They are overriden only in LayerTRW. */
		virtual void set_menu_selection(uint16_t selection);
		virtual uint16_t get_menu_selection();

		virtual void marshall(uint8_t ** data, int * len);

		virtual void cut_item(int subtype, void * sublayer);
		virtual void copy_item(int subtype, void * sublayer, uint8_t ** item, unsigned int * len);
		virtual bool paste_item(int subtype, uint8_t * item, size_t len);
		virtual void delete_item(int subtype, void * sublayer);

		virtual void change_coord_mode(VikCoordMode dest_mode);

		virtual time_t get_timestamp();

		/* Treeview drag and drop method. called on the
		   destination layer. it is given a source and
		   destination layer, and the source and destination
		   iters in the treeview. */
		virtual void drag_drop_request(Layer * src, GtkTreeIter * src_item_iter, GtkTreePath * dest_path);

		virtual int read_file(FILE * f, char const * dirpath);
		virtual void write_file(FILE * f);

		virtual void add_menu_items(GtkMenu * menu, void * vlp);
		virtual bool sublayer_add_menu_items(GtkMenu * menu, void * vlp, int subtype, void * sublayer, GtkTreeIter * iter, Viewport * viewport);
		virtual char const * sublayer_rename_request(const char * newname, void * vlp, int subtype, void * sublayer, GtkTreeIter * iter);
		virtual bool sublayer_toggle_visible(int subtype, void * sublayer);

		/* Do _not_ use this unless absolutely neccesary. Use the dynamic properties (see coordlayer for example)
		   returns true if OK was pressed. */
		virtual bool properties(void * viewport);

		/* Normally only needed for layers with sublayers. This is called when they
		   are added to the treeview so they can add sublayers to the treeview. */
		virtual void realize(VikTreeview * vt, GtkTreeIter * layer_iter);
		virtual void free_();




		VikLayer * vl;

		char type_string[10];


	};


}

#endif
