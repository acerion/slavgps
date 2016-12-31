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

#include "layer_defaults.h"
#include "layer.h"
#include "window.h"
#include "layer_trw.h"
#include "layer_aggregate.h"
#include "layer_coord.h"
#include "layer_dem.h"
#ifdef K
#include "vikmapslayer.h"
#include "vikgeoreflayer.h"
#include "vikgpslayer.h"
#include "vikmapniklayer.h"
#endif
#include "globals.h"
#include "tree_view.h"
#include "uibuilder_qt.h"




using namespace SlavGPS;




/* Functions common to all layers. */
/* TODO longone: rename interface free -> finalize. */

extern LayerInterface vik_aggregate_layer_interface;
extern LayerInterface vik_trw_layer_interface;
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

#ifdef K
static bool layer_defaults_register(LayerType layer_type);
#endif



void SlavGPS::layer_init(void)
{
#ifdef K
	/* Register all parameter defaults, early in the start up sequence. */
	for (LayerType layer_type = LayerType::AGGREGATE; layer_type < LayerType::NUM_TYPES; ++layer_type) {
		/* ATM ignore the returned value. */
		layer_defaults_register(layer_type);
	}
#endif
}




/**
 * Draw specified layer.
 */
void Layer::emit_changed()
{
	if (this->visible && this->connected_to_tree) {
		Window::set_redraw_trigger(this);
		qDebug() << "SIGNAL: Layer: layer" << this->name << "emits 'changed' signal";
		emit this->changed();
	}
}




/**
 * Should only be done by LayersPanel (hence never used from the background)
 * need to redraw and record trigger when we make a layer invisible.
 */
void Layer::emit_changed_although_invisible()
{
	Window::set_redraw_trigger(this);
	qDebug() << "SIGNAL: Layer: layer" << this->name << "emits 'changed' signal";
	emit this->changed();
}




/* Doesn't set the trigger. should be done by aggregate layer when child emits 'changed' signal. */
void Layer::child_layer_changed_cb(void) /* Slot. */
{
	qDebug() << "SLOT:" << this->name << "received 'child layer changed' signal";
	if (this->visible) {
		/* TODO: this can used from the background - e.g. in acquire
		   so will need to flow background update status through too. */
		qDebug() << "SIGNAL: Layer: layer" << this->name << "emits 'changed' signal";
		emit this->changed();
	}
}




