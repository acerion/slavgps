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

#include <cstring>
#include <cstdlib>
#include <cassert>

#include <QString>
#include <QDebug>

#include <glib/gi18n.h>

#include "viklayer_defaults.h"
#include "viklayer.h"
#ifndef SLAVGPS_QT
#include "vikaggregatelayer.h"
#include "viktrwlayer.h"
#endif
#include "vikcoordlayer.h"
#include "vikdemlayer.h"
#ifndef SLAVGPS_QT
#include "vikmapslayer.h"
#include "vikgeoreflayer.h"
#include "vikgpslayer.h"
#include "vikmapniklayer.h"
#endif
#include "globals.h"
#include "viktreeview.h"
#include "uibuilder_qt.h"




using namespace SlavGPS;




/* Functions common to all layers. */
/* TODO longone: rename interface free -> finalize. */

extern LayerInterface vik_aggregate_layer_interface;
#ifndef SLAVGPS_QT
extern LayerInterface vik_trw_layer_interface;
#endif
extern LayerInterface vik_coord_layer_interface;
#ifndef SLAVGPS_QT
extern LayerInterface vik_georef_layer_interface;
extern LayerInterface vik_gps_layer_interface;
extern LayerInterface vik_maps_layer_interface;
#endif
extern LayerInterface vik_dem_layer_interface;
#ifndef SLAVGPS_QT
#ifdef HAVE_LIBMAPNIK
extern LayerInterface vik_mapnik_layer_interface;
#endif
#endif

enum {
	VL_UPDATE_SIGNAL,
	VL_LAST_SIGNAL
};
static unsigned int layer_signals[VL_LAST_SIGNAL] = { 0 };


static bool vik_layer_properties_factory(Layer * layer, Viewport * viewport);
static bool layer_defaults_register(LayerType layer_type);




