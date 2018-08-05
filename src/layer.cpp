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
#include "tree_view_internal.h"
#include "ui_builder.h"
#include "vikutils.h"
#include "application_state.h"
#include "viewport_internal.h"
#include "file.h"




using namespace SlavGPS;




#define SG_MODULE "Layer"
#define PREFIX ": Layer:" << __FUNCTION__ << __LINE__ << ">"




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



extern Tree * g_tree;




/**
 * Draw specified layer.
 */
void Layer::emit_layer_changed(const QString & where)
{
	if (this->visible && this->tree_view) {
		Window::set_redraw_trigger(this);
		qDebug() << "SIGNAL" PREFIX << "layer" << this->name << "emits 'layer changed' signal @" << where;
		emit this->layer_changed(this->get_name());
	}
}




/**
 * Should only be done by LayersPanel (hence never used from the background)
 * need to redraw and record trigger when we make a layer invisible.
 */
void Layer::emit_layer_changed_although_invisible(const QString & where)
{
	Window::set_redraw_trigger(this);
	qDebug() << "SIGNAL" PREFIX << "layer" << this->name << "emits 'changed' signal @" << where;
	emit this->layer_changed(this->get_name());
}




static LayerInterface * vik_layer_interfaces[(int) LayerType::Max] = {
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
	assert (layer_type < LayerType::Max);
	return vik_layer_interfaces[(int) layer_type];
}



const LayerInterface & Layer::get_interface(void) const
{
	return *this->interface;
}




void Layer::preconfigure_interfaces(void)
{
	SGVariant param_value;

	for (SlavGPS::LayerType layer_type = SlavGPS::LayerType::Aggregate; layer_type < SlavGPS::LayerType::Max; ++layer_type) {

		qDebug() << "II" PREFIX << "preconfiguring interface for layer type" << layer_type;

		LayerInterface * interface = Layer::get_interface(layer_type);

		const QString path = QString(":/icons/layer/") + Layer::get_type_id_string(layer_type).toLower() + QString(".png");
		interface->action_icon = QIcon(path);

		if (!interface->parameters_c) {
			/* Some layer types don't have parameters. */
			continue;
		}

		for (ParameterSpecification * param_spec = interface->parameters_c; param_spec->name; param_spec++) {
			qDebug() << "II" PREFIX << "param spec name is" << param_spec->name << "type is" << param_spec->type_id << "id is" << param_spec->id;
			interface->parameter_specifications.insert(std::pair<param_id_t, ParameterSpecification *>(param_spec->id, param_spec));
		}
	}
}




void Layer::postconfigure_interfaces(void)
{
	SGVariant param_value;

	for (SlavGPS::LayerType layer_type = SlavGPS::LayerType::Aggregate; layer_type < SlavGPS::LayerType::Max; ++layer_type) {

		qDebug() << "II" PREFIX << "postconfiguring interface for layer type" << layer_type;

		LayerInterface * interface = Layer::get_interface(layer_type);

		if (!interface->parameters_c) {
			/* Some layer types don't have parameters. */
			continue;
		}

		for (ParameterSpecification * param_spec = interface->parameters_c; param_spec->name; param_spec++) {

			/* Read and store default values of layer's
			   parameters.

			   LayerDefaults module stores default values
			   of layer parameters (if the default values
			   were defined somewhere).

			   The LayerDefaults module reads the default
			   values from two sources: from program's
			   hardcoded values, and from config file.

			   So at this point we can just call get() and
			   don't care about exact source (config file
			   or hardcoded value) of the value. */

			/* kamilTODO: make sure that the value read from Layer Defaults is valid.
			   What if LayerDefaults doesn't contain value for given parameter? */
			qDebug() << "II" << PREFIX << "will call LayerDefaults::get() for param" << param_spec->type_id;
			param_value = LayerDefaults::get(layer_type, param_spec->name, param_spec->type_id);
			interface->parameter_default_values[param_spec->id] = param_value;
		}
	}
}




void Layer::set_name(const QString & new_name)
{
	this->name = new_name;
}




const QString Layer::get_name(void) const
{
	return this->name;
}




QString Layer::get_type_id_string(void) const
{
	return this->interface->fixed_layer_type_string;
}




QString Layer::get_type_id_string(LayerType type)
{
	return Layer::get_interface(type)->fixed_layer_type_string;
}




QString Layer::get_type_ui_label(void) const
{
	return this->get_interface(this->type)->ui_labels.layer_type;
}




QString Layer::get_type_ui_label(LayerType type)
{
	return Layer::get_interface(type)->ui_labels.layer_type;
}




