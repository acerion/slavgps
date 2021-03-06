/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2012-2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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
#include <cstdio>
#include <cassert>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QDebug>
#include <QHash>
#include <QDir>




#include "viewport_internal.h"
#include "layer_aggregate.h"
#include "layer_trw.h"
#include "layer_gps.h"
#include "jpg.h"
#include "gpspoint.h"
#include "gpsmapper.h"
#include "geojson.h"
#include "misc/strtod.h"
#include "file.h"
#include "gpx.h"
#include "file_utils.h"
#include "preferences.h"
#include "babel.h"
#include "tree_view_internal.h"
#include "layer_trw_track_internal.h"
#include "util.h"
#include "vikutils.h"
#include "preferences.h"




#ifdef WINDOWS
#define realpath(X,Y) _fullpath(Y,X,MAX_PATH)
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif




using namespace SlavGPS;




#define SG_MODULE "File"




#define VIKING_FILE_VERSION 1




template <class T>
class LayerStack {
public:
	void push(T data);
	void pop(void);
	bool empty(void) { return this->layers.empty(); };

	std::list<T> layers;

	T first{};  /* C++ Value Initialization. First layer (or list of layers) on top of stack. */
	T second{}; /* C++ Value Initialization. Second layer (or list of layers), counting from top (second after LayerStack::first). */
};




template <class T>
void LayerStack<T>::push(T data)
{
	this->layers.push_front(data);

	this->first = data;

	if (this->layers.size() > 1) {
		this->second = *std::next(this->layers.begin());
	} else {
		this->second = T(); /* Empty value. */
	}
}




class ReadParser {
public:
	ReadParser(QFile * new_file) : file(new_file) {};

	void handle_layer_begin(const char * line, GisViewport * gisview);
	void handle_layer_end(GisViewport * gisview);
	void handle_layer_data_begin(const QString & dirpath);

	/**
	   @brief Handle a single layer parameter contained in given @param line
	*/
	void handle_layer_parameter(const char * line, size_t line_len);

	/**
	   @brief Save layer's "string list" parameter(s) to the layer

	   During the reading of layer's parameters, we have been
	   treating string lists in a special way: we didn't save them
	   immediately to the layer, but have been accumulating the
	   members of "string list" parameter(s). This function saves
	   all such lists (there can be more than one per layer) into
	   the layer.
	*/
	void push_string_lists_to_layer(Layer * layer);

	sg_ret read_header(Layer * top_layer, GisViewport * gisview, LatLon & lat_lon);

	LayerStack<Layer *> layers_stack;
	ParameterSpecification * param_specs = nullptr; /* For current layer, so we don't have to keep on looking up interface. */
	uint8_t param_specs_count = 0;

	size_t line_num = 0;

	/*
	  Parameter id -> value of parameter (value of parameter is of type QStringList).

	  For now only DEM Layer has a parameter of type StringList,
	  and it's only one such parameter (list of SRTM files). But
	  let's be prepared to support layer types that have multiple
	  parameters of type StringList, and let's use hash map for
	  this.
	*/
	QHash<param_id_t, QStringList> string_lists;

	LoadStatus parse_status = LoadStatus::Code::Success;

	char buffer[4096];
	const size_t buffer_size = sizeof (ReadParser::buffer);

	QFile * file = NULL;
};




template <class T>
void LayerStack<T>::pop(void)
{
	this->layers.pop_front();

	this->first = T();  /* Empty value. */
	this->second = T(); /* Empty value. */

	if (this->layers.size() > 0) {
		this->first = *this->layers.begin();

		if (this->layers.size() > 1) {
			this->second = *std::next(this->layers.begin());
		}
	}
}




static bool str_starts_with(char const * haystack, char const * needle, uint16_t len_needle, bool must_be_longer)
{
	if (strlen(haystack) > len_needle - (!must_be_longer) && strncasecmp(haystack, needle, len_needle) == 0) {
		return true;
	}
	return false;
}




static void write_layer_params_and_data(FILE * file, const Layer * layer)
{
	fprintf(file, "name=%s\n", layer->get_name().isEmpty() ? "" : layer->get_name().toUtf8().constData());
	if (!layer->is_visible()) {
		fprintf(file, "visible=f\n");
	}

	for (auto iter = layer->get_interface().parameter_specifications.begin(); iter != layer->get_interface().parameter_specifications.end(); iter++) {

		/* Get current, per-layer-instance value of parameter. Refer to the parameter by its id ((*iter)->first). */
		const SGVariant param_value = layer->get_param_value(iter->first, true);
		if (iter->second->type_id != param_value.type_id) {
			qDebug() << SG_PREFIX_E << "type id mismatch for parameter named" << iter->second->name << ":"
				 << iter->second->type_id << "!=" << param_value.type_id;
			assert (iter->second->type_id == param_value.type_id);
		}
		param_value.write(file, iter->second->name);
	}

	layer->write_layer_data(file);
}




/**
   Discard initial empty characters in the @line.
   Discard line starting with '#' (comment character).

   Move beginning of @line accordingly, to point to beginning of
   non-empty data.

   Return length of non-empty, non-comment line.
   Return zero if the processed line turns out to be empty.
*/
size_t consume_empty_in_line(char ** line)
{
	while ((**line) == ' ' || (**line) == '\t') {
		(*line)++;
	}

	/* Comment. */
	if ((*line)[0] == '#') {
		return 0;
	}


	size_t len = strlen(*line);
	if (len > 0 && (*line)[len - 1] == '\n') {
		(*line)[--len] = '\0';
	}

	if (len > 0 && (*line)[len - 1] == '\r') {
		(*line)[--len] = '\0';
	}

	return len;
}