void SlavGPS::layer_init(void)
{
#ifndef SLAVGPS_QT
	layer_signals[VL_UPDATE_SIGNAL] = g_signal_new("update", G_TYPE_OBJECT,
						       (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), 0, NULL, NULL,
						       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
#endif
	/* Register all parameter defaults, early in the start up sequence. */
	for (LayerType layer_type = LayerType::AGGREGATE; layer_type < LayerType::NUM_TYPES; ++layer_type) {
		/* ATM ignore the returned value. */
		layer_defaults_register(layer_type);
	}
}




/**
 * Invoke the actual drawing via signal method.
 */
bool Layer::idle_draw() /* Slot. */
{
	fprintf(stderr, "layer %s emits 'update' signal\n", this->name);
	emit this->update();
	return false; /* Nothing else to do. */
}




/**
 * Draw specified layer.
 */
void Layer::emit_update()
{
#ifndef SLAVGPS_QT
	if (this->visible && this->realized) {
		GThread * thread = this->get_window()->get_thread();
		if (!thread) {
			/* Do nothing. */
			return;
		}

		Window::set_redraw_trigger(this);

		/* Only ever draw when there is time to do so. */
		if (g_thread_self() != thread) {
			/* Drawing requested from another (background) thread, so handle via the gdk thread method. */
#if 0
			gdk_threads_add_idle((GSourceFunc) idle_draw, this);
#else
			emit this->update();
#endif
		} else {
#if 0
			g_idle_add((GSourceFunc) idle_draw, this);
#else
			emit this->update();
#endif
		}
	}
#endif
}




/**
 * Should only be done by LayersPanel (hence never used from the background)
 * need to redraw and record trigger when we make a layer invisible.
 */
void vik_layer_emit_update_although_invisible(Layer * layer)
{
#ifndef SLAVGPS_QT
	Window::set_redraw_trigger(layer);
#endif
#if 0
	g_idle_add((GSourceFunc) idle_draw, layer);
#else
	emit layer->update();
#endif

}




/* Doesn't set the trigger. should be done by aggregate layer when child emits update. */
void vik_layer_emit_update_secondary(Layer * layer)
{
	if (layer->visible) {
		/* TODO: this can used from the background - e.g. in acquire
		   so will need to flow background update status through too. */
#if 0
		g_idle_add((GSourceFunc) idle_draw, layer);
#else
		emit layer->update();
#endif
	}
}




/* Doesn't set the trigger. should be done by aggregate layer when child emits update. */
void Layer::emit_update_secondary(void) /* Slot. */
{
	if (this->visible) {
		fprintf(stderr, "scheduling 'idle_draw' for layer %s\n", this->name);
		/* TODO: this can used from the background - e.g. in acquire
		   so will need to flow background update status through too. */
#if 0
		g_idle_add((GSourceFunc) idle_draw, this);
#else
		emit this->update();
#endif
	}
}




static LayerInterface * vik_layer_interfaces[(int) LayerType::NUM_TYPES] = {
	&vik_aggregate_layer_interface,
#ifndef SLAVGPS_QT
	&vik_trw_layer_interface,
#endif
	&vik_coord_layer_interface,
#ifndef SLAVGPS_QT
	&vik_georef_layer_interface,
	&vik_gps_layer_interface,
	&vik_maps_layer_interface,
#endif
	&vik_dem_layer_interface,
#ifndef SLAVGPS_QT
#ifdef HAVE_LIBMAPNIK
	&vik_mapnik_layer_interface,
#endif
#endif
};




LayerInterface * Layer::get_interface(LayerType layer_type)
{
	assert (layer_type < LayerType::NUM_TYPES);
	return vik_layer_interfaces[(int) layer_type];
}



LayerInterface * Layer::get_interface(void)
{
	return this->interface;
}



void Layer::configure_interface(LayerInterface * intf, LayerParam * parameters)
{
	this->interface = intf;

	if (parameters && this->interface && !this->interface->layer_parameters) {
		this->interface->layer_parameters = new std::map<layer_param_id_t, LayerParam *>;
		int i = 0;
		while (parameters[i].name) {
			this->interface->layer_parameters->insert(std::pair<layer_param_id_t, LayerParam *>(parameters[i].id, &parameters[i]));
			i++;
		}
	}
}




void Layer::preconfigure_interfaces(void)
{
	for (SlavGPS::LayerType i = SlavGPS::LayerType::AGGREGATE; i < SlavGPS::LayerType::NUM_TYPES; ++i) {
		QString path = QString(":/icons/layer/") + QString(Layer::get_interface(i)->fixed_layer_name).toLower() + QString(".png");
		qDebug() << "path is" << path;
		Layer::get_interface(i)->icon = new QIcon(path);
	}
}




/**
 * Store default values for this layer.
 *
 * Returns whether any parameters where registered.
 */
static bool layer_defaults_register(LayerType layer_type)
{
	/* See if any parameters. */
	LayerParam *params = vik_layer_interfaces[(int) layer_type]->params;
	if (!params) {
		return false;
	}

	bool answer = false; /* In case all parameters are 'not in properties'. */
	uint16_t params_count = vik_layer_interfaces[(int) layer_type]->params_count;

	/* Process each parameter. */
	for (uint16_t i = 0; i < params_count; i++) {
		if (params[i].group != VIK_LAYER_NOT_IN_PROPERTIES) {
			if (params[i].default_value) {
				LayerParamData paramd = params[i].default_value();
				a_layer_defaults_register(&params[i], paramd, vik_layer_interfaces[(int) layer_type]->fixed_layer_name);
				answer = true;
			}
		}
	}

	return answer;
}




/* Frees old name. */
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
#ifndef SLAVGPS_QT
	if (layer_type == LayerType::AGGREGATE) {
		fprintf(stderr, "\n\n\n NEW AGGREGATE\n\n\n");
		layer = new LayerAggregate(viewport);

	} else if (layer_type == LayerType::TRW) {
		fprintf(stderr, "\n\n\n NEW TRW\n\n\n");
		layer = new LayerTRW(viewport);

	} else
#endif
		if (layer_type == LayerType::COORD) {
		fprintf(stderr, "\n\n\n NEW COORD\n\n\n");
		layer = new LayerCoord(viewport);

	}
#ifndef SLAVGPS_QT
		else if (layer_type == LayerType::MAPS) {
		fprintf(stderr, "\n\n\n NEW MAPS\n\n\n");
		layer = new LayerMaps(viewport);

	} else
#endif
	if (layer_type == LayerType::DEM) {
		fprintf(stderr, "\n\n\n NEW DEM\n\n\n");
		layer = new LayerDEM(viewport);

	}
#ifndef SLAVGPS_QT
	else if (layer_type == LayerType::GEOREF) {
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
			/* We translate the name here in order to avoid translating name set by user. */
			layer->rename(_(layer->get_interface()->name));
		} else {
			layer->unref(); /* Cancel that. */
			delete layer;
			layer = NULL;
		}
	}
