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
#include "viking.h"

#include "jpg.h"
#include "gpx.h"
#include "geojson.h"
#include "babel.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef WINDOWS
#define realpath(X,Y) _fullpath(Y,X,MAX_PATH)
#endif
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "vikgpslayer.h"
#include "file.h"
#include "misc/strtod.h"
#include "gpspoint.h"
#include "gpsmapper.h"
#include "fileutils.h"
#include "globals.h"

#define TEST_BOOLEAN(str) (! ((str)[0] == '\0' || (str)[0] == '0' || (str)[0] == 'n' || (str)[0] == 'N' || (str)[0] == 'f' || (str)[0] == 'F'))
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

static void pop(Stack **stack) {
	Stack *tmp = (*stack)->under;
	free(*stack);
	*stack = tmp;
}

static void push(Stack **stack)
{
	Stack *tmp = (Stack *) malloc(sizeof (Stack));
	tmp->under = *stack;
	*stack = tmp;
}

static bool check_magic(FILE *f, const char *magic_number, unsigned int magic_length)
{
	char magic[magic_length];
	bool rv = false;
	int8_t i;
	if (fread(magic, 1, sizeof(magic), f) == sizeof(magic) &&
	     strncmp(magic, magic_number, sizeof(magic)) == 0) {
		rv = true;
	}

	for (i = sizeof(magic)-1; i >= 0; i--) { /* the ol' pushback */
		ungetc(magic[i],f);
	}
	return rv;
}


static bool str_starts_with(const char *haystack, const char *needle, uint16_t len_needle, bool must_be_longer)
{
	if (strlen(haystack) > len_needle - (!must_be_longer) && strncasecmp(haystack, needle, len_needle) == 0) {
		return true;
	}
	return false;
}

void file_write_layer_param(FILE *f, const char *name, VikLayerParamType type, VikLayerParamData data)
{
	/* string lists are handled differently. We get a GList (that shouldn't
	 * be freed) back for get_param and if it is null we shouldn't write
	 * anything at all (otherwise we'd read in a list with an empty string,
	 * not an empty string list.
	 */
	if (type == VIK_LAYER_PARAM_STRING_LIST) {
		if (data.sl) {
			GList *iter = (GList *)data.sl;
			while (iter) {
				fprintf(f, "%s=", name);
				fprintf(f, "%s\n", (char *)(iter->data));
				iter = iter->next;
			}
		}
	} else {
		fprintf(f, "%s=", name);
		switch (type)	{
		case VIK_LAYER_PARAM_DOUBLE: {
			//          char buf[15]; /* locale independent */
			//          fprintf(f, "%s\n", (char *) g_dtostr (data.d, buf, sizeof (buf))); break;
			fprintf(f, "%f\n", data.d);
			break;
		}
		case VIK_LAYER_PARAM_UINT:
			fprintf(f, "%d\n", data.u);
			break;
		case VIK_LAYER_PARAM_INT:
			fprintf(f, "%d\n", data.i);
			break;
		case VIK_LAYER_PARAM_BOOLEAN:
			fprintf(f, "%c\n", data.b ? 't' : 'f');
			break;
		case VIK_LAYER_PARAM_STRING:
			fprintf(f, "%s\n", data.s ? data.s : "");
			break;
		case VIK_LAYER_PARAM_COLOR:
			fprintf(f, "#%.2x%.2x%.2x\n", (int)(data.c.red/256),(int)(data.c.green/256),(int)(data.c.blue/256));
			break;
		default: break;
		}
	}
}

static void write_layer_params_and_data(VikLayer *l, FILE *f)
{
	Layer * layer = (Layer *) l->layer;
	VikLayerParam *params = vik_layer_get_interface(layer->type)->params;
	VikLayerFuncGetParam get_param = vik_layer_get_interface(layer->type)->get_param;


	fprintf(f, "name=%s\n", layer->name ? layer->name : "");
	if (!layer->visible) {
		fprintf(f, "visible=f\n");
	}

	if (params && get_param) {
		VikLayerParamData data;
		uint16_t i, params_count = vik_layer_get_interface(layer->type)->params_count;
		for (i = 0; i < params_count; i++) {
			data = get_param(l, i, true);
			file_write_layer_param(f, params[i].name, params[i].type, data);
		}
	}

	layer->write_file(f);

	/* foreach param:
	   write param, and get_value, etc.
	   then run layer data, and that's it.
	*/
}