void file_write_header(FILE * file, const LayerAggregate * top_level_layer, GisViewport * gisview)
{
	const LatLon lat_lon = gisview->get_center_coord().get_lat_lon();

	const QString mode_id_string = GisViewportDrawModes::get_id_string(gisview->get_draw_mode());

	char bg_color_string[8] = { 0 };
	SGUtils::color_to_string(bg_color_string, sizeof (bg_color_string), gisview->get_background_color());

	char hl_color_string[8] = { 0 };
	SGUtils::color_to_string(hl_color_string, sizeof (hl_color_string), gisview->get_highlight_color());

	fprintf(file, "#VIKING GPS Data file " PACKAGE_URL "\n");
	fprintf(file, "FILE_VERSION=%d\n", VIKING_FILE_VERSION);
	fprintf(file, "\nxmpp=%f\n", gisview->get_viking_scale().get_x());
	fprintf(file, "ympp=%f\n", gisview->get_viking_scale().get_y());
	fprintf(file, "lat=%f\n", lat_lon.lat);
	fprintf(file, "lon=%f\n", lat_lon.lon);
	fprintf(file, "mode=%s\n", mode_id_string.toUtf8().constData());
	fprintf(file, "color=%s\n", bg_color_string);
	fprintf(file, "highlightcolor=%s\n", hl_color_string);
	fprintf(file, "drawscale=%s\n", gisview->get_scale_visibility() ? "t" : "f");
	fprintf(file, "drawcentermark=%s\n", gisview->get_center_mark_visibility() ? "t" : "f");
	fprintf(file, "drawhighlight=%s\n", gisview->get_highlight_usage() ? "t" : "f");

	if (!top_level_layer->is_visible()) {
		fprintf(file, "visible=f\n");
	}

	return;
}




sg_ret ReadParser::read_header(Layer * top_layer, GisViewport * gisview, LatLon & lat_lon)
{
	sg_ret read_status = sg_ret::ok;

	while (0 < this->file->readLine(this->buffer, this->buffer_size))  {

		if (this->buffer[0] == '~') {
			/* Beginning of first layer. Stop reading
			   header, return the buffer through function
			   argument. */
			return sg_ret::ok;
		}

		this->line_num++;

		char * line = this->buffer;

		const size_t line_len = consume_empty_in_line(&line);
		if (line_len == 0) {
			continue;
		}

		size_t name_len = 0; /* Length of parameter's name. */
		const char * value_start = nullptr;
		for (size_t i = 0; i < line_len; i++) {
			if (line[i] == '=') {
				name_len = i;
				value_start = line + name_len + 1;
			}
		}


		if (name_len == 12 && strncasecmp(line, "FILE_VERSION", name_len) == 0) {
			const int version = strtol(value_start, nullptr, 10);
			if (version <= VIKING_FILE_VERSION) {
				qDebug() << SG_PREFIX_D << "File version" << version;
			} else {
				this->parse_status = LoadStatus::Code::ParseError;
				this->parse_status.parser_line = this->line_num;
				this->parse_status.parser_message = QString("Invalid file version %1.").arg(version);
				qDebug() << QObject::tr("Error in line %1: %2")
					.arg(this->parse_status.parser_line)
					.arg(this->parse_status.parser_message);

				read_status = sg_ret::err;
			}
			/* However we'll still carry and attempt to read whatever we can. */
		} else if (name_len == 4 && strncasecmp(line, "xmpp", name_len) == 0) { /* "hard coded" params: global & for all layer-types */
			gisview->set_viking_scale_x(strtod_i8n(value_start, nullptr));

		} else if (name_len == 4 && strncasecmp(line, "ympp", name_len) == 0) {
			gisview->set_viking_scale_y(strtod_i8n(value_start, nullptr));

		} else if (name_len == 3 && strncasecmp(line, "lat", name_len) == 0) {
			lat_lon.lat = strtod_i8n(value_start, nullptr);

		} else if (name_len == 3 && strncasecmp(line, "lon", name_len) == 0) {
			lat_lon.lon = strtod_i8n(value_start, nullptr);

		} else if (name_len == 4 && strncasecmp(line, "mode", name_len) == 0) {
			if (!GisViewportDrawModes::set_draw_mode_from_file(gisview, value_start)) {
				this->parse_status = LoadStatus::Code::ParseError;
				this->parse_status.parser_line = this->line_num;
				this->parse_status.parser_message = QString("Failed to set draw mode %1.").arg(value_start);
				qDebug() << QObject::tr("Error in line %1: %2")
					.arg(this->parse_status.parser_line)
					.arg(this->parse_status.parser_message);

				read_status = sg_ret::err;
			}

		} else if (name_len == 5 && strncasecmp(line, "color", name_len) == 0) {
			gisview->set_background_color(QString(value_start));

		} else if (name_len == 14 && strncasecmp(line, "highlightcolor", name_len) == 0) {
			gisview->set_highlight_color(QString(value_start));

		} else if (name_len == 9 && strncasecmp(line, "drawscale", name_len) == 0) {
			gisview->set_scale_visibility(TEST_BOOLEAN(value_start));

		} else if (name_len == 14 && strncasecmp(line, "drawcentermark", name_len) == 0) {
			gisview->set_center_mark_visibility(TEST_BOOLEAN(value_start));

		} else if (name_len == 13 && strncasecmp(line, "drawhighlight", name_len) == 0) {
			gisview->set_highlight_usage(TEST_BOOLEAN(value_start));

		} else if (name_len == 7 && strncasecmp(line, "visible", name_len) == 0) {
			/* This branch exists for both top level layer and sub-layers. */
			top_layer->set_visible(TEST_BOOLEAN(value_start));

		} else {
			this->parse_status = LoadStatus::Code::ParseError;
			this->parse_status.parser_line = this->line_num;
			this->parse_status.parser_message = QString("Invalid parameter or parameter outside of layer (%1).").arg(line);
			qDebug() << QObject::tr("Error in line %1: %2")
				.arg(this->parse_status.parser_line)
				.arg(this->parse_status.parser_message);

			read_status = sg_ret::err;
		}
	}

	return read_status;
}