Layer * Layer::construct_layer(LayerType layer_type, Viewport * viewport, bool interactive)
{
	qDebug() << "II" PREFIX << "will create new" << Layer::get_type_ui_label(layer_type) << "layer";

	Layer * layer = NULL;

	switch (layer_type) {
	case LayerType::Aggregate:
		layer = new LayerAggregate();
		break;
	case LayerType::TRW:
		layer = new LayerTRW();
		break;
	case LayerType::Coordinates:
		layer = new LayerCoord();
		break;
	case LayerType::Map:
		layer = new LayerMap();
		break;
	case LayerType::DEM:
		layer = new LayerDEM();
		break;
	case LayerType::Georef:
		layer = new LayerGeoref();
		((LayerGeoref *) layer)->configure_from_viewport(viewport);
		break;
#ifdef HAVE_LIBMAPNIK
	case LayerType::Mapnik:
		layer = new LayerMapnik();
		break;
#endif
	case LayerType::GPS:
		layer = new LayerGPS();
		break;
	case LayerType::Max:
	default:
		qDebug() << "EE" PREFIX << "unhandled layer type" << layer_type;
		break;
	}

	assert (layer);

	layer->set_coord_mode(viewport->get_coord_mode());


	if (interactive && layer->has_properties_dialog) {
		if (!layer->properties_dialog()) {
			delete layer;
			return NULL;
		}

		/* Layer::get_type_ui_label() returns localized string. */
		layer->set_name(Layer::get_type_ui_label(layer_type));
	}

	return layer;
}




void Layer::marshall(Pickle & pickle)
{
	pickle.put_raw_object((char *) &this->type, sizeof (this->type));

	this->marshall_params(pickle);
	return;
}




Layer * Layer::unmarshall(Pickle & pickle, Viewport * viewport)
{
	LayerType layer_type;
	pickle.take_raw_object((char *) &layer_type, sizeof (layer_type));

	return vik_layer_interfaces[(int) layer_type]->unmarshall(pickle, viewport);
}





void Layer::marshall_params(Pickle & pickle)
{
	/* Store the internal properties first. TODO: why we put these members here, in "marshall params"? */
	pickle.put_raw_object((char *) &this->visible, sizeof (this->visible));
	pickle.put_string(this->name);

	/* Now the actual parameters. */
	for (auto iter = this->get_interface().parameter_specifications.begin(); iter != this->get_interface().parameter_specifications.end(); iter++) {
		qDebug() << "DD" PREFIX << "Marshalling parameter" << iter->second->name;

		const SGVariant param_value = this->get_param_value(iter->first, false);
		param_value.marshall(pickle, iter->second->type_id);
	}
}



void Layer::unmarshall_params(Pickle & pickle)
{
	const pickle_size_t params_size = pickle.take_size();

	pickle.take_object(&this->visible);
	this->set_name(pickle.take_string());

	for (auto iter = this->get_interface().parameter_specifications.begin(); iter != this->get_interface().parameter_specifications.end(); iter++) {
		qDebug() << "DD" PREFIX << "Unmarshalling parameter" << iter->second->name;
		const SGVariant param_value = SGVariant::unmarshall(pickle, iter->second->type_id);
		this->set_param_value(iter->first, param_value, false);
	}
}




Layer::~Layer()
{
	delete right_click_menu;
}




bool Layer::handle_selection_in_tree(void)
{
#ifdef K_OLD_IMPLEMENTATION
	/* A generic layer doesn't want anyone know that it is
	   selected - no one would be interested in knowing that.
	   So we don't set it here. */
	g_tree->selected_tree_item = this;
#endif

	return g_tree->tree_get_main_window()->clear_highlight();
}




QIcon Layer::get_icon(void)
{
	return this->get_interface().action_icon;
}




/* Returns true if OK was pressed. */
bool Layer::properties_dialog()
{
	return this->properties_dialog(g_tree->tree_get_main_viewport());
}




/* Returns true if OK was pressed. */
bool Layer::properties_dialog(Viewport * viewport)
{
	qDebug() << SG_PREFIX_I << "Opening properties dialog for layer" << this->get_type_ui_label();

	const LayerInterface * iface = this->interface;


	/* Set of values of this layer's parameters.
	   The values will be used to fill widgets inside of Properties Dialog below. */
	std::map<param_id_t, SGVariant> values;
	for (auto iter = iface->parameter_specifications.begin(); iter != iface->parameter_specifications.end(); ++iter) {
		const SGVariant value = this->get_param_value(iter->first, false);
		values.insert(std::pair<param_id_t, SGVariant>(iter->first, value));
	}


	PropertiesDialog dialog(NULL);
	dialog.fill(iface->parameter_specifications, values, iface->parameter_groups);
	if (QDialog::Accepted != dialog.exec()) {
		return false;
	}


	bool must_redraw = false; /* TODO: why do we need this flag? */
	for (auto iter = iface->parameter_specifications.begin(); iter != iface->parameter_specifications.end(); ++iter) {

		const ParameterSpecification & param_spec = *(iter->second);
		const SGVariant param_value = dialog.get_param_value(param_spec);

		must_redraw |= this->set_param_value(iter->first, param_value, false);
	}


	this->post_read(viewport, false); /* Update any gc's. */
	return true;
}




