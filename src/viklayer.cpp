/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include "viking.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "viklayer_defaults.h"
#include "viklayer.h"
#include "globals.h"

/* functions common to all layers. */
/* TODO longone: rename interface free -> finalize */

extern VikLayerInterface vik_aggregate_layer_interface;
extern VikLayerInterface vik_trw_layer_interface;
extern VikLayerInterface vik_maps_layer_interface;
extern VikLayerInterface vik_coord_layer_interface;
extern VikLayerInterface vik_georef_layer_interface;
extern VikLayerInterface vik_gps_layer_interface;
extern VikLayerInterface vik_dem_layer_interface;
#ifdef HAVE_LIBMAPNIK
extern VikLayerInterface vik_mapnik_layer_interface;
#endif

enum {
  VL_UPDATE_SIGNAL,
  VL_LAST_SIGNAL
};
static unsigned int layer_signals[VL_LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class;

static void vik_layer_finalize ( VikLayer *vl );
static bool vik_layer_properties_factory ( VikLayer *vl, Viewport * viewport);
static bool layer_defaults_register ( VikLayerTypeEnum type );

G_DEFINE_TYPE (VikLayer, vik_layer, G_TYPE_OBJECT)

static void vik_layer_class_init (VikLayerClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = (GObjectFinalizeFunc) vik_layer_finalize;

  parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

  layer_signals[VL_UPDATE_SIGNAL] = g_signal_new ( "update", G_TYPE_FROM_CLASS (klass),
   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), G_STRUCT_OFFSET (VikLayerClass, update), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  // Register all parameter defaults, early in the start up sequence
  int layer;
  for ( layer = 0; ((VikLayerTypeEnum) layer) < VIK_LAYER_NUM_TYPES; layer++ )
    // ATM ignore the returned value
    layer_defaults_register((VikLayerTypeEnum) layer);
}

/**
 * Invoke the actual drawing via signal method
 */
static bool idle_draw ( VikLayer *vl )
{
  g_signal_emit ( G_OBJECT(vl), layer_signals[VL_UPDATE_SIGNAL], 0 );
  return false; // Nothing else to do
}

/**
 * Draw specified layer
 */
void vik_layer_emit_update ( VikLayer *vl )
{
  if ( vl->visible && vl->realized ) {
    GThread *thread = vik_window_get_thread ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vl)) );
    if ( !thread )
      // Do nothing
      return;

    vik_window_set_redraw_trigger(vl);

    // Only ever draw when there is time to do so
    if ( g_thread_self() != thread )
      // Drawing requested from another (background) thread, so handle via the gdk thread method
      gdk_threads_add_idle ( (GSourceFunc) idle_draw, vl );
    else
      g_idle_add ( (GSourceFunc) idle_draw, vl );
  }
}

/**
 * should only be done by VikLayersPanel (hence never used from the background)
 * need to redraw and record trigger when we make a layer invisible.
 */
void vik_layer_emit_update_although_invisible ( VikLayer *vl )
{
  vik_window_set_redraw_trigger(vl);
  g_idle_add ( (GSourceFunc) idle_draw, vl );
}

/* doesn't set the trigger. should be done by aggregate layer when child emits update. */
void vik_layer_emit_update_secondary ( VikLayer *vl )
{
  if ( vl->visible )
    // TODO: this can used from the background - eg in acquire
    //       so will need to flow background update status through too
    g_idle_add ( (GSourceFunc) idle_draw, vl );
}

static VikLayerInterface *vik_layer_interfaces[VIK_LAYER_NUM_TYPES] = {
  &vik_aggregate_layer_interface,
  &vik_trw_layer_interface,
  &vik_coord_layer_interface,
  &vik_georef_layer_interface,
  &vik_gps_layer_interface,
  &vik_maps_layer_interface,
  &vik_dem_layer_interface,
#ifdef HAVE_LIBMAPNIK
  &vik_mapnik_layer_interface,
#endif
};

VikLayerInterface *vik_layer_get_interface ( VikLayerTypeEnum type )
{
  assert ( type < VIK_LAYER_NUM_TYPES );
  return vik_layer_interfaces[type];
}

/**
 * Store default values for this layer
 *
 * Returns whether any parameters where registered
 */