static void file_write(FILE * file, LayerAggregate * top_level_layer, GisViewport * gisview)
{
	file_write_header(file, top_level_layer, gisview);

	LayerStack<std::list<const Layer *>> stack;
	stack.push(top_level_layer->get_child_layers()); /* List of layers pushed to stack may be empty (if Top Level Layer has no children). */

	while (stack.layers.size() && stack.layers.front().size()) {

		const Layer * current = stack.layers.front().front();
		fprintf(file, "\n~Layer %s\n", current->get_fixed_layer_kind_string().toUtf8().constData());
		write_layer_params_and_data(file, current);

		/* The layer at the front has been written, and we
		   will put its children (if any) on stack below. So
		   we don't need the front layer anymore. */
		stack.layers.front().pop_front();

		std::list<const Layer *> children = current->get_child_layers();
		if (!children.empty()) {
			/* This may happen for Aggregate or GPS layer. */
			stack.push(children);
		} else {
			/* This layer had no children, so we have completed writing it. */
			fprintf(file, "~EndLayer\n\n");

			while (!stack.empty() && stack.layers.front().empty()) {
				/* We have written all layers in given level.
				   Time to go back one level up (to parent level). */
				stack.pop();

				if (stack.empty()) {
					/* We have completed writing Top Level Layer. This layer doesn't require ~EndLayer tag. */
				} else {
					/* This marks end of aggregate or gps layer. */
					fprintf(file, "~EndLayer\n\n");
				}
			}
		}
	}
}




static void string_list_set_param(param_id_t param_id, const QStringList & string_list, Layer * layer)
{
	const SGVariant param_value(string_list);
	layer->set_param_value(param_id, param_value, true);
}




/**
   @brief Parse given @param line, create SGVariant from it

   Parse given string, create a SGVariant of type @param type_id.
*/
SGVariant value_string_to_sgvariant(const char * line, SGVariantType type_id)
{
	SGVariant new_val;
	switch (type_id) {
	case SGVariantType::Double:
		new_val = SGVariant((double) strtod_i8n(line, nullptr));
		break;

	case SGVariantType::Int: /* 'Int' and 'Enumeration' are distinct types, so keep them in separate cases. */
		new_val = SGVariant((int32_t) strtol(line, nullptr, 10), SGVariantType::Int);
		break;

	case SGVariantType::Enumeration: /* 'Int' and 'Enumeration' are distinct types, so keep them in separate cases. */
		new_val = SGVariant((int) strtol(line, nullptr, 10), SGVariantType::Enumeration);
		break;

	case SGVariantType::String:
		new_val = SGVariant(line);
		break;

	case SGVariantType::Boolean:
		new_val = SGVariant((bool) TEST_BOOLEAN(line));
		break;

	case SGVariantType::Color:
		new_val.val_color.setNamedColor(line);
		if (!new_val.val_color.isValid()) {
			new_val.val_color.setNamedColor("black"); /* Fallback value. */
		}
		break;

	case SGVariantType::StringList:
		/* StringList layer parameters should not be handled
		   by this function. Values of such parameters that we
		   read from file are added as simple strings to
		   ::string_lists[], and then saved to layer at the
		   end of processing of a layer. If we get here, it's
		   an error. */
		qDebug() << SG_PREFIX_E << "Attempting to process StringList in this function is an error; param value = " << line;
		break;

	default:
		/* Other types should not appear in a file. */
		qDebug() << SG_PREFIX_E << "Unexpected type id" << type_id;
		break;
	}

	return new_val;
}



