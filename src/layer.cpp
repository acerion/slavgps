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




static LayerInterface * vik_layer_interfaces[(int) LayerKind::Max] = {
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




LayerInterface * Layer::get_interface(LayerKind layer_kind)
{
	assert (layer_kind < LayerKind::Max);
	return vik_layer_interfaces[(int) layer_kind];
}



const LayerInterface & Layer::get_interface(void) const
{
	return *this->interface;
}




void Layer::preconfigure_interfaces(void)
{
	SGVariant param_value;

	for (SlavGPS::LayerKind layer_kind = SlavGPS::LayerKind::Aggregate; layer_kind < SlavGPS::LayerKind::Max; ++layer_kind) {

		qDebug() << SG_PREFIX_I << "Preconfiguring interface for layer kind" << layer_kind;

		LayerInterface * interface = Layer::get_interface(layer_kind);

		const QString path = QString(":/icons/layer/") + Layer::get_fixed_layer_kind_string(layer_kind).toLower() + QString(".png");
		interface->action_icon = QIcon(path);

		if (!interface->parameters_c) {
			/* Some layer kinds don't have parameters. */
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

	for (SlavGPS::LayerKind layer_kind = SlavGPS::LayerKind::Aggregate; layer_kind < SlavGPS::LayerKind::Max; ++layer_kind) {

		qDebug() << SG_PREFIX_I << "Postconfiguring interface for layer kind" << layer_kind;

		LayerInterface * interface = Layer::get_interface(layer_kind);

		if (!interface->parameters_c) {
			/* Some layer kinds don't have parameters. */
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

			/* TODO_LATER: make sure that the value read from Layer Defaults is valid.
			   What if LayerDefaults doesn't contain value for given parameter? */
			qDebug() << SG_PREFIX_I << "Wwill call LayerDefaults::get() for param" << param_spec->type_id;
			param_value = LayerDefaults::get(layer_kind, *param_spec);
			interface->parameter_default_values[param_spec->id] = param_value;
		}
	}
}




QString Layer::get_fixed_layer_kind_string(void) const
{
	return this->interface->fixed_layer_kind_string;
}




QString Layer::get_fixed_layer_kind_string(LayerKind layer_kind)
{
	return Layer::get_interface(layer_kind)->fixed_layer_kind_string;
}




QString Layer::get_translated_layer_kind_string(void) const
{
	return this->get_interface(this->m_kind)->ui_labels.translated_layer_kind;
}




QString Layer::get_translated_layer_kind_string(LayerKind layer_kind)
{
	return Layer::get_interface(layer_kind)->ui_labels.translated_layer_kind;
}




Layer * Layer::construct_layer(LayerKind layer_kind, GisViewport * gisview, bool interactive)
{
	qDebug() << SG_PREFIX_I << "Will create new" << Layer::get_translated_layer_kind_string(layer_kind) << "layer";

	Layer * layer = NULL;

	switch (layer_kind) {
	case LayerKind::Aggregate:
		layer = new LayerAggregate();
		break;
	case LayerKind::TRW:
		layer = new LayerTRW();
		break;
	case LayerKind::Coordinates:
		layer = new LayerCoord();
		break;
	case LayerKind::Map:
		layer = new LayerMap();
		break;
	case LayerKind::DEM:
		layer = new LayerDEM();
		break;
	case LayerKind::Georef:
		layer = new LayerGeoref();
		((LayerGeoref *) layer)->configure_from_viewport(gisview);
		break;
#ifdef HAVE_LIBMAPNIK
	case LayerKind::Mapnik:
		layer = new LayerMapnik();
		break;
#endif
	case LayerKind::GPS:
		layer = new LayerGPS();
		break;
	case LayerKind::Max:
	default:
		qDebug() << SG_PREFIX_E << "Unhandled layer kind" << layer_kind;
		break;
	}

	assert (layer);

	layer->set_coord_mode(gisview->get_coord_mode());


	if (interactive && layer->has_properties_dialog) {
		if (!layer->show_properties_dialog()) {
			delete layer;
			return NULL;
		}

		/* Layer::get_translated_layer_kind_string() returns localized string. */
		layer->set_name(Layer::get_translated_layer_kind_string(layer_kind));
	}

	return layer;
}




void Layer::marshall(Pickle & pickle)
{
	pickle.put_raw_object((char *) &this->m_kind, sizeof (this->m_kind));

	this->marshall_params(pickle);
	return;
}




Layer * Layer::unmarshall(Pickle & pickle, GisViewport * gisview)
{
	LayerKind layer_kind;
	pickle.take_raw_object((char *) &layer_kind, sizeof (layer_kind));

	return vik_layer_interfaces[(int) layer_kind]->unmarshall(pickle, gisview);
}





void Layer::marshall_params(Pickle & pickle)
{
	/* Store the internal properties first. TODO_LATER: why we put these members here, in "marshall params"? */
	pickle.put_raw_object((char *) &this->visible, sizeof (this->visible));
	pickle.put_string(this->get_name());

	/* Now the actual parameters. */
	for (auto iter = this->get_interface().parameter_specifications.begin(); iter != this->get_interface().parameter_specifications.end(); iter++) {
		qDebug() << SG_PREFIX_D << "Marshalling parameter" << iter->second->name;

		const SGVariant param_value = this->get_param_value(iter->first, false);
		param_value.marshall(pickle, iter->second->type_id);
	}
}



void Layer::unmarshall_params(Pickle & pickle)
{
	__attribute__((unused)) const pickle_size_t params_size = pickle.take_size();

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
bool Layer::show_properties_dialog(void)
{
	return this->show_properties_dialog(ThisApp::get_main_gis_view());
}




/* Returns true if OK was pressed. */
bool Layer::show_properties_dialog(GisViewport * gisview)
{
	qDebug() << SG_PREFIX_I << "Opening properties dialog for layer" << this->get_translated_layer_kind_string();

	const LayerInterface * iface = this->interface;


	/* Set of values of this layer's parameters.
	   The values will be used to fill widgets inside of Properties Dialog below. */
	std::map<param_id_t, SGVariant> current_parameter_values;
	for (auto iter = iface->parameter_specifications.begin(); iter != iface->parameter_specifications.end(); ++iter) {
		const SGVariant value = this->get_param_value(iter->first, false);
		current_parameter_values.insert(std::pair<param_id_t, SGVariant>(iter->first, value));
	}


	PropertiesDialog dialog(NULL);
	dialog.fill(iface->parameter_specifications, current_parameter_values, iface->parameter_groups);
	if (QDialog::Accepted != dialog.exec()) {
		return false;
	}


	bool must_redraw = false; /* TODO_LATER: why do we need this flag? */
	for (auto iter = iface->parameter_specifications.begin(); iter != iface->parameter_specifications.end(); ++iter) {

		const ParameterSpecification & param_spec = *(iter->second);
		const SGVariant param_value = dialog.get_param_value(param_spec);

		must_redraw |= this->set_param_value(iter->first, param_value, false);
	}


	this->post_read(gisview, false); /* Update any gc's. */
	return true;
}




LayerKind Layer::kind_from_layer_kind_string(const QString & layer_kind_string)
{
	for (LayerKind type = LayerKind::Aggregate; type < LayerKind::Max; ++type) {
		if (layer_kind_string == Layer::get_fixed_layer_kind_string(type)) {
			return type;
		}
	}
	return LayerKind::Max;
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
	for (auto iter = this->interface->parameter_specifications.begin(); iter != this->interface->parameter_specifications.end(); iter++) {
		const param_id_t param_id = iter->first;
		const SGVariant & param_value = this->interface->parameter_default_values.at(param_id);
		this->set_param_value(param_id, param_value, false);
	}

	this->has_properties_dialog = this->interface->has_properties_dialog();
}




Layer::Layer()
{
	strcpy(this->debug_string, "LayerKind::Max");

	this->m_menu_operation_ids.push_back(StandardMenuOperation::Properties);
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Cut);
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Copy);
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Paste);
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Delete);
	this->m_menu_operation_ids.push_back(StandardMenuOperation::New);
}