static bool layer_defaults_register ( VikLayerTypeEnum type )
{
  // See if any parameters
  VikLayerParam *params = vik_layer_interfaces[type]->params;
  if ( ! params )
    return false;

  bool answer = false; // Incase all parameters are 'not in properties'
  uint16_t params_count = vik_layer_interfaces[type]->params_count;
  uint16_t i;
  // Process each parameter
  for ( i = 0; i < params_count; i++ ) {
    if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES ) {
      if ( params[i].default_value ) {
        VikLayerParamData paramd = params[i].default_value();
        a_layer_defaults_register ( &params[i], paramd, vik_layer_interfaces[type]->fixed_layer_name );
        answer = true;
      }
    }
  }

  return answer;
}

static void vik_layer_init ( VikLayer *vl )
{
  vl->visible = true;
  vl->name = NULL;
  vl->realized = false;
}

void vik_layer_set_type ( VikLayer *vl, VikLayerTypeEnum type )
{
  vl->type = type;
}

/* frees old name */
void vik_layer_rename ( VikLayer *l, const char *new_name )
{
  assert ( l != NULL );
  assert ( new_name != NULL );
  free( l->name );
  l->name = g_strdup( new_name );
}

void vik_layer_rename_no_copy ( VikLayer *l, char *new_name )
{
  assert ( l != NULL );
  assert ( new_name != NULL );
  free( l->name );
  l->name = new_name;
}

const char *vik_layer_get_name ( VikLayer *l )
{
  assert ( l != NULL);
  return l->name;
}

time_t vik_layer_get_timestamp(VikLayer * vl)
{
	Layer * layer = (Layer *) vl->layer;
	return layer->get_timestamp();
}

VikLayer *vik_layer_create ( VikLayerTypeEnum type, Viewport * viewport, bool interactive )
{
  VikLayer *new_layer = NULL;
  assert ( type < VIK_LAYER_NUM_TYPES );

  new_layer = vik_layer_interfaces[type]->create(viewport);

  assert ( new_layer != NULL );

  if ( interactive )
  {
    if ( vik_layer_properties(new_layer, viewport) )
      /* We translate the name here */
      /* in order to avoid translating name set by user */
      vik_layer_rename ( VIK_LAYER(new_layer), _(vik_layer_interfaces[type]->name) );
    else
    {
      g_object_unref ( G_OBJECT(new_layer) ); /* cancel that */
      new_layer = NULL;
    }
  }
  return new_layer;
}

/* returns true if OK was pressed */
bool vik_layer_properties(VikLayer * layer, Viewport * viewport)
{
	if (layer->type == VIK_LAYER_GEOREF) {
		Layer * l = (Layer *) layer->layer;
		return l->properties(viewport);
	}

	return vik_layer_properties_factory(layer, viewport);
}

void vik_layer_draw(VikLayer * l, Viewport * viewport)
{
	if (l->visible) {
		((Layer *) l->layer)->draw(viewport);
	}
}

void vik_layer_change_coord_mode(VikLayer * l, VikCoordMode mode)
{
	Layer * layer = (Layer *) l->layer;
	layer->change_coord_mode(mode);
}

typedef struct {
  VikLayerTypeEnum layer_type;
  int len;
  uint8_t data[0];
} header_t;

void vik_layer_marshall ( VikLayer *vl, uint8_t **data, int *len )
{
  header_t *header;
  Layer * layer = (Layer *) vl->layer;
  layer->marshall(data, len);
  if (*data) {
      header = (header_t *) malloc(*len + sizeof(*header));
      header->layer_type = vl->type;
      header->len = *len;
      memcpy(header->data, *data, *len);
      free(*data);
      *data = (uint8_t *)header;
      *len = *len + sizeof(*header);
  }
}