void ReadParser::handle_layer_begin(const char * line, GisViewport * gisview)
{
	const LayerKind current_layer_kind = Layer::kind_from_layer_kind_string(QString(line + 6));
	if (current_layer_kind == LayerKind::Max) {

		/* We have encountered beginning of layer of unknown type. */
		this->parse_status = LoadStatus::Code::ParseError;
		this->parse_status.parser_line = this->line_num;
		this->parse_status.parser_message = QString("Unknown layer type '%1'").arg(line + 6);
		qDebug() << QObject::tr("Error in line %1: %2")
			.arg(this->parse_status.parser_line)
			.arg(this->parse_status.parser_message);

		this->layers_stack.push(nullptr); /* This will indicate that current layer is invalid. */
		return;
	}

	if (nullptr == this->layers_stack.first) {
		/* There must be some layer on stack that will be a
		   parent of currently read layer. The parent could be
		   a Top Layer, or some other Aggregate layer, or at
		   least a GPS layer. */
		this->parse_status = LoadStatus::Code::ParseError;
		this->parse_status.parser_line = this->line_num;
		this->parse_status.parser_message = QString("Layer command inside non-Aggregate, non-GPS Layer.");
		qDebug() << "Error:" + this->parse_status.parser_message + "\n";

		this->layers_stack.push(nullptr); /* This will indicate that current layer is invalid. */
		return;
	}


	const LayerKind parent_kind = this->layers_stack.first->m_kind;
	switch (parent_kind) {
	case LayerKind::GPS: {
		LayerGPS * parent = (LayerGPS *) this->layers_stack.second;

		/* This child layer has probably been created when a GPS layer has been read from file. */
		Layer * child = parent->get_a_child();
		this->layers_stack.push(child);

		/* In this function we handle beginning of a new
		   layer, and here we prepare references to the
		   layer's parameter specs. To be used by other parts
		   of parser. */
		this->param_specs = Layer::get_interface(current_layer_kind)->parameters_c;
		this->param_specs_count = Layer::get_interface(current_layer_kind)->parameter_specifications.size();

		qDebug() << SG_PREFIX_I << "Attaching item" << child->get_name() << "to tree under" << parent->get_name();
		parent->attach_child_to_tree(child);
		return;
	}
	case LayerKind::Aggregate: {
		Layer * layer = Layer::construct_layer(current_layer_kind, gisview);
		this->layers_stack.push(layer);

		/* In this function we handle beginning of a new
		   layer, and here we prepare references to the
		   layer's parameter specs. To be used by other parts
		   of parser. */
		this->param_specs = layer->get_interface().parameters_c;
		this->param_specs_count = layer->get_interface().parameter_specifications.size();

		/* Notice that stack.second may become available
		   only after stack.push() executed above, e.g.
		   when before the push() there was only Top Layer
		   in the stack. */

		//LayerAggregate * agg = (LayerAggregate *) stack.second;
		//qDebug() << SG_PREFIX_I << "Attaching item" << layer->get_name() << "to tree under" << agg->get_name();
		//agg->attach_child_to_tree(layer);
		return;
	}
	default:
		this->parse_status = LoadStatus::Code::ParseError;
		this->parse_status.parser_line = this->line_num;
		this->parse_status.parser_message = QString("Layer command inside non-Aggregate, non-GPS Layer (type %1).").arg((int) parent_kind);
		qDebug() << QObject::tr("Error in line %1: %2")
			.arg(this->parse_status.parser_line)
			.arg(this->parse_status.parser_message);

		this->layers_stack.push(nullptr); /* This will indicate that current layer is invalid. */
		return;
	}
}




void ReadParser::handle_layer_end(GisViewport * gisview)
{
	if (nullptr == this->layers_stack.second) {
		this->parse_status = LoadStatus::Code::ParseError;
		this->parse_status.parser_line = this->line_num;
		this->parse_status.parser_message = QString("Mismatched ~EndLayer command.");
		qDebug() << QObject::tr("Error in line %1: %2")
			.arg(this->parse_status.parser_line)
			.arg(this->parse_status.parser_message);

	} else {
		Layer * parent_layer = this->layers_stack.second;
		Layer * layer = this->layers_stack.first;

		/* Add to the layer any string lists we've accumulated. */
		this->push_string_lists_to_layer(layer);

		/* The layer is read, we don't need to keep its
		   StringList parameters anymore. It could be even
		   harmful if the next layer had StringList parameters
		   with the same names. */
		this->string_lists.clear();

		qDebug() << "------- EndLayer for pair of first/second = " << this->layers_stack.first->get_name() << this->layers_stack.second->get_name();

		if (layer && parent_layer) {
			if (parent_layer->m_kind == LayerKind::Aggregate) {
				layer->post_read(gisview, true);
			} else if (parent_layer->m_kind == LayerKind::GPS) {
				/* TODO_MAYBE: anything else needs to be done here? */
			} else {
				this->parse_status = LoadStatus::Code::ParseError;
				this->parse_status.parser_line = this->line_num;
				this->parse_status.parser_message = QString("EndLayer command inside non-Aggregate, non-GPS layer (kind %1).")
					.arg((int) this->layers_stack.first->m_kind);
				qDebug() << QObject::tr("Error in line %1: %2")
					.arg(this->parse_status.parser_line)
					.arg(this->parse_status.parser_message);
			}
		}
		this->layers_stack.pop();
	}

	return;
}




/**
   @reviewed-on 2019-12-07
*/
void ReadParser::push_string_lists_to_layer(Layer * layer)
{
	for (auto iter = this->string_lists.begin(); iter != this->string_lists.end(); iter++) {
		const param_id_t param_id = iter.key();
		const QStringList & string_list = iter.value();
		const SGVariant param_value(string_list);

		qDebug() << SG_PREFIX_I << "Saving StringList to layer" << layer->get_name() << ":";
		qDebug() << string_list;

		layer->set_param_value(param_id, param_value, true);
	}
	return;
}




