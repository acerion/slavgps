/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2016 - 2018 Kamil Ignacak <acerion@wp.pl>
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




#include <cassert>




#include <QSettings>
#include <QDebug>




#include "ui_builder.h"
#include "layer.h"
#include "layer_defaults.h"
#include "layer_interface.h"
#include "dir.h"




using namespace SlavGPS;




#define SG_MODULE "Layer Defaults"
#define VIKING_LAYER_DEFAULTS_INI_FILE "viking_layer_defaults.ini"




/* A list of the parameter types in use. */
QSettings * LayerDefaults::keyfile = NULL;
bool LayerDefaults::loaded = false;




SGVariant LayerDefaults::get_parameter_value(LayerKind layer_kind, const ParameterSpecification & param_spec)
{
	SGVariant value;
	QString group = Layer::get_fixed_layer_kind_string(layer_kind);


	if (!LayerDefaults::keyfile) { /* Don't use LayerDefaults::loaded, it may be set to false during initialization stage. */
		/* We shouldn't be even able to call this function. */
		qDebug() << SG_PREFIX_E << "Trying to get parameter value when layer defaults aren't initialized";
		return value;
	}


	QString key(group + QString("/") + param_spec.name);
	QVariant variant = LayerDefaults::keyfile->value(key);

	if (!variant.isValid()) {
		/* Not necessarily an error. Maybe this value simply
		   doesn't exist in config file. */
		qDebug() << SG_PREFIX_W << "Failed to read key" << key;
		return value; /* Here the value is invalid. */
	}

	switch (param_spec.type_id) {
	case SGVariantType::Double:
		value = SGVariant((double) variant.toDouble());
		break;

	case SGVariantType::Int: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		value = SGVariant((int32_t) variant.toLongLong(), SGVariantType::Int);
		break;

	case SGVariantType::Enumeration: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		value = SGVariant(variant.toInt(), SGVariantType::Enumeration);
		break;

	case SGVariantType::Boolean:
		value = SGVariant((bool) variant.toBool());
		break;

	case SGVariantType::String:
		value = SGVariant(variant.toString());
		break;

	case SGVariantType::StringList:
		value = SGVariant(variant.toStringList());
		break;

	case SGVariantType::Color:
		value = SGVariant(variant.value<QColor>());
		break;

	case SGVariantType::DurationType:
		/*
		  For Duration we don't use program's internal units
		  but parameter-specific units. Duration parameters
		  are always in some specific unit, e.g. days or hours
		  or seconds.
		*/
		{
			MeasurementScale<Duration> * scale = (MeasurementScale<Duration> *) param_spec.widget_data;
			value = SGVariant(Duration(variant.toLongLong(), scale->m_unit));
		}
		break;

	case SGVariantType::Latitude:
		value = SGVariant(variant.toDouble(), SGVariantType::Latitude);
		break;

	case SGVariantType::Longitude:
		value = SGVariant(variant.toDouble(), SGVariantType::Longitude);
		break;

	case SGVariantType::AltitudeType:
		/* Meters, because that's program's internal/default unit. */
		value = SGVariant(Altitude(variant.toDouble(), AltitudeType::Unit::E::Metres));
		break;

	case SGVariantType::ImageAlphaType:
		value = SGVariant(ImageAlpha(variant.toInt()));
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unhandled value type" << (int) param_spec.type_id;
		/* We don't set value using constructor, so the value will be invalid. */
		break;
	}

	qDebug() << SG_PREFIX_I << "Read value" << value;

	return value;

}




void LayerDefaults::save_parameter_value(const SGVariant & value, LayerKind layer_kind, const QString & param_name, SGVariantType type_id)
{
	QVariant variant;

	switch (type_id) {
	case SGVariantType::Double:
		variant = QVariant((double) value.u.val_double);
		break;
	case SGVariantType::Int: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		variant = QVariant((qlonglong) value.u.val_int);
		break;
	case SGVariantType::Enumeration: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		variant = QVariant(value.u.val_enumeration);
		break;
	case SGVariantType::Boolean:
		variant = QVariant((bool) value.u.val_bool);
		break;
	case SGVariantType::String:
		variant = value.val_string;
		break;
	case SGVariantType::Color:
		variant = value.val_color;
		break;
	case SGVariantType::DurationType:
		variant = QVariant((qlonglong) value.get_duration().ll_value());
		break;
	case SGVariantType::Latitude:
		variant = QVariant(value.get_latitude().value());
		break;
	case SGVariantType::Longitude:
		variant = QVariant(value.get_longitude().unbound_value());
		break;
	case SGVariantType::AltitudeType:
		variant = QVariant(value.get_altitude().ll_value());
		break;
	case SGVariantType::ImageAlphaType:
		variant = QVariant(value.alpha().value());
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled parameter type" << (int) type_id;
		break;
	}

	const QString group = Layer::get_fixed_layer_kind_string(layer_kind);
	QString key(group + QString("/") + param_name);
	LayerDefaults::keyfile->setValue(key, variant);
}




