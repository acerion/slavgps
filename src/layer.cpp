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



extern SelectedTreeItems g_selected;




/**
 * Draw specified layer.
 */
void Layer::emit_layer_changed(const QString & where)
{
	if (this->visible && this->tree_view) {
		Window::set_redraw_trigger(this);
		qDebug() << SG_PREFIX_SIGNAL << "Layer" << this->name << "emits 'layer changed' signal @" << where;
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
	qDebug() << SG_PREFIX_SIGNAL << "Layer" << this->name << "emits 'changed' signal @" << where;
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

		qDebug() << SG_PREFIX_I << "Preconfiguring interface for layer type" << layer_type;

		LayerInterface * interface = Layer::get_interface(layer_type);

		const QString path = QString(":/icons/layer/") + Layer::get_type_id_string(layer_type).toLower() + QString(".png");
		interface->action_icon = QIcon(path);

		if (!interface->parameters_c) {
			/* Some layer types don't have parameters. */
			continue;
		}

		for (ParameterSpecification * param_spec = interface->parameters_c; param_spec->type_id != SGVariantType::Empty; param_spec++) {
			qDebug() << SG_PREFIX_I << "Param spec name is" << param_spec->name << "type is" << param_spec->type_id << "id is" << param_spec->id;
			interface->parameter_specifications.insert(std::pair<param_id_t, ParameterSpecification *>(param_spec->id, param_spec));
		}
	}
}




void Layer::postconfigure_interfaces(void)
{
	SGVariant param_value;

	for (SlavGPS::LayerType layer_type = SlavGPS::LayerType::Aggregate; layer_type < SlavGPS::LayerType::Max; ++layer_type) {

		qDebug() << SG_PREFIX_I << "Postconfiguring interface for layer type" << layer_type;

		LayerInterface * interface = Layer::get_interface(layer_type);

		if (!interface->parameters_c) {
			/* Some layer types don't have parameters. */
			continue;
		}

		for (ParameterSpecification * param_spec = interface->parameters_c; param_spec->type_id != SGVariantType::Empty; param_spec++) {

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

			/* TODO_2_LATER: make sure that the value read from Layer Defaults is valid.
			   What if LayerDefaults doesn't contain value for given parameter? */
			qDebug() << SG_PREFIX_I << "Wwill call LayerDefaults::get() for param" << param_spec->type_id;
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
	qDebug() << SG_PREFIX_I << "Will create new" << Layer::get_type_ui_label(layer_type) << "layer";

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
		qDebug() << SG_PREFIX_E << "Unhandled layer type" << layer_type;
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
	/* Store the internal properties first. TODO_2_LATER: why we put these members here, in "marshall params"? */
	pickle.put_raw_object((char *) &this->visible, sizeof (this->visible));
	pickle.put_string(this->name);

	/* Now the actual parameters. */
	for (auto iter = this->get_interface().parameter_specifications.begin(); iter != this->get_interface().parameter_specifications.end(); iter++) {
		qDebug() << SG_PREFIX_D << "Marshalling parameter" << iter->second->name;

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
		qDebug() << SG_PREFIX_D << "Unmarshalling parameter" << iter->second->name;
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
	g_selected.selected_tree_item = this;
#endif

	return ThisApp::get_main_window()->clear_highlight();
}




QIcon Layer::get_icon(void)
{
	return this->get_interface().action_icon;
}




/* Returns true if OK was pressed. */
bool Layer::properties_dialog()
{
	return this->properties_dialog(ThisApp::get_main_viewport());
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


	bool must_redraw = false; /* TODO_2_LATER: why do we need this flag? */
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

   At the point when the function is called, the interface's default
   values of parameters should be already initialized with values from
   config file and with hardcoded values. So this function doesn't
   have to care about using correct source of default values.

   The function sets values of hidden parameters (group_id ==
   PARAMETER_GROUP_HIDDEN) as well.
*/
void Layer::set_initial_parameter_values(void)
{
	std::map<param_id_t, SGVariant> & defaults = this->interface->parameter_default_values;

	for (auto iter = this->interface->parameter_specifications.begin(); iter != this->interface->parameter_specifications.end(); iter++) {
		this->set_param_value(iter->first, defaults.at(iter->first), true); /* true = parameter value possibly comes from a file. TODO_LATER: verify the comment. */
	}

	this->has_properties_dialog = this->interface->has_properties_dialog();
}




Layer::Layer()
{
	qDebug() << SG_PREFIX_I;

	strcpy(this->debug_string, "LayerType::NUM_TYPES");

	this->tree_item_type = TreeItemType::Layer; /* TODO_2_LATER: re-think initializing parent classes of Layer and TreeItem. */
}




void Layer::post_read(Viewport * viewport, bool from_file)
{
	return;
}




QString Layer::get_tooltip(void) const
{
	return tr("%1 %2").arg(this->interface->ui_labels.layer_type).arg((long) this);
}




sg_ret Layer::drag_drop_request(TreeItem * tree_item, int row, int col)
{
	qDebug() << SG_PREFIX_E << "Can't drop tree item" << tree_item->name << "into this Layer";
	return sg_ret::err;
}




sg_ret Layer::dropped_item_is_acceptable(TreeItem * tree_item, bool * result) const
{
	if (NULL == result) {
		return sg_ret::err;
	}

	*result = false;
	return sg_ret::ok;
}




LayerDataReadStatus Layer::read_layer_data(QFile & file, const QString & dirpath)
{
	/* Value that indicates call of base class method. */
	return LayerDataReadStatus::Unrecognized;
}




sg_ret Layer::write_layer_data(FILE * file) const
{
	return sg_ret::ok;
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




bool Layer::compare_timestamp_ascending(const Layer * first, const Layer * second)
{
	return first->get_timestamp() < second->get_timestamp();
}




bool Layer::compare_timestamp_descending(const Layer * first, const Layer * second)
{
	return !Layer::compare_timestamp_ascending(first, second);
}




Window * Layer::get_window(void)
{
	return ThisApp::get_main_window();
}




void Layer::ref_layer(void)
{
#ifdef K_FIXME_RESTORE
	g_object_ref(this->vl);
#endif
}




void Layer::unref_layer(void)
{
#ifdef K_FIXME_RESTORE
	g_object_unref(this->vl);
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
		qDebug() << SG_PREFIX_E << "Unexpectedly handling NUM_TYPES layer type";
		debug << "(layer-type-last)";
		break;
	default:
		debug << "layer-type-unknown";
		qDebug() << SG_PREFIX_E << "Invalid layer type" << (int) layer_type;
		break;
	};

	return debug;
}




void Layer::location_info_cb(void) /* Slot. */
{

}




std::list<TreeItem *> Layer::get_items_by_date(const QDate & search_date) const
{
	std::list<TreeItem *> result;
	return result;
}