void ReadParser::handle_layer_data_begin(const QString & dirpath)
{
	Layer * layer = this->layers_stack.first;

	const LayerDataReadStatus read_status = layer->read_layer_data(*this->file, dirpath);
	switch (read_status) {
	case LayerDataReadStatus::Error:
		this->parse_status = LoadStatus::Code::ParseError;
		this->parse_status.parser_line = this->line_num;
		this->parse_status.parser_message = QString("LayerData for layer named '%1' read unsuccessfully.").arg(layer->get_name());
		qDebug() << QObject::tr("Error in line %1: %2")
			.arg(this->parse_status.parser_line)
			.arg(this->parse_status.parser_message);
		break;

	case LayerDataReadStatus::Success:
		qDebug() << SG_PREFIX_D << "LayerData for layer named" << layer->get_name() << "read successfully";
		/* Success, pass. */
		break;
	case LayerDataReadStatus::Unrecognized:
		/* Simply skip layer data over. */
		while (0 < this->file->readLine(this->buffer, this->buffer_size)) {
			qDebug() << SG_PREFIX_D << "Skipping over unrecognized layer data in line" << this->line_num << QString(this->buffer).left(30);
			this->line_num++;

			char * line = this->buffer;

			if (0 == consume_empty_in_line(&line)) {
				/* Skip empty line. */
				continue;
			}

			if (strcasecmp(line, "~EndLayerData") == 0) {
				qDebug() << SG_PREFIX_D << "encountered end of LayerData in line" << this->line_num << line;
				break;
			}
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid layer data read status" << (int) read_status;
		break;
	}

	return;
}




void ReadParser::handle_layer_parameter(const char * line, size_t line_len)
{
	/* The layer, for which we will set parameters. */
	Layer * layer = this->layers_stack.first;

	/* Parent layer of current layer. */
	Layer * parent_layer = this->layers_stack.second;


	size_t name_len = 0; /* Length of parameter's name. */
	const char * value_start = nullptr;
	for (size_t i = 0; i < line_len; i++) {
		if (line[i] == '=') {
			name_len = i;
			value_start = line + name_len + 1;
		}
	}

	if (0 == name_len) {
		this->parse_status = LoadStatus::Code::ParseError;
		this->parse_status.parser_line = this->line_num;
		this->parse_status.parser_message = QString("Invalid parameter or parameter outside of layer.");
		qDebug() << QObject::tr("Error in line %1: %2")
			.arg(this->parse_status.parser_line)
			.arg(this->parse_status.parser_message);

		return;
	}

	if (name_len == 4 && 0 == strncasecmp(line, "name", name_len)) {
		/* Generic parameter "name" - every layer has it. */
		layer->set_name(QString(value_start));

		/* TODO_MAYBE: why do we add layer here, when
		   processing layer name, and not when opening tag for
		   layer is discovered? */
		qDebug() << SG_PREFIX_I << "Calling add_child_item(), parent layer =" << parent_layer->get_name() << ", child (this) layer = " << layer->get_name();
		parent_layer->add_child_item(layer);

	} else if (name_len == 7 && 0 == strncasecmp(line, "visible", name_len)) {
		/* Generic parameter "visible" - every layer has it. */
		layer->set_visible(TEST_BOOLEAN(value_start));

	} else { /* Some layer-type-specific parameter. */
		if (nullptr == this->param_specs) {
			this->parse_status = LoadStatus::Code::ParseError;
			this->parse_status.parser_line = this->line_num;
			this->parse_status.parser_message = QString("Error when parsing parameters for layer.");
			qDebug() << QObject::tr("Error in line %1: %2")
				.arg(this->parse_status.parser_line)
				.arg(this->parse_status.parser_message);

			return;
		}

		bool parameter_found = false;

		/* Iterate over layer's parameters array, find
		   parameter whose name matches name that we have just
		   read from file. param_id is an id of parameter in
		   layer's array of ParameterSpecification
		   variables. */
		for (param_id_t param_id = 0; param_id < this->param_specs_count; param_id++) {

			const ParameterSpecification & param_spec = this->param_specs[param_id];

			if (0 != strncasecmp(line, param_spec.name.toUtf8().constData(), name_len)) {
				continue;
			}

			if (param_spec.type_id == SGVariantType::StringList) {

				/* Append current value to list of
				   strings, possibly making a new
				   string list. The string list will
				   be saved to layer later, in
				   push_string_lists_to_layer(), after
				   we read an ~EndLayer tag.

				   The list of strings is a value of layer's parameter 'param_id'.

				   [] operator returns modifiable reference to existing value,
				   or to default-constructed value. In both cases we append
				   new value (value_start) to list of strings at key 'param_id'. */
				qDebug() << SG_PREFIX_D << "Appending string" << value_start << "to string-list parameter" << param_spec.name;
				this->string_lists[param_id] << QString(value_start);
			} else {
				const SGVariant new_val = value_string_to_sgvariant(value_start, param_spec.type_id);

				qDebug() << SG_PREFIX_D << "Setting value of parameter named" << param_spec.name << "of layer named" << layer->get_name() << ":" << new_val;
				layer->set_param_value(param_id, new_val, true);
			}
			parameter_found = true;
			break;
		}

		if (!parameter_found) {
			/* ATM don't flow up this issue because at least one internal parameter has changed from version 1.3
			   and don't what to worry users about raising such issues.
			   TODO_LATER Maybe hold old values here - compare the line value against them and if a match
			   generate a different style of message in the GUI... */
			// this->parse_status = sg_ret::err;
			fprintf(stderr, "WARNING: Line %zd: Unknown parameter. Line:\n%s\n", this->line_num, line);
		}
	}

	return;
}




/**
   Read in a Viking file and return how successful the parsing was.
   ATM this will always work, in that even if there are parsing problems
   then there will be no new values to override the defaults.
*/
LoadStatus VikFile::read_file(QFile & file, LayerAggregate * top_layer, const QString & dirpath, GisViewport * gisview)
{
	LatLon lat_lon;

	ReadParser read_parser(&file);
	read_parser.layers_stack.push(top_layer);

	if (sg_ret::ok != read_parser.read_header(top_layer, gisview, lat_lon)) {
		return read_parser.parse_status;
	}

	/* First interesting line after header is already in
	   buffer. Therefore don't read it on top of this loop, use
	   do-while loop instead. */
	do {
		read_parser.line_num++;

		char * line = read_parser.buffer;
		size_t line_len = consume_empty_in_line(&line);
		if (0 == line_len) {
			continue;
		}

		if (line[0] == '~') {
			line++;
			line_len--;

			if (*line == '\0') {
				continue;

			} else if (str_starts_with(line, "Layer ", 6, true)) {
				qDebug() << SG_PREFIX_D << "Encountered begin of Layer in line" << read_parser.line_num << line;
				read_parser.handle_layer_begin(line, gisview);

			} else if (str_starts_with(line, "EndLayer", 8, false)) {
				qDebug() << SG_PREFIX_D << "Encountered end of Layer in line" << read_parser.line_num << line;
				read_parser.handle_layer_end(gisview);

			} else if (str_starts_with(line, "LayerData", 9, false)) {
				qDebug() << SG_PREFIX_D << "Encountered begin of LayerData in line" << read_parser.line_num << line;
				read_parser.handle_layer_data_begin(dirpath);

			} else {
				read_parser.parse_status = LoadStatus::Code::ParseError;
				read_parser.parse_status.parser_line = read_parser.line_num;
				read_parser.parse_status.parser_message = QString("Unknown tilde command.");
				qDebug() << QObject::tr("Error in line %1: %2")
					.arg(read_parser.parse_status.parser_line)
					.arg(read_parser.parse_status.parser_message);
			}
		} else {
			if (nullptr == read_parser.layers_stack.first || nullptr == read_parser.layers_stack.second) {
				/* There is no target layer into which
				   we could read data ('first') or
				   there is no target layers' parent
				   ('second'). */
				continue;
			} else {
				read_parser.handle_layer_parameter(line, line_len);
			}
		}
		/* could be:
		   [Layer Type=Bla]
		   [EndLayer]
		   [LayerData]
		   name=this
		   #comment
		*/

		if (LoadStatus::Code::Success != read_parser.parse_status) {
			return read_parser.parse_status;
		}
	} while (0 < file.readLine(read_parser.buffer, read_parser.buffer_size));

	while (!read_parser.layers_stack.empty()) {
		if (read_parser.layers_stack.second && read_parser.layers_stack.first){

#if 0 /* TODO: restore? */
			LayerAggregate * parent = (LayerAggregate *) read_parser.layers_stack.second;
			Layer * child = read_parser.layers_stack.first;

			qDebug() << SG_PREFIX_D << "Will call parent Aggregate Layer's" << parent->name << "add_child_item(" << child->name << ")";
			parent->add_child_item(child);

			qDebug() << SG_PREFIX_D << "Will call child layer's" << child->name << "post_read()";
			child->post_read(gisview, true);
#endif
		}
		read_parser.layers_stack.pop();
	}

	gisview->set_center_coord(lat_lon); /* The function will reject lat_lon if it's invalid. */

	if (top_layer->tree_view && !top_layer->is_visible()) {
		top_layer->tree_view->apply_tree_item_visibility(top_layer);
	}

	return read_parser.parse_status;
}




/*
read thru file
if "[Layer Type="
  push(&stack)
  new default layer of type (str_to_type) (check interface->name)
if "[EndLayer]"
  Layer * layer = stack.first;
  pop(&stack);
  ((AggregateLayer *) stack.first)->add_child_item(layer);
if "[LayerData]"
  vik_layer_data (VIK_LAYER_DATA(stack.first), file, gisview);

*/

/* ---------------------------------------------------- */




bool VikFile::has_vik_file_magic(const QString & file_full_path)
{
	bool result = false;

	QFile file(file_full_path);
	if (file.open(QIODevice::ReadOnly)) {
		result = FileUtils::file_has_magic(file, VIK_MAGIC, VIK_MAGIC_LEN);
	} else {
		result = false;
		qDebug() << SG_PREFIX_E << "Failed to open file" << file_full_path << "as read only:" << file.error();
	}
	return result;
}




/**
   \brief Append a file extension, if not already present.
   Returns: file name with extension.
*/
QString SlavGPS::append_file_ext(const QString & file_name, SGFileType file_type)
{
	QString ext;

	/* Select an extension. */
	switch (file_type) {
	case SGFileType::GPX:
		ext = ".gpx";
		break;
	case SGFileType::KML:
		ext = ".kml";
		break;
	case SGFileType::GeoJSON:
		ext = ".geojson";
		break;
	case SGFileType::GPSMapper:
	case SGFileType::GPSPoint:
	default:
		/* Do nothing, ext will be empty. */
		break;
	}

	/* Do. */
	QString new_name;
	if (!ext.isEmpty() && !FileUtils::has_extension(file_name, ext)) {
		new_name = file_name + ext;
	} else {
		/* Simply duplicate. */
		new_name = file_name;
	}

	return new_name;
}




LoadStatus VikFile::load(LayerAggregate * parent_layer, GisViewport * gisview, const QString & file_full_path)
{
	if (nullptr == gisview) {
		return LoadStatus::Code::InternalLogicError;
	}

	QString full_path;
	if (file_full_path.left(7) == "file://") {
		full_path = file_full_path.right(file_full_path.size() - 7);
	} else {
		full_path = file_full_path;
	}
	qDebug() << SG_PREFIX_D << "Reading from file" << full_path;

	QFile file(full_path);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << SG_PREFIX_E << "Failed to open file" << full_path << "as read only:" << file.error();
		return LoadStatus::Code::CantOpenFileError;
	}

	LoadStatus load_status = LoadStatus::Code::GenericError;

	/* Attempt loading the primary file type first - our internal .vik file: */
	FileUtils::FileType file_type = FileUtils::discover_file_type(file, full_path);
	if (FileUtils::FileType::Vik == file_type) {
		const QString dir_full_path = FileUtils::path_get_dirname(full_path);
		load_status = VikFile::read_file(file, parent_layer, dir_full_path, gisview);

	} else if (FileUtils::FileType::JPEG == file_type) {
		load_status = jpg_load_file(parent_layer, gisview, full_path);

	} else {
		/* For all other file types which consist of tracks, routes and/or waypoints,
		   must be loaded into a new TrackWaypoint layer (hence it be created). */

		LayerTRW * trw = new LayerTRW();
		trw->set_coord_mode(gisview->get_coord_mode());
		trw->set_name(FileUtils::get_base_name(full_path));

		if (FileUtils::FileType::KML == file_type) {
			qDebug() << SG_PREFIX_I << "Explicit import of kml file";

			BabelProcess importer;
			importer.set_input("kml", full_path);
			importer.set_output("gpx", "-");

			load_status = importer.convert_through_gpx(trw);

		} else if (FileUtils::FileType::GPX == file_type) {
			load_status = GPX::read_layer_from_file(file, trw);

		} else {
			/* Try final supported file type. Failure here
			   means we don't know how to handle the
			   file. */
			const QString dir_full_path = FileUtils::path_get_dirname(full_path);
			const LayerDataReadStatus rv = GPSPoint::read_layer_from_file(file, trw, dir_full_path);
			load_status = (rv == LayerDataReadStatus::Success ? LoadStatus::Code::Success : LoadStatus::Code::GenericError);
		}

		/* Clean up when we can't handle the file. */
		if (LoadStatus::Code::Success != load_status) {
			delete trw;
		} else {
			/* Complete the setup from the successful load. */
			trw->post_read(gisview, true);
			parent_layer->add_child_item(trw);
			trw->move_viewport_to_show_all(gisview);
		}
	}

	return load_status;
}