void LayerDefaults::fill_missing_from_hardcoded_defaults(LayerKind layer_kind)
{
	LayerInterface * interface = Layer::get_interface(layer_kind);

	/* Process each parameter. */
	for (auto iter = interface->parameter_specifications.begin(); iter != interface->parameter_specifications.end(); iter++) {
		const ParameterSpecification * param_spec = iter->second;
		if (param_spec->group_id == PARAMETER_GROUP_HIDDEN) {
			qDebug() << SG_PREFIX_I << "Parameter" << param_spec->name << "is hidden, skipping";
			continue;
		}

		/* Now we are dealing with a concrete, layer-specific
		   parameter.

		   See if its value has been read from configuration
		   file.  If not, try to get the value hardcoded in
		   application and add it to the configuration file so
		   that the file has full, consistent set of
		   values. */

		SGVariant value_from_file = LayerDefaults::get_parameter_value(layer_kind, *param_spec);
		if (value_from_file.is_valid()) {
			/* The parameter has already been read from
			   config file. No need to set the parameter
			   and its value using hardcoded value. */
			qDebug() << SG_PREFIX_I << "Parameter" << param_spec->name << "already existed with value" << value_from_file;
			continue;
		}


		/* Value of this parameter has not been read from
		   config file. Try to find it in program's hardcoded
		   values. */
		qDebug() << SG_PREFIX_I << "Getting hardcoded value of parameter" << layer_kind << param_spec->name;
		SGVariant hardcoded_value = param_spec->get_hardcoded_value();
		if (!hardcoded_value.is_valid()) {
			qDebug() << SG_PREFIX_I << "Parameter" << param_spec->name << "doesn't have hardcoded value";
			continue;
		}

		qDebug() << SG_PREFIX_I << "Using" << hardcoded_value << "for parameter named" << param_spec->name;
		save_parameter_value(hardcoded_value, layer_kind, param_spec->name, param_spec->type_id);
	}
}




/**
   \brief Save "layer defaults" configuration to a settings file

   \return true
*/
bool LayerDefaults::save_to_file(void)
{
	LayerDefaults::keyfile->sync();
	return true;
}




/**
   This displays a dialog window showing the default parameter values for the selected layer.
   It allows the parameters to be changed.

   \return true if "OK" key has been pressed in the dialog window,
   \return false otherwise
*/
bool LayerDefaults::show_window(LayerKind layer_kind, QWidget * parent)
{
	LayerInterface * interface = Layer::get_interface(layer_kind);

	/* We want the dialog to present values of layer defaults, so
	   the second argument must be interface->parameter_default_values. */
	std::map<param_id_t, SGVariant> & values = interface->parameter_default_values;


	PropertiesDialog dialog(interface->ui_labels.layer_defaults, parent);
	dialog.fill(interface->parameter_specifications, values, interface->parameter_groups);
	if (QDialog::Accepted != dialog.exec()) {
		return false;
	}


	for (auto iter = interface->parameter_specifications.begin(); iter != interface->parameter_specifications.end(); iter++) {

		const ParameterSpecification & param_spec = *(iter->second);
		const SGVariant param_value = dialog.get_param_value(param_spec);

		values.at(iter->first) = param_value;
		save_parameter_value(param_value, layer_kind, param_spec.name, param_spec.type_id);
	}


	LayerDefaults::save_to_file();

	return true;
}




/**
   @layer_kind
   @layer_param_spec
   @default_value

   Call this function to set the default value for the particular parameter.
*/
void LayerDefaults::set(LayerKind layer_kind, const ParameterSpecification & layer_param_spec, const SGVariant & default_value)
{
	save_parameter_value(default_value, layer_kind, layer_param_spec.name, layer_param_spec.type_id);
}




/**
   @brief Initialize LayerDefaults module

   Call this function at startup.
*/
bool LayerDefaults::init(void)
{
	/* Make sure that layer defaults are initialized after layer
	   interfaces have been configured.  In each program
	   configuration we will always have Coordinate Layer, and
	   that layer has more than zero configurable parameters. */
	assert(Layer::get_interface(LayerKind::Coordinates)->parameter_specifications.size());


	const QString full_path = SlavGPSLocations::get_file_full_path(VIKING_LAYER_DEFAULTS_INI_FILE);
	LayerDefaults::keyfile = new QSettings(full_path, QSettings::IniFormat);
	/* Even if we fail to open the location indicated by file
	   path, the object still may be created and serve as a
	   storage in memory. */
	if (!keyfile) {
		qDebug() << SG_PREFIX_E << "Failed to create storage for layer defaults using file" << full_path;
		return false;
	}


	const enum QSettings::Status status = LayerDefaults::keyfile->status();
	if (status != QSettings::NoError) {
		qDebug() << SG_PREFIX_E << "Invalid status of storage for layer defaults:" << status;
		return false;
	}


	/* Set any missing values from the program's internal/hardcoded defaults. */
	for (LayerKind layer_kind = LayerKind::Aggregate; layer_kind < LayerKind::Max; ++layer_kind) {
		qDebug() << SG_PREFIX_I << "Loading default values from hardcoded values for layer kind" << layer_kind;
		LayerDefaults::fill_missing_from_hardcoded_defaults(layer_kind);
	}


	LayerDefaults::loaded = true;


	return true;
}




/**
   @brief Deinitialize LayerDefaults module

   Call this function on program exit.
*/
void LayerDefaults::uninit(void)
{
	delete LayerDefaults::keyfile;
}




/**
   @layer_kind
   @param_name
   @param_type

   Call this function to get the default value for the parameter requested.
*/
SGVariant LayerDefaults::get(LayerKind layer_kind, const ParameterSpecification & param_spec)
{
	return LayerDefaults::get_parameter_value(layer_kind, param_spec);
}




/**
   \brief Save current layer defaults to settings file

   \return: true if saving was successful
*/
bool LayerDefaults::save(void)
{
	/*
	  Default values of layer parameters may be edited only through
	  dialog window in menu Edit -> Layer Defaults -> <layer kind>.

	  After every instance of editing the values in the dialog, the
	  modified values are saved into layer's Interface and into keyfile.

	  So all we have to do now is to call this function that will
	  simply sync keyfile to disc file.
	*/

	return LayerDefaults::save_to_file();
}
