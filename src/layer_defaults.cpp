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




#define PREFIX " Layer Defaults:" << __FUNCTION__ << __LINE__ << ">>"
#define VIKING_LAYER_DEFAULTS_INI_FILE "viking_layer_defaults.ini"




/* A list of the parameter types in use. */
static QSettings * g_keyfile = NULL;
static bool g_loaded;




SGVariant LayerDefaults::get_parameter_value(LayerType layer_type, const char * name, SGVariantType type_id)
{
	SGVariant value;
	QString group = Layer::get_type_id_string(layer_type);

	QString key(group + QString("/") + QString(name));
	QVariant variant = g_keyfile->value(key);

	if (!variant.isValid()) {
		qDebug() << "EE" PREFIX << "failed to read key" << key;
		return value; /* Here the value is invalid. */
	}

	switch (type_id) {
	case SGVariantType::Double:
		value = SGVariant((double) variant.toDouble());
		break;

	case SGVariantType::Uint:
		value = SGVariant((uint32_t) variant.toULongLong());
		break;

	case SGVariantType::Int:
		value = SGVariant((int32_t) variant.toLongLong());
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

	default:
		qDebug() << "EE" PREFIX << "unhandled value type" << (int) type_id;
		/* We don't set value using constructor, so the value will be invalid. */
		break;
	}

	qDebug() << "II" PREFIX << "read value" << value;

	return value;

}




void LayerDefaults::save_parameter_value(const SGVariant & value, LayerType layer_type, const char * name, SGVariantType ptype)
{
	QVariant variant;

	switch (ptype) {
	case SGVariantType::Double:
		variant = QVariant((double) value.u.val_double);
		break;
	case SGVariantType::Uint:
		variant = QVariant((qulonglong) value.u.val_uint);
		break;
	case SGVariantType::Int:
		variant = QVariant((qlonglong) value.u.val_int);
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
	default:
		qDebug() << "EE" PREFIX << "unhandled parameter type" << (int) ptype;
		return;
	}

	const QString group = Layer::get_type_id_string(layer_type);
	QString key(group + QString("/") + QString(name));
	g_keyfile->setValue(key, variant);
}




void LayerDefaults::fill_missing_from_hardcoded_defaults(LayerType layer_type)
{
	LayerInterface * interface = Layer::get_interface(layer_type);

	/* Process each parameter. */
	for (auto iter = interface->parameter_specifications.begin(); iter != interface->parameter_specifications.end(); iter++) {
		const ParameterSpecification * param_spec = iter->second;
		if (param_spec->group_id == PARAMETER_GROUP_HIDDEN) {
			qDebug() << "II" PREFIX << "Parameter" << param_spec->name << "is hidden, skipping";
			continue;
		}

		/* Now we are dealing with a concrete, layer-specific
		   parameter.

		   See if its value has been read from configuration
		   file.  If not, try to get the value hardcoded in
		   application and add it to the configuration file so
		   that the file has full, consistent set of
		   values. */

		SGVariant value_from_file = LayerDefaults::get_parameter_value(layer_type, param_spec->name, param_spec->type_id);
		if (value_from_file.is_valid()) {
			/* The parameter has already been read from
			   config file. No need to set the parameter
			   and its value using hardcoded value. */
			qDebug() << "II" PREFIX << "Parameter" << param_spec->name << "already existed with value" << value_from_file;
			continue;
		}


		/* Value of this parameter has not been read from
		   config file. Try to find it in program's hardcoded
		   values. */
		qDebug() << "II" PREFIX << "Getting hardcoded value of parameter" << layer_type << param_spec->name;
		SGVariant hardcoded_value = param_spec->get_hardcoded_value();
		if (!hardcoded_value.is_valid()) {
			qDebug() << "II" PREFIX << "Parameter" << param_spec->name << "doesn't have hardcoded value";
			continue;
		}

		qDebug() << "II" PREFIX << "Using" << hardcoded_value << "for parameter named" << param_spec->name;
		save_parameter_value(hardcoded_value, layer_type, param_spec->name, param_spec->type_id);
	}
}