sg_ret Layer::post_read(__attribute__((unused)) GisViewport * gisview, __attribute__((unused)) bool from_file)
{
	return sg_ret::ok;
}




QString Layer::get_tooltip(void) const
{
	return tr("%1 %2").arg(this->interface->ui_labels.translated_layer_kind).arg((long) this);
}




sg_ret Layer::drag_drop_request(TreeItem * tree_item, __attribute__((unused)) int row, __attribute__((unused)) int col)
{
	qDebug() << SG_PREFIX_E << "Can't drop tree item" << tree_item->get_name() << "into this Layer";
	return sg_ret::err;
}




LayerDataReadStatus Layer::read_layer_data(__attribute__((unused)) QFile & file, __attribute__((unused)) const QString & dirpath)
{
	/* Value that indicates call of base class method. */
	return LayerDataReadStatus::Unrecognized;
}




SaveStatus Layer::write_layer_data(__attribute__((unused)) FILE * file) const
{
	return SaveStatus::Code::Success;
}




sg_ret Layer::menu_add_type_specific_operations(__attribute__((unused)) QMenu & menu, __attribute__((unused)) bool in_tree_view)
{
	return sg_ret::ok;
}




SGVariant Layer::get_param_value(__attribute__((unused)) param_id_t param_id, __attribute__((unused)) bool is_file_operation) const
{
	 return SGVariant(); /* Type ID will be ::Empty. */
}




