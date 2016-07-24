/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005, Alex Foobarian <foobarian@gmail.com>
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

#include <string.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gi18n.h>

#include "viking.h"
#include "viktreeview.h"
#include "clipboard.h"
#include "dialog.h"
#include "viktrwlayer.h"
#include "globals.h"

using namespace SlavGPS;


typedef struct {
	void * clipboard;
	int pid;
	VikClipboardDataType type;
	int subtype;
	LayerType layer_type;
	unsigned int len;
	char *text;
	uint8_t data[0];
} vik_clipboard_t;

static GtkTargetEntry target_table[] = {
	{ (char *) "application/viking", 0, 0 },
	{ (char *) "STRING", 0, 1 },
};

/*****************************************************************
 ** functions which send to the clipboard client (we are owner) **
 *****************************************************************/

static void clip_get(GtkClipboard * c, GtkSelectionData * selection_data, unsigned int info, void * p)
{
	vik_clipboard_t * vc = (vik_clipboard_t *) p;
	if (info == 0) {
		// Viking Data Type
		//    fprintf(stdout, "clip_get: vc = %p, size = %d\n", vc, sizeof(*vc) + vc->len);
		gtk_selection_data_set(selection_data, gtk_selection_data_get_target(selection_data), 8, (const unsigned char *) vc, sizeof(*vc) + vc->len);
	}
	if (info == 1) {
		// Should be a string, but make sure it's something
		if (vc->text) {
			gtk_selection_data_set_text(selection_data, vc->text, -1); // string text is null terminated
		}
	}

}

static void clip_clear(GtkClipboard * c, void * p)
{
	vik_clipboard_t* vc = (vik_clipboard_t*)p;
	free(vc->text);
	free(vc);
}


/**************************************************************************
 ** functions which receive from the clipboard owner (we are the client) **
 **************************************************************************/

/* our own data type */
static void clip_receive_viking(GtkClipboard * c, GtkSelectionData * sd, void * p)
{
	LayersPanel * panel = (LayersPanel *) p;
	vik_clipboard_t *vc;
	if (gtk_selection_data_get_length(sd) == -1) {
		fprintf(stderr, _("WARNING: paste failed\n"));
		return;
	}
	//  fprintf(stdout, "clip receive: target = %s, type = %s\n", gdk_atom_name(gtk_selection_data_get_target(sd), gdk_atom_name(sd->type));
	//assert (!strcmp(gdk_atom_name(gtk_selection_data_get_target(sd)), target_table[0].target));

	vc = (vik_clipboard_t *) gtk_selection_data_get_data(sd);
	//  fprintf(stdout, "  sd->data = %p, sd->length = %d, vc->len = %d\n", sd->data, sd->length, vc->len);

	if (gtk_selection_data_get_length(sd) != sizeof(*vc) + vc->len) {
		fprintf(stderr, _("WARNING: wrong clipboard data size\n"));
		return;
	}

	if (vc->type == VIK_CLIPBOARD_DATA_LAYER) {
		Layer * new_layer = Layer::unmarshall(vc->data, vc->len, panel->get_viewport());
		panel->add_layer(new_layer);
	} else if (vc->type == VIK_CLIPBOARD_DATA_SUBLAYER) {
		Layer * selected = panel->get_selected();
		if (selected && selected->type == vc->layer_type) {
			selected->paste_item(vc->subtype, vc->data, vc->len);
		} else {
			a_dialog_error_msg_extra(VIK_GTK_WINDOW_FROM_WIDGET(GTK_WIDGET(panel->gob)),
						 _("The clipboard contains sublayer data for %s layers. "
						   "You must select a layer of this type to paste the clipboard data."),
						 vik_layer_get_interface(vc->layer_type)->name);
		}
	}
}



/**
 * clip_parse_latlon:
 * @text: text containing LatLon data.
 * @coord: computed coordinates.
 *
 * Utility func to handle pasted text:
 * search for N dd.dddddd W dd.dddddd, N dd° dd.dddd W dd° dd.ddddd and so forth.
 *
 * Returns: true if coordinates are set.
 */
