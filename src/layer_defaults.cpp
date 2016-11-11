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

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <QSettings>
#include <QDebug>

#include "uibuilder_qt.h"
#include "layer_defaults.h"
#include "dir.h"
#include "file.h"
#include "globals.h"




using namespace SlavGPS;




static LayerParamValue read_parameter_value(const char * group, const char * name, LayerParamType ptype, bool * success);
static LayerParamValue read_parameter_value(const char * group, const char * name, LayerParamType ptype);
static void write_parameter_value(LayerParamValue value, const char * group, const char * name, LayerParamType ptype);

static void defaults_run_setparam(void * index_ptr, param_id_t id, LayerParamValue value, Parameter * params);
static LayerParamValue defaults_run_getparam(void * index_ptr, param_id_t id, bool notused2);
static void use_internal_defaults_if_missing_default(LayerType layer_type);

static bool layer_defaults_load_from_file(void);
static bool layer_defaults_save_to_file(void);




#define VIKING_LAYER_DEFAULTS_INI_FILE "viking_layer_defaults.ini"




/* A list of the parameter types in use. */
static GPtrArray * paramsVD;

static QSettings * keyfile = NULL;
static bool loaded;



/* "read" is supposed to indicate that this is a low-level function,
   reading directly from file, even though the reading is made from QT
   abstraction of settings file. */
