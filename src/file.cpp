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
#ifdef K
#include "babel.h"
#include "misc/strtod.h"
#endif

#include "file.h"
#include "gpx.h"
#include "fileutils.h"
#include "globals.h"
#include "preferences.h"
#include "babel.h"
#include "tree_view_internal.h"
#include "layer_trw_track_internal.h"
#include "util.h"
#include "vikutils.h"




#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif




using namespace SlavGPS;




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




void SlavGPS::file_write_layer_param(FILE * f, char const * param_name, SGVariantType type, const SGVariant & data)
{
	/* String lists are handled differently. We get a QStringList (that shouldn't
	   be freed) back for get_param and if it is empty we shouldn't write
	   anything at all (otherwise we'd read in a list with an empty string,
	   not an empty string list). */
	if (type == SGVariantType::STRING_LIST) {
		for (auto iter = data.sl.constBegin(); iter != data.sl.constEnd(); iter++) {
			fprintf(f, "%s=", param_name);
			fprintf(f, "%s\n", (*iter).toUtf8().constData());
		}
	} else {
		fprintf(f, "%s=", param_name);
		switch (type)	{
		case SGVariantType::DOUBLE: {
			// char buf[15]; /* Locale independent. */
			// fprintf(f, "%s\n", (char *) g_dtostr (data.d, buf, sizeof (buf))); break;
			fprintf(f, "%f\n", data.d);
			break;
		}
		case SGVariantType::UINT:
			fprintf(f, "%d\n", data.u);
			break;
		case SGVariantType::INT:
			fprintf(f, "%d\n", data.i);
			break;
		case SGVariantType::BOOLEAN:
			fprintf(f, "%c\n", data.b ? 't' : 'f');
			break;
		case SGVariantType::STRING:
			fprintf(f, "%s\n", data.s.isEmpty() ? "" : data.s.toUtf8().constData());
			break;
		case SGVariantType::COLOR:
			fprintf(f, "#%.2x%.2x%.2x\n", (int)(data.c.r/256),(int)(data.c.g/256),(int)(data.c.b/256));
			break;
		default: break;
		}
	}
}




static void write_layer_params_and_data(Layer const * layer, FILE * f)
{
	fprintf(f, "name=%s\n", layer->name.isEmpty() ? "" : layer->name.toUtf8().constData());
	if (!layer->visible) {
		fprintf(f, "visible=f\n");
	}

	SGVariant param_value;
	for (auto iter = ((Layer * ) layer)->get_interface()->parameters.begin(); iter != ((Layer * ) layer)->get_interface()->parameters.end(); iter++) { /* TODO: get rid of cast. */

		/* Get current, per-layer-instance value of parameter. Refer to the parameter by its id ((*iter)->first). */
		param_value = ((Layer * ) layer)->get_param_value(iter->first, true); /* TODO: get rid of cast. */
		file_write_layer_param(f, iter->second->name, iter->second->type, param_value);
	}

	layer->write_file(f);

	/* foreach param:
	   write param, and get_value, etc.
	   then run layer data, and that's it.
	*/
}




