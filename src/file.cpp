/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2012-2013, Rob Norris <rw_norris@hotmail.com>
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
#include <cstdio>
#include <cassert>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef WINDOWS
#define realpath(X,Y) _fullpath(Y,X,MAX_PATH)
#endif
#include <glib.h>
#include <glib/gstdio.h>

#include <QDebug>

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




#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif




using namespace SlavGPS;




#define PREFIX ": File:" << __FUNCTION__ << __LINE__ << ">"




#define VIK_MAGIC "#VIK"
#define GPX_MAGIC "<?xm"
#define VIK_MAGIC_LEN 4
#define GPX_MAGIC_LEN 4

#define VIKING_FILE_VERSION 1



template <class T>
class LayerStack {
public:
	void push(T data);
	void pop(void);
	bool empty(void) { return this->layers.empty(); };

	std::list<T> layers;

	T first = NULL; /* First layer (or list of layers) on top of stack. */
	T second = NULL; /* Second layer (or list of layers), counting from top (second after LayerStack::first). */
};




template <class T>
void LayerStack<T>::push(T data)
{
	this->layers.push_front(data);

	this->first = data;

	if (this->layers.size() > 1) {
		this->second = *std::next(this->layers.begin());
	} else {
		this->second = NULL;
	}
}




template <class T>
void LayerStack<T>::pop(void)
{
	this->layers.pop_front();

	this->first = NULL;
	this->second = NULL;

	if (this->layers.size() > 0) {
		this->first = *this->layers.begin();

		if (this->layers.size() > 1) {
			this->second = *std::next(this->layers.begin());
		}
	}
}




static bool check_magic(FILE * f, char const * magic_number, unsigned int magic_length)
{
	char magic[magic_length];
	bool rv = false;
	int8_t i;
	if (fread(magic, 1, sizeof(magic), f) == sizeof(magic) &&
	     strncmp(magic, magic_number, sizeof(magic)) == 0) {
		rv = true;
	}

	for (i = sizeof(magic)-1; i >= 0; i--) { /* The ol' pushback. */
		ungetc(magic[i],f);
	}
	return rv;
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
	fprintf(file, "name=%s\n", layer->name.isEmpty() ? "" : layer->name.toUtf8().constData());
	if (!layer->visible) {
		fprintf(file, "visible=f\n");
	}

	for (auto iter = layer->get_interface().parameter_specifications.begin(); iter != layer->get_interface().parameter_specifications.end(); iter++) {

		/* Get current, per-layer-instance value of parameter. Refer to the parameter by its id ((*iter)->first). */
		const SGVariant param_value = layer->get_param_value(iter->first, true);
		if (iter->second->type_id != param_value.type_id) {
			qDebug() << "EE" PREFIX << "type id mismatch for parameter named" << iter->second->name << ":" << iter->second->type_id << param_value.type_id;
			assert (iter->second->type_id == param_value.type_id);
		}
		param_value.write(file, iter->second->name);
	}

	layer->write_layer_data(file);
}