bool Layer::set_param_value(__attribute__((unused)) param_id_t param_id, __attribute__((unused)) const SGVariant & param_value, __attribute__((unused)) bool is_file_operation)
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




Window * Layer::get_window(void) const
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




LayerKind& SlavGPS::operator++(LayerKind& layer_kind)
{
	layer_kind = static_cast<LayerKind>(static_cast<int>(layer_kind) + 1);
	return layer_kind;
}




QDebug SlavGPS::operator<<(QDebug debug, const LayerKind & layer_kind)
{
	switch (layer_kind) {
	case LayerKind::Aggregate:
		debug << "LayerAggregate";
		break;
	case LayerKind::TRW:
		debug << "LayerTRW";
		break;
	case LayerKind::Coordinates:
		debug << "LayerCoord";
		break;
	case LayerKind::Georef:
		debug << "LayerGeoref";
		break;
	case LayerKind::GPS:
		debug << "LayerGPS";
		break;
	case LayerKind::Map:
		debug << "LayerMap";
		break;
	case LayerKind::DEM:
		debug << "LayerDEM";
		break;
#ifdef HAVE_LIBMAPNIK
	case LayerKind::Mapnik:
		debug << "LayerMapnik";
		break;
#endif
	case LayerKind::Max:
		qDebug() << SG_PREFIX_E << "Unexpectedly handling LayerKind::Max";
		debug << "(layer-type-last)";
		break;
	default:
		debug << "layer-type-unknown";
		qDebug() << SG_PREFIX_E << "Invalid layer kind" << (int) layer_kind;
		break;
	};

	return debug;
}




void Layer::location_info_cb(void) /* Slot. */
{

}




std::list<TreeItem *> Layer::get_items_by_date(__attribute__((unused)) const QDate & search_date) const
{
	std::list<TreeItem *> result;
	return result;
}




Time Layer::get_timestamp(void) const
{
	return Time(0, TimeType::Unit::internal_unit());
}




void Layer::request_new_viewport_center(GisViewport * gisview, const Coord & coord)
{
	if (gisview) {
		gisview->set_center_coord(coord);
		this->emit_tree_item_changed("Requesting change of center coordinate of viewport");
	}
}




/**
   @reviewed-on 2019-12-29
*/
Layer * Layer::get_immediate_layer(void)
{
	/* We are already a layer, no need to go up in hierarchy of
	   tree items to find our immediate layer. */
	return this;
}




/**
   @reviewed-on 2019-12-29
*/
bool Layer::is_layer(void) const
{
	return true;
}




/**
   @reviewed-on 2019-12-31
*/
sg_ret Layer::add_child_item(__attribute__((unused)) TreeItem * item, __attribute__((unused)) bool allow_reordering)
{
	qDebug() << SG_PREFIX_E << "Called the method for base class";
	return sg_ret::err;

}




/**
   @reviewed-on 2019-12-30
*/
sg_ret Layer::cut_child_item(__attribute__((unused)) TreeItem * item)
{
	qDebug() << SG_PREFIX_E << "Called the method for base class";
	return sg_ret::err;
}




/**
   @reviewed-on 2019-12-30
*/
sg_ret Layer::copy_child_item(__attribute__((unused)) TreeItem * item)
{
	qDebug() << SG_PREFIX_E << "Called the method for base class";
	return sg_ret::err;
}




/**
   @reviewed-on 2019-12-30
*/
sg_ret Layer::delete_child_item(__attribute__((unused)) TreeItem * item, __attribute__((unused)) bool confirm_deleting)
{
	qDebug() << SG_PREFIX_E << "Called the method for base class";
	return sg_ret::err;
}
