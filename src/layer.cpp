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

#include "layers_panel.h"
#include "layer_defaults.h"
#include "layer.h"
#include "window.h"
#include "layer_trw.h"
#include "layer_aggregate.h"
#include "layer_coord.h"
#include "layer_map.h"
#include "layer_dem.h"
#include "layer_georef.h"
#include "layer_gps.h"
#include "layer_mapnik.h"
#include "globals.h"
#include "tree_view_internal.h"
#include "ui_builder.h"
#include "vikutils.h"




using namespace SlavGPS;




/* Functions common to all layers. */
/* TODO longone: rename interface free -> finalize. */

extern LayerAggregateInterface vik_aggregate_layer_interface;
extern LayerTRWInterface vik_trw_layer_interface;
extern LayerCoordInterface vik_coord_layer_interface;
extern LayerGeorefInterface vik_georef_layer_interface;
extern LayerGPSInterface vik_gps_layer_interface;
extern LayerMapInterface vik_map_layer_interface;
extern LayerDEMInterface vik_dem_layer_interface;
#ifdef HAVE_LIBMAPNIK
extern LayerMapnikInterface vik_mapnik_layer_interface;
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
	/* Register all parameter defaults, early in the start up sequence. */
	for (LayerType layer_type = LayerType::AGGREGATE; layer_type < LayerType::NUM_TYPES; ++layer_type) {
#ifdef K
		/* ATM ignore the returned value. */
		layer_defaults_register(layer_type);
#endif
	}
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
	&vik_georef_layer_interface,
	&vik_gps_layer_interface,
	&vik_map_layer_interface,
	&vik_dem_layer_interface,
#ifdef HAVE_LIBMAPNIK
	&vik_mapnik_layer_interface,
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




LayerInterface::LayerInterface()
{
}




Layer * LayerInterface::unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	return NULL;
}




void LayerInterface::change_param(GtkWidget *, ui_change_values * )
{
}




void Layer::preconfigure_interfaces(void)
{
	for (SlavGPS::LayerType i = SlavGPS::LayerType::AGGREGATE; i < SlavGPS::LayerType::NUM_TYPES; ++i) {

		LayerInterface * interface = Layer::get_interface(i);

		QString path = QString(":/icons/layer/") + QString(interface->layer_type_string).toLower() + QString(".png");
		qDebug() << "II: Layer: preconfiguring interface, action icon path is" << path;
		interface->action_icon = QIcon(path);

		if (!interface->parameters_c) {
			continue;
		}

		for (Parameter * param_template = interface->parameters_c; param_template->name; param_template++) {
			interface->parameters.insert(std::pair<param_id_t, Parameter *>(param_template->id, param_template));

			/* Read and store default values of layer's parameters.
			   First try to get program's internal/hardwired value.
			   Then try to get value from settings file. */

			SGVariant param_value;

			/* param_value will be overwritten below by value from settings file. */
			parameter_get_hardwired_value(param_value, *param_template);

			/* kamilTODO: make sure that the value read from Layer Defaults is valid. */
			/* kamilTODO: if invalid, call LayerDefaults::set() to save the value? */
			/* kamilTODO: what if LayerDefaults doesn't contain value for given parameter? The line below overwrites hardwired value. */
			param_value = LayerDefaults::get(interface->layer_type_string, param_template->name, param_template->type);
			interface->parameter_default_values[param_template->id] = param_value;
		}
	}
}



/**
 * Store default values for this layer.
 *
 * Returns whether any parameters where registered.
 */
static bool layer_defaults_register(LayerType layer_type)
{
	LayerInterface * layer_interface = Layer::get_interface(layer_type);
	bool answer = false; /* In case all parameters are 'not in properties'. */

	/* Process each parameter. */
	SGVariant value;
	for (auto iter = layer_interface->parameters.begin(); iter != layer_interface->parameters.end(); iter++) {
		if (iter->second->group_id != PARAMETER_GROUP_HIDDEN) {
			if (parameter_get_hardwired_value(value, *iter->second)) {
				LayerDefaults::set(layer_interface->layer_type_string, iter->second, value);
				answer = true;
			}
		}
	}

	return answer;
}



/* Frees old name. */
void Layer::rename(const QString & new_name)
{
	this->name = new_name;
}




