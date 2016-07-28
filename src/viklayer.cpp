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
#include "vikaggregatelayer.h"
#include "viktrwlayer.h"
#include "vikcoordlayer.h"
#include "vikmapslayer.h"
#include "vikdemlayer.h"
#include "vikgeoreflayer.h"
#include "vikgpslayer.h"
#include "vikmapniklayer.h"
#include "globals.h"
#include "viktreeview.h"


using namespace SlavGPS;

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

static GObjectClass * parent_class;

static void vik_layer_finalize(VikLayer * vl);
static bool vik_layer_properties_factory(Layer * layer, Viewport * viewport);
static bool layer_defaults_register(LayerType layer_type);

G_DEFINE_TYPE (VikLayer, vik_layer, G_TYPE_OBJECT)

static void vik_layer_class_init(VikLayerClass * klass)
{
	GObjectClass * object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = (GObjectFinalizeFunc) vik_layer_finalize;

	parent_class = (GObjectClass *) g_type_class_peek_parent(klass);

	layer_signals[VL_UPDATE_SIGNAL] = g_signal_new("update", G_TYPE_FROM_CLASS (klass),
						       (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), G_STRUCT_OFFSET (VikLayerClass, update), NULL, NULL,
						       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	// Register all parameter defaults, early in the start up sequence
	for (LayerType layer_type = LayerType::AGGREGATE; layer_type < LayerType::NUM_TYPES; ++layer_type) {
		// ATM ignore the returned value
		layer_defaults_register(layer_type);
	}
}

/**
 * Invoke the actual drawing via signal method
 */
static bool idle_draw(Layer * layer)
{
	g_signal_emit(G_OBJECT (layer->vl), layer_signals[VL_UPDATE_SIGNAL], 0);
	return false; // Nothing else to do
}





/**
 * Draw specified layer
 */
void Layer::emit_update()
{
	if (this->visible && this->realized) {
		GThread * thread = window_from_layer(this)->get_thread();
		if (!thread) {
			// Do nothing
			return;
		}

		Window::set_redraw_trigger(this);

		// Only ever draw when there is time to do so
		if (g_thread_self() != thread) {
			// Drawing requested from another (background) thread, so handle via the gdk thread method
			gdk_threads_add_idle((GSourceFunc) idle_draw, this);
		} else {
			g_idle_add((GSourceFunc) idle_draw, this);
		}
	}
}





/**
 * should only be done by LayersPanel (hence never used from the background)
 * need to redraw and record trigger when we make a layer invisible.
 */
void vik_layer_emit_update_although_invisible(Layer * layer)
{
	Window::set_redraw_trigger(layer);
	g_idle_add((GSourceFunc) idle_draw, layer);
}

/* doesn't set the trigger. should be done by aggregate layer when child emits update. */
void vik_layer_emit_update_secondary(Layer * layer)
{
	if (layer->visible) {
		// TODO: this can used from the background - eg in acquire
		//       so will need to flow background update status through too
		g_idle_add((GSourceFunc) idle_draw, layer);
	}
}

static VikLayerInterface * vik_layer_interfaces[(int) LayerType::NUM_TYPES] = {
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

VikLayerInterface * vik_layer_get_interface(LayerType layer_type)
{
	assert (layer_type < LayerType::NUM_TYPES);
	return vik_layer_interfaces[(int) layer_type];
}

/**
 * Store default values for this layer
 *
 * Returns whether any parameters where registered
 */
static bool layer_defaults_register(LayerType layer_type)
{
	// See if any parameters
	VikLayerParam *params = vik_layer_interfaces[(int) layer_type]->params;
	if (!params) {
		return false;
	}

	bool answer = false; // Incase all parameters are 'not in properties'
	uint16_t params_count = vik_layer_interfaces[(int) layer_type]->params_count;
	uint16_t i;
	// Process each parameter
	for (i = 0; i < params_count; i++) {
		if (params[i].group != VIK_LAYER_NOT_IN_PROPERTIES) {
			if (params[i].default_value) {
				VikLayerParamData paramd = params[i].default_value();
				a_layer_defaults_register(&params[i], paramd, vik_layer_interfaces[(int) layer_type]->fixed_layer_name);
				answer = true;
			}
		}
	}

	return answer;
}

static void vik_layer_init(VikLayer * vl)
{
	return;
}

/* frees old name */
void Layer::rename(char const * new_name)
{
	assert (new_name);
	free(this->name);
	this->name = strdup(new_name);
}

void Layer::rename_no_copy(char * new_name)
{
	assert (new_name);
	free(this->name);
	this->name = new_name;
}

char const * Layer::get_name()
{
	return this->name;
}


Layer * Layer::new_(LayerType layer_type, Viewport * viewport, bool interactive)
{
	assert (layer_type != LayerType::NUM_TYPES);

	Layer * layer = NULL;
	if (layer_type == LayerType::AGGREGATE) {
		fprintf(stderr, "\n\n\n NEW AGGREGATE\n\n\n");
		layer = new LayerAggregate(viewport);

	} else if (layer_type == LayerType::TRW) {
		fprintf(stderr, "\n\n\n NEW TRW\n\n\n");
		layer = new LayerTRW(viewport);

	} else if (layer_type == LayerType::COORD) {
		fprintf(stderr, "\n\n\n NEW COORD\n\n\n");
		layer = new LayerCoord(viewport);

	} else if (layer_type == LayerType::MAPS) {
		fprintf(stderr, "\n\n\n NEW MAPS\n\n\n");
		layer = new LayerMaps(viewport);

	} else if (layer_type == LayerType::DEM) {
		fprintf(stderr, "\n\n\n NEW DEM\n\n\n");
		layer = new LayerDEM(viewport);

	} else if (layer_type == LayerType::GEOREF) {
		fprintf(stderr, "\n\n\n NEW GEOREF\n\n\n");
		layer = new LayerGeoref(viewport);

#ifdef HAVE_LIBMAPNIK
	} else if (layer_type == LayerType::MAPNIK) {
		fprintf(stderr, "\n\n\n NEW MAPNIK\n\n\n");
		layer = new LayerMapnik(viewport);
#endif

	} else if (layer_type == LayerType::GPS) {
		fprintf(stderr, "\n\n\n NEW GPS\n\n\n");
		layer = new LayerGPS(viewport);
	} else {
		assert (0);
	}

	assert (layer);

	if (interactive) {
		if (vik_layer_properties(layer, viewport)) {
			/* We translate the name here */
			/* in order to avoid translating name set by user */
			layer->rename(_(vik_layer_interfaces[(int) layer_type]->name));
		} else {
			g_object_unref(G_OBJECT(layer->vl)); /* cancel that */
			delete layer;
			layer = NULL;
		}
	}
	return layer;
}

/* returns true if OK was pressed */
bool vik_layer_properties(Layer * layer, Viewport * viewport)
{
	if (layer->type == LayerType::GEOREF) {
		return layer->properties(viewport);
	}

	return vik_layer_properties_factory(layer, viewport);
}

void Layer::draw_visible(Viewport * viewport)
{
	if (this->visible) {
		this->draw(viewport);
	}
}


typedef struct {
	LayerType layer_type;
	int len;
	uint8_t data[0];
} header_t;

void Layer::marshall(Layer * layer, uint8_t ** data, int * len)
{
	layer->marshall(data, len);
	if (*data) {
		header_t * header = (header_t *) malloc(*len + sizeof (*header));
		header->layer_type = layer->type;
		header->len = *len;
		memcpy(header->data, *data, *len);
		free(*data);
		*data = (uint8_t *) header;
		*len = *len + sizeof (*header);
	}
}

void Layer::marshall(uint8_t ** data, int * len)
{
	this->marshall_params(data, len);
	return;
}

void Layer::marshall_params(uint8_t ** data, int * datalen)
{
	VikLayerParam * params = vik_layer_get_interface(this->type)->params;
	VikLayerFuncGetParam get_param = vik_layer_get_interface(this->type)->get_param;

	GByteArray* b = g_byte_array_new();
	int len;

#define vlm_append(obj, sz) 	\
	len = (sz);					\
	g_byte_array_append(b, (uint8_t *) &len, sizeof(len));	\
	g_byte_array_append(b, (uint8_t *) (obj), len);

	// Store the internal properties first
	vlm_append(&this->visible, sizeof (this->visible));
	vlm_append(this->name, strlen(this->name));

	// Now the actual parameters
	if (params && get_param) {
		VikLayerParamData d;
		uint16_t i, params_count = vik_layer_get_interface(this->type)->params_count;
		for (i = 0; i < params_count; i++) {
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, params[i].name);
			d = get_param(this, i, false);
			switch (params[i].type) {
			case VIK_LAYER_PARAM_STRING:
				// Remember need braces as these are macro calls, not single statement functions!
				if (d.s) {
					vlm_append(d.s, strlen(d.s));
				} else {
					// Need to insert empty string otherwise the unmarshall will get confused
					vlm_append("", 0);
				}
				break;
				/* print out the string list in the array */
			case VIK_LAYER_PARAM_STRING_LIST: {
				std::list<char *> * a_list = d.sl;

				/* write length of list (# of strings) */
				int listlen = a_list->size();
				g_byte_array_append(b, (uint8_t *) &listlen, sizeof (listlen));

				/* write each string */
				for (auto iter = a_list->begin(); iter != a_list->end(); iter++) {
					char * s = *iter;
					vlm_append(s, strlen(s));
				}

				break;
			}
			default:
				vlm_append(&d, sizeof (d));
				break;
			}
		}
	}

	*data = b->data;
	*datalen = b->len;
	g_byte_array_free (b, false);

#undef vlm_append
}

void Layer::unmarshall_params(uint8_t * data, int datalen, Viewport * viewport)
{
	VikLayerParam *params = vik_layer_get_interface(this->type)->params;
	VikLayerFuncSetParam set_param = vik_layer_get_interface(this->type)->set_param;

	char *s;
	uint8_t *b = (uint8_t *) data;

#define vlm_size (*(int *) b)
#define vlm_read(obj)				\
	memcpy((obj), b+sizeof(int), vlm_size);	\
	b += sizeof(int) + vlm_size;

	vlm_read(&this->visible);

	s = (char *) malloc(vlm_size + 1);
	s[vlm_size]=0;
	vlm_read(s);
	this->rename(s);
	free(s);

	if (params && set_param) {
		VikLayerParamData d;
		uint16_t params_count = vik_layer_get_interface(this->type)->params_count;
		for (uint16_t i = 0; i < params_count; i++){
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, params[i].name);
			switch (params[i].type) {
			case VIK_LAYER_PARAM_STRING:
				s = (char *) malloc(vlm_size + 1);
				s[vlm_size] = 0;
				vlm_read(s);
				d.s = s;
				set_param(this, i, d, viewport, false);
				free(s);
				break;
			case VIK_LAYER_PARAM_STRING_LIST: {
				int listlen = vlm_size;
				std::list<char *> * list = new std::list<char *>;
				b += sizeof(int); /* skip listlen */;

				for (int j = 0; j < listlen; j++) {
					/* get a string */
					s = (char *) malloc(vlm_size + 1);
					s[vlm_size]=0;
					vlm_read(s);
					list->push_back(s);
				}
				d.sl = list;
				set_param(this, i, d, viewport, false);
				/* don't free -- string list is responsibility of the layer */

				break;
			}
			default:
				vlm_read(&d);
				set_param(this, i, d, viewport, false);
				break;
			}
		}
	}
}