SaveStatus VikFile::save(LayerAggregate * top_layer, GisViewport * gisview, const QString & file_full_path)
{
	SaveStatus save_result;

	QString full_path;
	if (file_full_path.left(7) == "file://") {
		full_path = file_full_path.right(file_full_path.size() - 7);
	} else {
		full_path = file_full_path;
	}
	qDebug() << SG_PREFIX_D << "Saving to file" << full_path;

	FILE * file = fopen(full_path.toUtf8().constData(), "w");
	if (!file) {
		return SaveStatus::Code::CantOpenFileError;
	}

	/* Enable relative paths in .vik files to work. */
	const QString cwd = QDir::currentPath();
	const QString dir_full_path = FileUtils::path_get_dirname(full_path);
	if (!dir_full_path.isEmpty()) {
		if (!QDir::setCurrent(dir_full_path)) {
			qDebug() << SG_PREFIX_W << "Could not change directory to" << dir_full_path;
			/* TODO_LATER: better error handling here. */
		}
	}

	file_write(file, top_layer, gisview);

	/* Restore previous working directory. */
	if (!QDir::setCurrent(cwd)) {
		qDebug() << SG_PREFIX_W << "Could not return to directory" << cwd;
	}

	fclose(file);
	file = nullptr;

	return SaveStatus::Code::Success;
}