static bool clip_parse_latlon(const char * text, struct LatLon * coord)
{
	int latdeg, londeg, latmi, lonmi;
	double lats, lons;
	double latm, lonm;
	double lat, lon;
	char *cand;
	int len, i;
	char *s = g_strdup(text);

	//  fprintf(stdout, "parsing %s\n", s);

	len = strlen(s);

	/* remove non-digits following digits; gets rid of degree symbols or whatever people use, and
	 * punctuation
	 */
	for (i=0; i<len-2; i++) {
		if (g_ascii_isdigit (s[i]) && s[i+1] != '.' && !g_ascii_isdigit (s[i+1])) {
			s[i+1] = ' ';
			if (!g_ascii_isalnum (s[i+2]) && s[i+2] != '-') {
				s[i+2] = ' ';
			}
		}
	}

	/* now try reading coordinates */
	for (i=0; i<len; i++) {
		if (s[i] == 'N' || s[i] == 'S' || g_ascii_isdigit (s[i])) {
			char latc[3] = "SN";
			char lonc[3] = "WE";
			int j, k;
			cand = s+i;

			// First try matching strings containing the cardinal directions
			for (j=0; j<2; j++) {
				for (k=0; k<2; k++) {
					// DMM
					char fmt1[] = "N %d%*[ ]%lf W %d%*[ ]%lf";
					char fmt2[] = "%d%*[ ]%lf N %d%*[ ]%lf W";
					// DDD
					char fmt3[] = "N %lf W %lf";
					char fmt4[] = "%lf N %lf W";
					// DMS
					char fmt5[] = "N%d%*[ ]%d%*[ ]%lf%*[ ]W%d%*[ ]%d%*[ ]%lf";

					// Substitute in 'N','E','S' or 'W' values for each attempt
					fmt1[0]  = latc[j];	  fmt1[13] = lonc[k];
					fmt2[11] = latc[j];	  fmt2[24] = lonc[k];
					fmt3[0]  = latc[j];	  fmt3[6]  = lonc[k];
					fmt4[4]  = latc[j];	  fmt4[10] = lonc[k];
					fmt5[0]  = latc[j];	  fmt5[23] = lonc[k];

					if (sscanf(cand, fmt1, &latdeg, &latm, &londeg, &lonm) == 4 ||
					    sscanf(cand, fmt2, &latdeg, &latm, &londeg, &lonm) == 4) {
						lat = (j*2-1) * (latdeg + latm / 60);
						lon = (k*2-1) * (londeg + lonm / 60);
						break;
					}
					if (sscanf(cand, fmt3, &lat, &lon) == 2 ||
					    sscanf(cand, fmt4, &lat, &lon) == 2) {
						lat *= (j*2-1);
						lon *= (k*2-1);
						break;
					}
					int am = sscanf(cand, fmt5, &latdeg, &latmi, &lats, &londeg, &lonmi, &lons);
					if (am == 6) {
						lat = (j*2-1) * (latdeg + latmi / 60.0 + lats / 3600.0);
						lon = (k*2-1) * (londeg + lonmi / 60.0 + lons / 3600.0);
						break;
					}
				}
				if (k!=2) {
					break;
				}
			}
			if (j!=2) {
				break;
			}

			// DMM without Cardinal directions
			if (sscanf(cand, "%d%*[ ]%lf*[ ]%d%*[ ]%lf", &latdeg, &latm, &londeg, &lonm) == 4) {
				lat = latdeg/abs(latdeg) * (abs(latdeg) + latm / 60);
				lon = londeg/abs(londeg) * (abs(londeg) + lonm / 60);
				break;
			}
			// DMS without Cardinal directions
			if (sscanf(cand, "%d%*[ ]%d%*[ ]%lf%*[ ]%d%*[ ]%d%*[ ]%lf", &latdeg, &latmi, &lats, &londeg, &lonmi, &lons) == 6) {
				lat = latdeg/abs(latdeg) * (abs(latdeg) + latm / 60.0 + lats / 3600.0);
				lon = londeg/abs(londeg) * (abs(londeg) + lonm / 60.0 + lons / 3600.0);
				break;
			}
			// Raw values
			if (sscanf(cand, "%lf %lf", &lat, &lon) == 2) {
				break;
			}
		}
	}
	free(s);

	/* did we get to the end without actually finding a coordinate? */
	if (i<len && lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0) {
		coord->lat = lat;
		coord->lon = lon;
		return true;
	}
	return false;
}

static void clip_add_wp(LayersPanel * panel, struct LatLon * coord)
{
	VikCoord vc;
	Layer * selected = panel->get_selected();

	vik_coord_load_from_latlon (&vc, VIK_COORD_LATLON, coord);

	if (selected && selected->type == LayerType::TRW) {
		((LayerTRW *) selected)->new_waypoint(gtk_window_from_layer(selected), &vc);
		((LayerTRW *) selected)->calculate_bounds_waypoints();
		selected->emit_update();
	} else {
		a_dialog_error_msg_extra(VIK_GTK_WINDOW_FROM_WIDGET(GTK_WIDGET(panel->gob)), _("In order to paste a waypoint, please select an appropriate layer to paste into."), NULL);
	}
}