static void file_write(LayerAggregate * top, FILE * f, Viewport * viewport)
{
	LayerAggregate * aggregate = top;
	char * modestring = NULL;

	/* Crazhy CRAZHY. */
	struct LatLon ll = viewport->get_center()->get_latlon();

	ViewportDrawMode mode = viewport->get_drawmode();
	switch (mode) {
	case ViewportDrawMode::UTM:
		modestring = (char *) "utm";
		break;
	case ViewportDrawMode::EXPEDIA:
		modestring = (char *) "expedia";
		break;
	case ViewportDrawMode::MERCATOR:
		modestring = (char *) "mercator";
		break;
	case ViewportDrawMode::LATLON:
		modestring = (char *) "latlon";
		break;
	default:
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. mode=%d\n", (int) mode);
	}

	fprintf(f, "#VIKING GPS Data file " VIKING_URL "\n");
	fprintf(f, "FILE_VERSION=%d\n", VIKING_FILE_VERSION);
	fprintf(f, "\nxmpp=%f\nympp=%f\nlat=%f\nlon=%f\nmode=%s\ncolor=%s\nhighlightcolor=%s\ndrawscale=%s\ndrawcentermark=%s\ndrawhighlight=%s\n",
		viewport->get_xmpp(), viewport->get_ympp(), ll.lat, ll.lon,
		modestring, viewport->get_background_color(),
		viewport->get_highlight_color(),
		viewport->get_draw_scale() ? "t" : "f",
		viewport->get_draw_centermark() ? "t" : "f",
		viewport->get_draw_with_highlight() ? "t" : "f");

	if (!aggregate->visible) {
		fprintf(f, "visible=f\n");
	}


	std::list<Layer const *> * children = aggregate->get_children();
	Stack * aggregates = NULL;
	push(&aggregates);
	aggregates->data = (void *) children;
	aggregates->under = NULL;

	while (aggregates && aggregates->data && ((std::list<Layer const *> *) aggregates->data)->size()) {
		Layer * current = (Layer *) ((std::list<Layer const *> *) aggregates->data)->front(); /* kamilTODO: remove cast. */
		fprintf(f, "\n~Layer %s\n", current->get_type_id_string().toUtf8().constData());
		write_layer_params_and_data(current, f);
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
			fprintf(f, "~EndLayer\n\n");
			while (aggregates && (!aggregates->data)) {
				pop(&aggregates);
				if (aggregates) {
					((std::list<Layer const *> *) aggregates->data)->pop_front();
					fprintf(f, "~EndLayer\n\n");
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
	/* 20071021 bugfix */
	GList * iter = (GList *) l;
	while (iter) {
		free(iter->data);
		iter = iter->next;
	}
	g_list_free ((GList *) l);
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
static bool file_read(LayerAggregate * top, FILE * f, const char * dirpath, Viewport * viewport)
{
	struct LatLon ll = { 0.0, 0.0 };
	char buffer[4096];
	char *line;
	uint16_t len;
	long line_num = 0;

	Parameter *params = NULL; /* For current layer, so we don't have to keep on looking up interface. */
	uint8_t params_count = 0;

	GHashTable *string_lists = g_hash_table_new(g_direct_hash,g_direct_equal);
	LayerAggregate * aggregate = top;

	bool successful_read = true;

	Stack *stack = NULL;
	push(&stack);
	stack->under = NULL;
	stack->data = (void *) top;

	while (fgets (buffer, 4096, f))  {
		line_num++;

		line = buffer;
		while (*line == ' ' || *line =='\t') {
			line++;
		}

		if (line[0] == '#') {
			continue;
		}


		len = strlen(line);
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
			line++; len--;
			if (*line == '\0') {
				continue;
			} else if (str_starts_with(line, "Layer ", 6, true)) {
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
						params = Layer::get_interface(layer_type)->parameters_c;
						params_count = Layer::get_interface(layer_type)->parameters.size();

					} else { /* Any other LayerType::X type. */
						Layer * layer = Layer::construct_layer(layer_type, viewport);
						stack->data = (void *) layer;
						params = Layer::get_interface(layer_type)->parameters_c;
						params_count = Layer::get_interface(layer_type)->parameters.size();
					}
				}
			} else if (str_starts_with(line, "EndLayer", 8, false)) {
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
				Layer * layer = (Layer *) stack->data;
				int rv = layer->read_file(f, dirpath);
				if (rv == 0) {
					successful_read = false;
				} else if (rv > 0) {
					/* Success, pass. */
				} else { /* Simply skip layer data over. */
					while (fgets(buffer, 4096, f)) {
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
#ifdef K
				viewport->set_xmpp(strtod_i8n(line+5, NULL));
#else
				viewport->set_xmpp(strtod(line+5, NULL));
#endif
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "ympp", eq_pos) == 0) {
#ifdef K
				viewport->set_ympp(strtod_i8n(line+5, NULL));
#else
				viewport->set_ympp(strtod(line+5, NULL));
#endif
			} else if (stack->under == NULL && eq_pos == 3 && strncasecmp(line, "lat", eq_pos) == 0) {
#ifdef K
				ll.lat = strtod_i8n(line+4, NULL);
#else
				ll.lat = strtod(line+4, NULL);
#endif
			} else if (stack->under == NULL && eq_pos == 3 && strncasecmp(line, "lon", eq_pos) == 0) {
#ifdef K
				ll.lon = strtod_i8n(line+4, NULL);
#else
				ll.lon = strtod(line+4, NULL);
#endif
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "utm") == 0) {
				viewport->set_drawmode(ViewportDrawMode::UTM);
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "expedia") == 0) {
				viewport->set_drawmode(ViewportDrawMode::EXPEDIA);
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "google") == 0) {
				successful_read = false;
				fprintf(stderr, _("WARNING: Draw mode '%s' no more supported\n"), "google");
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "kh") == 0) {
				successful_read = false;
				fprintf(stderr, _("WARNING: Draw mode '%s' no more supported\n"), "kh");
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "mercator") == 0) {
				viewport->set_drawmode(ViewportDrawMode::MERCATOR);
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "latlon") == 0) {
				viewport->set_drawmode(ViewportDrawMode::LATLON);
			} else if (stack->under == NULL && eq_pos == 5 && strncasecmp(line, "color", eq_pos) == 0) {
				viewport->set_background_color(line+6);
			} else if (stack->under == NULL && eq_pos == 14 && strncasecmp(line, "highlightcolor", eq_pos) == 0) {
				viewport->set_highlight_color(line+15);
			} else if (stack->under == NULL && eq_pos == 9 && strncasecmp(line, "drawscale", eq_pos) == 0) {
				viewport->set_draw_scale(TEST_BOOLEAN(line+10));
			} else if (stack->under == NULL && eq_pos == 14 && strncasecmp(line, "drawcentermark", eq_pos) == 0) {
				viewport->set_draw_centermark(TEST_BOOLEAN(line+15));
			} else if (stack->under == NULL && eq_pos == 13 && strncasecmp(line, "drawhighlight", eq_pos) == 0) {
				viewport->set_draw_with_highlight(TEST_BOOLEAN(line+14));

			} else if (stack->under && eq_pos == 4 && strncasecmp(line, "name", eq_pos) == 0) {
				layer->set_name(QString(line+5));

			} else if (eq_pos == 7 && strncasecmp(line, "visible", eq_pos) == 0) {
				layer->visible = TEST_BOOLEAN(line+8);

			} else if (eq_pos != -1 && stack->under) {
				bool found_match = false;

				/* Go though layer params. If len == eq_pos && starts_with jazz, set it. */
				/* Also got to check for name and visible. */

				if (! params) {
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: No options for this kind of layer\n", line_num);
					continue;
				}

				for (i = 0; i < params_count; i++) {
					if (strlen(params[i].name) == eq_pos && strncasecmp(line, params[i].name, eq_pos) == 0) {

						line += eq_pos+1;
						if (params[i].type == SGVariantType::STRING_LIST) {
							GList *l = g_list_append((GList *) g_hash_table_lookup(string_lists, KINT_TO_POINTER ((int) i)),
										   g_strdup(line));
							g_hash_table_replace(string_lists, KINT_TO_POINTER ((int)i), l);
							/* Aadd the value to a list, possibly making a new list.
							   This will be passed to the layer when we read an ~EndLayer. */
						} else {
							SGVariant new_val;
							switch (params[i].type) {
							case SGVariantType::DOUBLE:
#ifdef K
								new_val = SGVariant((double) strtod_i8n(line, NULL));
#else
								new_val = SGVariant((double) strtod(line, NULL));
#endif
								break;

							case SGVariantType::UINT:
								new_val = SGVariant((uint32_t) strtoul(line, NULL, 10));
								break;

							case SGVariantType::INT:
								new_val = SGVariant((int32_t) strtol(line, NULL, 10));
								break;

							case SGVariantType::BOOLEAN:
								new_val = SGVariant((bool) TEST_BOOLEAN(line));
								break;

							case SGVariantType::COLOR:
#ifdef K
								memset(new_val.c, 0, sizeof (new_val.c)); /* default: black */
								gdk_color_parse(line, &new_val.c);
#endif
								break;

							default:
								/* STRING or STRING_LIST -- if STRING_LIST, just set param to add a STRING. */
								/* TODO: review this section. */
								new_val = SGVariant(line);
							}
							Layer * l_a_y_e_r = (Layer *) stack->data;
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
			((LayerAggregate *) stack->under->data)->add_layer(layer, false);
			layer->post_read(viewport, true);
		}
		pop(&stack);
	}

	if (ll.lat != 0.0 || ll.lon != 0.0) {
		viewport->set_center_latlon(&ll, true);
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




/*
 * Function to determine if a filename is a 'viking' type file.
 */
bool SlavGPS::check_file_magic_vik(char const * filename)
{
	bool result = false;
	FILE * ff = fopen(filename, "r");
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
	const char * ext = NULL;

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
		/* Do nothing, ext already set to NULL. */
		break;
	}

	/* Do. */
	QString new_name;
	if (ext != NULL && !a_file_check_ext(file_name, ext)) {
		new_name = file_name + ext;
	} else {
		/* Simply duplicate. */
		new_name = file_name;
	}

	return new_name;
}




VikLoadType_t SlavGPS::a_file_load(LayerAggregate * top, Viewport * viewport, char const * filename_or_uri)
{
	if (!viewport) {
		return LOAD_TYPE_READ_FAILURE;
	}

	char * filename = (char *) filename_or_uri;
	if (strncmp(filename, "file://", 7) == 0) {
		/* Consider replacing this with:
		   filename = g_filename_from_uri (entry, NULL, NULL);
		   Since this doesn't support URIs properly (i.e. will failure if is it has %20 characters in it). */
		filename = filename + 7;
		fprintf(stderr, "DEBUG: Loading file %s from URI %s\n", filename, filename_or_uri);
	}
	FILE * f = fopen(filename, "r");

	if (!f) {
		return LOAD_TYPE_READ_FAILURE;
	}

	VikLoadType_t load_answer = LOAD_TYPE_OTHER_SUCCESS;

	char * dirpath = g_path_get_dirname(filename);
	/* Attempt loading the primary file type first - our internal .vik file: */
	if (check_magic(f, VIK_MAGIC, VIK_MAGIC_LEN)) {
		if (file_read(top, f, dirpath, viewport)) {
			load_answer = LOAD_TYPE_VIK_SUCCESS;
		} else {
			load_answer = LOAD_TYPE_VIK_FAILURE_NON_FATAL;
		}
	} else if (jpg_magic_check(filename)) {
		if (!jpg_load_file(top, filename, viewport)) {
			load_answer = LOAD_TYPE_UNSUPPORTED_FAILURE;
		}
	} else {
		/* For all other file types which consist of tracks, routes and/or waypoints,
		   must be loaded into a new TrackWaypoint layer (hence it be created). */
		bool success = true; /* Detect load failures - mainly to remove the layer created as it's not required. */

		LayerTRW * layer = new LayerTRW();
		layer->set_coord_mode(viewport->get_coord_mode());
		layer->set_name(file_basename(filename));

		/* In fact both kml & gpx files start the same as they are in xml. */
		if (a_file_check_ext(filename, ".kml") && check_magic(f, GPX_MAGIC, GPX_MAGIC_LEN)) {
			/* Implicit Conversion. */
			ProcessOptions po((char *) "-i kml", filename, NULL, NULL); /* kamil FIXME: memory leak through these pointers? */
			if (! (success = a_babel_convert_from(layer, &po, NULL, NULL, NULL))) {
				load_answer = LOAD_TYPE_GPSBABEL_FAILURE;
			}
		}
		/* NB use a extension check first, as a GPX file header may have a Byte Order Mark (BOM) in it
		   - which currently confuses our check_magic function. */
		else if (a_file_check_ext(filename, ".gpx") || check_magic(f, GPX_MAGIC, GPX_MAGIC_LEN)) {
			if (! (success = a_gpx_read_file(layer, f))) {
				load_answer = LOAD_TYPE_GPX_FAILURE;
			}
		} else {
			/* Try final supported file type. */
			if (! (success = a_gpspoint_read_file(layer, f, dirpath))) {
				/* Failure here means we don't know how to handle the file. */
				load_answer = LOAD_TYPE_UNSUPPORTED_FAILURE;
			}
		}
		free(dirpath);

		/* Clean up when we can't handle the file. */
		if (! success) {
			/* free up layer. */
			layer->unref();
		} else {
			/* Complete the setup from the successful load. */
			layer->post_read(viewport, true);
			top->add_layer(layer, false);
			layer->auto_set_view(viewport);
		}
	}
	fclose(f);
	return load_answer;
}




bool SlavGPS::a_file_save(LayerAggregate * top, Viewport * viewport, char const * filename)
{
	FILE * f;

	if (strncmp(filename, "file://", 7) == 0) {
		filename = filename + 7;
	}

	f = fopen(filename, "w");

	if (!f) {
		return false;
	}

	/* Enable relative paths in .vik files to work. */
	char * cwd = g_get_current_dir();
	char * dir = g_path_get_dirname(filename);
	if (dir) {
		if (g_chdir(dir)) {
			fprintf(stderr, "WARNING: Could not change directory to %s\n", dir);
		}
		free(dir);
	}

	file_write(top, f, viewport);

	/* Restore previous working directory. */
	if (cwd) {
		if (g_chdir(cwd)) {
			fprintf(stderr, "WARNING: Could not return to directory %s\n", cwd);
		}
		free(cwd);
	}

	fclose(f);
	f = NULL;

	return true;
}




/* Example:
   bool is_gpx = a_file_check_ext ("a/b/c.gpx", ".gpx");
*/
bool SlavGPS::a_file_check_ext(const QString & file_name, char const * fileext)
{
	if (file_name.isEmpty()) {
		return false;
	}

	if (!fileext || fileext[0] != '.') {
		return false;
	}

	char const * basename = file_basename(file_name.toUtf8().constData());
	if (!basename) {
		return false;
	}

	char const * dot = strrchr(basename, '.');
	if (dot && !strcmp(dot, fileext)) {
		return true;
	}

	return false;
}




bool SlavGPS::a_file_export_track(Track * trk, const QString & file_path, SGFileType file_type, bool write_hidden)
{
	GpxWritingOptions options = { false, false, write_hidden, false };
	FILE * f = fopen(file_path.toUtf8().constData(), "w");
	if (!f) {
		return false;
	}

	switch (file_type) {
	case SGFileType::GPX:
		/* trk defined so can set the option. */
		options.is_route = trk->type_id == "sg.trw.route";
		a_gpx_write_track_file(trk, f, &options);
		fclose(f);
		return true;
		break;
	default:
		qDebug() << "EE: File: Export: unexpected file type for track" << (int) file_type;
		fclose(f);
		return false;
	}
}


/* Call it when 'trk' argument to 'a_file_export()' is NULL. */
bool SlavGPS::a_file_export_layer(LayerTRW * trw, const QString & file_path, SGFileType file_type, bool write_hidden)
{
	GpxWritingOptions options = { false, false, write_hidden, false };
	FILE * f = fopen(file_path.toUtf8().constData(), "w");
	if (!f) {
		return false;
	}

	bool result = true;

	switch (file_type) {
	case SGFileType::GPSMAPPER:
		gpsmapper_write_file(f, trw);
		break;
	case SGFileType::GPX:
		a_gpx_write_file(trw, f, &options);
		break;
	case SGFileType::GPSPOINT:
		a_gpspoint_write_file(trw, f);
		break;
	case SGFileType::GEOJSON:
		result = geojson_write_file(trw, f);
		break;
	case SGFileType::KML:
		fclose(f);
		switch (Preferences::get_kml_export_units()) {
		case VIK_KML_EXPORT_UNITS_STATUTE:
			return a_babel_convert_to(trw, NULL, "-o kml", file_path, NULL, NULL);
			break;
		case VIK_KML_EXPORT_UNITS_NAUTICAL:
			return a_babel_convert_to(trw, NULL, "-o kml,units=n", file_path, NULL, NULL);
			break;
		default:
			/* VIK_KML_EXPORT_UNITS_METRIC: */
			return a_babel_convert_to(trw, NULL, "-o kml,units=m", file_path, NULL, NULL);
			break;
		}
		break;
	default:
		qDebug() << "EE: File: Export: unexpected file type for non-track" << (int) file_type;
	}

	fclose(f);
	return result;
}



/**
 * @file_path: The path of the file to be written
 * @file_type: Choose one of the supported file types for the export
 * @trk: If specified then only export this track rather than the whole layer
 * @write_hidden: Whether to write invisible items
 *
 * A general export command to convert from Viking TRW layer data to an external supported format.
 * The write_hidden option is provided mainly to be able to transfer selected items when uploading to a GPS.
 */
bool SlavGPS::a_file_export(LayerTRW * trw, const QString & file_path, SGFileType file_type, Track * trk, bool write_hidden)
{
	if (trk) {
		return a_file_export_track(trk, file_path, file_type, write_hidden);
	} else {
		return a_file_export_layer(trw, file_path, file_type, write_hidden);
	}
}




bool SlavGPS::a_file_export_babel(LayerTRW * trw, const QString & output_file_path, const QString & output_file_type, bool tracks, bool routes, bool waypoints)
{
	const QString babel_args = QString("%1 %2 %3 -o %4")
		.arg(tracks ? "-t" : "")
		.arg(routes ? "-r" : "")
		.arg(waypoints ? "-w" : "")
		.arg(output_file_type);

	return a_babel_convert_to(trw, NULL, babel_args, output_file_path, NULL, NULL);
}




/**
 * Just a wrapper around realpath, which itself is platform dependent.
 */
char * SlavGPS::file_realpath(char const * path, char * real)
{
	return realpath(path, real);
}




/**
 * Always return the canonical filename in a newly allocated string.
 */
char * SlavGPS::file_realpath_dup(char const * path)
{
	char real[MAXPATHLEN];

	if (!path) {
		return NULL;
	}

	if (file_realpath(path, real)) {
		return g_strdup(real);
	}

	return g_strdup(path);
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