SaveStatus VikFile::export_trw_track(Track * trk, const QString & file_full_path, SGFileType file_type, bool write_hidden)
{
	GPXWriteOptions options(false, false, write_hidden, false);

	FILE * file = fopen(file_full_path.toUtf8().constData(), "w");
	if (nullptr == file) {
		qDebug() << SG_PREFIX_E << "Failed to open file" << file_full_path << "as write only";
		return SaveStatus::Code::CantOpenFileError;
	}

	SaveStatus save_status;
	switch (file_type) {
	case SGFileType::GPX:
		qDebug() << SG_PREFIX_I << "Exporting as GPX:" << file_full_path;
		/* trk defined so can set the option. */
		options.is_route = trk->is_route();
		save_status = GPX::write_track_to_file(file, trk, &options);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected file type for track" << (int) file_type;
		save_status = SaveStatus::Code::InternalLogicError;
	}

	fclose(file);
	return save_status;
}




static SaveStatus export_to_kml(const QString & file_full_path, LayerTRW * trw)
{
	SaveStatus save_status = SaveStatus::Code::Success;

	BabelProcess * exporter = new BabelProcess();

	const KMLExportUnits units = Preferences::get_kml_export_units();
	switch (units) {
	case KMLExportUnits::Metric:
		exporter->set_output("kml,units=m", file_full_path);
		break;
	case KMLExportUnits::Statute:
		exporter->set_output("kml", file_full_path);
		break;
	case KMLExportUnits::Nautical:
		exporter->set_output("kml,units=n", file_full_path);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid KML Export units" << (int) units;
		save_status = SaveStatus::Code::InternalLogicError;
		break;
	}


	if (SaveStatus::Code::Success == save_status) {
		save_status = exporter->export_through_gpx(trw, nullptr);
	}

	delete exporter;

	return save_status;
}




/* Call it when @trk argument to VikFile::export() is NULL. */
SaveStatus VikFile::export_trw_layer(LayerTRW * trw, const QString & file_full_path, SGFileType file_type, bool write_hidden)
{
	GPXWriteOptions options(false, false, write_hidden, false);

	FILE * file = fopen(file_full_path.toUtf8().constData(), "w");
	if (nullptr == file) {
		qDebug() << SG_PREFIX_E << "Failed to open file" << file_full_path;
		return SaveStatus::Code::CantOpenFileError;
	}

	SaveStatus result = SaveStatus::Code::GenericError;

	switch (file_type) {
	case SGFileType::GPSMapper:
		result = GPSMapper::write_layer_to_file(file, trw);
		break;
	case SGFileType::GPX:
		result = GPX::write_layer_to_file(file, trw, &options);
		break;
	case SGFileType::GPSPoint:
		result = GPSPoint::write_layer_to_file(file, trw);
		break;
	case SGFileType::GeoJSON:
		result = GeoJSON::write_layer_to_file(file, trw);
		break;
	case SGFileType::KML:
		result = export_to_kml(file_full_path, trw);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid file type for non-track" << (int) file_type;
	}

	fclose(file);

	return result;
}