static void file_write(VikAggregateLayer *top, FILE *f, void * vp)
{
	LayerAggregate * aggregate = (LayerAggregate *) ((VikLayer *) top)->layer;
	Stack *stack = NULL;
	VikLayer *current_layer;
	struct LatLon ll;
	VikViewportDrawMode mode;
	char *modestring = NULL;

	const std::list<Layer *> * children = aggregate->get_children();

	push(&stack);
	//stack->data = children; /* kamilFIXME: fix the assignment. */
	stack->under = NULL;

	/* crazhy CRAZHY */
	vik_coord_to_latlon((VIK_VIEWPORT(vp))->port.get_center(), &ll);

	mode = VIK_VIEWPORT(vp)->port.get_drawmode();
	switch (mode) {
	case VIK_VIEWPORT_DRAWMODE_UTM:
		modestring = (char *) "utm";
		break;
	case VIK_VIEWPORT_DRAWMODE_EXPEDIA:
		modestring = (char *) "expedia";
		break;
	case VIK_VIEWPORT_DRAWMODE_MERCATOR:
		modestring = (char *) "mercator";
		break;
	case VIK_VIEWPORT_DRAWMODE_LATLON:
		modestring = (char *) "latlon";
		break;
	default:
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. mode=%d\n", mode);
	}

	fprintf(f, "#VIKING GPS Data file " VIKING_URL "\n");
	fprintf(f, "FILE_VERSION=%d\n", VIKING_FILE_VERSION);
	fprintf(f, "\nxmpp=%f\nympp=%f\nlat=%f\nlon=%f\nmode=%s\ncolor=%s\nhighlightcolor=%s\ndrawscale=%s\ndrawcentermark=%s\ndrawhighlight=%s\n",
		VIK_VIEWPORT(vp)->port.get_xmpp(), VIK_VIEWPORT(vp)->port.get_ympp(), ll.lat, ll.lon,
		modestring, VIK_VIEWPORT(vp)->port.get_background_color(),
		VIK_VIEWPORT(vp)->port.get_highlight_color(),
		VIK_VIEWPORT(vp)->port.get_draw_scale() ? "t" : "f",
		VIK_VIEWPORT(vp)->port.get_draw_centermark() ? "t" : "f",
		VIK_VIEWPORT(vp)->port.get_draw_highlight() ? "t" : "f");

	if (!aggregate->visible) {
		fprintf(f, "visible=f\n");
	}

	while (stack && stack->data) {
		current_layer = VIK_LAYER(((GList *)stack->data)->data);
		Layer * current = (Layer *) current_layer->layer;
		fprintf(f, "\n~Layer %s\n", vik_layer_get_interface(current->type)->fixed_layer_name);
		write_layer_params_and_data(current_layer, f);
		if (current->type == VIK_LAYER_AGGREGATE && !((LayerAggregate *) current)->is_empty()) {
			push(&stack);
			const std::list<Layer *> * children = ((LayerAggregate *) current)->get_children();
			// stack->data = children; /* kamilFIXME: fix the assignment. */
		} else if (current->type == VIK_LAYER_GPS && !vik_gps_layer_is_empty(VIK_GPS_LAYER(current_layer))) {
			push(&stack);
			stack->data = (void *) vik_gps_layer_get_children(VIK_GPS_LAYER(current_layer));
		} else {
			stack->data = (void *) ((GList *)stack->data)->next;
			fprintf(f, "~EndLayer\n\n");
			while (stack && (!stack->data)) {
				pop(&stack);
				if (stack) {
					stack->data = (void *) ((GList *)stack->data)->next;
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
	GList *iter = (GList *) l;
	while (iter) {
		free(iter->data);
		iter = iter->next;
	}
	g_list_free ((GList *) l);
}

static void string_list_set_param(int i, GList *list, void * *layer_and_vp)
{
	VikLayerParamData x;
	x.sl = list;
	vik_layer_set_param(VIK_LAYER(layer_and_vp[0]), i, x, layer_and_vp[1], true);
}

/**
 * Read in a Viking file and return how successful the parsing was
 * ATM this will always work, in that even if there are parsing problems
 *  then there will be no new values to override the defaults
 *
 * TODO flow up line number(s) / error messages of problems encountered...
 *
 */
static bool file_read(VikAggregateLayer *top, FILE *f, const char *dirpath, VikViewport *vp)
{
	Stack *stack = NULL;
	struct LatLon ll = { 0.0, 0.0 };
	char buffer[4096];
	char *line;
	uint16_t len;
	long line_num = 0;

	VikLayerParam *params = NULL; /* for current layer, so we don't have to keep on looking up interface */
	uint8_t params_count = 0;

	GHashTable *string_lists = g_hash_table_new(g_direct_hash,g_direct_equal);
	LayerAggregate * aggregate = (LayerAggregate *) ((VikLayer *) top)->layer;

	bool successful_read = true;

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
				int parent_type = ((Layer *) ((VikLayer *) stack->data)->layer)->type;
				if ((! stack->data) || ((parent_type != VIK_LAYER_AGGREGATE) && (parent_type != VIK_LAYER_GPS))) {
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: Layer command inside non-Aggregate Layer (type %d)\n", line_num, parent_type);
					push(&stack); /* inside INVALID layer */
					stack->data = NULL;
					continue;
				} else {
					VikLayerTypeEnum type = vik_layer_type_from_string(line+6);
					push(&stack);
					if (type == VIK_LAYER_NUM_TYPES) {
						successful_read = false;
						fprintf(stderr, "WARNING: Line %ld: Unknown type %s\n", line_num, line+6);
						stack->data = NULL;
					} else if (parent_type == VIK_LAYER_GPS) {
						stack->data = (void *) vik_gps_layer_get_a_child(VIK_GPS_LAYER(stack->under->data));
						params = vik_layer_get_interface(type)->params;
						params_count = vik_layer_get_interface(type)->params_count;
					} else {
						stack->data = (void *) vik_layer_create(type, &vp->port, false);
						params = vik_layer_get_interface(type)->params;
						params_count = vik_layer_get_interface(type)->params_count;
					}
				}
			} else if (str_starts_with(line, "EndLayer", 8, false)) {
				if (stack->under == NULL) {
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: Mismatched ~EndLayer command\n", line_num);
				} else {
					/* add any string lists we've accumulated */
					void * layer_and_vp[2];
					layer_and_vp[0] = stack->data;
					layer_and_vp[1] = vp;
					g_hash_table_foreach(string_lists, (GHFunc) string_list_set_param, layer_and_vp);
					g_hash_table_remove_all(string_lists);

					if (stack->data && stack->under->data) {
						if (((Layer *) ((VikLayer *) stack->under->data)->layer)->type == VIK_LAYER_AGGREGATE) {
							Layer * layer = (Layer *) (VIK_LAYER(stack->data))->layer;
							((LayerAggregate *) ((VikLayer *) stack->under->data)->layer)->add_layer(layer, false);
							vik_layer_post_read(VIK_LAYER(stack->data), &vp->port, true);
						} else if (((Layer *) ((VikLayer *) stack->under->data)->layer)->type == VIK_LAYER_GPS) {
							/* TODO: anything else needs to be done here ? */
						} else {
							successful_read = false;
							fprintf(stderr, "WARNING: Line %ld: EndLayer command inside non-Aggregate Layer (type %d)\n", line_num, ((Layer *) ((VikLayer *) stack->data)->layer)->type);
						}
					}
					pop(&stack);
				}
			} else if (str_starts_with(line, "LayerData", 9, false)) {
				VikLayer * vl = VIK_LAYER(stack->data);
				Layer * layer = (Layer *) vl->layer;
				int rv = layer->read_file(f, dirpath);
				if (rv == 0) {
					successful_read = false;
				} else if (rv > 0) {
					/* Success, pass. */
				} else { /* simply skip layer data over */
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

			Layer * layer = (Layer *) (VIK_LAYER(stack->data))->layer;

			if (stack->under == NULL && eq_pos == 12 && strncasecmp(line, "FILE_VERSION", eq_pos) == 0) {
				int version = strtol(line+13, NULL, 10);
				fprintf(stderr, "DEBUG: %s: reading file version %d\n", __FUNCTION__, version);
				if (version > VIKING_FILE_VERSION) {
					successful_read = false;
				}
				// However we'll still carry and attempt to read whatever we can
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "xmpp", eq_pos) == 0) { /* "hard coded" params: global & for all layer-types */
				VIK_VIEWPORT(vp)->port.set_xmpp(strtod_i8n(line+5, NULL));
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "ympp", eq_pos) == 0) {
				VIK_VIEWPORT(vp)->port.set_ympp(strtod_i8n(line+5, NULL));
			} else if (stack->under == NULL && eq_pos == 3 && strncasecmp(line, "lat", eq_pos) == 0) {
				ll.lat = strtod_i8n(line+4, NULL);
			} else if (stack->under == NULL && eq_pos == 3 && strncasecmp(line, "lon", eq_pos) == 0) {
				ll.lon = strtod_i8n(line+4, NULL);
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "utm") == 0) {
				VIK_VIEWPORT(vp)->port.set_drawmode(VIK_VIEWPORT_DRAWMODE_UTM);
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "expedia") == 0) {
				VIK_VIEWPORT(vp)->port.set_drawmode(VIK_VIEWPORT_DRAWMODE_EXPEDIA);
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "google") == 0) {
				successful_read = false;
				fprintf(stderr, _("WARNING: Draw mode '%s' no more supported\n"), "google");
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "kh") == 0) {
				successful_read = false;
				fprintf(stderr, _("WARNING: Draw mode '%s' no more supported\n"), "kh");
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "mercator") == 0) {
				VIK_VIEWPORT(vp)->port.set_drawmode(VIK_VIEWPORT_DRAWMODE_MERCATOR);
			} else if (stack->under == NULL && eq_pos == 4 && strncasecmp(line, "mode", eq_pos) == 0 && strcasecmp(line+5, "latlon") == 0) {
				VIK_VIEWPORT(vp)->port.set_drawmode(VIK_VIEWPORT_DRAWMODE_LATLON);
			} else if (stack->under == NULL && eq_pos == 5 && strncasecmp(line, "color", eq_pos) == 0) {
				VIK_VIEWPORT(vp)->port.set_background_color(line+6);
			} else if (stack->under == NULL && eq_pos == 14 && strncasecmp(line, "highlightcolor", eq_pos) == 0) {
				VIK_VIEWPORT(vp)->port.set_highlight_color(line+15);
			} else if (stack->under == NULL && eq_pos == 9 && strncasecmp(line, "drawscale", eq_pos) == 0) {
				VIK_VIEWPORT(vp)->port.set_draw_scale(TEST_BOOLEAN(line+10));
			} else if (stack->under == NULL && eq_pos == 14 && strncasecmp(line, "drawcentermark", eq_pos) == 0) {
				VIK_VIEWPORT(vp)->port.set_draw_centermark(TEST_BOOLEAN(line+15));
			} else if (stack->under == NULL && eq_pos == 13 && strncasecmp(line, "drawhighlight", eq_pos) == 0) {
				VIK_VIEWPORT(vp)->port.set_draw_highlight(TEST_BOOLEAN(line+14));

			} else if (stack->under && eq_pos == 4 && strncasecmp(line, "name", eq_pos) == 0) {
				layer->rename(line+5);

			} else if (eq_pos == 7 && strncasecmp(line, "visible", eq_pos) == 0) {
				layer->visible = TEST_BOOLEAN(line+8);

			} else if (eq_pos != -1 && stack->under) {
				bool found_match = false;

				/* go thru layer params. if len == eq_pos && starts_with jazz, set it. */
				/* also got to check for name and visible. */

				if (! params) {
					successful_read = false;
					fprintf(stderr, "WARNING: Line %ld: No options for this kind of layer\n", line_num);
					continue;
				}

				for (i = 0; i < params_count; i++) {
					if (strlen(params[i].name) == eq_pos && strncasecmp(line, params[i].name, eq_pos) == 0) {
						VikLayerParamData x;
						line += eq_pos+1;
						if (params[i].type == VIK_LAYER_PARAM_STRING_LIST) {
							GList *l = g_list_append((GList *) g_hash_table_lookup(string_lists, KINT_TO_POINTER ((int) i)),
										   g_strdup(line));
							g_hash_table_replace(string_lists, KINT_TO_POINTER ((int)i), l);
							/* add the value to a list, possibly making a new list.
							 * this will be passed to the layer when we read an ~EndLayer */
						} else {
							switch (params[i].type) {
							case VIK_LAYER_PARAM_DOUBLE:
								x.d = strtod_i8n(line, NULL);
								break;
							case VIK_LAYER_PARAM_UINT:
								x.u = strtoul(line, NULL, 10);
								break;
							case VIK_LAYER_PARAM_INT:
								x.i = strtol(line, NULL, 10);
								break;
							case VIK_LAYER_PARAM_BOOLEAN:
								x.b = TEST_BOOLEAN(line);
								break;
							case VIK_LAYER_PARAM_COLOR:
								memset(&(x.c), 0, sizeof(x.c)); /* default: black */
								gdk_color_parse(line, &(x.c));
								break;
								/* STRING or STRING_LIST -- if STRING_LIST, just set param to add a STRING */
							default: x.s = line;
							}
							vik_layer_set_param(VIK_LAYER(stack->data), i, x, vp, true);
						}
						found_match = true;
						break;
					}
				}
				if (! found_match) {
					// ATM don't flow up this issue because at least one internal parameter has changed from version 1.3
					//   and don't what to worry users about raising such issues
					// TODO Maybe hold old values here - compare the line value against them and if a match
					//       generate a different style of message in the GUI...
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
			Layer * layer = (Layer *) (VIK_LAYER(stack->data))->layer;
			((LayerAggregate *) ((VikLayer *) stack->under->data)->layer)->add_layer(layer, false);
			vik_layer_post_read(VIK_LAYER(stack->data), &vp->port, true);
		}
		pop(&stack);
	}

	if (ll.lat != 0.0 || ll.lon != 0.0) {
		VIK_VIEWPORT(vp)->port.set_center_latlon(&ll, true);
	}

	if ((!aggregate->visible) && aggregate->realized) {
		aggregate->vt->tree->set_visibility(&aggregate->iter, false);
	}

	/* delete anything we've forgotten about -- should only happen when file ends before an EndLayer */
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
  VikLayer *vl = stack->data;
  pop(&stack);
  vik_aggregate_layer_add_layer(stack->data, vl);