Layer * Layer::unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	header_t * header = (header_t *) data;

	if (vik_layer_interfaces[(int) header->layer_type]->unmarshall) {
		return vik_layer_interfaces[(int) header->layer_type]->unmarshall(header->data, header->len, viewport);
	} else {
		return NULL;
	}
}

static void vik_layer_finalize(VikLayer * vl)
{
	assert (vl != NULL);

	Layer * layer = (Layer *) vl->layer;
	if (layer->name) {
		free(layer->name);
	}

	delete layer;

	G_OBJECT_CLASS(parent_class)->finalize(G_OBJECT(vl));
}

bool vik_layer_selected(Layer * layer, int subtype, void * sublayer, int type, void * panel)
{
	bool result = layer->selected(subtype, sublayer, type, panel);
	if (result) {
		return result;
	} else {
		return window_from_layer(layer)->clear_highlight();
	}
}

uint16_t vik_layer_get_menu_items_selection(Layer * layer)
{
	uint16_t rv = layer->get_menu_selection();
	if (rv == (uint16_t) -1) {
		/* Perhaps this line could go to base class. */
		return vik_layer_interfaces[(int) layer->type]->menu_items_selection;
	} else {
		return rv;
	}
}



GdkPixbuf * vik_layer_load_icon(LayerType layer_type)
{
	assert (layer_type < LayerType::NUM_TYPES);
	if (vik_layer_interfaces[(int) layer_type]->icon) {
		return gdk_pixbuf_from_pixdata(vik_layer_interfaces[(int) layer_type]->icon, false, NULL);
	}
	return NULL;
}