const QString & Layer::get_name()
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
	} else if (layer_type == LayerType::MAP) {
		layer = new LayerMap();
	} else if (layer_type == LayerType::DEM) {
		layer = new LayerDEM();
	} else if (layer_type == LayerType::GEOREF) {
		layer = new LayerGeoref();
		((LayerGeoref *) layer)->configure_from_viewport(viewport);
#ifdef HAVE_LIBMAPNIK
	} else if (layer_type == LayerType::MAPNIK) {
		layer = new LayerMapnik();
#endif
	} else if (layer_type == LayerType::GPS) {
		layer = new LayerGPS();
		((LayerGPS *) layer)->set_coord_mode(viewport->get_coord_mode());
	} else {
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
	vlm_append(this->name.toUtf8().constData(), this->name.size());

	/* Now the actual parameters. */
	SGVariant param_value;
	for (auto iter = this->get_interface()->parameters.begin(); iter != this->get_interface()->parameters.end(); iter++) {
		qDebug() << "DD: Layer: Marshalling parameter" << iter->second->name;

		param_value = this->get_param_value(iter->first, false);
		switch (iter->second->type) {
		case SGVariantType::STRING:
			/* Remember need braces as these are macro calls, not single statement functions! */
			if (param_value.s) {
				vlm_append(param_value.s, strlen(param_value.s));
			} else {
				/* Need to insert empty string otherwise the unmarshall will get confused. */
				vlm_append("", 0);
			}
			break;
			/* Print out the string list in the array. */
		case SGVariantType::STRING_LIST: {
			QStringList * a_list = param_value.sl;

			/* Write length of list (# of strings). */
			int listlen = a_list->size();
			g_byte_array_append(b, (uint8_t *) &listlen, sizeof (listlen));

			/* Write each string. */
			for (auto l = a_list->constBegin(); l != a_list->constEnd(); l++) {
				QByteArray arr = (*l).toUtf8();
				const char * s = arr.constData();
				vlm_append(s, strlen(s));
			}

			break;
		}
		default:
			vlm_append(&param_value, sizeof (param_value));
			break;
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
	this->rename(QString(s));
	free(s);

	SGVariant param_value;
	for (auto iter = this->get_interface()->parameters.begin(); iter != this->get_interface()->parameters.end(); iter++) {
		qDebug() << "DD: Layer: Unmarshalling parameter" << iter->second->name;
		switch (iter->second->type) {
		case SGVariantType::STRING:
			s = (char *) malloc(vlm_size + 1);
			s[vlm_size] = 0;
			vlm_read(s);
			param_value.s = s;
			this->set_param_value(iter->first, param_value, false);
			free(s);
			break;
		case SGVariantType::STRING_LIST: {
			int listlen = vlm_size;
			QStringList* list = new QStringList;
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




Layer * Layer::unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	header_t * header = (header_t *) data;
	return vik_layer_interfaces[(int) header->layer_type]->unmarshall(header->data, header->len, viewport);
}




Layer::~Layer()
{
	delete right_click_menu;

	delete this->menu_data;
}




bool Layer::layer_selected(TreeItemType item_type, Sublayer * sublayer)
{
	bool result = this->kamil_selected(item_type, sublayer);
	if (result) {
		return result;
	} else {
		return this->get_window()->clear_highlight();
	}

}




LayerMenuItem Layer::get_menu_items_selection(void)
{
	LayerMenuItem rv = this->get_menu_selection();
	if (rv == LayerMenuItem::NONE) {
		/* Perhaps this line could go to base class. */
		return this->get_interface()->menu_items_selection;
	} else {
		return rv;
	}
}




QIcon Layer::get_icon(void)
{
	return this->get_interface()->action_icon;
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

		for (auto iter = this->get_interface()->parameters.begin(); iter != this->get_interface()->parameters.end(); iter++) {
			const SGVariant param_value = dialog.get_param_value(iter->first, iter->second);
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
						  viewport->get_window(),
						  layer->get_interface()->parameters_c,
						  layer->get_interface()->parameters.size(),
						  layer->get_interface()->parameter_groups,
						  layer->get_interface()->parameter_groups_count,
						  (bool (*)(void*, uint16_t, SGVariant, void*, bool)) layer->set_param,
						  layer,
						  viewport,
						  (SGVariant (*)(void*, uint16_t, bool)) layer->get_param,
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




/**
   Every layer has a set of parameters.
   Every new layer gets assigned some initial/default values of these parameters.
   These initial/default values of parameters are stored in the Layer Interface.
   This method copies the values from the interface into given layer.
*/
void Layer::set_initial_parameter_values(void)
{
	char const * layer_name = this->get_interface()->layer_type_string;
	SGVariant param_value;

	std::map<param_id_t, SGVariant> * defaults = &this->interface->parameter_default_values;

	for (auto iter = this->interface->parameters.begin(); iter != this->interface->parameters.end(); iter++) {
		/* Ensure parameter is for use. */
		if (true || iter->second->group_id > PARAMETER_GROUP_HIDDEN) { /* TODO: how to correctly determine if parameter is "for use"? */
			/* ATM can't handle string lists.
			   Only DEM files uses this currently. */
			if (iter->second->type != SGVariantType::STRING_LIST) {
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




bool Layer::select_click(QMouseEvent * ev, Viewport * viewport, LayerTool * tool)
{
	return false;
}




bool Layer::select_move(QMouseEvent * ev, Viewport * viewport, LayerTool * tool)
{
	return false;
}




void Layer::post_read(Viewport * viewport, bool from_file)
{
	return;
}




bool Layer::select_release(QMouseEvent * ev, Viewport * viewport, LayerTool * tool)
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




bool Layer::kamil_selected(TreeItemType item_type, Sublayer * sublayer)
{
	return false;
}





bool Layer::select_tool_context_menu(QMouseEvent * ev, Viewport * viewport)
{
	return false;
}




void Layer::set_menu_selection(LayerMenuItem selection)
{
	return;
}




LayerMenuItem Layer::get_menu_selection()
{
	return LayerMenuItem::NONE;
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




void Layer::change_coord_mode(CoordMode dest_mode)
{
	return;
}




time_t Layer::get_timestamp()
{
	return 0;
}




void Layer::drag_drop_request(Layer * src, TreeIndex * src_item_iter, void * GtkTreePath_dest_path)
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




QString Layer::sublayer_rename_request(Sublayer * sublayer, const QString & new_name, LayersPanel * panel)
{
	return QString("");
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




SGVariant Layer::get_param_value(param_id_t id, bool is_file_operation) const
{
	SGVariant param_value; /* Type ID will be ::EMPTY. */
	return param_value;
}




bool Layer::set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation)
{
	return false;
}




LayerTool::LayerTool(Window * window_, Viewport * viewport_, LayerType layer_type_)
{
	this->window = window_;
	this->viewport = viewport_;
	this->layer_type = layer_type_;
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
#endif

	delete this->cursor_click;
	delete this->cursor_release;

	delete this->sublayer_edit; /* This may not be the best place to do this delete (::sublayer_edit is alloced in subclasses)... */
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




void LayerTool::sublayer_edit_click(int x, int y)
{
	assert (this->sublayer_edit);

	/* We have clicked on a point, and we are holding it.
	   We hold it during move, until we release it. */
	this->sublayer_edit->holding = true;

	//gdk_gc_set_function(this->sublayer_edit->pen, GDK_INVERT);
	this->viewport->draw_rectangle(this->sublayer_edit->pen, x - 3, y - 3, 6, 6);
	this->viewport->sync();
	this->sublayer_edit->oldx = x;
	this->sublayer_edit->oldy = y;
	this->sublayer_edit->moving = false;
}




void LayerTool::sublayer_edit_move(int x, int y)
{
	assert (this->sublayer_edit);

	this->viewport->draw_rectangle(this->sublayer_edit->pen, this->sublayer_edit->oldx - 3, this->sublayer_edit->oldy - 3, 6, 6);
	this->viewport->draw_rectangle(this->sublayer_edit->pen, x - 3, y - 3, 6, 6);
	this->sublayer_edit->oldx = x;
	this->sublayer_edit->oldy = y;
	this->sublayer_edit->moving = true;

	if (tool_sync_done) {
		this->viewport->sync();
		tool_sync_done = true;
	}
}




void LayerTool::sublayer_edit_release(void)
{
	assert (this->sublayer_edit);

	this->viewport->draw_rectangle(this->sublayer_edit->pen, this->sublayer_edit->oldx - 3, this->sublayer_edit->oldy - 3, 6, 6);
	this->sublayer_edit->holding = false;
	this->sublayer_edit->moving = false;
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
	/* TODO: is it '>' or '<'? */
	return (first->name > second->name);
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
			qDebug() << "[II] Layer" << this->debug_string << this->name << "slot 'changed' called, visibility =" << item->checkState();
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




SublayerEdit::SublayerEdit()
{
	this->pen.setColor(QColor("black"));
	this->pen.setWidth(2);
}




LayerMenuItem operator&(LayerMenuItem& arg1, LayerMenuItem& arg2)
{
	LayerMenuItem result = static_cast<LayerMenuItem>(static_cast<uint16_t>(arg1) | static_cast<uint16_t>(arg2));
	return result;
}


LayerMenuItem operator|(LayerMenuItem& arg1, LayerMenuItem& arg2)
{
	LayerMenuItem result = static_cast<LayerMenuItem>(static_cast<uint16_t>(arg1) & static_cast<uint16_t>(arg2));
	return result;
}


LayerMenuItem operator~(const LayerMenuItem& arg)
{
	LayerMenuItem result = static_cast<LayerMenuItem>(~(static_cast<uint16_t>(arg)));
	return result;
}