if "[LayerData]"
  vik_layer_data (VIK_LAYER_DATA(stack->data), f, vp);

*/

/* ---------------------------------------------------- */

static FILE *xfopen(const char *fn)
{
	if (strcmp(fn,"-") == 0) {
		return stdin;
	} else {
		return g_fopen(fn, "r");
	}
}

static void xfclose(FILE *f)
{
	if (f != stdin && f != stdout) {
		fclose(f);
		f = NULL;
	}
}

/*
 * Function to determine if a filename is a 'viking' type file
 */
bool check_file_magic_vik(const char *filename)
{
	bool result = false;
	FILE *ff = xfopen(filename);
	if (ff) {
		result = check_magic(ff, VIK_MAGIC, VIK_MAGIC_LEN);
		xfclose(ff);
	}
	return result;
}

/**
 * append_file_ext:
 *
 * Append a file extension, if not already present.
 *
 * Returns: a newly allocated string
 */
char *append_file_ext(const char *filename, VikFileType_t type)
{
	char *new_name = NULL;
	const char *ext = NULL;

	/* Select an extension */
	switch (type) {
	case FILE_TYPE_GPX:
		ext = ".gpx";
		break;
	case FILE_TYPE_KML:
		ext = ".kml";
		break;
	case FILE_TYPE_GEOJSON:
		ext = ".geojson";
		break;
	case FILE_TYPE_GPSMAPPER:
	case FILE_TYPE_GPSPOINT:
	default:
		/* Do nothing, ext already set to NULL */
		break;
	}

	/* Do */
	if (ext != NULL && ! a_file_check_ext(filename, ext)) {
		new_name = g_strconcat(filename, ext, NULL);
	} else {
		/* Simply duplicate */
		new_name = g_strdup(filename);
	}

	return new_name;
}