void vik_layer_marshall_params ( VikLayer *vl, uint8_t **data, int *datalen )
{
  VikLayerParam *params = vik_layer_get_interface(vl->type)->params;
  VikLayerFuncGetParam get_param = vik_layer_get_interface(vl->type)->get_param;
  GByteArray* b = g_byte_array_new ();
  int len;

#define vlm_append(obj, sz) 	\
  len = (sz);    		\
  g_byte_array_append ( b, (uint8_t *)&len, sizeof(len) );	\
  g_byte_array_append ( b, (uint8_t *)(obj), len );

  // Store the internal properties first
  vlm_append(&vl->visible, sizeof(vl->visible));
  vlm_append(vl->name, strlen(vl->name));

  // Now the actual parameters
  if ( params && get_param )
  {
    VikLayerParamData d;
    uint16_t i, params_count = vik_layer_get_interface(vl->type)->params_count;
    for ( i = 0; i < params_count; i++ )
    {
      fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, params[i].name);
      d = get_param(vl, i, false);
      switch ( params[i].type )
      {
      case VIK_LAYER_PARAM_STRING:
        // Remember need braces as these are macro calls, not single statement functions!
        if ( d.s ) {
          vlm_append(d.s, strlen(d.s));
        }
        else {
          // Need to insert empty string otherwise the unmarshall will get confused
          vlm_append("", 0);
        }
        break;
      /* print out the string list in the array */
      case VIK_LAYER_PARAM_STRING_LIST: {
        GList *list = d.sl;

        /* write length of list (# of strings) */
        int listlen = g_list_length ( list );
        g_byte_array_append ( b, (uint8_t *)&listlen, sizeof(listlen) );

        /* write each string */
        while ( list ) {
          char *s = (char *) list->data;
          vlm_append(s, strlen(s));
          list = list->next;
        }

	break;
      }
      default:
	vlm_append(&d, sizeof(d));
	break;
      }
    }
  }

  *data = b->data;
  *datalen = b->len;
  g_byte_array_free ( b, false );

#undef vlm_append
}

void vik_layer_unmarshall_params ( VikLayer *vl, uint8_t *data, int datalen, Viewport * viewport)
{
  VikLayerParam *params = vik_layer_get_interface(vl->type)->params;
  VikLayerFuncSetParam set_param = vik_layer_get_interface(vl->type)->set_param;
  char *s;
  uint8_t *b = (uint8_t *)data;

#define vlm_size (*(int *)b)
#define vlm_read(obj)				\
  memcpy((obj), b+sizeof(int), vlm_size);	\
  b += sizeof(int) + vlm_size;

  vlm_read(&vl->visible);

  s = (char *) malloc(vlm_size + 1);
  s[vlm_size]=0;
  vlm_read(s);
  vik_layer_rename(vl, s);
  free(s);

  if ( params && set_param )
  {
    VikLayerParamData d;
    uint16_t i, params_count = vik_layer_get_interface(vl->type)->params_count;
    for ( i = 0; i < params_count; i++ )
    {
      fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, params[i].name);
      switch ( params[i].type )
      {
      case VIK_LAYER_PARAM_STRING:
	s = (char *) malloc(vlm_size + 1);
	s[vlm_size]=0;
	vlm_read(s);
	d.s = s;
	set_param(vl, i, d, viewport, false);
	free(s);
	break;
      case VIK_LAYER_PARAM_STRING_LIST:  {
        int listlen = vlm_size, j;
        GList *list = NULL;
        b += sizeof(int); /* skip listlen */;

        for ( j = 0; j < listlen; j++ ) {
          /* get a string */
          s = (char *) malloc(vlm_size + 1);
	  s[vlm_size]=0;
	  vlm_read(s);
          list = g_list_append ( list, s );
        }
        d.sl = list;
        set_param(vl, i, d, viewport, false);
        /* don't free -- string list is responsibility of the layer */

        break;
        }
      default:
	vlm_read(&d);
	set_param(vl, i, d, viewport, false);
	break;
      }
    }
  }
}

VikLayer *vik_layer_unmarshall ( uint8_t *data, int len, Viewport * viewport)
{
  header_t *header;

  header = (header_t *)data;

  if ( vik_layer_interfaces[header->layer_type]->unmarshall ) {
    return vik_layer_interfaces[header->layer_type]->unmarshall ( header->data, header->len, viewport);
  } else {
    return NULL;
  }
}

static void vik_layer_finalize ( VikLayer *vl )
{
  assert ( vl != NULL );

  Layer * layer = (Layer *) vl->layer;
  layer->free_();

  if ( vl->name )
    free( vl->name );
  G_OBJECT_CLASS(parent_class)->finalize(G_OBJECT(vl));
}

/* sublayer switching */
bool vik_layer_sublayer_toggle_visible ( VikLayer *l, int subtype, void * sublayer )
{
	Layer * layer = (Layer *) l->layer;
	return layer->sublayer_toggle_visible(subtype, sublayer);
}