bool layer_set_param(Layer * layer, uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation)
{
	return layer->set_param(id, data, viewport, is_file_operation);
}

VikLayerParamData layer_get_param(Layer const * layer, uint16_t id, bool is_file_operation)
{
	return layer->get_param(id, is_file_operation);
}

static bool vik_layer_properties_factory(Layer * layer, Viewport * viewport)
{
	switch (a_uibuilder_properties_factory(_("Layer Properties"),
					       VIK_GTK_WINDOW_FROM_WIDGET(viewport->vvp),
					       vik_layer_interfaces[(int) layer->type]->params,
					       vik_layer_interfaces[(int) layer->type]->params_count,
					       vik_layer_interfaces[(int) layer->type]->params_groups,
					       vik_layer_interfaces[(int) layer->type]->params_groups_count,
					       (bool (*)(void*, uint16_t, VikLayerParamData, void*, bool)) vik_layer_interfaces[(int) layer->type]->set_param,
					       layer,
					       viewport,
					       (VikLayerParamData (*)(void*, uint16_t, bool)) vik_layer_interfaces[(int) layer->type]->get_param,
					       layer,
					       vik_layer_interfaces[(int) layer->type]->change_param)) {
	case 0:
	case 3:
		return false;
		/* redraw (?) */
	case 2: {
		layer->post_read(viewport, false); /* update any gc's */
	}
	default:
		return true;
	}
}