VikLoadType_t a_file_load(VikAggregateLayer *top, VikViewport *vp, const char *filename_or_uri)
{
	g_return_val_if_fail(vp != NULL, LOAD_TYPE_READ_FAILURE);

	char *filename = (char *)filename_or_uri;
	if (strncmp(filename, "file://", 7) == 0) {
		// Consider replacing this with:
		// filename = g_filename_from_uri (entry, NULL, NULL);
		// Since this doesn't support URIs properly (i.e. will failure if is it has %20 characters in it)
		filename = filename + 7;
		fprintf(stderr, "DEBUG: Loading file %s from URI %s\n", filename, filename_or_uri);
	}
	FILE *f = xfopen(filename);

	if (! f) {
		return LOAD_TYPE_READ_FAILURE;
	}

	VikLoadType_t load_answer = LOAD_TYPE_OTHER_SUCCESS;

	char *dirpath = g_path_get_dirname(filename);
	// Attempt loading the primary file type first - our internal .vik file:
	if (check_magic(f, VIK_MAGIC, VIK_MAGIC_LEN)) {
		if (file_read(top, f, dirpath, vp)) {
			load_answer = LOAD_TYPE_VIK_SUCCESS;
		} else {
			load_answer = LOAD_TYPE_VIK_FAILURE_NON_FATAL;
		}
	} else if (a_jpg_magic_check(filename)) {
		if (! a_jpg_load_file(top, filename, vp)) {
			load_answer = LOAD_TYPE_UNSUPPORTED_FAILURE;
		}
	} else {
		// For all other file types which consist of tracks, routes and/or waypoints,
		//  must be loaded into a new TrackWaypoint layer (hence it be created)
		bool success = true; // Detect load failures - mainly to remove the layer created as it's not required

		VikLayer *vtl = vik_layer_create(VIK_LAYER_TRW, &vp->port, false);
		((Layer *) ((VikLayer *) vtl)->layer)->rename(a_file_basename(filename));

		// In fact both kml & gpx files start the same as they are in xml
		if (a_file_check_ext(filename, ".kml") && check_magic(f, GPX_MAGIC, GPX_MAGIC_LEN)) {
			// Implicit Conversion
			ProcessOptions po = { (char *) "-i kml", filename, NULL, NULL, NULL, NULL };
			if (! (success = a_babel_convert_from(VIK_TRW_LAYER(vtl), &po, NULL, NULL, NULL))) {
				load_answer = LOAD_TYPE_GPSBABEL_FAILURE;
			}
		}
		// NB use a extension check first, as a GPX file header may have a Byte Order Mark (BOM) in it
		//    - which currently confuses our check_magic function
		else if (a_file_check_ext(filename, ".gpx") || check_magic(f, GPX_MAGIC, GPX_MAGIC_LEN)) {
			if (! (success = a_gpx_read_file(VIK_TRW_LAYER(vtl), f))) {
				load_answer = LOAD_TYPE_GPX_FAILURE;
			}
		} else {
			// Try final supported file type
			if (! (success = a_gpspoint_read_file(VIK_TRW_LAYER(vtl), f, dirpath))) {
				// Failure here means we don't know how to handle the file
				load_answer = LOAD_TYPE_UNSUPPORTED_FAILURE;
			}
		}
		free(dirpath);

		// Clean up when we can't handle the file
		if (! success) {
			// free up layer
			g_object_unref(vtl);
		} else {
			// Complete the setup from the successful load
			vik_layer_post_read(vtl, &vp->port, true);
			Layer * layer = (Layer *) vtl->layer;
			((LayerAggregate *) ((VikLayer *) top)->layer)->add_layer(layer, false);
			(VIK_TRW_LAYER(vtl))->trw->auto_set_view(&vp->port);
		}
	}
	xfclose(f);
	return load_answer;
}