size_t handle_line(char ** line)
{
	while ((**line) == ' ' || (**line) =='\t') {
		(*line)++;
	}

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




void file_write_header(FILE * file, const LayerAggregate * top_level_layer, Viewport * viewport)
{
	const LatLon lat_lon = viewport->get_center()->get_latlon();

	const QString mode_id_string = ViewportDrawModes::get_id_string(viewport->get_drawmode());

	char bg_color_string[8] = { 0 };
	SGUtils::color_to_string(bg_color_string, sizeof (bg_color_string), viewport->get_background_color());

	char hl_color_string[8] = { 0 };
	SGUtils::color_to_string(hl_color_string, sizeof (hl_color_string), viewport->get_highlight_color());

	fprintf(file, "#VIKING GPS Data file " VIKING_URL "\n");
	fprintf(file, "FILE_VERSION=%d\n", VIKING_FILE_VERSION);
	fprintf(file, "\nxmpp=%f\n", viewport->get_xmpp());
	fprintf(file, "ympp=%f\n", viewport->get_ympp());
	fprintf(file, "lat=%f\n", lat_lon.lat);
	fprintf(file, "lon=%f\n", lat_lon.lon);
	fprintf(file, "mode=%s\n", mode_id_string.toUtf8().constData());
	fprintf(file, "color=%s\n", bg_color_string);
	fprintf(file, "highlightcolor=%s\n", hl_color_string);
	fprintf(file, "drawscale=%s\n", viewport->get_scale_visibility() ? "t" : "f");
	fprintf(file, "drawcentermark=%s\n", viewport->get_center_mark_visibility() ? "t" : "f");
	fprintf(file, "drawhighlight=%s\n", viewport->get_highlight_usage() ? "t" : "f");

	if (!top_level_layer->visible) {
		fprintf(file, "visible=f\n");
	}

	return;
}




bool read_header(FILE * file, char * buffer, size_t buffer_size, Layer * top_layer, Viewport * viewport, LatLon & lat_lon, long & line_num)
{
	bool successful_read = true;

	while (fgets(buffer, buffer_size, file))  {

		if (buffer[0] == '~') {
			/* Beginning of first layer. Stop reading
			   header, return the buffer through function
			   argument. */
			return successful_read;
		}

		line_num++;

		char * line = buffer;

		const size_t line_len = handle_line(&line);
		if (line_len == 0) {
			continue;
		}


		size_t name_len = 0; /* Length of parameter's name. */
		for (size_t i = 0; i < line_len; i++) {
			if (line[i] == '=') {
				name_len = i;
			}
		}


		if (name_len == 12 && strncasecmp(line, "FILE_VERSION", name_len) == 0) {
			int version = strtol(line+13, NULL, 10);
			fprintf(stderr, "DEBUG: %s: reading file version %d\n", __FUNCTION__, version);
			if (version > VIKING_FILE_VERSION) {
				successful_read = false;
			}
			/* However we'll still carry and attempt to read whatever we can. */
		} else if (name_len == 4 && strncasecmp(line, "xmpp", name_len) == 0) { /* "hard coded" params: global & for all layer-types */
			viewport->set_xmpp(strtod_i8n(line+5, NULL));

		} else if (name_len == 4 && strncasecmp(line, "ympp", name_len) == 0) {
			viewport->set_ympp(strtod_i8n(line+5, NULL));

		} else if (name_len == 3 && strncasecmp(line, "lat", name_len) == 0) {
			lat_lon.lat = strtod_i8n(line+4, NULL);

		} else if (name_len == 3 && strncasecmp(line, "lon", name_len) == 0) {
			lat_lon.lon = strtod_i8n(line+4, NULL);

		} else if (name_len == 4 && strncasecmp(line, "mode", name_len) == 0) {
			if (!ViewportDrawModes::set_draw_mode_from_file(viewport, line + 5)) {
				successful_read = false;
			}

		} else if (name_len == 5 && strncasecmp(line, "color", name_len) == 0) {
			viewport->set_background_color(QString(line+6));

		} else if (name_len == 14 && strncasecmp(line, "highlightcolor", name_len) == 0) {
			viewport->set_highlight_color(QString(line+15));

		} else if (name_len == 9 && strncasecmp(line, "drawscale", name_len) == 0) {
			viewport->set_scale_visibility(TEST_BOOLEAN(line+10));

		} else if (name_len == 14 && strncasecmp(line, "drawcentermark", name_len) == 0) {
			viewport->set_center_mark_visibility(TEST_BOOLEAN(line+15));

		} else if (name_len == 13 && strncasecmp(line, "drawhighlight", name_len) == 0) {
			viewport->set_highlight_usage(TEST_BOOLEAN(line+14));

		} else if (name_len == 7 && strncasecmp(line, "visible", name_len) == 0) {
			/* This branch exists for both top level layer and sub-layers. */
			top_layer->visible = TEST_BOOLEAN(line+8);

		} else {
			successful_read = false;
			fprintf(stderr, "WARNING: Line %ld: Invalid parameter or parameter outside of layer (%s).\n", line_num, line);
		}
	}

	return successful_read;
}




static void file_write(FILE * file, LayerAggregate * top_level_layer, Viewport * viewport)
{
	file_write_header(file, top_level_layer, viewport);

	LayerStack<std::list<const Layer *> *> stack;
	stack.push(top_level_layer->get_children()); /* This is Top Level Aggregate Layer, so this is safe even if there are no children. */

	while (stack.layers.size() && stack.layers.front()->size()) {

		const Layer * current = stack.layers.front()->front();
		fprintf(file, "\n~Layer %s\n", current->get_type_id_string().toUtf8().constData());
		write_layer_params_and_data(file, current);

		/* The layer at the front has been written, and we
		   will put its children (if any) on stack below. So
		   we don't need the front layer anymore. */
		stack.layers.front()->pop_front();

		std::list<const Layer *> * children = current->get_children();
		if (children && !children->empty()) {
			/* This may happen for Aggregate or GPS layer. */
			stack.push(children);
		} else {
			/* This layer had no children, so we have completed writing it. */
			fprintf(file, "~EndLayer\n\n");

			while (!stack.empty() && stack.layers.front()->empty()) {
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




static void string_list_delete(void * key, void * l, void * user_data)
{
#ifdef K_FIXME_RESTORE
	/* 20071021 bugfix */
	if (list) {
		for (auto iter = list->begin(); iter != list->end(); iter++) {
			free(*iter);
		}
		delete list;
	}
#endif
}




static void string_list_set_param(int i, const QStringList & string_list, Layer * layer)
{
	const SGVariant param_value(string_list);
	layer->set_param_value(i, param_value, true);
}




SGVariant new_sgvariant_sub(const char * line, SGVariantType type_id)
{
	SGVariant new_val;
	switch (type_id) {
	case SGVariantType::Double:
		new_val = SGVariant((double) strtod_i8n(line, NULL));
		break;

	case SGVariantType::Uint:
		new_val = SGVariant((uint32_t) strtoul(line, NULL, 10));
		break;

	case SGVariantType::Int:
		new_val = SGVariant((int32_t) strtol(line, NULL, 10));
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

	default:
		/* STRING or STRING_LIST -- if STRING_LIST, just set param to add a STRING. */
		/* TODO: review this section. */
		new_val = SGVariant(line);
	}

	return new_val;
}




/**
 * Read in a Viking file and return how successful the parsing was.
 * ATM this will always work, in that even if there are parsing problems
 * then there will be no new values to override the defaults.
 *
 * TODO flow up line number(s) / error messages of problems encountered...
 */
bool VikFile::read_file(FILE * file, LayerAggregate * top_layer, const char * dirpath, Viewport * viewport)
{
	LatLon lat_lon;
	char buffer[4096];
	long line_num = 0;

	ParameterSpecification * param_specs = NULL; /* For current layer, so we don't have to keep on looking up interface. */
	uint8_t param_specs_count = 0;

	GHashTable * string_lists = g_hash_table_new(g_direct_hash, g_direct_equal);
	LayerAggregate * aggregate = top_layer;

	bool successful_read = true;

	LayerStack<Layer *> stack;
	stack.push(top_layer);

	if (!read_header(file, buffer, sizeof (buffer), top_layer, viewport, lat_lon, line_num)) {
		return false;
	}

	/* First interesting line after header is already in
	   buffer. Therefore don't read it on top of this loop. */
	do {
		line_num++;

		char * line = buffer;

		size_t len = handle_line(&line);
		if (len == 0) {
			continue;
		}


		if (line[0] == '~') {
			line++;
			len--;
			if (*line == '\0') {
				continue;
			} else if (str_starts_with(line, "Layer ", 6, true)) {
				qDebug() << "DD: File: Read: encountered begin of Layer:" << line;

				if (!stack.first) { /* TODO: verify that this condition is handled correctly inside of this branch. */
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: Layer command inside non-Aggregate Layer\n", line_num);
					stack.push(NULL); /* Inside INVALID layer. */
					continue;
				}


				const LayerType parent_type = stack.first->type;
				if (parent_type != LayerType::AGGREGATE && parent_type != LayerType::GPS) {
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: Layer command inside non-Aggregate Layer (type %d)\n", line_num, (int) parent_type);
					stack.push(NULL); /* Inside INVALID layer. */
					continue;
				}

				const LayerType layer_type = Layer::type_from_type_id_string(QString(line + 6));
				TreeView * tree_view = NULL;

				if (layer_type == LayerType::NUM_TYPES) {
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: Unknown type %s\n", line_num, line+6);
					stack.push(NULL);
				} else if (parent_type == LayerType::GPS) {
					LayerGPS * gps = (LayerGPS *) stack.second;
					Layer * child = gps->get_a_child();
					stack.push(child);
					param_specs = Layer::get_interface(layer_type)->parameters_c;
					param_specs_count = Layer::get_interface(layer_type)->parameter_specifications.size();

					gps->tree_view->append_tree_item(gps->index, child, child->name);

				} else { /* Any other LayerType::X type. */

					Layer * layer = Layer::construct_layer(layer_type, viewport);
					stack.push(layer);
					param_specs = Layer::get_interface(layer_type)->parameters_c;
					param_specs_count = Layer::get_interface(layer_type)->parameter_specifications.size();

					/* Notice that stack.second may become available
					   only after stack.push() executed above, e.g.
					   when before the push() there was only Top Layer
					   in the stack. */

					//LayerAggregate * agg = (LayerAggregate *) stack.second;
					//qDebug() << "II" PREFIX << "Appending to tree a child layer named" << layer->name << "under aggregate named" << agg->name;
					//agg->tree_view->append_tree_item(agg->index, layer, layer->name);
				}

			} else if (str_starts_with(line, "EndLayer", 8, false)) {
				qDebug() << "DD: File: Read: encountered end of Layer:" << line;
				if (stack.second == NULL) {
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: Mismatched ~EndLayer command\n", line_num);
				} else {
					/* Add any string lists we've accumulated. */
					g_hash_table_foreach(string_lists, (GHFunc) string_list_set_param, stack.first);
					g_hash_table_remove_all(string_lists);

					qDebug() << "------- EndLayer for pair of first/second = " << stack.first->name << stack.second->name;

					if (stack.first && stack.second) {
						if (stack.second->type == LayerType::AGGREGATE) {
							Layer * layer = stack.first;
							//layer->add_children_to_tree();
							layer->post_read(viewport, true);
						} else if (stack.second->type == LayerType::GPS) {
							/* TODO: anything else needs to be done here? */
						} else {
							successful_read = false;
							fprintf(stderr, "WARNING: Line %ld: EndLayer command inside non-Aggregate Layer (type %d)\n", line_num, (int) stack.first->type);
						}
					}
					stack.pop();
				}
			} else if (str_starts_with(line, "LayerData", 9, false)) {
				qDebug() << "DD" PREFIX << "encountered begin of LayerData:" << line;
				Layer * layer = stack.first;
				const LayerDataReadStatus rv = layer->read_layer_data(file, dirpath);
				switch (rv) {
				case LayerDataReadStatus::Error:
					qDebug() << "EE" PREFIX << "LayerData for layer named" << layer->name << "read unsuccessfully";
					successful_read = false;
					break;
				case LayerDataReadStatus::Success:
					qDebug() << "DD" PREFIX << "LayerData for layer named" << layer->name << "read successfully";
					/* Success, pass. */
					break;
				case LayerDataReadStatus::Unrecognized:
					/* Simply skip layer data over. */
					while (fgets(buffer, sizeof (buffer), file)) {
						qDebug() << "DD" PREFIX "skipping over layer data:" << QString(line).left(30);
						line_num++;

						line = buffer;

						len = strlen(line);
						if (len > 0 && line[len-1] == '\n') {
							line[--len] = '\0';
						}

						if (len > 0 && line[len-1] == '\r') {
							line[--len] = '\0';
						}

						if (strcasecmp(line, "~EndLayerData") == 0) {
							qDebug() << "DD: File: Read: encountered end of LayerData:" << line;
							break;
						}
					}
					break;
				default:
					qDebug() << "EE" PREFIX << "invalid file read status" << (int) rv;
					break;
				}
			} else {
				successful_read = false;
				fprintf(stderr, "WARNING: Line %ld: Unknown tilde command\n", line_num);
			}
		} else {
			int eq_pos = -1;
			if (!stack.first) {
				continue;
			}

			for (int i = 0; i < (int) len; i++) {
				if (line[i] == '=') {
					eq_pos = i;
				}
			}

			Layer * layer = stack.first;

			if (stack.second && eq_pos == 4 && strncasecmp(line, "name", eq_pos) == 0) {

				/* Notice the condition:
				   "stack.second". This means that we
				   operate on non-Top Level Layer and
				   we can set its name. */

				layer->set_name(QString(line+5));

				qDebug() << "II" PREFIX << "calling add_layer(), parent / child = " << stack.second->name << "->" << layer->name;
				((LayerAggregate *) stack.second)->add_layer(layer, false);

			} else if (eq_pos == 7 && strncasecmp(line, "visible", eq_pos) == 0) {
				layer->visible = TEST_BOOLEAN(line+8);

			} else if (stack.second && eq_pos != -1) {
				bool parameter_found = false;

				/* Go though layer params. If len == eq_pos && starts_with jazz, set it. */
				/* Also got to check for name and visible. */

				if (!param_specs) {
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: No options for this kind of layer\n", line_num);
					continue;
				}

				for (int i = 0; i < param_specs_count; i++) {

					const ParameterSpecification * param_spec = &param_specs[i];

					if (!(strlen(param_spec->name) == eq_pos && strncasecmp(line, param_spec->name, eq_pos) == 0)) {
						continue;
					}

					line += eq_pos + 1;

					if (param_spec->type_id == SGVariantType::StringList) {
						GList *l = g_list_append((GList *) g_hash_table_lookup(string_lists, ((void *) (long) i)), g_strdup(line));
						g_hash_table_replace(string_lists, (void *) (long) i, l);
						/* Aadd the value to a list, possibly making a new list.
						   This will be passed to the layer when we read an ~EndLayer. */
					} else {
						const SGVariant new_val = new_sgvariant_sub(line, param_spec->type_id);

						qDebug() << "DD" PREFIX << "setting value of parameter named" << param_spec->name << "of layer named" << layer->name << ":" << new_val;
						layer->set_param_value(i, new_val, true);
					}
					parameter_found = true;
					break;
				}

				if (!parameter_found) {
					/* ATM don't flow up this issue because at least one internal parameter has changed from version 1.3
					   and don't what to worry users about raising such issues.
					   TODO Maybe hold old values here - compare the line value against them and if a match
					   generate a different style of message in the GUI... */
					// successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: Unknown parameter. Line:\n%s\n", line_num, line);
				}

			} else {
				successful_read = false;
				fprintf(stderr, "WARNING: Line %ld: Invalid parameter or parameter outside of layer.\n", line_num);
			}
		}
		/* could be:
		   [Layer Type=Bla]
		   [EndLayer]
		   [LayerData]
		   name=this
		   #comment
		*/
	} while (fgets(buffer, sizeof (buffer), file));

	while (!stack.empty()) {
		if (stack.second && stack.first){

			LayerAggregate * parent = (LayerAggregate *) stack.second;
			Layer * child = stack.first;

			//qDebug() << "DD" PREFIX << "will call parent Aggregate Layer's" << parent->name << "add_layer(" << child->name << ")";
			//parent->add_layer(child, false);

			//qDebug() << "DD" PREFIX << "will call child layer's" << child->name << "post_read()";
			//child->post_read(viewport, true);
		}
		stack.pop();
	}

	viewport->set_center_from_latlon(lat_lon, true); /* The function will reject lat_lon if it's invalid. */

	if ((!aggregate->visible) && aggregate->tree_view) {
		aggregate->tree_view->set_tree_item_visibility(aggregate->index, false);
	}

	/* Delete anything we've forgotten about -- should only happen when file ends before an EndLayer. */
	g_hash_table_foreach(string_lists, string_list_delete, NULL);
	g_hash_table_destroy(string_lists);

	return successful_read;
}




/*
read thru file
if "[Layer Type="
  push(&stack)
  new default layer of type (str_to_type) (check interface->name)
if "[EndLayer]"
  Layer * layer = stack.first;
  pop(&stack);
  vik_aggregate_layer_add_layer(stack.first, layer);
if "[LayerData]"
  vik_layer_data (VIK_LAYER_DATA(stack.first), f, viewport);

*/

/* ---------------------------------------------------- */




bool VikFile::has_vik_file_magic(const QString & file_full_path)
{
	bool result = false;
	FILE * file = fopen(file_full_path.toUtf8().constData(), "r");
	if (file) {
		result = check_magic(file, VIK_MAGIC, VIK_MAGIC_LEN);
		fclose(file);
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
	case SGFileType::GEOJSON:
		ext = ".geojson";
		break;
	case SGFileType::GPSMAPPER:
	case SGFileType::GPSPOINT:
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




FileLoadResult VikFile::load(LayerAggregate * parent_layer, Viewport * viewport, const QString & file_full_path)
{
	if (!viewport) {
		return FileLoadResult::READ_FAILURE;
	}

	QString full_path;
	if (file_full_path.left(7) == "file://") {
		full_path = file_full_path.right(file_full_path.size() - 7);
	} else {
		full_path = file_full_path;
	}
	qDebug() << "DD: VikFile: load: reading from file" << full_path;

	FILE * file = fopen(full_path.toUtf8().constData(), "r");
	if (!file) {
		return FileLoadResult::READ_FAILURE;
	}

	FileLoadResult load_answer = FileLoadResult::OTHER_SUCCESS;

	char * dirpath = g_path_get_dirname(full_path.toUtf8().constData());
	/* Attempt loading the primary file type first - our internal .vik file: */
	if (check_magic(file, VIK_MAGIC, VIK_MAGIC_LEN)) {
		if (VikFile::read_file(file, parent_layer, dirpath, viewport)) {
			load_answer = FileLoadResult::VIK_SUCCESS;
		} else {
			load_answer = FileLoadResult::VIK_FAILURE_NON_FATAL;
		}
	} else if (jpg_magic_check(full_path)) {
		if (!jpg_load_file(parent_layer, viewport, full_path)) {
			load_answer = FileLoadResult::UNSUPPORTED_FAILURE;
		}
	} else {
		/* For all other file types which consist of tracks, routes and/or waypoints,
		   must be loaded into a new TrackWaypoint layer (hence it be created). */
		bool success = true; /* Detect load failures - mainly to remove the layer created as it's not required. */

		LayerTRW * trw = new LayerTRW();
		trw->set_coord_mode(viewport->get_coord_mode());
		trw->set_name(FileUtils::get_base_name(full_path));

		/* In fact both kml & gpx files start the same as they are in xml. */
		if (FileUtils::has_extension(full_path, ".kml") && check_magic(file, GPX_MAGIC, GPX_MAGIC_LEN)) {
			/* Implicit Conversion. */
			BabelOptions babel_options(BabelOptionsMode::FromFile);
			babel_options.input = full_path;
			babel_options.babel_args = "-i kml";
			success = babel_options.import_from_local_file(trw, NULL);
			if (!success) {
				load_answer = FileLoadResult::GPSBABEL_FAILURE;
			}
		}
		/* NB use a extension check first, as a GPX file header may have a Byte Order Mark (BOM) in it
		   - which currently confuses our check_magic function. */
		else if (FileUtils::has_extension(full_path, ".gpx") || check_magic(file, GPX_MAGIC, GPX_MAGIC_LEN)) {
			success = GPX::read_file(file, trw);
			if (!success) {
				load_answer = FileLoadResult::GPX_FAILURE;
			}
		} else {
			/* Try final supported file type. */
			const LayerDataReadStatus rv = GPSPoint::read_layer(file, trw, dirpath);
			success = (rv == LayerDataReadStatus::Success);
			if (!success) {
				/* Failure here means we don't know how to handle the file. */
				load_answer = FileLoadResult::UNSUPPORTED_FAILURE;
			}
		}
		free(dirpath);

		/* Clean up when we can't handle the file. */
		if (!success) {
			delete trw;
		} else {
			/* Complete the setup from the successful load. */
			trw->post_read(viewport, true);
			parent_layer->add_layer(trw, false);
			trw->move_viewport_to_show_all(viewport);
		}
	}
	fclose(file);
	return load_answer;
}




bool VikFile::save(LayerAggregate * top_layer, Viewport * viewport, const QString & file_full_path)
{
	QString full_path;
	if (file_full_path.left(7) == "file://") {
		full_path = file_full_path.right(file_full_path.size() - 7);
	} else {
		full_path = file_full_path;
	}
	qDebug() << "DD: VikFile: load: saving to file" << full_path;

	FILE * file = fopen(full_path.toUtf8().constData(), "w");
	if (!file) {
		return false;
	}

	/* Enable relative paths in .vik files to work. */
	char * cwd = g_get_current_dir();
	char * dir = g_path_get_dirname(full_path.toUtf8().constData());
	if (dir) {
		if (g_chdir(dir)) {
			qDebug() << "WW: VikFile: save: could not change directory to" << dir;
		}
		free(dir);
	}

	file_write(file, top_layer, viewport);

	/* Restore previous working directory. */
	if (cwd) {
		if (g_chdir(cwd)) {
			qDebug() << "WW: VikFile: save: could not return to directory" << dir;
		}
		free(cwd);
	}

	fclose(file);
	file = NULL;

	return true;
}




bool VikFile::export_track(Track * trk, const QString & file_full_path, SGFileType file_type, bool write_hidden)
{
	GPXWriteOptions options(false, false, write_hidden, false);
	FILE * file = fopen(file_full_path.toUtf8().constData(), "w");
	if (!file) {
		return false;
	}

	switch (file_type) {
	case SGFileType::GPX:
		/* trk defined so can set the option. */
		options.is_route = trk->type_id == "sg.trw.route";
		GPX::write_track_file(file, trk, &options);
		fclose(file);
		return true;
	default:
		qDebug() << "EE: File: Export: unexpected file type for track" << (int) file_type;
		fclose(file);
		return false;
	}
}




static bool export_to_kml(const QString & file_full_path, LayerTRW * trw)
{
	bool status = true;

	BabelOptions export_options(BabelOptionsMode::FromFile); /* TODO: ::FromFile, but no input file specified. */
	export_options.output = file_full_path;

	const KMLExportUnits units = Preferences::get_kml_export_units();
	switch (units) {
	case KMLExportUnits::Metric:
		export_options.babel_args = "-o kml,units=m";
		break;
	case KMLExportUnits::Statute:
		export_options.babel_args = "-o kml";
		break;
	case KMLExportUnits::Nautical:
		export_options.babel_args = "-o kml,units=n";
		break;
	default:
		qDebug() << "EE" PREFIX << "invalid KML Export units" << (int) units;
		status = false;
		break;
	}

	if (status) {
		status = export_options.universal_export_fn(trw, NULL, NULL);
	}

	return status;
}




/* Call it when @trk argument to VikFile::export() is NULL. */
bool VikFile::export_layer(LayerTRW * trw, const QString & file_full_path, SGFileType file_type, bool write_hidden)
{
	GPXWriteOptions options(false, false, write_hidden, false);
	FILE * file = fopen(file_full_path.toUtf8().constData(), "w");
	if (!file) {
		return false;
	}

	bool result = true;

	switch (file_type) {
	case SGFileType::GPSMAPPER:
		gpsmapper_write_file(file, trw);
		break;
	case SGFileType::GPX:
		GPX::write_file(file, trw, &options);
		break;
	case SGFileType::GPSPOINT:
		GPSPoint::write_layer(file, trw);
		break;
	case SGFileType::GEOJSON:
		result = geojson_write_file(file, trw);
		break;
	case SGFileType::KML:
		result = export_to_kml(file_full_path, trw);
		break;
	default:
		qDebug() << "EE" PREFIX << "invalid file type for non-track" << (int) file_type;
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
bool VikFile::export_(LayerTRW * trw, const QString & file_full_path, SGFileType file_type, Track * trk, bool write_hidden)
{
	if (trk) {
		return VikFile::export_track(trk, file_full_path, file_type, write_hidden);
	} else {
		return VikFile::export_layer(trw, file_full_path, file_type, write_hidden);
	}
}




bool VikFile::export_with_babel(LayerTRW * trw, const QString & output_file_full_path, const QString & output_data_format, bool tracks, bool routes, bool waypoints)
{
	BabelOptions export_options(BabelOptionsMode::FromFile); /* TODO: ::FromFile, but no input file specified, just output. */
	export_options.output = output_file_full_path;
	export_options.babel_args = QString("%1 %2 %3 -o %4")
		.arg(tracks ? "-t" : "")
		.arg(routes ? "-r" : "")
		.arg(waypoints ? "-w" : "")
		.arg(output_data_format);;

	return export_options.universal_export_fn(trw, NULL, NULL);
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
		return NULL;
	}

	static char relativeFilename[MAXPATHLEN+1];
	/* Handle DOS names that are on different drives: */
	if (currentDirectory[0] != absoluteFilename[0]) {
		/* Not on the same drive, so only absolute filename will do. */
		g_strlcpy(relativeFilename, absoluteFilename, MAXPATHLEN+1);
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

		g_strlcpy(relativeFilename, &absoluteFilename[i], MAXPATHLEN+1);
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
		return NULL;
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
