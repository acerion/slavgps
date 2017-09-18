/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
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
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <vector>

//#include <glib/gstdio.h>

#include <QSettings>
#include <QDebug>

#include "ui_builder.h"
#include "layer_defaults.h"
#include "layer_interface.h"
#include "dir.h"
#include "file.h"
#include "globals.h"




using namespace SlavGPS;




static SGVariant read_parameter_value(LayerType layer_type, const char * name, SGVariantType ptype, bool * success);
static SGVariant read_parameter_value(LayerType layer_type, const char * name, SGVariantType ptype);
static void write_parameter_value(const SGVariant & value, LayerType layer_type, const char * name, SGVariantType ptype);

#if 0
static void defaults_run_setparam(void * index_ptr, param_id_t id, const SGVariant & value, Parameter * params);
static SGVariant defaults_run_getparam(void * index_ptr, param_id_t id, bool notused2);
static void use_internal_defaults_if_missing_default(LayerType layer_type);
#endif

static bool layer_defaults_load_from_file(void);
static bool layer_defaults_save_to_file(void);




#define VIKING_LAYER_DEFAULTS_INI_FILE "viking_layer_defaults.ini"


/* A list of the parameter types in use. */
std::vector<Parameter *> default_parameters;

static QSettings * keyfile = NULL;
static bool loaded;



/* "read" is supposed to indicate that this is a low-level function,
   reading directly from file, even though the reading is made from QT
   abstraction of settings file. */
static SGVariant read_parameter_value(LayerType layer_type, const char * name, SGVariantType ptype, bool * success)
{
	SGVariant value;
	QString group = Layer::get_type_string(layer_type);

	QString key(group + QString("/") + QString(name));
	QVariant variant = keyfile->value(key);

	if (!variant.isValid()) {
		qDebug() << "EE: Layer Defaults: failed to read key" << key;
		*success = false;
		return value;
	}
	*success = true;

	switch (ptype) {
	case SGVariantType::DOUBLE: {
		value = SGVariant((double) variant.toDouble());
		break;
	}
	case SGVariantType::UINT: {
		value = SGVariant((uint32_t) variant.toULongLong());
		break;
	}
	case SGVariantType::INT: {
		value = SGVariant((int32_t) variant.toLongLong());
		break;
	}
	case SGVariantType::BOOLEAN: {
		value = SGVariant((bool) variant.toBool());
		break;
	}
	case SGVariantType::STRING: {
		value = SGVariant(variant.toString());
		qDebug() << "II: Layer Defaults: read string" << value.s;
		break;
	}
#ifdef K
	case SGVariantType::STRING_LIST: {
		char **str = g_key_file_get_string_list(keyfile, group, name, &error);
		value = SGVariant(str_to_glist(str)); /* TODO convert. */
		break;
	}
#endif
	case SGVariantType::COLOR: {
		value = SGVariant(variant.value<QColor>());
		break;
	}
	default:
		qDebug() << "EE: Layer Defaults: unhandled value type" << (int) ptype;
		*success = false;
		break;
	}

	return value;

}




/* "read" is supposed to indicate that this is a low-level function,
   reading directly from file, even though the reading is made from QT
   abstraction of settings file. */
static SGVariant read_parameter_value(LayerType layer_type, const char * name, SGVariantType ptype)
{
	bool success = true;
	/* This should always succeed - don't worry about 'success'. */
	return read_parameter_value(layer_type, name, ptype, &success);
}




/* "write" is supposed to indicate that this is a low-level function,
   writing directly to file, even though the writing is made to QT
   abstraction of settings file. */
static void write_parameter_value(const SGVariant & value, LayerType layer_type, const char * name, SGVariantType ptype)
{
	QVariant variant;

	switch (ptype) {
	case SGVariantType::DOUBLE:
		variant = QVariant((double) value.d);
		break;
	case SGVariantType::UINT:
		variant = QVariant((qulonglong) value.u);
		break;
	case SGVariantType::INT:
		variant = QVariant((qlonglong) value.i);
		break;
	case SGVariantType::BOOLEAN:
		variant = QVariant((bool) value.b);
		break;
	case SGVariantType::STRING:
		variant = value.s;
		break;
	case SGVariantType::COLOR: {
		variant = QColor(value.c.r, value.c.g, value.c.b, value.c.a);
		break;
	}
	default:
		qDebug() << "EE: Layer Defaults: unhandled parameter type" << (int) ptype;
		return;
	}

	const QString group = Layer::get_type_string(layer_type);
	QString key(group + QString("/") + QString(name));
	keyfile->setValue(key, variant);
}



#if 0


static void defaults_run_setparam(void * index_ptr, param_id_t id, const SGVariant & value, Parameter * params)
{
	/* Index is only an index into values from this layer. */
	int index = KPOINTER_TO_INT (index_ptr);
	Parameter * layer_param = default_parameters.at(index + id);

	write_parameter_value(value, layer_param->layer_type, layer_param->name, layer_param->type);
}