bool a_file_save(VikAggregateLayer *top, void * vp, const char *filename)
{
	FILE *f;

	if (strncmp(filename, "file://", 7) == 0) {
		filename = filename + 7;
	}

	f = g_fopen(filename, "w");

	if (! f) {
		return false;
	}

	// Enable relative paths in .vik files to work
	char *cwd = g_get_current_dir();
	char *dir = g_path_get_dirname(filename);
	if (dir) {
		if (g_chdir(dir)) {
			fprintf(stderr, "WARNING: Could not change directory to %s\n", dir);
		}
		free(dir);
	}

	file_write(top, f, vp);

	// Restore previous working directory
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


/* example:
     bool is_gpx = a_file_check_ext ("a/b/c.gpx", ".gpx");
*/
bool a_file_check_ext(const char *filename, const char *fileext)
{
	g_return_val_if_fail(filename != NULL, false);
	g_return_val_if_fail(fileext && fileext[0]=='.', false);
	const char *basename = a_file_basename(filename);
	if (!basename) {
		return false;
	}

	const char * dot = strrchr(basename, '.');
	if (dot && !strcmp(dot, fileext)) {
		return true;
	}

	return false;
}

/**
 * a_file_export:
 * @vtl: The TrackWaypoint to export data from
 * @filename: The name of the file to be written
 * @file_type: Choose one of the supported file types for the export
 * @trk: If specified then only export this track rather than the whole layer
 * @write_hidden: Whether to write invisible items
 *
 * A general export command to convert from Viking TRW layer data to an external supported format.
 * The write_hidden option is provided mainly to be able to transfer selected items when uploading to a GPS
 */
bool a_file_export(VikTrwLayer *vtl, const char *filename, VikFileType_t file_type, Track * trk, bool write_hidden)
{
	GpxWritingOptions options = { false, false, write_hidden, false };
	FILE *f = g_fopen(filename, "w");
	if (f) {
		bool result = true;

		if (trk) {
			switch (file_type) {
			case FILE_TYPE_GPX:
				// trk defined so can set the option
				options.is_route = trk->is_route;
				a_gpx_write_track_file(trk, f, &options);
				break;
			default:
				fprintf(stderr, "CRITICAL: Houston, we've had a problem. file_type=%d\n", file_type);
			}
		} else {
			switch (file_type) {
			case FILE_TYPE_GPSMAPPER:
				a_gpsmapper_write_file(f, vtl->trw);
				break;
			case FILE_TYPE_GPX:
				a_gpx_write_file(vtl, f, &options);
				break;
			case FILE_TYPE_GPSPOINT:
				a_gpspoint_write_file(vtl, f);
				break;
			case FILE_TYPE_GEOJSON:
				result = a_geojson_write_file(vtl, f);
				break;
			case FILE_TYPE_KML:
				fclose(f);
				switch (a_vik_get_kml_export_units()) {
				case VIK_KML_EXPORT_UNITS_STATUTE:
					return a_babel_convert_to(vtl, NULL, "-o kml", filename, NULL, NULL);
					break;
				case VIK_KML_EXPORT_UNITS_NAUTICAL:
					return a_babel_convert_to(vtl, NULL, "-o kml,units=n", filename, NULL, NULL);
					break;
				default:
					// VIK_KML_EXPORT_UNITS_METRIC:
					return a_babel_convert_to(vtl, NULL, "-o kml,units=m", filename, NULL, NULL);
					break;
				}
				break;
			default:
				fprintf(stderr, "CRITICAL: Houston, we've had a problem. file_type=%d\n", file_type);
			}
		}
		fclose(f);
		return result;
	}
	return false;
}

/**
 * a_file_export_babel:
 */
bool a_file_export_babel(VikTrwLayer *vtl, const char *filename, const char *format,
			  bool tracks, bool routes, bool waypoints)
{
	char *args = g_strdup_printf("%s %s %s -o %s",
				     tracks ? "-t" : "",
				     routes ? "-r" : "",
				     waypoints ? "-w" : "",
				     format);
	bool result = a_babel_convert_to(vtl, NULL, args, filename, NULL, NULL);
	free(args);
	return result;
}

/**
 * Just a wrapper around realpath, which itself is platform dependent
 */
char *file_realpath(const char *path, char *real)
{
	return realpath(path, real);
}

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif
/**
 * Always return the canonical filename in a newly allocated string
 */
char *file_realpath_dup(const char *path)
{
	char real[MAXPATHLEN];

	g_return_val_if_fail(path != NULL, NULL);

	if (file_realpath(path, real)) {
		return g_strdup(real);
	}

	return g_strdup(path);
}

/**
 * Permission granted to use this code after personal correspondance
 * Slightly reworked for better cross platform use, glibisms, function rename and a compacter format
 *
 * FROM http://www.codeguru.com/cpp/misc/misc/fileanddirectorynaming/article.php/c263
 */

// GetRelativeFilename(), by Rob Fisher.
// rfisher@iee.org
// http://come.to/robfisher

// The number of characters at the start of an absolute filename.  e.g. in DOS,
// absolute filenames start with "X:\" so this value should be 3, in UNIX they start
// with "\" so this value should be 1.
#ifdef WINDOWS
#define ABSOLUTE_NAME_START 3
#else
#define ABSOLUTE_NAME_START 1
#endif

// Given the absolute current directory and an absolute file name, returns a relative file name.
// For example, if the current directory is C:\foo\bar and the filename C:\foo\whee\text.txt is given,
// GetRelativeFilename will return ..\whee\text.txt.

const char *file_GetRelativeFilename(char *currentDirectory, char *absoluteFilename)
{
	int afMarker = 0, rfMarker = 0;
	int cdLen = 0, afLen = 0;
	int i = 0;
	int levels = 0;
	static char relativeFilename[MAXPATHLEN+1];

	cdLen = strlen(currentDirectory);
	afLen = strlen(absoluteFilename);

	// make sure the names are not too long or too short
	if (cdLen > MAXPATHLEN || cdLen < ABSOLUTE_NAME_START+1 ||
	    afLen > MAXPATHLEN || afLen < ABSOLUTE_NAME_START+1) {
		return NULL;
	}

	// Handle DOS names that are on different drives:
	if (currentDirectory[0] != absoluteFilename[0]) {
		// not on the same drive, so only absolute filename will do
		g_strlcpy(relativeFilename, absoluteFilename, MAXPATHLEN+1);
		return relativeFilename;
	}

	// they are on the same drive, find out how much of the current directory
	// is in the absolute filename
	i = ABSOLUTE_NAME_START;
	while (i < afLen && i < cdLen && currentDirectory[i] == absoluteFilename[i]) {
		i++;
	}

	if (i == cdLen && (absoluteFilename[i] == G_DIR_SEPARATOR || absoluteFilename[i-1] == G_DIR_SEPARATOR)) {
		// the whole current directory name is in the file name,
		// so we just trim off the current directory name to get the
		// current file name.
		if (absoluteFilename[i] == G_DIR_SEPARATOR) {
			// a directory name might have a trailing slash but a relative
			// file name should not have a leading one...
			i++;
		}

		g_strlcpy(relativeFilename, &absoluteFilename[i], MAXPATHLEN+1);
		return relativeFilename;
	}

	// The file is not in a child directory of the current directory, so we
	// need to step back the appropriate number of parent directories by
	// using "..\"s.  First find out how many levels deeper we are than the
	// common directory
	afMarker = i;
	levels = 1;

	// count the number of directory levels we have to go up to get to the
	// common directory
	while (i < cdLen) {
		i++;
		if (currentDirectory[i] == G_DIR_SEPARATOR) {
			// make sure it's not a trailing slash
			i++;
			if (currentDirectory[i] != '\0') {
				levels++;
			}
		}
	}

	// move the absolute filename marker back to the start of the directory name
	// that it has stopped in.
	while (afMarker > 0 && absoluteFilename[afMarker-1] != G_DIR_SEPARATOR) {
		afMarker--;
	}

	// check that the result will not be too long
	if (levels * 3 + afLen - afMarker > MAXPATHLEN) {
		return NULL;
	}

	// add the appropriate number of "..\"s.
	rfMarker = 0;
	for (i = 0; i < levels; i++) {
		relativeFilename[rfMarker++] = '.';
		relativeFilename[rfMarker++] = '.';
		relativeFilename[rfMarker++] = G_DIR_SEPARATOR;
	}

	// copy the rest of the filename into the result string
	strcpy(&relativeFilename[rfMarker], &absoluteFilename[afMarker]);

	return relativeFilename;
}
/* END http://www.codeguru.com/cpp/misc/misc/fileanddirectorynaming/article.php/c263 */