static void clip_receive_text(GtkClipboard * c, const char * text, void * p)
{
	LayersPanel * panel = (LayersPanel *) p;

	fprintf(stderr, "DEBUG: got text: %s\n", text);

	Layer * selected = panel->get_selected();

	if (selected && selected->tree_view->get_editing()) {
		GtkTreeIter iter;
		if (selected->tree_view->get_selected_iter(&iter)) {
			// Try to sanitize input:
			char *name = g_strescape(text, NULL);

			selected->rename(name);
			selected->tree_view->set_name(&iter, name);
			free(name);
		}
		return;
	}

	struct LatLon coord;
	if (clip_parse_latlon(text, &coord)) {
		clip_add_wp(panel, &coord);
	}
}

static void clip_receive_html(GtkClipboard * c, GtkSelectionData * sd, void * p)
{
	LayersPanel * panel = (LayersPanel *) p;
	size_t r, w;
	GError *err = NULL;
	char *s, *span;
	int tag = 0, i;
	struct LatLon coord;

	if (gtk_selection_data_get_length(sd) == -1) {
		return;
	}

	/* - copying from Mozilla seems to give html in UTF-16. */
	if (!(s =  g_convert((char *)gtk_selection_data_get_data(sd), gtk_selection_data_get_length(sd), "UTF-8", "UTF-16", &r, &w, &err))) {
		return;
	}
	//  fprintf(stdout, "html is %d bytes long: %s\n", gtk_selection_data_get_length(sd), s);

	/* scrape a coordinate pasted from a geocaching.com page: look for a
	 * telltale tag if possible, and then remove tags
	 */
	if (!(span = g_strstr_len(s, w, "<span id=\"LatLon\">"))) {
		span = s;
	}
	for (i=0; i<strlen(span); i++) {
		char ch = span[i];
		if (ch == '<') {
			tag++;
		}
		if (tag>0) {
			span[i] = ' ';
		}
		if (ch == '>') {
			if (tag>0) tag--;
		}
	}
	if (clip_parse_latlon(span, &coord)) {
		clip_add_wp(panel, &coord);
	}

	free(s);
}

/**
 * clip_receive_targets:
 *
 * Deal with various data types a clipboard may hold.
 */
void clip_receive_targets(GtkClipboard * c, GdkAtom * a, int n, void * p)
{
	LayersPanel * panel = (LayersPanel *) p;
	int i;

	for (i=0; i<n; i++) {
		char * name = gdk_atom_name(a[i]);
		//fprintf(stdout, "  ""%s""\n", name);
		bool breaktime = false;
		if (!strcmp(name, "text/html")) {
			gtk_clipboard_request_contents(c, gdk_atom_intern("text/html", true), clip_receive_html, panel);
			breaktime = true;
		}
		if (a[i] == GDK_TARGET_STRING) {
			gtk_clipboard_request_text(c, clip_receive_text, panel);
			breaktime = true;
		}
		if (!strcmp(name, "application/viking")) {
			gtk_clipboard_request_contents(c, gdk_atom_intern("application/viking", true), clip_receive_viking, panel);
			breaktime = true;
		}

		free(name);

		if (breaktime) {
			break;
		}
  }
}

/*********************************************************************************
 ** public functions                                                            **
 *********************************************************************************/

/**
 * a_clipboard_copy_selected:
 *
 * Make a copy of selected object and associate ourselves with the clipboard.
 */
