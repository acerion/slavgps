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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cstdlib>
#include <cstdio>

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
#include "globals.h"
#include "preferences.h"
#include "babel.h"
#include "tree_view_internal.h"
#include "layer_trw_track_internal.h"
#include "util.h"
#include "vikutils.h"

#ifdef K_INCLUDES
#include "babel.h"
#endif




#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif




using namespace SlavGPS;




#define PREFIX " File: "




#define VIK_MAGIC "#VIK"
#define GPX_MAGIC "<?xm"
#define VIK_MAGIC_LEN 4
#define GPX_MAGIC_LEN 4

#define VIKING_FILE_VERSION 1




typedef struct _Stack Stack;

struct _Stack {
	Stack *under;
	void * data;
};




static void pop(Stack **stack)
{
	Stack * tmp = (*stack)->under;
	free(*stack);
	*stack = tmp;
}




static void push(Stack ** stack)
{
	Stack * tmp = (Stack *) malloc(sizeof (Stack));
	tmp->under = *stack;
	*stack = tmp;
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




void SlavGPS::file_write_layer_param(FILE * file, char const * param_name, SGVariantType type, const SGVariant & data)
{
	/* String lists are handled differently. We get a QStringList (that shouldn't
	   be freed) back for get_param and if it is empty we shouldn't write
	   anything at all (otherwise we'd read in a list with an empty string,
	   not an empty string list). */
	if (type == SGVariantType::StringList) {
		for (auto iter = data.val_string_list.constBegin(); iter != data.val_string_list.constEnd(); iter++) {
			fprintf(file, "%s=", param_name);
			fprintf(file, "%s\n", (*iter).toUtf8().constData());
		}
	} else {
		fprintf(file, "%s=", param_name);
		switch (type)	{
		case SGVariantType::Double: {
			// char buf[15]; /* Locale independent. */
			// fprintf(file, "%s\n", (char *) g_dtostr (data.d, buf, sizeof (buf))); break;
			fprintf(file, "%f\n", data.val_double);
			break;
		}
		case SGVariantType::Uint:
			fprintf(file, "%u\n", data.val_uint); /* kamilkamil: in viking code the format specifier was incorrect. */
			break;

		case SGVariantType::Int:
			fprintf(file, "%d\n", data.val_int);
			break;

		case SGVariantType::Boolean:
			fprintf(file, "%c\n", data.val_bool ? 't' : 'f');
			break;

		case SGVariantType::String:
			fprintf(file, "%s\n", data.val_string.isEmpty() ? "" : data.val_string.toUtf8().constData());
			break;

		case SGVariantType::Color:
			fprintf(file, "#%.2x%.2x%.2x\n", data.val_color.red(), data.val_color.green(), data.val_color.blue());
			break;

		default:
			break;
		}
	}
}




static void write_layer_params_and_data(FILE * file, Layer const * layer)
{
	fprintf(file, "name=%s\n", layer->name.isEmpty() ? "" : layer->name.toUtf8().constData());
	if (!layer->visible) {
		fprintf(file, "visible=f\n");
	}

	SGVariant param_value;
	for (auto iter = ((Layer * ) layer)->get_interface()->parameter_specifications.begin(); iter != ((Layer * ) layer)->get_interface()->parameter_specifications.end(); iter++) { /* TODO: get rid of cast. */

		/* Get current, per-layer-instance value of parameter. Refer to the parameter by its id ((*iter)->first). */
		param_value = ((Layer * ) layer)->get_param_value(iter->first, true); /* TODO: get rid of cast. */
		file_write_layer_param(file, iter->second->name, iter->second->type_id, param_value);
	}

	layer->write_layer_data(file);

	/* foreach param:
	   write param, and get_value, etc.
	   then run layer data, and that's it.
	*/
}




static void file_write(FILE * file, LayerAggregate * parent_layer, Viewport * viewport)
{
	LayerAggregate * aggregate = parent_layer;

	/* Crazhy CRAZHY. */
	const LatLon lat_lon = viewport->get_center()->get_latlon();

	const QString mode_id_string = ViewportDrawModes::get_id_string(viewport->get_drawmode());

	char bg_color_string[8] = { 0 };
	SGUtils::color_to_string(bg_color_string, sizeof (bg_color_string), viewport->get_background_color());

	char hl_color_string[8] = { 0 };
	SGUtils::color_to_string(hl_color_string, sizeof (hl_color_string), viewport->get_highlight_color());

	fprintf(file, "#VIKING GPS Data file " VIKING_URL "\n");
	fprintf(file, "FILE_VERSION=%d\n", VIKING_FILE_VERSION);
	fprintf(file, "\nxmpp=%f\nympp=%f\nlat=%f\nlon=%f\nmode=%s\ncolor=%s\nhighlightcolor=%s\ndrawscale=%s\ndrawcentermark=%s\ndrawhighlight=%s\n",
		viewport->get_xmpp(), viewport->get_ympp(), lat_lon.lat, lat_lon.lon,
		mode_id_string.toUtf8().constData(),
		bg_color_string,
		hl_color_string,
		viewport->get_scale_visibility() ? "t" : "f",
		viewport->get_center_mark_visibility() ? "t" : "f",
		viewport->get_highlight_usage() ? "t" : "f");

	if (!aggregate->visible) {
		fprintf(file, "visible=f\n");
	}


	std::list<Layer const *> * children = aggregate->get_children();
	Stack * aggregates = NULL;
	push(&aggregates);
	aggregates->data = (void *) children;
	aggregates->under = NULL;

	while (aggregates && aggregates->data && ((std::list<Layer const *> *) aggregates->data)->size()) {
		Layer * current = (Layer *) ((std::list<Layer const *> *) aggregates->data)->front(); /* kamilTODO: remove cast. */
		fprintf(file, "\n~Layer %s\n", current->get_type_id_string().toUtf8().constData());
		write_layer_params_and_data(file, current);
		if (current->type == LayerType::AGGREGATE && !((LayerAggregate *) current)->is_empty()) {
			push(&aggregates);
			std::list<Layer const *> * children_ = ((LayerAggregate *) current)->get_children();
			aggregates->data = (void *) children_;
		} else if (current->type == LayerType::GPS && !((LayerGPS *) current)->is_empty()) {
			push(&aggregates);
			std::list<Layer const *> * children_ = ((LayerGPS *) current)->get_children();
			aggregates->data = (void *) children_;
		} else {
			((std::list<Layer const *> *) aggregates->data)->pop_front();
			fprintf(file, "~EndLayer\n\n");
			while (aggregates && (!aggregates->data)) {
				pop(&aggregates);
				if (aggregates) {
					((std::list<Layer const *> *) aggregates->data)->pop_front();
					fprintf(file, "~EndLayer\n\n");
				}
			}
		}
	}
	/*
	  get vikaggregatelayer's children (?)
	  foreach write ALL params,
	  then layer data (IF function exists)
	  then endlayer

	  impl:
	  stack of layers (LIST) we are working on
	  when layer->next == NULL ...
	  we move on.
	*/
}




static void string_list_delete(void * key, void * l, void * user_data)
{
#ifdef K_TODO
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




/**
 * Read in a Viking file and return how successful the parsing was.
 * ATM this will always work, in that even if there are parsing problems
 * then there will be no new values to override the defaults.
 *
 * TODO flow up line number(s) / error messages of problems encountered...
 */
static bool file_read(FILE * file, LayerAggregate * parent_layer, const char * dirpath, Viewport * viewport)
{
	LatLon latlon;
	char buffer[4096];
	long line_num = 0;

	ParameterSpecification * param_specs = NULL; /* For current layer, so we don't have to keep on looking up interface. */
	uint8_t param_specs_count = 0;

	GHashTable * string_lists = g_hash_table_new(g_direct_hash, g_direct_equal);
	LayerAggregate * aggregate = parent_layer;

	bool successful_read = true;

	Stack * stack = NULL;
	push(&stack);
	stack->under = NULL;
	stack->data = (void *) parent_layer;

	while (fgets(buffer, sizeof (buffer), file))  {
		line_num++;

		char * line = buffer;
		while (*line == ' ' || *line =='\t') {
			line++;
		}

		if (line[0] == '#') {
			continue;
		}


		size_t len = strlen(line);
		if (len > 0 && line[len-1] == '\n') {
			line[--len] = '\0';
		}

		if (len > 0 && line[len-1] == '\r') {
			line[--len] = '\0';
		}

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
				LayerType parent_type = ((Layer *) stack->data)->type;
				if ((! stack->data) || ((parent_type != LayerType::AGGREGATE)
							&& (parent_type != LayerType::GPS))) {
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: Layer command inside non-Aggregate Layer (type %d)\n", line_num, (int) parent_type);
					push(&stack); /* Inside INVALID layer. */
					stack->data = NULL;
					continue;
				} else {
					LayerType layer_type = Layer::type_from_type_id_string(QString(line + 6));
					push(&stack);
					if (layer_type == LayerType::NUM_TYPES) {
						successful_read = false;
						fprintf(stderr, "WARNING: Line %ld: Unknown type %s\n", line_num, line+6);
						stack->data = NULL;
					} else if (parent_type == LayerType::GPS) {
						LayerGPS * g = (LayerGPS *) stack->under->data;
						stack->data = (void *) g->get_a_child();
						param_specs = Layer::get_interface(layer_type)->parameters_c;
						param_specs_count = Layer::get_interface(layer_type)->parameter_specifications.size();

					} else { /* Any other LayerType::X type. */
						Layer * layer = Layer::construct_layer(layer_type, viewport);
						stack->data = (void *) layer;
						param_specs = Layer::get_interface(layer_type)->parameters_c;
						param_specs_count = Layer::get_interface(layer_type)->parameter_specifications.size();
					}
				}
			} else if (str_starts_with(line, "EndLayer", 8, false)) {
				qDebug() << "DD: File: Read: encountered end of Layer:" << line;
				if (stack->under == NULL) {
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: Mismatched ~EndLayer command\n", line_num);
				} else {
					/* Add any string lists we've accumulated. */
					g_hash_table_foreach(string_lists, (GHFunc) string_list_set_param, (Layer *) stack->data);
					g_hash_table_remove_all(string_lists);

					if (stack->data && stack->under->data) {
						if (((Layer *) stack->under->data)->type == LayerType::AGGREGATE) {
							Layer * layer = (Layer *) stack->data;
							((LayerAggregate *) stack->under->data)->add_layer(layer, false);
							layer->add_children_to_tree();
							layer->post_read(viewport, true);
						} else if (((Layer *) stack->under->data)->type == LayerType::GPS) {
							/* TODO: anything else needs to be done here? */
						} else {
							successful_read = false;
							fprintf(stderr, "WARNING: Line %ld: EndLayer command inside non-Aggregate Layer (type %d)\n", line_num, (int) ((Layer *) stack->data)->type);
						}
					}
					pop(&stack);
				}
			} else if (str_starts_with(line, "LayerData", 9, false)) {
				qDebug() << "DD: File: Read: encountered begin of LayerData:" << line;
				Layer * layer = (Layer *) stack->data;
				int rv = layer->read_layer_data(file, dirpath);
				if (rv == 0) {
					qDebug() << "DD: File: Read: LayerData read unsuccessfully";
					successful_read = false;
				} else if (rv > 0) {
					qDebug() << "DD: File: Read: LayerData read successfully";
					/* Success, pass. */
				} else { /* Simply skip layer data over. */
					while (fgets(buffer, sizeof (buffer), file)) {
						qDebug() << "DD: File: Read: skipping over layer data:" << QString(line).left(20);
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
					continue;
				}
			} else {
				successful_read = false;
				fprintf(stderr, "WARNING: Line %ld: Unknown tilde command\n", line_num);
			}
		} else {
			int32_t eq_pos = -1;
			uint16_t i;
			if (! stack->data) {
				continue;
			}

			for (i = 0; i < len; i++) {
				if (line[i] == '=') {
					eq_pos = i;
				}
			}

			Layer * layer = (Layer *) stack->data;

			if (stack->under == NULL && eq_pos == 12 && strncasecmp(line, "FILE_VERSION", eq_pos) == 0) {
				int version = strtol(line+13, NULL, 10);
				fprintf(stderr, "DEBUG: %s: reading file version %d\n", __FUNCTION__, version);
				if (version > VIKING_FILE_VERSION) {
					successful_read = false;
				}
				/* However we'll still carry and attempt to read whatever we can. */
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "xmpp", eq_pos) == 0) { /* "hard coded" params: global & for all layer-types */
				viewport->set_xmpp(strtod_i8n(line+5, NULL));
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "ympp", eq_pos) == 0) {
				viewport->set_ympp(strtod_i8n(line+5, NULL));
			} else if (stack->under == NULL && eq_pos == 3 && strncasecmp(line, "lat", eq_pos) == 0) {
				latlon.lat = strtod_i8n(line+4, NULL);
			} else if (stack->under == NULL && eq_pos == 3 && strncasecmp(line, "lon", eq_pos) == 0) {
				latlon.lon = strtod_i8n(line+4, NULL);

			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0) {
				if (!ViewportDrawModes::set_draw_mode_from_file(viewport, line + 5)) {
					successful_read = false;
				}

			} else if (stack->under == NULL && eq_pos == 5 && strncasecmp(line, "color", eq_pos) == 0) {
				viewport->set_background_color(QString(line+6));
			} else if (stack->under == NULL && eq_pos == 14 && strncasecmp(line, "highlightcolor", eq_pos) == 0) {
				viewport->set_highlight_color(QString(line+15));
			} else if (stack->under == NULL && eq_pos == 9 && strncasecmp(line, "drawscale", eq_pos) == 0) {
				viewport->set_scale_visibility(TEST_BOOLEAN(line+10));
			} else if (stack->under == NULL && eq_pos == 14 && strncasecmp(line, "drawcentermark", eq_pos) == 0) {
				viewport->set_center_mark_visibility(TEST_BOOLEAN(line+15));
			} else if (stack->under == NULL && eq_pos == 13 && strncasecmp(line, "drawhighlight", eq_pos) == 0) {
				viewport->set_highlight_usage(TEST_BOOLEAN(line+14));

			} else if (stack->under && eq_pos == 4 && strncasecmp(line, "name", eq_pos) == 0) {
				layer->set_name(QString(line+5));

			} else if (eq_pos == 7 && strncasecmp(line, "visible", eq_pos) == 0) {
				layer->visible = TEST_BOOLEAN(line+8);

			} else if (eq_pos != -1 && stack->under) {
				bool found_match = false;

				/* Go though layer params. If len == eq_pos && starts_with jazz, set it. */
				/* Also got to check for name and visible. */

				if (!param_specs) {
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: No options for this kind of layer\n", line_num);
					continue;
				}

				for (i = 0; i < param_specs_count; i++) {

					const ParameterSpecification * param_spec = &param_specs[i];

					if (strlen(param_spec->name) == eq_pos && strncasecmp(line, param_spec->name, eq_pos) == 0) {

						line += eq_pos+1;
						if (param_spec->type_id == SGVariantType::StringList) {
							GList *l = g_list_append((GList *) g_hash_table_lookup(string_lists, KINT_TO_POINTER ((int) i)),
										   g_strdup(line));
							g_hash_table_replace(string_lists, KINT_TO_POINTER ((int)i), l);
							/* Aadd the value to a list, possibly making a new list.
							   This will be passed to the layer when we read an ~EndLayer. */
						} else {
							SGVariant new_val;
							switch (param_spec->type_id) {
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
							Layer * l_a_y_e_r = (Layer *) stack->data;
							qDebug() << "DD: File: Read: setting value of parameter" << param_spec->name << "of layer" << layer->name;
							l_a_y_e_r->set_param_value(i, new_val, true);
						}
						found_match = true;
						break;
					}
				}
				if (! found_match) {
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
	}

	while (stack) {
		if (stack->under && stack->under->data && stack->data){
			Layer * layer = (Layer *) stack->data;
			qDebug() << "DD: Read File: will call Aggregate Layer's add_layer()";
			((LayerAggregate *) stack->under->data)->add_layer(layer, false);
			qDebug() << "DD: Read File: will call child layer's post_read()";
			layer->post_read(viewport, true);
		}
		pop(&stack);
	}

	if (latlon.lat != 0.0 || latlon.lon != 0.0) { /* TODO: is this condition correct? Isn't 0.0/0.0 a correct coordinate? */
		viewport->set_center_from_latlon(latlon, true);
	}

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
  Layer * layer = stack->data;
  pop(&stack);
  vik_aggregate_layer_add_layer(stack->data, layer);
if "[LayerData]"
  vik_layer_data (VIK_LAYER_DATA(stack->data), f, viewport);

*/

/* ---------------------------------------------------- */

static FILE * xfopen(char const * fn)
{
	if (strcmp(fn,"-") == 0) {
		return stdin;
	} else {
		return fopen(fn, "r");
	}
}




static void xfclose(FILE * f)
{
	if (f != stdin && f != stdout) {
		fclose(f);
		f = NULL;
	}
}




bool VikFile::has_vik_file_magic(const QString & file_full_path)
{
	bool result = false;
	FILE * ff = fopen(file_full_path.toUtf8().constData(), "r");
	if (ff) {
		result = check_magic(ff, VIK_MAGIC, VIK_MAGIC_LEN);
		fclose(ff);
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
		if (file_read(file, parent_layer, dirpath, viewport)) {
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

		LayerTRW * layer = new LayerTRW();
		layer->set_coord_mode(viewport->get_coord_mode());
		layer->set_name(FileUtils::get_base_name(full_path));

		/* In fact both kml & gpx files start the same as they are in xml. */
		if (FileUtils::has_extension(full_path, ".kml") && check_magic(file, GPX_MAGIC, GPX_MAGIC_LEN)) {
			/* Implicit Conversion. */
			ProcessOptions po("-i kml", full_path, NULL, NULL);
			if (! (success = a_babel_convert_from(layer, &po, NULL, NULL, NULL))) {
				load_answer = FileLoadResult::GPSBABEL_FAILURE;
			}
		}
		/* NB use a extension check first, as a GPX file header may have a Byte Order Mark (BOM) in it
		   - which currently confuses our check_magic function. */
		else if (FileUtils::has_extension(full_path, ".gpx") || check_magic(file, GPX_MAGIC, GPX_MAGIC_LEN)) {
			if (! (success = GPX::read_file(file, layer))) {
				load_answer = FileLoadResult::GPX_FAILURE;
			}
		} else {
			/* Try final supported file type. */
			if (! (success = a_gpspoint_read_file(file, layer, dirpath))) {
				/* Failure here means we don't know how to handle the file. */
				load_answer = FileLoadResult::UNSUPPORTED_FAILURE;
			}
		}
		free(dirpath);

		/* Clean up when we can't handle the file. */
		if (!success) {
			/* free up layer. */
			layer->unref();
		} else {
			/* Complete the setup from the successful load. */
			layer->post_read(viewport, true);
			parent_layer->add_layer(layer, false);
			layer->auto_set_view(viewport);
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
		a_gpspoint_write_file(file, trw);
		break;
	case SGFileType::GEOJSON:
		result = geojson_write_file(file, trw);
		break;
	case SGFileType::KML:
		fclose(file);
		switch (Preferences::get_kml_export_units()) {
		case VIK_KML_EXPORT_UNITS_STATUTE:
			return a_babel_convert_to(trw, NULL, "-o kml", file_full_path, NULL, NULL);
			break;
		case VIK_KML_EXPORT_UNITS_NAUTICAL:
			return a_babel_convert_to(trw, NULL, "-o kml,units=n", file_full_path, NULL, NULL);
			break;
		default:
			/* VIK_KML_EXPORT_UNITS_METRIC: */
			return a_babel_convert_to(trw, NULL, "-o kml,units=m", file_full_path, NULL, NULL);
			break;
		}
		break;
	default:
		qDebug() << "EE: File: Export: unexpected file type for non-track" << (int) file_type;
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




bool VikFile::export_with_babel(LayerTRW * trw, const QString & full_output_file_path, const QString & output_file_type, bool tracks, bool routes, bool waypoints)
{
	const QString babel_args = QString("%1 %2 %3 -o %4")
		.arg(tracks ? "-t" : "")
		.arg(routes ? "-r" : "")
		.arg(waypoints ? "-w" : "")
		.arg(output_file_type);

	return a_babel_convert_to(trw, NULL, babel_args, full_output_file_path, NULL, NULL);
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