#endif
	return layer;
}




/* Returns true if OK was pressed. */
bool vik_layer_properties(Layer * layer, Viewport * viewport)
{
#ifndef SLAVGPS_QT
	if (layer->type == LayerType::GEOREF) {
		return layer->properties(viewport);
	}
#endif

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
#ifndef SLAVGPS_QT
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
#endif
}




void Layer::marshall(uint8_t ** data, int * len)
{
	this->marshall_params(data, len);
	return;
}




void Layer::marshall_params(uint8_t ** data, int * datalen)
{


	GByteArray* b = g_byte_array_new();
	int len;

#define vlm_append(obj, sz) 	\
	len = (sz);					\
	g_byte_array_append(b, (uint8_t *) &len, sizeof(len));	\
	g_byte_array_append(b, (uint8_t *) (obj), len);

	/* Store the internal properties first. */
	vlm_append(&this->visible, sizeof (this->visible));
	vlm_append(this->name, strlen(this->name));

	/* Now the actual parameters. */
	std::map<layer_param_id_t, LayerParam *> * parameters = this->get_interface()->layer_parameters;
	if (parameters) {
		LayerParamValue param_value;
		for (auto iter = parameters->begin(); iter != parameters->end(); iter++) {
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, iter->second->name);

			param_value = this->get_param_value(iter->first, false);
			switch (iter->second->type) {
			case LayerParamType::STRING:
				/* Remember need braces as these are macro calls, not single statement functions! */
				if (param_value.s) {
					vlm_append(param_value.s, strlen(param_value.s));
				} else {
					/* Need to insert empty string otherwise the unmarshall will get confused. */
					vlm_append("", 0);
				}
				break;
				/* Print out the string list in the array. */
			case LayerParamType::STRING_LIST: {
				std::list<char *> * a_list = param_value.sl;

				/* Write length of list (# of strings). */
				int listlen = a_list->size();
				g_byte_array_append(b, (uint8_t *) &listlen, sizeof (listlen));

				/* Write each string. */
				for (auto l = a_list->begin(); l != a_list->end(); l++) {
					char * s = *l;
					vlm_append(s, strlen(s));
				}

				break;
			}
			default:
				vlm_append(&param_value, sizeof (param_value));
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

	std::map<layer_param_id_t, LayerParam *> * parameters = this->get_interface()->layer_parameters;
	if (parameters) {
		LayerParamValue param_value;

		for (auto iter = parameters->begin(); iter != parameters->end(); iter++) {
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, iter->second->name);

			switch (iter->second->type) {
			case LayerParamType::STRING:
				s = (char *) malloc(vlm_size + 1);
				s[vlm_size] = 0;
				vlm_read(s);
				param_value.s = s;
				this->set_param_value(iter->first, param_value, viewport, false);
				free(s);
				break;
			case LayerParamType::STRING_LIST: {
				int listlen = vlm_size;
				std::list<char *> * list = new std::list<char *>;
				b += sizeof(int); /* Skip listlen. */;

				for (int j = 0; j < listlen; j++) {
					/* Get a string. */
					s = (char *) malloc(vlm_size + 1);
					s[vlm_size] = 0;
					vlm_read(s);
					list->push_back(s);
				}
				param_value.sl = list;
				this->set_param_value(iter->first, param_value, viewport, false);
				/* Don't free -- string list is responsibility of the layer. */

				break;
			}
			default:
				vlm_read(&param_value);
				this->set_param_value(iter->first, param_value, viewport, false);
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




Layer::~Layer()
{
	if (this->name) {
		free(this->name);
	}
#ifndef SLAVGPS_QT
	/* kamilTODO: free this object. */
	this->vl = NULL;
#endif
}




bool Layer::layer_selected(SublayerType sublayer_type, sg_uid_t sublayer_uid, TreeItemType type)
{
	bool result = this->selected(sublayer_type, sublayer_uid, type);
	if (result) {
		return result;
	} else {
#ifndef SLAVGPS_QT
		return this->get_window()->clear_highlight();
#endif
	}

}




uint16_t vik_layer_get_menu_items_selection(Layer * layer)
{
	uint16_t rv = layer->get_menu_selection();
	if (rv == (uint16_t) -1) {
		/* Perhaps this line could go to base class. */
		return layer->get_interface()->menu_items_selection;
	} else {
		return rv;
	}
}




GdkPixbuf * vik_layer_load_icon(LayerType layer_type)
{
#ifndef SLAVGPS_QT
	assert (layer_type < LayerType::NUM_TYPES);
	if (vik_layer_interfaces[(int) layer_type]->icon) {
		return gdk_pixbuf_from_pixdata(vik_layer_interfaces[(int) layer_type]->icon, false, NULL);
	}
#endif
	return NULL;
}




static bool vik_layer_properties_factory(Layer * layer, Viewport * viewport)
{
#ifdef SLAVGPS_QT

	LayerPropertiesDialog dialog(NULL);
	dialog.fill(layer);
	int dialog_code = dialog.exec();

	if (dialog_code == QDialog::Accepted) {

		bool must_redraw = false;

		std::map<layer_param_id_t, LayerParam *> * parameters = layer->get_interface()->layer_parameters;
		for (auto iter = parameters->begin(); iter != parameters->end(); iter++) {
			LayerParamValue param_value = dialog.get_param_value(iter->first, iter->second);
			bool set = layer->set_param_value(iter->first, param_value, viewport, false);
			if (set) {
				must_redraw = true;
			}
		}

		layer->post_read(viewport, false); /* Update any gc's. */
		return true;
	} else {
		/* Redraw (?). */
		return false;
	}

#else
	int prop = a_uibuilder_properties_factory(_("Layer Properties"),
						  viewport->get_toolkit_window(),
						  layer->get_interface()->params,
						  layer->get_interface()->params_count,
						  layer->get_interface()->params_groups,
						  layer->get_interface()->params_groups_count,
						  (bool (*)(void*, uint16_t, LayerParamData, void*, bool)) layer->set_param,
						  layer,
						  viewport,
						  (LayerParamData (*)(void*, uint16_t, bool)) layer->get_param,
						  layer,
						  layer->get_interface()->change_param)

	switch (prop) {
	case 0:
	case 3:
		return false;
		/* Redraw (?). */
	case 2: {
		layer->post_read(viewport, false); /* Update any gc's. */
	}
	default:
		return true;
	}
#endif
}




LayerType Layer::type_from_string(char const * str)
{
	for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {
		if (strcasecmp(str, Layer::get_interface(i)->fixed_layer_name) == 0) {
			return i;
		}
	}
	return LayerType::NUM_TYPES;
}




void vik_layer_typed_param_data_free(void * gp)
{
	LayerTypedParamData *val = (LayerTypedParamData *)gp;
	switch (val->type) {
	case LayerParamType::STRING:
		if (val->data.s) {
			free((void *) val->data.s);
		}
		break;
		/* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
		 * the internals call get_param -- i.e. it should be managed w/in the layer.
		 * The value passed by the internals into set_param should also be managed
		 * by the layer -- i.e. free'd by the layer.
		 */
	case LayerParamType::STRING_LIST:
		fprintf(stderr, "WARNING: Param strings not implemented\n"); //fake it
		break;
	default:
		break;
	}
	free(val);
}




LayerTypedParamData * vik_layer_typed_param_data_copy_from_data(LayerParamType type, LayerParamData val)
{
	LayerTypedParamData * newval = (LayerTypedParamData *) malloc(1 * sizeof (LayerTypedParamData));
	newval->data = val;
	newval->type = type;
	switch (newval->type) {
	case LayerParamType::STRING: {
		char * s = g_strdup(newval->data.s);
		newval->data.s = s;
		break;
	}
		/* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
		 * the internals call get_param -- i.e. it should be managed w/in the layer.
		 * The value passed by the internals into set_param should also be managed
		 * by the layer -- i.e. free'd by the layer.
		 */
	case LayerParamType::STRING_LIST:
		fprintf(stderr, "CRITICAL: Param strings not implemented\n"); //fake it
		break;
	default:
		break;
	}
	return newval;
}




#define TEST_BOOLEAN(str) (! ((str)[0] == '\0' || (str)[0] == '0' || (str)[0] == 'n' || (str)[0] == 'N' || (str)[0] == 'f' || (str)[0] == 'F'))

LayerTypedParamData * vik_layer_data_typed_param_copy_from_string(LayerParamType type, const char * str)
{
	LayerTypedParamData * rv = (LayerTypedParamData *) malloc(1 * sizeof (LayerTypedParamData));
	rv->type = type;
	switch (type) {
	case LayerParamType::DOUBLE:
		rv->data.d = strtod(str, NULL);
		break;
	case LayerParamType::UINT:
		rv->data.u = strtoul(str, NULL, 10);
		break;
	case LayerParamType::INT:
		rv->data.i = strtol(str, NULL, 10);
		break;
	case LayerParamType::BOOLEAN:
		rv->data.b = TEST_BOOLEAN(str);
		break;
#ifndef SLAVGPS_QT
	case LayerParamType::COLOR:
		memset(&(rv->data.c), 0, sizeof(rv->data.c)); /* Default: black. */
		gdk_color_parse (str, &(rv->data.c));
		break;
#endif
	/* STRING or STRING_LIST -- if STRING_LIST, just set param to add a STRING. */
	default: {
		char *s = g_strdup(str);
		rv->data.s = s;
	}
	}
	return rv;
}




/**
 * Loop around all parameters for the specified layer to call the
 * function to get the default value for that parameter.
 */
void Layer::set_defaults(Viewport * viewport)
{
	/* Sneaky initialize of the viewport value here. */
	this->viewport = viewport;

	char const * layer_name = this->get_interface()->fixed_layer_name;
	LayerParamValue param_value;

	std::map<layer_param_id_t, LayerParam *> * parameters = this->interface->layer_parameters;
	for (auto iter = parameters->begin(); iter != parameters->end(); iter++) {
		/* Ensure parameter is for use. */
		if (iter->second->group > VIK_LAYER_NOT_IN_PROPERTIES) {
			/* ATM can't handle string lists.
			   Only DEM files uses this currently. */
			if (iter->second->type != LayerParamType::STRING_LIST) {
				param_value = a_layer_defaults_get(layer_name, iter->second->name, iter->second->type);
				this->set_param_value(iter->first, param_value, viewport, true); /* Possibly comes from a file. */
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
#ifndef SLAVGPS_QT
	this->vl = (VikLayer *) g_object_new(G_TYPE_OBJECT, NULL);
	g_object_set_data((GObject *) this->vl, "layer", this);
#endif
}




Layer::Layer(VikLayer * vl_) : Layer()
{
	fprintf(stderr, "Layer::Layer(vl)\n");
}




bool Layer::select_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool)
{
	return false;
}




bool Layer::select_move(QMouseEvent * event, Viewport * viewport, LayerTool * tool)
{
	return false;
}




void Layer::post_read(Viewport * viewport, bool from_file)
{
	return;
}




bool Layer::select_release(QMouseEvent * event, Viewport * viewport, LayerTool * tool)
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




char const * Layer::sublayer_tooltip(SublayerType sublayer_type, sg_uid_t sublayer_uid)
{
      static char tmp_buf[32];
      snprintf(tmp_buf, sizeof(tmp_buf), _("Layer::sublayer_tooltip"));
      return tmp_buf;
}




bool Layer::selected(SublayerType sublayer_type, sg_uid_t sublayer_uid, TreeItemType type)
{
	return false;
}





bool Layer::show_selected_viewport_menu(QMouseEvent * event, Viewport * viewport)
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




void Layer::cut_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid)
{
	return;
}




void Layer::copy_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid, uint8_t ** item, unsigned int * len)
{
	return;
}




bool Layer::paste_sublayer(SublayerType sublayer_type, uint8_t * item, size_t len)
{
	return false;
}




void Layer::delete_sublayer(SublayerType sublayer_type, sg_uid_t sublayer_uid)
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
	/* kamilFIXME: Magic number to indicate call of base class method. */
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




bool Layer::sublayer_add_menu_items(GtkMenu * menu, void * panel, SublayerType sublayer_type, sg_uid_t sublayer_uid, GtkTreeIter * iter, Viewport * viewport)
{
	return false;
}




char const * Layer::sublayer_rename_request(const char * newname, void * panel, SublayerType sublayer_type, sg_uid_t sublayer_uid, GtkTreeIter * iter)
{
	return NULL;
}




bool Layer::sublayer_toggle_visible(SublayerType sublayer_type, sg_uid_t sublayer_uid)
{
	/* If unknown, will always be visible. */
	return true;
}




bool Layer::properties(void * viewport)
{
	return false;
}




void Layer::realize(TreeView * tree_view_, QStandardItem * layer_item)
{
	this->tree_view = tree_view_;
	this->item = layer_item;
	this->realized = true;

	return;
}




LayerParamValue Layer::get_param_value(layer_param_id_t id, bool is_file_operation) const
{
	LayerParamValue param_value;
	return param_value;
}




bool Layer::set_param_value(uint16_t id, LayerParamValue param_value, Viewport * viewport, bool is_file_operation)
{
	return false;
}




GtkWindow * Layer::get_toolkit_window()
{
#ifndef SLAVGPS_QT
	return this->tree_view->get_toolkit_window();
#endif
}




LayerTool::LayerTool(Window * window, Viewport * viewport, LayerType layer_type)
{
	this->window = window;
	this->viewport = viewport;
	this->layer_type = layer_type;
}




LayerTool::~LayerTool()
{
#ifndef SLAVGPS_QT
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
#endif

	delete this->cursor_click;
	delete this->cursor_release;
}




bool Layer::compare_timestamp_descending(Layer * first, Layer * second)
{
	return first->get_timestamp() > second->get_timestamp();
}




bool Layer::compare_timestamp_ascending(Layer * first, Layer * second)
{
	return !Layer::compare_timestamp_descending(first, second);
}




bool Layer::compare_name_descending(Layer * first, Layer * second)
{
	return 0 > g_strcmp0(first->name, second->name);
}




bool Layer::compare_name_ascending(Layer * first, Layer * second)
{
	return !Layer::compare_name_descending(first, second);
}




Window * Layer::get_window(void)
{
	return this->viewport->get_window();
}




void Layer::ref(void)
{
#ifndef SLAVGPS_QT
	g_object_ref(this->vl);
#endif
}




void Layer::unref(void)
{
#ifndef SLAVGPS_QT
	g_object_unref(this->vl);
	return;
#endif
}




void Layer::weak_ref(LayerRefCB cb, void * obj)
{
#ifndef SLAVGPS_QT
	g_object_weak_ref(G_OBJECT (this->vl), cb, obj);
	return;
#endif
}




void Layer::weak_unref(LayerRefCB cb, void * obj)
{
#ifndef SLAVGPS_QT
	g_object_weak_unref(G_OBJECT (this->vl), cb, obj);
	return;
#endif
}



bool Layer::the_same_object(Layer const * layer)
{
#ifndef SLAVGPS_QT
	return layer && this->vl == layer->vl;
#endif
}




void Layer::disconnect_layer_signal(Layer * layer)
{
#ifndef SLAVGPS_QT
	unsigned int number_handlers = g_signal_handlers_disconnect_matched(layer->vl, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this->vl);
	if (number_handlers != 1) {
		fprintf(stderr, "CRITICAL: %s: Unexpected number of disconnect handlers: %d\n", __FUNCTION__, number_handlers);
	}
#endif
}




void * Layer::get_toolkit_object(void)
{
#ifndef SLAVGPS_QT
	return (void *) this->vl;
#endif
}




Layer * Layer::get_layer(VikLayer * vl)
{
#ifndef SLAVGPS_QT
	Layer * layer = (Layer *) g_object_get_data((GObject *) vl, "layer");
	return layer;
#endif
}




LayerType& SlavGPS::operator++(LayerType& layer_type)
{
	layer_type = static_cast<LayerType>(static_cast<int>(layer_type) + 1);
	return layer_type;
}



void Layer::visibility_toggled(QStandardItem * item) /* Slot. */
{
	if (item->column() == (int) LayersTreeColumn::VISIBLE) {
		QVariant layer_variant = item->data(RoleLayerData);
		Layer * layer = layer_variant.value<Layer *>();
		if (layer == this) {
			fprintf(stderr, "Layer %s/%s: slot 'changed' called, visibility = %d\n", this->type_string, this->name, (int) item->checkState());
		}
	}
}