/**
 * @file_full_path: The path of the file to be written
 * @file_type: Choose one of the supported file types for the export
 * @trk: If specified then only export this track rather than the whole layer
 * @write_hidden: Whether to write invisible items
 *
 * A general export command to convert from Viking TRW layer data to an external supported format.
 * The write_hidden option is provided mainly to be able to transfer selected items when uploading to a GPS.
 */
SaveStatus VikFile::export_trw(LayerTRW * trw, const QString & file_full_path, SGFileType file_type, Track * trk, bool write_hidden)
{
	if (trk) {
		return VikFile::export_trw_track(trk, file_full_path, file_type, write_hidden);
	} else {
		return VikFile::export_trw_layer(trw, file_full_path, file_type, write_hidden);
	}
}




SaveStatus VikFile::export_with_babel(LayerTRW * trw, const QString & output_file_full_path, const QString & output_data_format, bool tracks, bool routes, bool waypoints)
{
	BabelProcess * exporter = new BabelProcess();

	exporter->set_options(BabelProcess::get_trw_string(tracks, routes, waypoints));
	exporter->set_output(output_data_format, output_file_full_path);

	const SaveStatus save_status = exporter->export_through_gpx(trw, nullptr);

	delete exporter;

	return save_status;
}




/**
 * Permission granted to use this code after personal correspondance.
 * Slightly reworked for better cross platform use, glibisms, function rename and a compacter format.
 *
 * FROM http://www.codeguru.com/cpp/misc/misc/fileanddirectorynaming/article.php/c263
 */

// GetRelativeFilename(), by Rob Fisher.
// rfisher@iee.org
// http://come.to/robfisher

/* The number of characters at the start of an absolute filename.  e.g. in DOS,
   absolute filenames start with "X:\" so this value should be 3, in UNIX they start
   / with "\" so this value should be 1. */
#ifdef WINDOWS
#define ABSOLUTE_NAME_START 3
#else
#define ABSOLUTE_NAME_START 1
#endif

/* Given the absolute current directory and an absolute file name, returns a relative file name.
   For example, if the current directory is C:\foo\bar and the filename C:\foo\whee\text.txt is given,
   GetRelativeFilename will return ..\whee\text.txt. */

char const * SlavGPS::file_GetRelativeFilename(const char * currentDirectory, const char * absoluteFilename)
{
	int cdLen = strlen(currentDirectory);
	int afLen = strlen(absoluteFilename);

	/* Make sure the names are not too long or too short */
	if (cdLen > MAXPATHLEN || cdLen < ABSOLUTE_NAME_START+1
	    || afLen > MAXPATHLEN || afLen < ABSOLUTE_NAME_START+1) {
		return nullptr;
	}

	static char relativeFilename[MAXPATHLEN+1];
	/* Handle DOS names that are on different drives: */
	if (currentDirectory[0] != absoluteFilename[0]) {
		/* Not on the same drive, so only absolute filename will do. */
		strncpy(relativeFilename, absoluteFilename, sizeof (relativeFilename));
		relativeFilename[sizeof (relativeFilename) - 1] = '\0';

		return relativeFilename;
	}

	/* They are on the same drive, find out how much of the current directory
	   is in the absolute filename. */
	int i = ABSOLUTE_NAME_START;
	while (i < afLen && i < cdLen && currentDirectory[i] == absoluteFilename[i]) {
		i++;
	}

	if (i == cdLen && (absoluteFilename[i] == G_DIR_SEPARATOR || absoluteFilename[i-1] == G_DIR_SEPARATOR)) {
		/* The whole current directory name is in the file name,
		   so we just trim off the current directory name to get the
		   current file name. */
		if (absoluteFilename[i] == G_DIR_SEPARATOR) {
			/* A directory name might have a trailing slash but a relative
			   file name should not have a leading one... */
			i++;
		}

		strncpy(relativeFilename, &absoluteFilename[i], sizeof (relativeFilename));
		relativeFilename[sizeof (relativeFilename) - 1] = '\0';

		return relativeFilename;
	}

	/* The file is not in a child directory of the current directory, so we
	   need to step back the appropriate number of parent directories by
	   using "..\"s.  First find out how many levels deeper we are than the
	   common directory. */
	int afMarker = i;
	int levels = 1;

	/* Count the number of directory levels we have to go up to get to the
	   common directory. */
	while (i < cdLen) {
		i++;
		if (currentDirectory[i] == G_DIR_SEPARATOR) {
			/* Make sure it's not a trailing slash. */
			i++;
			if (currentDirectory[i] != '\0') {
				levels++;
			}
		}
	}

	/* Move the absolute filename marker back to the start of the directory name
	   that it has stopped in. */
	while (afMarker > 0 && absoluteFilename[afMarker-1] != G_DIR_SEPARATOR) {
		afMarker--;
	}

	/* Check that the result will not be too long. */
	if (levels * 3 + afLen - afMarker > MAXPATHLEN) {
		return nullptr;
	}

	/* Add the appropriate number of "..\"s. */
	int rfMarker = 0;
	for (i = 0; i < levels; i++) {
		relativeFilename[rfMarker++] = '.';
		relativeFilename[rfMarker++] = '.';
		relativeFilename[rfMarker++] = G_DIR_SEPARATOR;
	}

	/* Copy the rest of the filename into the result string. */
	strcpy(&relativeFilename[rfMarker], &absoluteFilename[afMarker]);

	return relativeFilename;
}

QString SlavGPS::file_GetRelativeFilename(const QString & current_dir_path, const QString & file_path)
{
	const char * s = file_GetRelativeFilename(current_dir_path.toUtf8().constData(), file_path.toUtf8().constData());
	if (!s) {
		return QString("");
	} else {
		return QString(s);
	}
}

/* END http://www.codeguru.com/cpp/misc/misc/fileanddirectorynaming/article.php/c263 */
