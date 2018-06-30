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




#include <vector>




#include <QSettings>
#include <QDebug>




#include "ui_builder.h"
#include "layer.h"
#include "layer_defaults.h"
#include "layer_interface.h"
#include "dir.h"




using namespace SlavGPS;




#define PREFIX ": Layer Defaults:" << __FUNCTION__ << __LINE__ << ">>"
#define VIKING_LAYER_DEFAULTS_INI_FILE "viking_layer_defaults.ini"




/* A list of the parameter types in use. */
static std::vector<ParameterSpecification *> default_parameters_specifications;
static QSettings * keyfile = NULL;
static bool loaded;




SGVariant LayerDefaults::get_parameter_value(LayerType layer_type, const char * name, SGVariantType type_id, bool * success)
{
	SGVariant value;
	QString group = Layer::get_type_id_string(layer_type);

	QString key(group + QString("/") + QString(name));
	QVariant variant = keyfile->value(key);

	if (!variant.isValid()) {
		qDebug() << "EE" PREFIX << "failed to read key" << key;
		*success = false;
		return value;
	}
	*success = true;

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
		*success = false;
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
	keyfile->setValue(key, variant);
}




void LayerDefaults::fill_from_hardwired_defaults(LayerType layer_type)
{
	LayerInterface * interface = Layer::get_interface(layer_type);

	/* Process each parameter. */
	for (auto iter = interface->parameter_specifications.begin(); iter != interface->parameter_specifications.end(); iter++) {
		const ParameterSpecification * param_spec = iter->second;
		if (param_spec->group_id == PARAMETER_GROUP_HIDDEN) {
			continue;
		}

		/* Now we are dealing with a concrete, layer-specific
		   parameter.

		   See if its value has been read from configuration
		   file.  If not, try to get the value hardwired in
		   application and add it to the configuration file so
		   that the file has full, consistent set of
		   values. */

		bool success = false;
		LayerDefaults::get_parameter_value(layer_type, param_spec->name, param_spec->type_id, &success);
		if (!success) {
			/* Value of this parameter has not been read
			   from config file. Try to find it in
			   program's hardwired values. */
			qDebug() << "II" PREFIX << "Getting hardwired value of parameter" << layer_type << param_spec->name;
			SGVariant value;
			if (param_spec->get_hardwired_value(value)) {
				save_parameter_value(value, layer_type, param_spec->name, param_spec->type_id);
			}
		}
	}
}




/**
   \brief Load "layer defaults" configuration from a settings file

   kamilFIXME: return value is not checked.
*/
bool LayerDefaults::load_from_file(void)
{
	//GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS;

	if (!keyfile) {
		qDebug() << "EE" PREFIX << "key file is not initialized";
		exit(EXIT_FAILURE);
	}

	enum QSettings::Status status = keyfile->status();
	if (status != QSettings::NoError) {
		qDebug() << "EE" PREFIX << "key file status is" << status;
		return false;
	}

	/* Set any missing values from the program's internal/hardwired defaults. */
	for (LayerType layer_type = LayerType::Aggregate; layer_type < LayerType::Max; ++layer_type) {
		LayerDefaults::fill_from_hardwired_defaults(layer_type);
	}

	return true;
}




/**
   \brief Save "layer defaults" configuration to a settings file

   \return true
*/
bool LayerDefaults::save_to_file(void)
{
	keyfile->sync();
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
	if (!loaded) {
		/* Since we can't load the file in a_defaults_init (no params registered yet),
		   do it once before we display the params. */
		LayerDefaults::load_from_file();
		loaded = true;
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
	/* Copy value. */
	ParameterSpecification * new_spec = new ParameterSpecification;
	*new_spec = layer_param_spec;
	default_parameters_specifications.push_back(new_spec);

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
	keyfile = new QSettings(full_path, QSettings::IniFormat);

	qDebug() << "II" PREFIX << "key file initialized as" << keyfile << "with path" << full_path;

	loaded = false;
}




/**
   @brief Deinitialize LayerDefaults module

   Call this function on program exit.
*/
void LayerDefaults::uninit(void)
{
	delete keyfile;

	for (auto iter = default_parameters_specifications.begin(); iter != default_parameters_specifications.end(); iter++) {
		delete *iter;
	}
	default_parameters_specifications.clear();
}




/**
   @layer_type
   @param_name
   @param_type

   Call this function to get the default value for the parameter requested.
*/
SGVariant LayerDefaults::get(LayerType layer_type, const char * param_name, SGVariantType param_type)
{
	if (!loaded) {
		/* Since we can't load the file in
		   LayerDefaults::init() (no params registered yet),
		   do it once before we get the first key. */
		LayerDefaults::load_from_file();
		loaded = true;
	}

	bool dummy;
	return LayerDefaults::get_parameter_value(layer_type, param_name, param_type, &dummy);
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




#ifdef K_OLD_IMPLEMENTATION




static void defaults_run_setparam(void * index_ptr, param_id_t id, const SGVariant & value, ParameterSpecification * param_spec)
{
	/* Index is only an index into values from this layer. */
	int index = (int) (long) (index_ptr);
	ParameterSpecification * layer_param_spec = default_parameters_specifications.at(index + id);

	save_parameter_value(value, layer_param_spec->layer_type, layer_param_spec->name, layer_param_spec->type);
}




static SGVariant defaults_run_getparam(void * index_ptr, param_id_t id, bool notused2)
{
	/* Index is only an index into values from this layer. */
	int index = (int) (long) (index_ptr);
	ParameterSpecification * layer_param_spec = default_parameters_specifications.at(index + id);

	bool dummy;
	return LayerDefaults::get_parameter_value(layer_param_spec->layer_type, layer_param_spec->name, layer_param_spec->type, &dummy);
}




#endif
