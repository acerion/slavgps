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
	VikViewport *vvp;/* simply a reference */
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
typedef VikLayer *    (*VikLayerFuncCreate)                (VikViewport *);

/* normally only needed for layers with sublayers. This is called when they
 * are added to the treeview so they can add sublayers to the treeview. */
typedef void          (*VikLayerFuncRealize)               (VikLayer *,VikTreeview *,GtkTreeIter *);

/* rarely used, this is called after a read operation or properties box is run.
 * usually used to create GC's that depend on params,
 * but GC's can also be created from create() or set_param() */
typedef void          (*VikLayerFuncPostRead)              (VikLayer *,VikViewport *vp,bool from_file);

typedef void          (*VikLayerFuncFree)                  (VikLayer *);

/* do _not_ use this unless absolutely neccesary. Use the dynamic properties (see coordlayer for example)
  * returns true if OK was pressed */
typedef bool      (*VikLayerFuncProperties)            (VikLayer *,VikViewport *);

typedef void          (*VikLayerFuncDraw)                  (VikLayer *,VikViewport *);
typedef void          (*VikLayerFuncChangeCoordMode)       (VikLayer *,VikCoordMode);

typedef void          (*VikLayerFuncSetMenuItemsSelection)          (VikLayer *,uint16_t);
typedef uint16_t          (*VikLayerFuncGetMenuItemsSelection)          (VikLayer *);

typedef void          (*VikLayerFuncAddMenuItems)          (VikLayer *,GtkMenu *,void *); /* void * is a VikLayersPanel */
typedef bool      (*VikLayerFuncSublayerAddMenuItems)  (VikLayer *,GtkMenu *,void *, /* first void * is a VikLayersPanel */
                                                            int,void *,GtkTreeIter *,VikViewport *);
typedef const char * (*VikLayerFuncSublayerRenameRequest) (VikLayer *,const char *,void *,
                                                            int,VikViewport *,GtkTreeIter *); /* first void * is a VikLayersPanel */
typedef bool      (*VikLayerFuncSublayerToggleVisible) (VikLayer *,int,void *);
typedef const char * (*VikLayerFuncSublayerTooltip)       (VikLayer *,int,void *);
typedef const char * (*VikLayerFuncLayerTooltip)          (VikLayer *);
typedef bool      (*VikLayerFuncLayerSelected)         (VikLayer *,int,void *,int,void *); /* 2nd void * is a VikLayersPanel */

typedef void          (*VikLayerFuncMarshall)              (VikLayer *, uint8_t **, int *);
typedef VikLayer *    (*VikLayerFuncUnmarshall)            (uint8_t *, int, VikViewport *);

/* returns true if needs to redraw due to changed param */
/* in parameter bool denotes if for file I/O, as opposed to display/cut/copy etc... operations */
typedef bool      (*VikLayerFuncSetParam)              (VikLayer *, uint16_t, VikLayerParamData, VikViewport *, bool);

/* in parameter bool denotes if for file I/O, as opposed to display/cut/copy etc... operations */
typedef VikLayerParamData
                      (*VikLayerFuncGetParam)              (VikLayer *, uint16_t, bool);

typedef void          (*VikLayerFuncChangeParam)           (GtkWidget *, ui_change_values );

typedef bool      (*VikLayerFuncReadFileData)          (VikLayer *, FILE *, const char *); // char* is the directory path. Function should report success or failure
typedef void          (*VikLayerFuncWriteFileData)         (VikLayer *, FILE *);

/* item manipulation */
typedef void          (*VikLayerFuncDeleteItem)            (VikLayer *, int, void *);
                                                         /*      layer, subtype, pointer to sub-item */
typedef void          (*VikLayerFuncCutItem)               (VikLayer *, int, void *);
typedef void          (*VikLayerFuncCopyItem)              (VikLayer *, int, void *, uint8_t **, unsigned int *);
                                                         /*      layer, subtype, pointer to sub-item, return pointer, return len */
typedef bool      (*VikLayerFuncPasteItem)             (VikLayer *, int, uint8_t *, unsigned int);
typedef void          (*VikLayerFuncFreeCopiedItem)        (int, void *);

/* treeview drag and drop method. called on the destination layer. it is given a source and destination layer,
 * and the source and destination iters in the treeview.
 */
typedef void 	      (*VikLayerFuncDragDropRequest)       (VikLayer *, VikLayer *, GtkTreeIter *, GtkTreePath *);

typedef bool      (*VikLayerFuncSelectClick)           (VikLayer *, GdkEventButton *, Viewport *, tool_ed_t*);
typedef bool      (*VikLayerFuncSelectMove)            (VikLayer *, GdkEventMotion *, Viewport *, tool_ed_t*);
typedef bool      (*VikLayerFuncSelectRelease)         (VikLayer *, GdkEventButton *, Viewport *, tool_ed_t*);
typedef bool      (*VikLayerFuncSelectedViewportMenu)  (VikLayer *, GdkEventButton *, Viewport *);