LayerType Layer::type_from_type_id_string(const QString & type_id_string)
{
	for (LayerType type = LayerType::Aggregate; type < LayerType::Max; ++type) {
		if (type_id_string == Layer::get_type_id_string(type)) {
			return type;
		}
	}
	return LayerType::Max;
}




/**
   Every layer has a set of parameters.
   Every new layer gets assigned some initial/default values of these parameters.
   These initial/default values of parameters are stored in the Layer Interface.
   This method copies the values from the interface into given layer.
*/
void Layer::set_initial_parameter_values(void)
{
	SGVariant param_value;

	std::map<param_id_t, SGVariant> * defaults = &this->interface->parameter_default_values;

	for (auto iter = this->interface->parameter_specifications.begin(); iter != this->interface->parameter_specifications.end(); iter++) {
		/* Ensure parameter is for use. */
		if (true || iter->second->group_id > PARAMETER_GROUP_HIDDEN) { /* TODO: how to correctly determine if parameter is "for use"? */
			/* ATM can't handle string lists.
			   Only DEM files uses this currently. */
			if (iter->second->type_id != SGVariantType::StringList) {
				param_value = defaults->at(iter->first);
				this->set_param_value(iter->first, param_value, true); /* Possibly comes from a file. */
			}
		}
	}

	this->has_properties_dialog = this->interface->has_properties_dialog();
}




Layer::Layer()
{
	qDebug() << "II" PREFIX << "Layer::Layer()";

	strcpy(this->debug_string, "LayerType::NUM_TYPES");

	this->tree_item_type = TreeItemType::LAYER; /* TODO: re-think initializing parent classes of Layer and TreeItem. */
}




void Layer::post_read(Viewport * viewport, bool from_file)
{
	return;
}




QString Layer::get_tooltip(void) const
{
	return this->interface->ui_labels.layer_type;
}




void Layer::drag_drop_request(Layer * src, TreeIndex & src_item_index, void * GtkTreePath_dest_path)
{
	return;
}




LayerDataReadStatus Layer::read_layer_data(FILE * file, const QString & dirpath)
{
	/* Value that indicates call of base class method. */
	return LayerDataReadStatus::Unrecognized;
}




void Layer::write_layer_data(FILE * file) const
{
	return;
}




void Layer::add_menu_items(QMenu & menu)
{
	return;
}




SGVariant Layer::get_param_value(param_id_t param_id, bool is_file_operation) const
{
	 return SGVariant(); /* Type ID will be ::Empty. */
}




bool Layer::set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation)
{
	return false;
}




bool Layer::compare_timestamp_descending(const Layer * first, const Layer * second)
{
	return first->get_timestamp() > second->get_timestamp();
}




bool Layer::compare_timestamp_ascending(const Layer * first, const Layer * second)
{
	return !Layer::compare_timestamp_descending(first, second);
}




Window * Layer::get_window(void)
{
	return g_tree->tree_get_main_window();
}




void Layer::ref(void)
{
#ifdef K_FIXME_RESTORE
	g_object_ref(this->vl);
#endif
}




void Layer::unref(void)
{
#ifdef K_FIXME_RESTORE
	g_object_unref(this->vl);
	return;
#endif
}




void Layer::weak_ref(LayerRefCB cb, void * obj)
{
#ifdef K_FIXME_RESTORE
	g_object_weak_ref(G_OBJECT (this->vl), cb, obj);
	return;
#endif
}




void Layer::weak_unref(LayerRefCB cb, void * obj)
{
#ifdef K_FIXME_RESTORE
	g_object_weak_unref(G_OBJECT (this->vl), cb, obj);
	return;
#endif
}




LayerType& SlavGPS::operator++(LayerType& layer_type)
{
	layer_type = static_cast<LayerType>(static_cast<int>(layer_type) + 1);
	return layer_type;
}




QDebug SlavGPS::operator<<(QDebug debug, const LayerType & layer_type)
{
	switch (layer_type) {
	case LayerType::Aggregate:
		debug << "LayerAggregate";
		break;
	case LayerType::TRW:
		debug << "LayerTRW";
		break;
	case LayerType::Coordinates:
		debug << "LayerCoord";
		break;
	case LayerType::Georef:
		debug << "LayerGeoref";
		break;
	case LayerType::GPS:
		debug << "LayerGPS";
		break;
	case LayerType::Map:
		debug << "LayerMap";
		break;
	case LayerType::DEM:
		debug << "LayerDEM";
		break;
#ifdef HAVE_LIBMAPNIK
	case LayerType::Mapnik:
		debug << "LayerMapnik";
		break;
#endif
	case LayerType::Max:
		qDebug() << "EE" PREFIX << "unexpectedly handling NUM_TYPES layer type";
		debug << "(layer-type-last)";
		break;
	default:
		debug << "layer-type-unknown";
		qDebug() << "EE" PREFIX << "invalid layer type" << (int) layer_type;
		break;
	};

	return debug;
}




void Layer::location_info_cb(void) /* Slot. */
{

}