bool vik_layer_selected(VikLayer * l, int subtype, void * sublayer, int type, void * vlp)
{
	Layer * layer = (Layer *) l->layer;
	bool result = layer->selected(subtype, sublayer, type, vlp);
	if (result) {
		return result;
	} else {
		return vik_window_clear_highlight((VikWindow *) VIK_GTK_WINDOW_FROM_LAYER(l));
	}
}

void vik_layer_set_menu_items_selection(VikLayer *l, uint16_t selection)
{
	Layer * layer = (Layer *) l->layer;
	layer->set_menu_selection(selection);
}

uint16_t vik_layer_get_menu_items_selection(VikLayer *l)
{
	Layer * layer = (Layer *) l->layer;
	uint16_t rv = layer->get_menu_selection();
	if (rv == (uint16_t) -1) {
		/* Perhaps this line could go to base class. */
		return vik_layer_interfaces[l->type]->menu_items_selection;
	} else {
		return rv;
	}
}

void vik_layer_add_menu_items ( VikLayer *l, GtkMenu *menu, void * vlp )
{
	Layer * layer = (Layer *) l->layer;
	layer->add_menu_items(menu, vlp);
}

bool vik_layer_sublayer_add_menu_items ( VikLayer *l, GtkMenu *menu, void * vlp, int subtype, void * sublayer, GtkTreeIter *iter, Viewport * viewport)
{
	Layer * layer = (Layer *) l->layer;
	return layer->sublayer_add_menu_items(menu, vlp, subtype, sublayer, iter, viewport);
}


const char *vik_layer_sublayer_rename_request ( VikLayer *l, const char *newname, void * vlp, int subtype, void * sublayer, GtkTreeIter *iter )
{
	Layer * layer = (Layer *) l->layer;
	return layer->sublayer_rename_request(newname, vlp, subtype, (VikViewport *) sublayer, iter);
}

GdkPixbuf *vik_layer_load_icon ( VikLayerTypeEnum type )
{
  assert ( type < VIK_LAYER_NUM_TYPES );
  if ( vik_layer_interfaces[type]->icon )
    return gdk_pixbuf_from_pixdata ( vik_layer_interfaces[type]->icon, false, NULL );
  return NULL;
}

bool vik_layer_set_param ( VikLayer *layer, uint16_t id, VikLayerParamData data, void * viewport, bool is_file_operation )
{
  if ( vik_layer_interfaces[layer->type]->set_param )
    return vik_layer_interfaces[layer->type]->set_param ( layer, id, data, (Viewport *) viewport, is_file_operation );
  return false;
}

void vik_layer_post_read ( VikLayer *layer, Viewport * viewport, bool from_file )
{
	Layer * l = (Layer *) layer->layer;
	l->post_read(viewport, from_file);
}

static bool vik_layer_properties_factory ( VikLayer *vl, Viewport * viewport)
{
  switch ( a_uibuilder_properties_factory ( _("Layer Properties"),
					    VIK_GTK_WINDOW_FROM_WIDGET(viewport->vvp),
					    vik_layer_interfaces[vl->type]->params,
					    vik_layer_interfaces[vl->type]->params_count,
					    vik_layer_interfaces[vl->type]->params_groups,
					    vik_layer_interfaces[vl->type]->params_groups_count,
					    (bool (*)(void*, uint16_t, VikLayerParamData, void*, bool)) vik_layer_interfaces[vl->type]->set_param,
					    vl,
					    viewport->vvp,
					    (VikLayerParamData (*)(void*, uint16_t, bool)) vik_layer_interfaces[vl->type]->get_param,
					    vl,
					    (void (*)(GtkWidget*, void**)) vik_layer_interfaces[vl->type]->change_param ) ) {
    case 0:
    case 3:
      return false;
      /* redraw (?) */
    case 2:
	    {
		    Layer * layer = (Layer *) vl->layer;
		    layer->post_read(viewport, false); /* update any gc's */
	    }
    default:
      return true;
  }
}

VikLayerTypeEnum vik_layer_type_from_string ( const char *str )
{
  int i;
  for ( i = 0; ((VikLayerTypeEnum) i) < VIK_LAYER_NUM_TYPES; i++ )
    if ( strcasecmp ( str, vik_layer_get_interface((VikLayerTypeEnum) i)->fixed_layer_name ) == 0 )
      return (VikLayerTypeEnum) i;
  return VIK_LAYER_NUM_TYPES;
}