/**
   \brief Load "layer defaults" configuration from a settings file
*/
bool LayerDefaults::load_from_file(void)
{
	/* Make sure that layer defaults are initialized after layer
	   interfaces have been configured.  In each program
	   configuration we will always have Coordinate Layer, and
	   that layer has more than zero configurable parameters. */
	assert(Layer::get_interface(LayerType::Coordinates)->parameter_specifications.size());

	if (!g_keyfile) {
		qDebug() << "EE" PREFIX << "key file is not initialized";
		exit(EXIT_FAILURE);
	}

	enum QSettings::Status status = g_keyfile->status();
	if (status != QSettings::NoError) {
		qDebug() << "EE" PREFIX << "key file status is" << status;
		return false;
	}

	/* Set any missing values from the program's internal/hardcoded defaults. */
	for (LayerType layer_type = LayerType::Aggregate; layer_type < LayerType::Max; ++layer_type) {
		qDebug() << "II" PREFIX << "Loading default values from hardcoded values for layer type" << layer_type;
		LayerDefaults::fill_missing_from_hardcoded_defaults(layer_type);
	}

	return true;
}




/**
   \brief Save "layer defaults" configuration to a settings file

   \return true
*/
bool LayerDefaults::save_to_file(void)
{
	g_keyfile->sync();
	return true;
}




/**
   This displays a dialog window showing the default parameter values for the selected layer.
   It allows the parameters to be changed.

   \return true if "OK" key has been pressed in the dialog window,
   \return false otherwise
*/
bool LayerDefaults::show_window(LayerType layer_type, QWidget * parent)
{
	if (!g_loaded) {
		/* Since we can't load the file in a_defaults_init (no params registered yet),
		   do it once before we display the params. */
		LayerDefaults::load_from_file();
		g_loaded = true;
	}

	LayerInterface * interface = Layer::get_interface(layer_type);

	PropertiesDialog dialog(interface->ui_labels.layer_defaults, parent);
	dialog.fill(interface);
	int dialog_code = dialog.exec();

	if (dialog_code == QDialog::Accepted) {

		std::map<param_id_t, SGVariant> * values = &interface->parameter_default_values;

		for (auto iter = interface->parameter_specifications.begin(); iter != interface->parameter_specifications.end(); iter++) {
			const SGVariant param_value = dialog.get_param_value(iter->first, *(iter->second));
			values->at(iter->first) = param_value;
			save_parameter_value(param_value, layer_type, iter->second->name, iter->second->type_id);
		}

		LayerDefaults::save_to_file();

		return true;
	} else {
		return false;
	}
}




/**
   @layer_type
   @layer_param_spec
   @default_value

   Call this function to set the default value for the particular parameter.
*/
void LayerDefaults::set(LayerType layer_type, const ParameterSpecification & layer_param_spec, const SGVariant & default_value)
{
	save_parameter_value(default_value, layer_type, layer_param_spec.name, layer_param_spec.type_id);
}




/**
   @brief Initialize LayerDefaults module

   Call this function at startup.
*/
void LayerDefaults::init(void)
{
	/* kamilFIXME: improve this section. Make sure that the file exists. */
	const QString full_path = SlavGPSLocations::get_file_full_path(VIKING_LAYER_DEFAULTS_INI_FILE);
	g_keyfile = new QSettings(full_path, QSettings::IniFormat);

	qDebug() << "II" PREFIX << "key file initialized with path" << full_path;

	g_loaded = false;
}




/**
   @brief Deinitialize LayerDefaults module

   Call this function on program exit.
*/
void LayerDefaults::uninit(void)
{
	delete g_keyfile;
}




/**
   @layer_type
   @param_name
   @param_type

   Call this function to get the default value for the parameter requested.
*/
SGVariant LayerDefaults::get(LayerType layer_type, const char * param_name, SGVariantType param_type)
{
	if (!g_loaded) {
		/*
		  Since we can't load the file in
		  LayerDefaults::init() (no params registered yet), do
		  it once before we get the first key.

		  We ignore errors that may have occurred during
		  loading, we can't do anything about it. We will have
		  to rely on layer's hardcoded defaults.
		*/
		LayerDefaults::load_from_file();
		g_loaded = true;
	}

	return LayerDefaults::get_parameter_value(layer_type, param_name, param_type);
}




/**
   \brief Save current layer defaults to settings file

   \return: true if saving was successful
*/
bool LayerDefaults::save(void)
{
	/*
	  Default values of layer parameters may be edited only through
	  dialog window in menu Edit -> Layer Defaults -> <layer type>.

	  After every instance of editing the values in the dialog, the
	  modified values are saved into layer's Interface and into keyfile.

	  So all we have to do now is to call this function that will
	  simply sync keyfile to disc file.
	*/

	return LayerDefaults::save_to_file();
}