static SGVariant defaults_run_getparam(void * index_ptr, param_id_t id, bool notused2)
{
	/* Index is only an index into values from this layer. */
	int index = (int) (long) (index_ptr);
	Parameter * layer_param = default_parameters.at(index + id);

	return read_parameter_value(layer_param->layer_type, layer_param->name, layer_param->type);
}




#endif




static void use_internal_defaults_if_missing_default(LayerType layer_type)
{
	LayerInterface * interface = Layer::get_interface(layer_type);

	/* Process each parameter. */
	for (auto iter = interface->parameters.begin(); iter != interface->parameters.end(); iter++) {
		const Parameter * param = iter->second;
		if (param->group_id == PARAMETER_GROUP_HIDDEN) {
			continue;
		}

		bool success = false;
		/* Check if a value is stored in settings file. If not, get program's internal, hardwired value. */
		read_parameter_value(layer_type, param->name, param->type, &success);
		if (!success) {
			SGVariant value;
			if (parameter_get_hardwired_value(value, *param)) {
				write_parameter_value(value, layer_type, param->name, param->type);
			}
		}
	}
}




/**
   \brief Load "layer defaults" configuration from a settings file

   kamilFIXME: return value is not checked.
*/
static bool layer_defaults_load_from_file(void)
{
	//GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS;

	if (!keyfile) {
		qDebug() << "EE: Layer Defaults: key file is not initialized";
		exit(EXIT_FAILURE);
	}

	enum QSettings::Status status = keyfile->status();
	if (status != QSettings::NoError) {
		qDebug() << "EE: Layer Defaults: key file status is" << status;
		return false;
	}

#if 0
	/* Ensure if have a key file, then any missing values are set from the internal defaults. */
	for (LayerType layer_type = LayerType::AGGREGATE; layer_type < LayerType::NUM_TYPES; ++layer_type) {
		use_internal_defaults_if_missing_default(layer_type);
	}
#endif

	return true;
}




/**
   \brief Save "layer defaults" configuration to a settings file

   \return true
*/
static bool layer_defaults_save_to_file(void)
{
	keyfile->sync();
	return true;
}




/**
 * @parent:      The Window
 * @layer_name:  The layer
 *
 * This displays a Window showing the default parameter values for the selected layer.
 * It allows the parameters to be changed.
 *
 * Returns: %true if the window is displayed (because there are parameters to view).
 */
bool LayerDefaults::show_window(LayerType layer_type, QWidget * parent)
{
	if (!loaded) {
		/* Since we can't load the file in a_defaults_init (no params registered yet),
		   do it once before we display the params. */
		layer_defaults_load_from_file();
		loaded = true;
	}

	LayerInterface * interface = Layer::get_interface(layer_type);

	PropertiesDialog dialog(interface->ui_labels.layer_defaults, parent);
	dialog.fill(interface);
	int dialog_code = dialog.exec();

	if (dialog_code == QDialog::Accepted) {

		std::map<param_id_t, SGVariant> * values = &interface->parameter_default_values;

		for (auto iter = interface->parameters.begin(); iter != interface->parameters.end(); iter++) {
			const SGVariant param_value = dialog.get_param_value(iter->first, iter->second);
			values->at(iter->first) = param_value;
			write_parameter_value(param_value, layer_type, iter->second->name, iter->second->type);
		}

		layer_defaults_save_to_file();

		return true;
	} else {
		return false;
	}
}




/**
   @layer_param:     The parameter
   @default_value:   The default value
   @layer_type:      Type of layer in which the parameter resides

   Call this function on to set the default value for the particular parameter.
*/
void LayerDefaults::set(LayerType layer_type, Parameter * layer_param, const SGVariant & default_value)
{
	/* Copy value. */
	Parameter * new_param = new Parameter;
	*new_param = *layer_param;
	default_parameters.push_back(new_param);

	write_parameter_value(default_value, layer_type, layer_param->name, layer_param->type);
}




/**
 * Call this function at startup.
 */
void LayerDefaults::init(void)
{
	/* kamilFIXME: improve this section. Make sure that the file exists. */
	const QString path = get_viking_dir() + QDir::separator() + VIKING_LAYER_DEFAULTS_INI_FILE;
	keyfile = new QSettings(path, QSettings::IniFormat);

	qDebug() << "II: Layer Defaults: key file initialized as" << keyfile << "with path" << path;

	loaded = false;
}




/**
 * Call this function on program exit.
 */
void LayerDefaults::uninit(void)
{
	delete keyfile;
	default_parameters.clear();
}




/**
 * @layer_name:  String name of the layer
 * @param_name:  String name of the parameter
 * @param_type:  The parameter type
 *
 * Call this function to get the default value for the parameter requested.
 */
SGVariant LayerDefaults::get(LayerType layer_type, const char * param_name, SGVariantType param_type)
{
	if (!loaded) {
		/* Since we can't load the file in a_defaults_init (no params registered yet),
		   do it once before we get the first key. */
		layer_defaults_load_from_file();
		loaded = true;
	}

	return read_parameter_value(layer_type, param_name, param_type);
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

	return layer_defaults_save_to_file();
}