void vik_layer_typed_param_data_free ( void * gp )
{
  VikLayerTypedParamData *val = (VikLayerTypedParamData *)gp;
  switch ( val->type ) {
    case VIK_LAYER_PARAM_STRING:
      if ( val->data.s )
        free( (void *)val->data.s );
      break;
    /* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
     * the internals call get_param -- i.e. it should be managed w/in the layer.
     * The value passed by the internals into set_param should also be managed
     * by the layer -- i.e. free'd by the layer.
     */
    case VIK_LAYER_PARAM_STRING_LIST:
      fprintf(stderr, "WARNING: Param strings not implemented\n"); //fake it
      break;
    default:
      break;
  }
  free( val );
}

VikLayerTypedParamData *vik_layer_typed_param_data_copy_from_data (VikLayerParamType type, VikLayerParamData val) {
  VikLayerTypedParamData *newval = (VikLayerTypedParamData *) malloc(1 * sizeof (VikLayerTypedParamData));
  newval->data = val;
  newval->type = type;
  switch ( newval->type ) {
    case VIK_LAYER_PARAM_STRING: {
      char *s = g_strdup(newval->data.s);
      newval->data.s = s;
      break;
    }
    /* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
     * the internals call get_param -- i.e. it should be managed w/in the layer.
     * The value passed by the internals into set_param should also be managed
     * by the layer -- i.e. free'd by the layer.
     */
    case VIK_LAYER_PARAM_STRING_LIST:
      fprintf(stderr, "CRITICAL: Param strings not implemented\n"); //fake it
      break;
    default:
      break;
  }
  return newval;
}

#define TEST_BOOLEAN(str) (! ((str)[0] == '\0' || (str)[0] == '0' || (str)[0] == 'n' || (str)[0] == 'N' || (str)[0] == 'f' || (str)[0] == 'F') )

VikLayerTypedParamData *vik_layer_data_typed_param_copy_from_string ( VikLayerParamType type, const char *str )
{
  VikLayerTypedParamData *rv = (VikLayerTypedParamData *) malloc(1 * sizeof (VikLayerTypedParamData));
  rv->type = type;
  switch ( type )
  {
    case VIK_LAYER_PARAM_DOUBLE: rv->data.d = strtod(str, NULL); break;
    case VIK_LAYER_PARAM_UINT: rv->data.u = strtoul(str, NULL, 10); break;
    case VIK_LAYER_PARAM_INT: rv->data.i = strtol(str, NULL, 10); break;
    case VIK_LAYER_PARAM_BOOLEAN: rv->data.b = TEST_BOOLEAN(str); break;
    case VIK_LAYER_PARAM_COLOR: memset(&(rv->data.c), 0, sizeof(rv->data.c)); /* default: black */
      gdk_color_parse ( str, &(rv->data.c) ); break;
    /* STRING or STRING_LIST -- if STRING_LIST, just set param to add a STRING */
    default: {
      char *s = g_strdup(str);
      rv->data.s = s;
    }
  }
  return rv;
}


/**
 * vik_layer_set_defaults:
 *
 * Loop around all parameters for the specified layer to call the function to get the
 *  default value for that parameter
 */
void vik_layer_set_defaults ( VikLayer *vl, Viewport * viewport)
{
  // Sneaky initialize of the viewport value here
  vl->viewport = viewport;
  VikLayerInterface *vli = vik_layer_get_interface ( vl->type );
  const char *layer_name = vli->fixed_layer_name;
  VikLayerParamData data;

  int i;
  for ( i = 0; i < vli->params_count; i++ ) {
    // Ensure parameter is for use
    if ( vli->params[i].group > VIK_LAYER_NOT_IN_PROPERTIES ) {
      // ATM can't handle string lists
      // only DEM files uses this currently
      if ( vli->params[i].type != VIK_LAYER_PARAM_STRING_LIST ) {
        data = a_layer_defaults_get ( layer_name, vli->params[i].name, vli->params[i].type );
        vik_layer_set_param ( vl, i, data, viewport, true ); // Possibly come from a file
      }
    }
  }
}