static LayerParamValue read_parameter_value(const char * group, const char * name, LayerParamType ptype, bool * success)
{
	LayerParamValue value = VIK_LPD_BOOLEAN (false);

	QString key(QString(group) + QString("/") + QString(name));
	QVariant variant = keyfile->value(key);

	if (!variant.isValid()) {
		qDebug() << "EE: Layer Defaults: failed to read key" << key;
		*success = false;
		return value;
	}
	*success = true;

	switch (ptype) {
	case LayerParamType::DOUBLE: {
		value.d = variant.toDouble();
		break;
	}
	case LayerParamType::UINT: {
		value.u = (uint32_t) variant.toULongLong();
		break;
	}
	case LayerParamType::INT: {
		value.i = (int32_t) variant.toLongLong();
		break;
	}
	case LayerParamType::BOOLEAN: {
		value.b = variant.toBool();
		break;
	}
	case LayerParamType::STRING: {
		value.s = strdup(variant.toString().toUtf8().constData());
		qDebug() << "II: Layer Defaults: read string" << value.s;
		break;
	}
#if 0
	case LayerParamType::STRING_LIST: {
		char **str = g_key_file_get_string_list(keyfile, group, name, &error);
		value.sl = str_to_glist(str); /* TODO convert. */
		break;
	}
#endif
	case LayerParamType::COLOR: {
		QColor color = variant.value<QColor>();
		value.c.r = color.red();
		value.c.g = color.green();
		value.c.b = color.blue();
		value.c.a = color.alpha();
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
static LayerParamValue read_parameter_value(const char * group, const char * name, LayerParamType ptype)
{
	bool success = true;
	/* This should always succeed - don't worry about 'success'. */
	return read_parameter_value(group, name, ptype, &success);
}




/* "write" is supposed to indicate that this is a low-level function,
   writing directly to file, even though the writing is made to QT
   abstraction of settings file. */
static void write_parameter_value(LayerParamValue value, const char * group, const char * name, LayerParamType ptype)
{
	QVariant variant;

	switch (ptype) {
	case LayerParamType::DOUBLE:
		variant = QVariant((double) value.d);
		break;
	case LayerParamType::UINT:
		variant = QVariant((qulonglong) value.u);
		break;
	case LayerParamType::INT:
		variant = QVariant((qlonglong) value.i);
		break;
	case LayerParamType::BOOLEAN:
		variant = QVariant((bool) value.b);
		break;
	case LayerParamType::STRING:
		variant = QString(value.s);
		break;
	case LayerParamType::COLOR: {
		variant = QColor(value.c.r, value.c.g, value.c.b, value.c.a);
		break;
	}
	default:
		qDebug() << "EE: Layer Defaults: unhandled parameter type" << (int) ptype;
		return;
	}

	QString key(QString(group) + QString("/") + QString(name));
	keyfile->setValue(key, variant);
}




static void defaults_run_setparam(void * index_ptr, param_id_t id, LayerParamValue value, Parameter * params)
{
	/* Index is only an index into values from this layer. */
	int index = KPOINTER_TO_INT (index_ptr);
	Parameter * layer_param = (Parameter *) g_ptr_array_index(paramsVD, index + id);

	write_parameter_value(value, Layer::get_interface(layer_param->layer_type)->fixed_layer_name, layer_param->name, layer_param->type);
}




static LayerParamValue defaults_run_getparam(void * index_ptr, param_id_t id, bool notused2)
{
	/* Index is only an index into values from this layer. */
	int index = (int) (long) (index_ptr);
	Parameter * layer_param = (Parameter *) g_ptr_array_index(paramsVD, index + id);

	return read_parameter_value(Layer::get_interface(layer_param->layer_type)->fixed_layer_name, layer_param->name, layer_param->type);
}




static void use_internal_defaults_if_missing_default(LayerType layer_type)
{
	Parameter * params = Layer::get_interface(layer_type)->params;
	if (!params) {
		return;
	}

	uint16_t params_count = Layer::get_interface(layer_type)->params_count;
	/* Process each parameter. */
	for (uint16_t i = 0; i < params_count; i++) {
		if (params[i].group == VIK_LAYER_NOT_IN_PROPERTIES) {
			continue;
		}

		bool success = false;
		/* Check if a value is stored in settings file. If not, get program's internal, hardwired value. */
		read_parameter_value(Layer::get_interface(layer_type)->fixed_layer_name, params[i].name, params[i].type, &success);
		if (!success) {
			if (params[i].hardwired_default_value) {
				LayerParamValue value = params[i].hardwired_default_value();
				write_parameter_value(value, Layer::get_interface(layer_type)->fixed_layer_name, params[i].name, params[i].type);
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


	/* Ensure if have a key file, then any missing values are set from the internal defaults. */
	for (LayerType layer_type = LayerType::AGGREGATE; layer_type < LayerType::NUM_TYPES; ++layer_type) {
		use_internal_defaults_if_missing_default(layer_type);
	}

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
bool layer_defaults_show_window(LayerType layer_type, QWidget * parent)
{
	if (!loaded) {
		/* Since we can't load the file in a_defaults_init (no params registered yet),
		   do it once before we display the params. */
		layer_defaults_load_from_file();
		loaded = true;
	}

	char const * layer_name = Layer::get_interface(layer_type)->name;

	/*
	   Need to know where the params start and they finish for this layer

	   1. inspect every registered param - see if it has the layer value required to determine overall size
	      [they are contiguous from the start index]
	   2. copy the these params from the main list into a tmp struct

	   Then pass this tmp struct to uibuilder for display
	*/

	unsigned int layer_params_count = 0;

	bool found_first = false;
	int index = 0;
	for (unsigned int i = 0; i < paramsVD->len; i++) {
		Parameter * param = (Parameter *) g_ptr_array_index(paramsVD, i);
		if (param->layer_type == layer_type) {
			layer_params_count++;
			if (!found_first) {
				index = i;
				found_first = true;
			}
		}
	}

	/* Have we any parameters to show! */
	if (!layer_params_count) {
		return false;
	}

	Parameter * params = (Parameter *) malloc(layer_params_count * sizeof (Parameter));
	for (unsigned int i = 0; i < layer_params_count; i++) {
		params[i] = *((Parameter *) (g_ptr_array_index(paramsVD,i+index)));
	}

#ifdef K
	if (a_uibuilder_properties_factory(/* title */,
					   /* parent */,
					   params,
					   layer_params_count,
					   Layer::get_interface(layer_type)->params_groups,
					   Layer::get_interface(layer_type)->params_groups_count,
					   (bool (*) (void *,uint16_t,LayerParamValue,void *,bool)) defaults_run_setparam,
					   ((void *) (long) (index)),
					   params,
					   defaults_run_getparam,
					   ((void *) (long) (index)),
					   NULL)) {

	}
#endif

	PropertiesDialog dialog(QString("%1: Layer Defaults").arg(QString(layer_name)), parent);
#ifdef K
	dialog.fill(this);
#endif
	int dialog_code = dialog.exec();

	if (dialog_code == QDialog::Accepted) {
#ifdef K
		std::map<layer_param_id_t, Parameter *> * parameters = this->get_interface()->layer_parameters;
		for (auto iter = parameters->begin(); iter != parameters->end(); iter++) {
			LayerParamValue param_value = dialog.get_param_value(iter->first, iter->second);
			bool set = this->set_param_value(iter->first, param_value, viewport, false);
			if (set) {
				must_redraw = true;
			}
		}

		/* Save. */
		layer_defaults_save_to_file();
#endif
		free(params);
		return true;
	} else {
		free(params);
		return false;
	}
}




/**
 * @layer_param:     The parameter
 * @default_value:   The default value
 * @layer_name:      The layer in which the parameter resides
 *
 * Call this function on to set the default value for the particular parameter.
 */
void a_layer_defaults_register(const char * layer_name, Parameter * layer_param, LayerParamValue default_value)
{
	/* Copy value. */
	Parameter * new_layer_param = (Parameter *) malloc(1 * sizeof (Parameter));
	*new_layer_param = *layer_param;

	g_ptr_array_add(paramsVD, new_layer_param);

	write_parameter_value(default_value, layer_name, layer_param->name, layer_param->type);
}




/**
 * Call this function at startup.
 */
void a_layer_defaults_init()
{
	/* kamilFIXME: improve this section. Make sure that the file exists. */
	QString path(QString(get_viking_dir()) + "/" + VIKING_LAYER_DEFAULTS_INI_FILE);
	keyfile = new QSettings(path, QSettings::IniFormat);

	qDebug() << "II: Layer Defaults: key file initialized as" << keyfile << "with path" << path;

	/* Not copied. */
	paramsVD = g_ptr_array_new();

	loaded = false;
}




/**
 * Call this function on program exit.
 */
void a_layer_defaults_uninit()
{
	delete keyfile;

	g_ptr_array_foreach(paramsVD, (GFunc)g_free, NULL);
	g_ptr_array_free(paramsVD, true);
}




/**
 * @layer_name:  String name of the layer
 * @param_name:  String name of the parameter
 * @param_type:  The parameter type
 *
 * Call this function to get the default value for the parameter requested.
 */
LayerParamValue a_layer_defaults_get(const char * layer_name, const char * param_name, LayerParamType param_type)
{
	if (!loaded) {
		/* Since we can't load the file in a_defaults_init (no params registered yet),
		   do it once before we get the first key. */
		layer_defaults_load_from_file();
		loaded = true;
	}

	return read_parameter_value(layer_name, param_name, param_type);
}




/**
 * Call this function to save the current layer defaults.
 * Normally should only be performed if any layer defaults have been changed via direct manipulation of the layer
 * rather than the user changing the preferences via the dialog window above.
 *
 * This must only be performed once all layer parameters have been initialized.
 *
 * Returns: %true if saving was successful
 */
bool a_layer_defaults_save()
{
	/* Generate defaults. */
	for (LayerType layer_type = LayerType::AGGREGATE; layer_type < LayerType::NUM_TYPES; ++layer_type) {
		use_internal_defaults_if_missing_default(layer_type);
	}

	return layer_defaults_save_to_file();
}