static LayerInterface * vik_layer_interfaces[(int) LayerType::NUM_TYPES] = {
	&vik_aggregate_layer_interface,
	&vik_trw_layer_interface,
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




LayerInterface::LayerInterface(LayerInterfaceConfigure config_function)
{
	this->configure = config_function;
}




void Layer::preconfigure_interfaces(void)
{
	for (SlavGPS::LayerType i = SlavGPS::LayerType::AGGREGATE; i < SlavGPS::LayerType::NUM_TYPES; ++i) {

		LayerInterface * interface = Layer::get_interface(i);
		interface->configure(interface);
		QString path = QString(":/icons/layer/") + QString(interface->layer_type_string).toLower() + QString(".png");
		qDebug() << "II: Layer: preconfiguring interface, action icon path is" << path;
		interface->action_icon = QIcon(path);

		interface->parameter_value_defaults = new std::map<param_id_t, ParameterValue>;

		if (!interface->params) {
			continue;
		}

		interface->layer_parameters = new std::map<param_id_t, Parameter *>;
		int j = 0;
		while (interface->params[j].name) {
			interface->layer_parameters->insert(std::pair<param_id_t, Parameter *>(interface->params[j].id, &interface->params[j]));

			/* Read and store default values of layer's parameters.
			   First try to get program's internal/hardwired value.
			   Then try to get value from settings file. */
			ParameterValue param_value;
			if (interface->params[j].hardwired_default_value) {
				/* This will be overwritten below by value from settings file. */
				param_value = interface->params[j].hardwired_default_value();
			}
			/* kamilTODO: make sure that the value read from Layer Defaults is valid. */
			/* kamilTODO: if invalid, call a_layer_defaults_register() to save the value? */
			param_value = a_layer_defaults_get(interface->layer_type_string, interface->params[j].name, interface->params[j].type);
			(*interface->parameter_value_defaults)[interface->params[j].id] = param_value;

			j++;
		}
	}
}



#ifdef K
/**
 * Store default values for this layer.
 *
 * Returns whether any parameters where registered.
 */
static bool layer_defaults_register(LayerType layer_type)
{
	LayerInterface * layer_interface = Layer::get_interface(layer_type);

	Parameter * params = layer_interface->params;
	if (!params) {
		return false;
	}

	bool answer = false; /* In case all parameters are 'not in properties'. */
	uint16_t params_count = layer_interface->params_count;

	/* Process each parameter. */
	for (uint16_t i = 0; i < params_count; i++) {
		if (params[i].group != VIK_LAYER_NOT_IN_PROPERTIES) {
			if (params[i].hardwired_default_value) {
				ParameterValue value = params[i].hardwired_default_value();
				a_layer_defaults_register(layer_interface->fixed_layer_name, &params[i], value);
				answer = true;
			}
		}
	}

	return answer;
}
#endif




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




Layer * Layer::new_(LayerType layer_type, Viewport * viewport)
{
	qDebug() << "II: Layer: will create new" << Layer::get_interface(layer_type)->layer_type_string << "layer";

	assert (layer_type != LayerType::NUM_TYPES);

	Layer * layer = NULL;

	if (layer_type == LayerType::AGGREGATE) {
		layer = new LayerAggregate();

	} else if (layer_type == LayerType::TRW) {
		layer = new LayerTRW();
		((LayerTRW *) layer)->set_coord_mode(viewport->get_coord_mode());

	} else if (layer_type == LayerType::COORD) {
		layer = new LayerCoord();
	}
#ifndef SLAVGPS_QT
	else if (layer_type == LayerType::MAPS) {
		layer = new LayerMaps();
	}
#endif
	else if (layer_type == LayerType::DEM) {
		layer = new LayerDEM();
	}
#ifndef SLAVGPS_QT
	else if (layer_type == LayerType::GEOREF) {
		layer = new LayerGeoref();
		((LayerGeoref *) layer)->configure_form_viewport(viewport);

#ifdef HAVE_LIBMAPNIK
	} else if (layer_type == LayerType::MAPNIK) {
		layer = new LayerMapnik();
#endif
	} else if (layer_type == LayerType::GPS) {
		layer = new LayerGPS();
		((LayerGPS *) layer)->set_coord_mode(viewport->get_coord_mode());
	}
#endif
	else {
		assert (0);
	}

	assert (layer);

	return layer;
}





void Layer::draw_visible(Viewport * viewport)
{
	if (this->visible) {
		qDebug() << "II: Layer: calling draw() for" << this->name;
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
	std::map<param_id_t, Parameter *> * parameters = this->get_interface()->layer_parameters;
	if (parameters) {
		ParameterValue param_value;
		for (auto iter = parameters->begin(); iter != parameters->end(); iter++) {
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, iter->second->name);

			param_value = this->get_param_value(iter->first, false);
			switch (iter->second->type) {
			case ParameterType::STRING:
				/* Remember need braces as these are macro calls, not single statement functions! */
				if (param_value.s) {
					vlm_append(param_value.s, strlen(param_value.s));
				} else {
					/* Need to insert empty string otherwise the unmarshall will get confused. */
					vlm_append("", 0);
				}
				break;
				/* Print out the string list in the array. */
			case ParameterType::STRING_LIST: {
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




void Layer::unmarshall_params(uint8_t * data, int datalen)
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

	std::map<param_id_t, Parameter *> * parameters = this->get_interface()->layer_parameters;
	if (parameters) {
		ParameterValue param_value;

		for (auto iter = parameters->begin(); iter != parameters->end(); iter++) {
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, iter->second->name);

			switch (iter->second->type) {
			case ParameterType::STRING:
				s = (char *) malloc(vlm_size + 1);
				s[vlm_size] = 0;
				vlm_read(s);
				param_value.s = s;
				this->set_param_value(iter->first, param_value, false);
				free(s);
				break;
			case ParameterType::STRING_LIST: {
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
				this->set_param_value(iter->first, param_value, false);
				/* Don't free -- string list is responsibility of the layer. */

				break;
			}
			default:
				vlm_read(&param_value);
				this->set_param_value(iter->first, param_value, false);
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

	delete right_click_menu;

	delete this->menu_data;
}




bool Layer::layer_selected(TreeItemType type, Sublayer * sublayer)
{
	bool result = this->selected(type, sublayer);
	if (result) {
		return result;
	} else {
		return this->get_window()->clear_highlight();
	}

}




uint16_t SlavGPS::vik_layer_get_menu_items_selection(Layer * layer)
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




/* Returns true if OK was pressed. */
bool Layer::properties_dialog(Viewport * viewport)
{
	qDebug() << "II: Layer: opening properties dialog for layer" << this->get_interface(this->type)->layer_type_string;

	PropertiesDialog dialog(NULL);
	dialog.fill(this);
	int dialog_code = dialog.exec();

	if (dialog_code == QDialog::Accepted) {

		bool must_redraw = false;

		std::map<param_id_t, Parameter *> * parameters = this->get_interface()->layer_parameters;
		for (auto iter = parameters->begin(); iter != parameters->end(); iter++) {
			ParameterValue param_value = dialog.get_param_value(iter->first, iter->second);
			bool set = this->set_param_value(iter->first, param_value, false);
			if (set) {
				must_redraw = true;
			}
		}

		this->post_read(viewport, false); /* Update any gc's. */
		return true;
	} else {
		/* Redraw (?). */
		return false;
	}

#if 0
	int prop = a_uibuilder_properties_factory(_("Layer Properties"),
						  viewport->get_toolkit_window(),
						  layer->get_interface()->params,
						  layer->get_interface()->params_count,
						  layer->get_interface()->params_groups,
						  layer->get_interface()->params_groups_count,
						  (bool (*)(void*, uint16_t, ParameterValue, void*, bool)) layer->set_param,
						  layer,
						  viewport,
						  (ParameterValue (*)(void*, uint16_t, bool)) layer->get_param,
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
		if (strcasecmp(str, Layer::get_interface(i)->layer_type_string) == 0) {
			return i;
		}
	}
	return LayerType::NUM_TYPES;
}




void vik_layer_typed_param_data_free(void * gp)
{
	ParameterValueTyped * val = (ParameterValueTyped *) gp;
	switch (val->type) {
	case ParameterType::STRING:
		if (val->data.s) {
			free((void *) val->data.s);
		}
		break;
		/* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
		 * the internals call get_param -- i.e. it should be managed w/in the layer.
		 * The value passed by the internals into set_param should also be managed
		 * by the layer -- i.e. free'd by the layer.
		 */
	case ParameterType::STRING_LIST:
		fprintf(stderr, "WARNING: Param strings not implemented\n"); //fake it
		break;
	default:
		break;
	}
	free(val);
}




ParameterValueTyped * SlavGPS::vik_layer_typed_param_data_copy_from_data(ParameterType type, ParameterValue val)
{
	ParameterValueTyped * newval = (ParameterValueTyped *) malloc(1 * sizeof (ParameterValueTyped));
	newval->data = val;
	newval->type = type;
	switch (newval->type) {
	case ParameterType::STRING: {
		char * s = g_strdup(newval->data.s);
		newval->data.s = s;
		break;
	}
		/* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
		 * the internals call get_param -- i.e. it should be managed w/in the layer.
		 * The value passed by the internals into set_param should also be managed
		 * by the layer -- i.e. free'd by the layer.
		 */
	case ParameterType::STRING_LIST:
		fprintf(stderr, "CRITICAL: Param strings not implemented\n"); //fake it
		break;
	default:
		break;
	}
	return newval;
}




#define TEST_BOOLEAN(str) (! ((str)[0] == '\0' || (str)[0] == '0' || (str)[0] == 'n' || (str)[0] == 'N' || (str)[0] == 'f' || (str)[0] == 'F'))

ParameterValueTyped * SlavGPS::vik_layer_data_typed_param_copy_from_string(ParameterType type, const char * str)
{
	ParameterValueTyped * rv = (ParameterValueTyped *) malloc(1 * sizeof (ParameterValueTyped));
	rv->type = type;
	switch (type) {
	case ParameterType::DOUBLE:
		rv->data.d = strtod(str, NULL);
		break;
	case ParameterType::UINT:
		rv->data.u = strtoul(str, NULL, 10);
		break;
	case ParameterType::INT:
		rv->data.i = strtol(str, NULL, 10);
		break;
	case ParameterType::BOOLEAN:
		rv->data.b = TEST_BOOLEAN(str);
		break;
	case ParameterType::COLOR:
		{
			QColor color(str);
			rv->data.c.r = color.red();
			rv->data.c.g = color.green();
			rv->data.c.b = color.blue();
			rv->data.c.a = color.alpha();
			break;
		}
	/* STRING or STRING_LIST -- if STRING_LIST, just set param to add a STRING. */
	default: {
		char *s = g_strdup(str);
		rv->data.s = s;
	}
	}
	return rv;
}




/**
   Every layer has a set of parameters.
   Every new layer gets assigned some initial/default values of these parameters.
   These initial/default values of parameters are stored in the Layer Interface.
   This method copies the values from the interface into given layer.
*/
void Layer::set_initial_parameter_values(void)
{
	char const * layer_name = this->get_interface()->layer_type_string;
	ParameterValue param_value;

	std::map<param_id_t, Parameter *> * parameters = this->interface->layer_parameters;
	std::map<param_id_t, ParameterValue> * defaults = this->interface->parameter_value_defaults;

	for (auto iter = parameters->begin(); iter != parameters->end(); iter++) {
		/* Ensure parameter is for use. */
		if (iter->second->group > VIK_LAYER_NOT_IN_PROPERTIES) {
			/* ATM can't handle string lists.
			   Only DEM files uses this currently. */
			if (iter->second->type != ParameterType::STRING_LIST) {
				param_value = defaults->at(iter->first);
				this->set_param_value(iter->first, param_value, true); /* Possibly comes from a file. */
			}
		}
	}
}




Layer::Layer()
{
	qDebug() << "II: Layer::Layer()";

	strcpy(this->debug_string, "LayerType::NUM_TYPES");

	this->tree_item_type = TreeItemType::LAYER; /* TODO: re-think initializing parent classes of Layer and Sublayer. */

	this->menu_data = new trw_menu_sublayer_t;
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




QString Layer::tooltip()
{
	return QString(tr("Layer::tooltip"));
}




QString Layer::sublayer_tooltip(Sublayer * sublayer)
{
	return QString("Layer::sublayer_tooltip");
}




bool Layer::selected(TreeItemType type, Sublayer * sublayer)
{
	return false;
}





bool Layer::select_tool_context_menu(QMouseEvent * event, Viewport * viewport)
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




void Layer::cut_sublayer(Sublayer * sublayer)
{
	return;
}




void Layer::copy_sublayer(Sublayer * sublayer, uint8_t ** item, unsigned int * len)
{
	return;
}




bool Layer::paste_sublayer(Sublayer * sublayer, uint8_t * item, size_t len)
{
	return false;
}




void Layer::delete_sublayer(Sublayer * sublayer)
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




void Layer::add_menu_items(QMenu & menu)
{
	return;
}




bool Layer::sublayer_add_menu_items(QMenu & menu)
{
	return false;
}




char const * Layer::sublayer_rename_request(Sublayer * sublayer, const char * newname, LayersPanel * panel)
{
	return NULL;
}




bool Layer::sublayer_toggle_visible(Sublayer * sublayer)
{
	/* If unknown, will always be visible. */
	return true;
}




void Layer::connect_to_tree(TreeView * tree_view_, TreeIndex const & layer_index)
{
	this->tree_view = tree_view_;
	this->index = layer_index;
	this->connected_to_tree = true;

	return;
}




ParameterValue Layer::get_param_value(param_id_t id, bool is_file_operation) const
{
	ParameterValue param_value;
	return param_value;
}




bool Layer::set_param_value(uint16_t id, ParameterValue param_value, bool is_file_operation)
{
	return false;
}




LayerTool::LayerTool(Window * window, Viewport * viewport, LayerType layer_type)
{
	this->window = window;
	this->viewport = viewport;
	this->layer_type = layer_type;
	if (layer_type == LayerType::NUM_TYPES) {
		strcpy(this->debug_string, "LayerType::generic");
	} else {
		strcpy(this->debug_string, "LayerType::");
		strcpy(this->debug_string + 11, Layer::get_interface(layer_type)->layer_type_string);
	}
}




LayerTool::~LayerTool()
{
#ifndef SLAVGPS_QT
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




/**
   @brief Return Pretty-print name of tool that can be used in UI
*/
QString LayerTool::get_description() const
{
	return this->action_tooltip;
}




/* Background drawing hook, to be passed the viewport. */
static bool tool_sync_done = true; /* TODO: get rid of this global variable. */




void LayerTool::marker_begin_move(int x, int y)
{
	assert (this->ed);

	this->ed->holding = true;

	this->ed->ed_pen = new QPen(QColor("black"));
	this->ed->ed_pen->setWidth(2);

	//gdk_gc_set_function(this->ed->en_pen, GDK_INVERT);
	this->viewport->draw_rectangle(*this->ed->ed_pen, x-3, y-3, 6, 6);
	this->viewport->sync();
	this->ed->oldx = x;
	this->ed->oldy = y;
	this->ed->moving = false;
}




void LayerTool::marker_moveto(int x, int y)
{
	assert (this->ed);

	this->viewport->draw_rectangle(*this->ed->ed_pen, this->ed->oldx - 3, this->ed->oldy - 3, 6, 6);
	this->viewport->draw_rectangle(*this->ed->ed_pen, x-3, y-3, 6, 6);
	this->ed->oldx = x;
	this->ed->oldy = y;
	this->ed->moving = true;

	if (tool_sync_done) {
		this->viewport->sync();
		tool_sync_done = true;
	}
}




void LayerTool::marker_end_move(void)
{
	assert (this->ed);

	this->viewport->draw_rectangle(*this->ed->ed_pen, this->ed->oldx - 3, this->ed->oldy - 3, 6, 6);
	delete this->ed->ed_pen;
	this->ed->holding = false;
	this->ed->moving = false;
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
	return this->tree_view->get_layers_panel()->get_window();
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




LayerType& SlavGPS::operator++(LayerType& layer_type)
{
	layer_type = static_cast<LayerType>(static_cast<int>(layer_type) + 1);
	return layer_type;
}




void Layer::visibility_toggled_cb(QStandardItem * item) /* Slot. */
{
	if (item->column() == (int) LayersTreeColumn::VISIBLE) {
		QVariant layer_variant = item->data(RoleLayerData);
		Layer * layer = layer_variant.value<Layer *>();
		if (layer == this) {
			fprintf(stderr, "Layer %s/%s: slot 'changed' called, visibility = %d\n", this->debug_string, this->name, (int) item->checkState());
		}
	}
}




void Layer::location_info_cb(void) /* Slot. */
{

}




sg_uid_t Sublayer::get_uid(void)
{
	return this->uid;
}