typedef time_t        (*VikLayerFuncGetTimestamp)          (VikLayer *);

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
  VikLayerFuncRealize               realize;
  VikLayerFuncPostRead              post_read;
  VikLayerFuncFree                  free;

  VikLayerFuncProperties            properties;
  VikLayerFuncDraw                  draw;
  VikLayerFuncChangeCoordMode       change_coord_mode;

  VikLayerFuncGetTimestamp          get_timestamp;

  VikLayerFuncSetMenuItemsSelection set_menu_selection;
  VikLayerFuncGetMenuItemsSelection get_menu_selection;

  VikLayerFuncAddMenuItems          add_menu_items;
  VikLayerFuncSublayerAddMenuItems  sublayer_add_menu_items;
  VikLayerFuncSublayerRenameRequest sublayer_rename_request;
  VikLayerFuncSublayerToggleVisible sublayer_toggle_visible;
  VikLayerFuncSublayerTooltip       sublayer_tooltip;
  VikLayerFuncLayerTooltip          layer_tooltip;
  VikLayerFuncLayerSelected         layer_selected;

  VikLayerFuncMarshall              marshall;
  VikLayerFuncUnmarshall            unmarshall;

  /* for I/O */
  VikLayerFuncSetParam              set_param;
  VikLayerFuncGetParam              get_param;
  VikLayerFuncChangeParam           change_param;

  /* for I/O -- extra non-param data like TrwLayer data */
  VikLayerFuncReadFileData          read_file_data;
  VikLayerFuncWriteFileData         write_file_data;

  VikLayerFuncDeleteItem            delete_item;
  VikLayerFuncCutItem               cut_item;
  VikLayerFuncCopyItem              copy_item;
  VikLayerFuncPasteItem             paste_item;
  VikLayerFuncFreeCopiedItem        free_copied_item;

  VikLayerFuncDragDropRequest       drag_drop_request;

  VikLayerFuncSelectClick           select_click;
  VikLayerFuncSelectMove            select_move;
  VikLayerFuncSelectRelease         select_release;
  VikLayerFuncSelectedViewportMenu  show_viewport_menu;
};

VikLayerInterface *vik_layer_get_interface ( VikLayerTypeEnum type );


void vik_layer_set_type ( VikLayer *vl, VikLayerTypeEnum type );
void vik_layer_draw ( VikLayer *l, VikViewport *vp );
void vik_layer_change_coord_mode ( VikLayer *l, VikCoordMode mode );
void vik_layer_rename ( VikLayer *l, const char *new_name );
void vik_layer_rename_no_copy ( VikLayer *l, char *new_name );
const char *vik_layer_get_name ( VikLayer *l );

time_t vik_layer_get_timestamp ( VikLayer *vl );

bool vik_layer_set_param (VikLayer *layer, uint16_t id, VikLayerParamData data, void * vp, bool is_file_operation);

void vik_layer_set_defaults ( VikLayer *vl, VikViewport *vvp );

void vik_layer_emit_update ( VikLayer *vl );

/* GUI */
void vik_layer_set_menu_items_selection(VikLayer *l, uint16_t selection);
uint16_t vik_layer_get_menu_items_selection(VikLayer *l);
void vik_layer_add_menu_items ( VikLayer *l, GtkMenu *menu, void * vlp );
VikLayer *vik_layer_create ( VikLayerTypeEnum type, VikViewport *vp, bool interactive );
bool vik_layer_properties ( VikLayer *layer, VikViewport *vp );

void vik_layer_realize ( VikLayer *l, VikTreeview *vt, GtkTreeIter * layer_iter );
void vik_layer_post_read ( VikLayer *layer, VikViewport *vp, bool from_file );

bool vik_layer_sublayer_add_menu_items ( VikLayer *l, GtkMenu *menu, void * vlp, int subtype, void * sublayer, GtkTreeIter *iter, VikViewport *vvp );

VikLayer *vik_layer_copy ( VikLayer *vl, void * vp );
void      vik_layer_marshall ( VikLayer *vl, uint8_t **data, int *len );
VikLayer *vik_layer_unmarshall ( uint8_t *data, int len, VikViewport *vvp );
void      vik_layer_marshall_params ( VikLayer *vl, uint8_t **data, int *len );
void      vik_layer_unmarshall_params ( VikLayer *vl, uint8_t *data, int len, VikViewport *vvp );

const char *vik_layer_sublayer_rename_request ( VikLayer *l, const char *newname, void * vlp, int subtype, void * sublayer, GtkTreeIter *iter );

bool vik_layer_sublayer_toggle_visible ( VikLayer *l, int subtype, void * sublayer );

//const char* vik_layer_sublayer_tooltip ( VikLayer *l, int subtype, void * sublayer );

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
		virtual char const * tooltip();
		virtual char const * sublayer_tooltip(int subtype, void * sublayer);
		virtual bool show_selected_viewport_menu(GdkEventButton * event, Viewport * viewport);


		VikLayer * vl;

		char type_string[10];


	};


}

#endif