Layer::Layer(VikLayer * vl_)
{
	fprintf(stderr, "-------- Layer constructor, assigning vl = %x\n", (unsigned long) vl_);
	this->vl = vl_;
	this->vl->layer = this;


	switch (this->vl->type) {
	case VIK_LAYER_AGGREGATE:
		strcpy(type_string, "AGGREGATE");
		break;
	case VIK_LAYER_TRW:
		strcpy(type_string, "TRW");
		break;
	case VIK_LAYER_COORD:
		strcpy(type_string, "COORD");
		break;
	case VIK_LAYER_GEOREF:
		strcpy(type_string, "GEOREF");
		break;
	case VIK_LAYER_GPS:
		strcpy(type_string, "GPS");
		break;
	case VIK_LAYER_MAPS:
		strcpy(type_string, "MAPS");
		break;
	case VIK_LAYER_DEM:
		strcpy(type_string, "DEM");
		break;
#ifdef HAVE_LIBMAPNIK
	case VIK_LAYER_MAPNIK:
		strcpy(type_string, "MAPNIK");
		break;
#endif
	default:
		strcpy(type_string, "LAST");
		break;
	}
}

bool Layer::select_click(GdkEventButton * event, Viewport * viewport, tool_ed_t * tet)
{
	return false;
}

bool Layer::select_move(GdkEventMotion * event, Viewport * viewport, tool_ed_t * t)
{
	return false;
}

void Layer::post_read(Viewport * viewport, bool from_file)
{
	return;
}

bool Layer::select_release(GdkEventButton * event, Viewport * viewport, tool_ed_t * t)
{
	return false;
}


void Layer::draw(Viewport * viewport)
{
	return;
}

char const * Layer::tooltip()
{
      static char tmp_buf[32];
      snprintf(tmp_buf, sizeof(tmp_buf), _("Layer::tooltip"));
      return tmp_buf;
}



char const * Layer::sublayer_tooltip(int subtype, void * sublayer)
{
      static char tmp_buf[32];
      snprintf(tmp_buf, sizeof(tmp_buf), _("Layer::sublayer_tooltip"));
      return tmp_buf;
}

bool Layer::selected(int subtype, void * sublayer, int type, void * vlp)
{
	return false;
}


bool Layer::show_selected_viewport_menu(GdkEventButton * event, Viewport * viewport)
{
	return false;
}

void Layer::set_menu_selection(uint16_t selection)
{
	return;
}

uint16_t Layer::get_menu_selection()
{
	return (uint16_t) -1;
}

void Layer::marshall(uint8_t ** data, int * len)
{
	return;
}

void Layer::cut_item(int subtype, void * sublayer)
{
	return;
}

void Layer::copy_item(int subtype, void * sublayer, uint8_t ** item, unsigned int * len)
{
	return;
}

bool Layer::paste_item(int subtype, uint8_t * item, size_t len)
{
	return false;
}

void Layer::delete_item(int subtype, void * sublayer)
{
	return;
}


void Layer::change_coord_mode(VikCoordMode dest_mode)
{
	return;
}

time_t Layer::get_timestamp()
{
	return 0;
}

void Layer::drag_drop_request(Layer * src, GtkTreeIter * src_item_iter, GtkTreePath * dest_path)
{
	return;
}

int Layer::read_file(FILE * f, char const * dirpath)
{
	/* KamilFIXME: Magic number to indicate call of base class method. */
	return -5;
}

void Layer::write_file(FILE * f)
{
	return;
}


void Layer::add_menu_items(GtkMenu * menu, void * vlp)
{
	return;
}

bool Layer::sublayer_add_menu_items(GtkMenu * menu, void * vlp, int subtype, void * sublayer, GtkTreeIter * iter, Viewport * viewport)
{
	return false;
}

char const * Layer::sublayer_rename_request(const char * newname, void * vlp, int subtype, void * sublayer, GtkTreeIter * iter)
{
	return NULL;
}

bool Layer::sublayer_toggle_visible(int subtype, void * sublayer)
{
	/* if unknown, will always be visible */
	return true;
}

bool Layer::properties(void * viewport)
{
	return false;
}


void Layer::realize(VikTreeview * vt, GtkTreeIter * layer_iter)
{
	this->vl->vt = vt;
	this->vl->iter = *layer_iter;
	this->vl->realized = true;

	return;
}


void Layer::free_()
{
	return;
}