void a_clipboard_copy_selected(LayersPanel * panel)
{
	Layer * selected = panel->get_selected();
	GtkTreeIter iter;
	VikClipboardDataType type = VIK_CLIPBOARD_DATA_NONE;
	LayerType layer_type = LayerType::AGGREGATE;
	int subtype = 0;
	uint8_t *data = NULL;
	unsigned int len = 0;
	const char *name = NULL;

	if (!selected) {
		return;
	}

	if (!selected->tree_view->get_selected_iter(&iter)) {
		return;
	}
	layer_type = selected->type;

	// Since we intercept copy and paste keyboard operations, this is called even when a cell is being edited
	if (selected->tree_view->get_editing()) {
		type = VIK_CLIPBOARD_DATA_TEXT;
		//  I don't think we can access what is actually selected (internal to GTK) so we go for the name of the item
		// At least this is better than copying the layer data - which is even further away from what the user would be expecting...
		name = selected->tree_view->get_name(&iter);
		len = 0;
	} else {
		if (selected->tree_view->get_type(&iter) == VIK_TREEVIEW_TYPE_SUBLAYER) {
			type = VIK_CLIPBOARD_DATA_SUBLAYER;
			subtype = selected->tree_view->get_data(&iter);

			selected->copy_item(subtype, selected->tree_view->get_pointer(&iter), &data, &len);
			// This name is used in setting the text representation of the item on the clipboard.
			name = selected->tree_view->get_name(&iter);
		} else {
			int ilen;
			type = VIK_CLIPBOARD_DATA_LAYER;
			Layer::marshall(selected, &data, &ilen);
			len = ilen;
			name = ((Layer *) selected->tree_view->get_layer(&iter))->get_name();
		}
	}
	a_clipboard_copy(type, layer_type, subtype, len, name, data);
}

void a_clipboard_copy(VikClipboardDataType type, LayerType layer_type, int subtype, unsigned int len, const char * text, uint8_t * data)
{
	vik_clipboard_t * vc = (vik_clipboard_t *) malloc(sizeof(*vc) + len); /* kamil: + len? */
	GtkClipboard * c = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	vc->type = type;
	vc->layer_type = layer_type;
	vc->subtype = subtype;
	vc->len = len;
	vc->text = g_strdup(text);
	if (data) {
		memcpy(vc->data, data, len);
		free(data);
	}
	vc->pid = getpid();

	// Simple clipboard copy when necessary
	if (type == VIK_CLIPBOARD_DATA_TEXT) {
		gtk_clipboard_set_text(c, text, -1);
	} else {
		gtk_clipboard_set_with_data(c, target_table, G_N_ELEMENTS(target_table), clip_get, clip_clear, vc);
	}
}

/**
 * a_clipboard_paste:
 *
 * To deal with multiple data types, we first request the type of data on the clipboard,
 * and handle them in the callback.
 */
bool a_clipboard_paste(LayersPanel * panel)
{
	GtkClipboard * c = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_targets(c, clip_receive_targets, panel);
	return true;
}

/**
 *
 * Detect our own data types
 */
static void clip_determine_viking_type(GtkClipboard * c, GtkSelectionData * sd, void * p)
{
	VikClipboardDataType * vdct = (VikClipboardDataType *) p;
	// Default value
	*vdct = VIK_CLIPBOARD_DATA_NONE;
	vik_clipboard_t *vc;
	if (gtk_selection_data_get_length(sd) == -1) {
		fprintf(stderr, "WARNING: DETERMINING TYPE: length failure\n");
		return;
	}

	vc = (vik_clipboard_t *) gtk_selection_data_get_data(sd);

	if (!vc->type) {
		return;
	}

	if (vc->type == VIK_CLIPBOARD_DATA_LAYER) {
		*vdct = VIK_CLIPBOARD_DATA_LAYER;
	} else if (vc->type == VIK_CLIPBOARD_DATA_SUBLAYER) {
		*vdct = VIK_CLIPBOARD_DATA_SUBLAYER;
	} else {
		fprintf(stderr, "WARNING: DETERMINING TYPE: THIS SHOULD NEVER HAPPEN\n");
	}
}

static void clip_determine_type(GtkClipboard * c, GdkAtom * a, int n, void * p)
{
	int i;
	for (i=0; i<n; i++) {
		char *name = gdk_atom_name(a[i]);
		// fprintf(stdout, "  ""%s""\n", name);
		bool breaktime = false;
		if (!strcmp(name, "application/viking")) {
			gtk_clipboard_request_contents (c, gdk_atom_intern("application/viking", true), clip_determine_viking_type, p);
			breaktime = true;
		}

		free(name);

		if (breaktime) {
			break;
		}
	}
}

/**
 * a_clipboard_type:
 *
 * Return the type of data held in the clipboard if any
 */
VikClipboardDataType a_clipboard_type()
{
	GtkClipboard * c = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	VikClipboardDataType * vcdt = (VikClipboardDataType *) malloc(sizeof (VikClipboardDataType));

	gtk_clipboard_request_targets(c, clip_determine_type, vcdt);
	VikClipboardDataType answer = * vcdt;
	free(vcdt);
	return answer;
}