LayerType Layer::type_from_string(char const * str)
{
	for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {
		if (strcasecmp(str, vik_layer_get_interface(i)->fixed_layer_name) == 0) {
			return i;
		}
	}
	return LayerType::NUM_TYPES;
}

void vik_layer_typed_param_data_free(void * gp)
{
	VikLayerTypedParamData *val = (VikLayerTypedParamData *)gp;
	switch (val->type) {
	case VIK_LAYER_PARAM_STRING:
		if (val->data.s) {
			free((void *) val->data.s);
		}
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
	free(val);
}

VikLayerTypedParamData * vik_layer_typed_param_data_copy_from_data(VikLayerParamType type, VikLayerParamData val)
{
	VikLayerTypedParamData * newval = (VikLayerTypedParamData *) malloc(1 * sizeof (VikLayerTypedParamData));
	newval->data = val;
	newval->type = type;
	switch (newval->type) {
	case VIK_LAYER_PARAM_STRING: {
		char * s = g_strdup(newval->data.s);
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

#define TEST_BOOLEAN(str) (! ((str)[0] == '\0' || (str)[0] == '0' || (str)[0] == 'n' || (str)[0] == 'N' || (str)[0] == 'f' || (str)[0] == 'F'))

VikLayerTypedParamData * vik_layer_data_typed_param_copy_from_string(VikLayerParamType type, const char * str)
{
	VikLayerTypedParamData * rv = (VikLayerTypedParamData *) malloc(1 * sizeof (VikLayerTypedParamData));
	rv->type = type;
	switch (type) {
	case VIK_LAYER_PARAM_DOUBLE:
		rv->data.d = strtod(str, NULL);
		break;
	case VIK_LAYER_PARAM_UINT:
		rv->data.u = strtoul(str, NULL, 10);
		break;
	case VIK_LAYER_PARAM_INT:
		rv->data.i = strtol(str, NULL, 10);
		break;
	case VIK_LAYER_PARAM_BOOLEAN:
		rv->data.b = TEST_BOOLEAN(str);
		break;
	case VIK_LAYER_PARAM_COLOR:
		memset(&(rv->data.c), 0, sizeof(rv->data.c)); /* default: black */
		gdk_color_parse (str, &(rv->data.c));
		break;
		/* STRING or STRING_LIST -- if STRING_LIST, just set param to add a STRING */
	default: {
		char *s = g_strdup(str);
		rv->data.s = s;
	}
	}
	return rv;
}


/**
 * Layer::set_defaults:
 *
 * Loop around all parameters for the specified layer to call the function to get the
 *  default value for that parameter
 */
void Layer::set_defaults(Viewport * viewport)
{
	// Sneaky initialize of the viewport value here
	this->viewport = viewport;

	VikLayerInterface * vli = vik_layer_get_interface(this->type);
	char const * layer_name = vli->fixed_layer_name;
	VikLayerParamData data;

	for (int i = 0; i < vli->params_count; i++) {
		// Ensure parameter is for use
		if (vli->params[i].group > VIK_LAYER_NOT_IN_PROPERTIES) {
			// ATM can't handle string lists
			// only DEM files uses this currently
			if (vli->params[i].type != VIK_LAYER_PARAM_STRING_LIST) {
				data = a_layer_defaults_get(layer_name, vli->params[i].name, vli->params[i].type);
				this->set_param(i, data, viewport, true); // Possibly come from a file
			}
		}
	}
}



Layer::Layer()
{
	fprintf(stderr, "Layer::Layer()\n");

	this->name = NULL;
	this->visible = true;
	this->realized = false;

	this->tree_view = NULL;

	fprintf(stderr, "setting iter\n");
	memset(&this->iter, 0, sizeof (this->iter));

	strcpy(this->type_string, "LAST");

	this->vl = (VikLayer *) g_object_new(VIK_LAYER_TYPE, NULL);
	this->vl->layer = this;
}




Layer::Layer(VikLayer * vl_) : Layer()
{
	fprintf(stderr, "Layer::Layer(vl)\n");
}

bool Layer::select_click(GdkEventButton * event, Viewport * viewport, LayerTool * tool)
{
	return false;
}

bool Layer::select_move(GdkEventMotion * event, Viewport * viewport, LayerTool * tool)
{
	return false;
}

void Layer::post_read(Viewport * viewport, bool from_file)
{
	return;
}

bool Layer::select_release(GdkEventButton * event, Viewport * viewport, LayerTool * tool)
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

bool Layer::selected(int subtype, void * sublayer, int type, void * panel)
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

void Layer::write_file(FILE * f) const
{
	return;
}


void Layer::add_menu_items(GtkMenu * menu, void * panel)
{
	return;
}

bool Layer::sublayer_add_menu_items(GtkMenu * menu, void * panel, int subtype, void * sublayer, GtkTreeIter * iter, Viewport * viewport)
{
	return false;
}

char const * Layer::sublayer_rename_request(const char * newname, void * panel, int subtype, void * sublayer, GtkTreeIter * iter)
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


void Layer::realize(TreeView * tree_view_, GtkTreeIter * layer_iter)
{
	this->tree_view = tree_view_;
	this->iter = *layer_iter;
	this->realized = true;

	return;
}

VikLayerParamData Layer::get_param(uint16_t id, bool is_file_operation) const
{
	VikLayerParamData data;
	return data;
}

bool Layer::set_param(uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation)
{
	return false;
}

GtkWindow * gtk_window_from_layer(Layer * layer)
{
	return GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(layer->tree_view->vt)));
}





LayerTool::LayerTool(Window * window, Viewport * viewport, LayerType layer_type)
{
	this->window = window;
	this->viewport = viewport;
	this->layer_type = layer_type;
}





LayerTool::~LayerTool()
{
	if (radioActionEntry.name) {
		free((void *) radioActionEntry.name);
		radioActionEntry.name = NULL;
	}
	if (radioActionEntry.stock_id) {
		free((void *) radioActionEntry.stock_id);
		radioActionEntry.stock_id = NULL;
	}
	if (radioActionEntry.label) {
		free((void *) radioActionEntry.label);
		radioActionEntry.label = NULL;
	}
	if (radioActionEntry.accelerator) {
		free((void *) radioActionEntry.accelerator);
		radioActionEntry.accelerator = NULL;
	}
	if (radioActionEntry.tooltip) {
		free((void *) radioActionEntry.tooltip);
		radioActionEntry.tooltip = NULL;
	}

	if (ruler) {
		free(ruler);
		ruler = NULL;
	}
	if (zoom) {
		if (zoom->pixmap) {
			g_object_unref(G_OBJECT (zoom->pixmap));
		}

		free(zoom);
		zoom = NULL;
	}
	if (ed) {
		free(ed);
		ed = NULL;
	}

}
