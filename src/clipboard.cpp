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

#include <cstring>
#include <cstdlib>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib.h>

#include "tree_view.h"
#include "tree_view_internal.h"
#include "clipboard.h"
#include "dialog.h"
#include "layer_trw.h"
#include "layers_panel.h"
#include "window.h"




using namespace SlavGPS;




extern Tree * g_tree;




typedef struct {
	void * clipboard;
	int pid;
	ClipboardDataType type;
	QString type_id;    /* Type of tree item. */
	LayerType layer_type;
	unsigned int len;
	char *text;
	uint8_t data[0];
} vik_clipboard_t;

#ifdef K
static GtkTargetEntry target_table[] = {
	{ (char *) "application/viking", 0, 0 },
	{ (char *) "STRING", 0, 1 },
};
#endif

typedef int GtkClipboard;
typedef int GtkSelectionData;
typedef int GdkAtom;



/******************************************************************
 ** Functions which send to the clipboard client (we are owner). **
 ******************************************************************/




static void clip_get(GtkClipboard * c, GtkSelectionData * selection_data, unsigned int info, void * p)
{
#ifdef K
	vik_clipboard_t * vc = (vik_clipboard_t *) p;
	if (info == 0) {
		/* Viking Data Type. */
		//    fprintf(stdout, "clip_get: vc = %p, size = %d\n", vc, sizeof(*vc) + vc->len);
		gtk_selection_data_set(selection_data, gtk_selection_data_get_target(selection_data), 8, (const unsigned char *) vc, sizeof(*vc) + vc->len);
	}
	if (info == 1) {
		/* Should be a string, but make sure it's something. */
		if (vc->text) {
			gtk_selection_data_set_text(selection_data, vc->text, -1); /* String text is null terminated. */
		}
	}
#endif
}




static void clip_clear(GtkClipboard * c, void * p)
{
#ifdef K
	vik_clipboard_t* vc = (vik_clipboard_t*)p;
	free(vc->text);
	free(vc);
#endif
}




/***************************************************************************
 ** Functions which receive from the clipboard owner (we are the client). **
 ***************************************************************************/




/* Our own data type. */
static void clip_receive_viking(GtkClipboard * c, GtkSelectionData * sd, void * p)
{
#ifdef K
	LayersPanel * panel = (LayersPanel *) p;
	if (gtk_selection_data_get_length(sd) == -1) {
		qDebug() << QObject::tr("WARNING: paste failed");
		return;
	}
	//  fprintf(stdout, "clip receive: target = %s, type = %s\n", gdk_atom_name(gtk_selection_data_get_target(sd), gdk_atom_name(sd->type));
	//assert (!strcmp(gdk_atom_name(gtk_selection_data_get_target(sd)), target_table[0].target));

	vik_clipboard_t * vc = (vik_clipboard_t *) gtk_selection_data_get_data(sd);
	//  fprintf(stdout, "  sd->data = %p, sd->length = %d, vc->len = %d\n", sd->data, sd->length, vc->len);

	if (gtk_selection_data_get_length(sd) != sizeof(*vc) + vc->len) {
		qDebug() << QObject::tr("WARNING: wrong clipboard data size");
		return;
	}

	if (vc->type == ClipboardDataType::LAYER) {
		Layer * new_layer = Layer::unmarshall(vc->data, vc->len, panel->get_viewport());
		panel->add_layer(new_layer, viewport->get_coord_mode());
	} else if (vc->type == ClipboardDataType::SUBLAYER) {
		Layer * selected = panel->get_selected_layer();
		if (selected && selected->type == vc->layer_type) {
			selected->paste_sublayer(vc->sublayer, vc->data, vc->len);
		} else {
			Dialog::error(tr("The clipboard contains sublayer data for %1 layers. "
					 "You must select a layer of this type to paste the clipboard data.")
				      .arg(Layer::get_interface(vc->layer_type)->name),
				      panel->get_window());
		}
	}
#endif
}




/**
 * @text: text containing LatLon data.
 * @lat_lon: computed coordinates.
 *
 * Utility func to handle pasted text:
 * search for N dd.dddddd W dd.dddddd, N dd° dd.dddd W dd° dd.ddddd and so forth.
 *
 * Returns: true if coordinates are set.
 */
static bool clip_parse_latlon(const char * text, LatLon & lat_lon)
{
	int latdeg, londeg, latmi, lonmi;
	double lats, lons;
	double latm, lonm;
	double lat, lon;
	QString s = text;

	//  fprintf(stdout, "parsing %s\n", s);

	int len = s.size();
#ifdef K

	/* Remove non-digits following digits; gets rid of degree
	   symbols or whatever people use, and punctuation. */
	for (int i = 0; i < len - 2; i++) {
		if (g_ascii_isdigit (s[i]) && s[i + 1] != '.' && !g_ascii_isdigit (s[i + 1])) {
			s[i + 1] = ' ';
			if (!g_ascii_isalnum (s[i + 2]) && s[i + 2] != '-') {
				s[i + 2] = ' ';
			}
		}
	}

	/* Now try reading coordinates. */
	int i;
	for (i = 0; i < len; i++) {
		if (s[i] == 'N' || s[i] == 'S' || g_ascii_isdigit (s[i])) {
			char latc[3] = "SN";
			char lonc[3] = "WE";
			int j, k;
			char * cand = s + i;

			/* First try matching strings containing the cardinal directions. */
			for (j=0; j<2; j++) {
				for (k=0; k<2; k++) {
					/* DMM. */
					char fmt1[] = "N %d%*[ ]%lf W %d%*[ ]%lf";
					char fmt2[] = "%d%*[ ]%lf N %d%*[ ]%lf W";
					/* DDD. */
					char fmt3[] = "N %lf W %lf";
					char fmt4[] = "%lf N %lf W";
					/* DMS. */
					char fmt5[] = "N%d%*[ ]%d%*[ ]%lf%*[ ]W%d%*[ ]%d%*[ ]%lf";

					/* Substitute in 'N','E','S' or 'W' values for each attempt. */
					fmt1[0]  = latc[j];	  fmt1[13] = lonc[k];
					fmt2[11] = latc[j];	  fmt2[24] = lonc[k];
					fmt3[0]  = latc[j];	  fmt3[6]  = lonc[k];
					fmt4[4]  = latc[j];	  fmt4[10] = lonc[k];
					fmt5[0]  = latc[j];	  fmt5[23] = lonc[k];

					if (sscanf(cand, fmt1, &latdeg, &latm, &londeg, &lonm) == 4
					    || sscanf(cand, fmt2, &latdeg, &latm, &londeg, &lonm) == 4) {

						lat = (j*2-1) * (latdeg + latm / 60);
						lon = (k*2-1) * (londeg + lonm / 60);
						break;
					}
					if (sscanf(cand, fmt3, &lat, &lon) == 2
					    || sscanf(cand, fmt4, &lat, &lon) == 2) {

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

			/* DMM without Cardinal directions. */
			if (sscanf(cand, "%d%*[ ]%lf*[ ]%d%*[ ]%lf", &latdeg, &latm, &londeg, &lonm) == 4) {
				lat = latdeg/abs(latdeg) * (abs(latdeg) + latm / 60);
				lon = londeg/abs(londeg) * (abs(londeg) + lonm / 60);
				break;
			}
			/* DMS without Cardinal directions. */
			if (sscanf(cand, "%d%*[ ]%d%*[ ]%lf%*[ ]%d%*[ ]%d%*[ ]%lf", &latdeg, &latmi, &lats, &londeg, &lonmi, &lons) == 6) {
				lat = latdeg/abs(latdeg) * (abs(latdeg) + latm / 60.0 + lats / 3600.0);
				lon = londeg/abs(londeg) * (abs(londeg) + lonm / 60.0 + lons / 3600.0);
				break;
			}
			/* Raw values. */
			if (sscanf(cand, "%lf %lf", &lat, &lon) == 2) {
				break;
			}
		}
	}
	free(s);

	/* Did we get to the end without actually finding a coordinate? */
	if (i<len && lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0) {
		lat_lon.lat = lat;
		lat_lon.lon = lon;
		return true;
	}
#endif
	return false;
}




static void clip_add_wp(LayersPanel * panel, const LatLon & lat_lon)
{
	Layer * selected = panel->get_selected_layer();

	if (selected && selected->type == LayerType::TRW) {
		((LayerTRW *) selected)->new_waypoint(Coord(lat_lon, CoordMode::LATLON), selected->get_window());
		((LayerTRW *) selected)->get_waypoints_node().recalculate_bbox();
		selected->emit_layer_changed();
	} else {
		Dialog::error(QObject::tr("In order to paste a waypoint, please select an appropriate layer to paste into."), g_tree->tree_get_main_window());
	}

}




static void clip_receive_text(GtkClipboard * c, const char * text, void * p)
{
	LayersPanel * panel = (LayersPanel *) p;

	fprintf(stderr, "DEBUG: got text: %s\n", text);

	Layer * selected = panel->get_selected_layer();

	if (selected && selected->tree_view->is_editing_in_progress()) {

		/* Try to sanitize input: */
		char *name = g_strescape(text, NULL);

		selected->set_name(name);
		selected->tree_view->set_tree_item_name(selected->index, name);
		free(name);

		return;
	}

	LatLon lat_lon;
	if (clip_parse_latlon(text, lat_lon)) {
		clip_add_wp(panel, lat_lon);
	}
}




static void clip_receive_html(GtkClipboard * c, GtkSelectionData * sd, void * p)
{
#ifdef K
	LayersPanel * panel = (LayersPanel *) p;
	size_t r, w;
	GError *err = NULL;
	char *s, *span;
	int tag = 0;

	if (gtk_selection_data_get_length(sd) == -1) {
		return;
	}

	/* - copying from Mozilla seems to give html in UTF-16. */
	if (!(s =  g_convert((char *)gtk_selection_data_get_data(sd), gtk_selection_data_get_length(sd), "UTF-8", "UTF-16", &r, &w, &err))) {
		return;
	}
	//  fprintf(stdout, "html is %d bytes long: %s\n", gtk_selection_data_get_length(sd), s);

	/* Scrape a coordinate pasted from a geocaching.com page: look
	   for a telltale tag if possible, and then remove tags. */
	if (!(span = g_strstr_len(s, w, "<span id=\"LatLon\">"))) {
		span = s;
	}
	for (size_t i = 0; i < strlen(span); i++) {
		char ch = span[i];
		if (ch == '<') {
			tag++;
		}
		if (tag>0) {
			span[i] = ' ';
		}
		if (ch == '>') {
			if (tag>0) {
				tag--;
			}
		}
	}

	LatLon lat_lon;
	if (clip_parse_latlon(span, lat_lon)) {
		clip_add_wp(panel, lat_lon);
	}

	free(s);
#endif
}




/**
 * Deal with various data types a clipboard may hold.
 */
void clip_receive_targets(GtkClipboard * c, GdkAtom * a, int n, void * p)
{
#ifdef K
	LayersPanel * panel = (LayersPanel *) p;

	for (int i = 0; i < n; i++) {
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
#endif
}




/*********************************************************************************
 ** public functions                                                            **
 *********************************************************************************/



/**
 * Make a copy of selected object and associate ourselves with the clipboard.
 */
void Clipboard::copy_selected(LayersPanel * panel)
{

	Layer * selected = panel->get_selected_layer();
	TreeIndex index;
	ClipboardDataType type = ClipboardDataType::NONE;
	LayerType layer_type = LayerType::AGGREGATE;
	QString type_id; /* Type ID of copied tree item. */
	uint8_t *data = NULL;
	unsigned int len = 0;

	if (!selected || !selected->index.isValid()) {
		return;
	}

	layer_type = selected->type;

	QString name = selected->name; /* FIXME: look at how viking gets name in this function. */

	/* Since we intercept copy and paste keyboard operations, this is called even when a cell is being edited. */
	if (selected->tree_view->is_editing_in_progress()) {
		type = ClipboardDataType::TEXT;

		/* I don't think we can access what is actually selected (internal to GTK) so we go for the name of the item.
		   At least this is better than copying the layer data - which is even further away from what the user would be expecting... */
		len = 0;
	} else {
		TreeItem * item = selected->tree_view->get_tree_item(selected->index);
		if (item->tree_item_type == TreeItemType::SUBLAYER) {
			type = ClipboardDataType::SUBLAYER;
			selected->copy_sublayer(item, &data, &len);
		} else {
			int ilen;
			type = ClipboardDataType::LAYER;
#ifdef K
			Layer::marshall(selected, &data, &ilen);
#endif
			len = ilen;
		}
	}

	Clipboard::copy(type, layer_type, type_id, len, name, data);
}




void Clipboard::copy(ClipboardDataType type, LayerType layer_type, const QString & type_id, unsigned int len, const QString & text, uint8_t * data)
{
#ifdef K
	vik_clipboard_t * vc = (vik_clipboard_t *) malloc(sizeof(*vc) + len); /* kamil: + len? */
	GtkClipboard * c = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	vc->type = type;
	vc->layer_type = layer_type;
	vc->type_id = type_id;
	vc->len = len;
	vc->text = text;
	if (data) {
		memcpy(vc->data, data, len);
		free(data);
	}
	vc->pid = getpid();

	/* Simple clipboard copy when necessary. */
#ifdef K
	if (type == ClipboardDataType::TEXT) {
		gtk_clipboard_set_text(c, text, -1);
	} else {
		gtk_clipboard_set_with_data(c, target_table, G_N_ELEMENTS(target_table), clip_get, clip_clear, vc);
	}
#endif
#endif
}




/**
 * To deal with multiple data types, we first request the type of data on the clipboard,
 * and handle them in the callback.
 */
bool Clipboard::paste(LayersPanel * panel)
{
#ifdef K
	GtkClipboard * c = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_targets(c, clip_receive_targets, panel);
#endif
	return true;
}




/**
 * Detect our own data types
 */
static void clip_determine_viking_type(GtkClipboard * c, GtkSelectionData * sd, void * p)
{
#ifdef K
	ClipboardDataType * vdct = (ClipboardDataType *) p;
	/* Default value. */
	*vdct = ClipboardDataType::NONE;

	if (gtk_selection_data_get_length(sd) == -1) {
		fprintf(stderr, "WARNING: DETERMINING TYPE: length failure\n");
		return;
	}

	vik_clipboard_t * vc = (vik_clipboard_t *) gtk_selection_data_get_data(sd);

	if (!vc->type) {
		return;
	}

	if (vc->type == ClipboardDataType::LAYER) {
		*vdct = ClipboardDataType::LAYER;
	} else if (vc->type == ClipboardDataType::SUBLAYER) {
		*vdct = ClipboardDataType::SUBLAYER;
	} else {
		fprintf(stderr, "WARNING: DETERMINING TYPE: THIS SHOULD NEVER HAPPEN\n");
	}
#endif
}




static void clip_determine_type(GtkClipboard * c, GdkAtom * a, int n, void * p)
{
#ifdef K
	for (int i = 0; i < n; i++) {
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
#endif
}




/**
 * Return the type of data held in the clipboard if any.
 */
ClipboardDataType Clipboard::get_current_type()
{
	ClipboardDataType answer;
#ifdef K
	GtkClipboard * c = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	ClipboardDataType * vcdt = (VikClipboardDataType *) malloc(sizeof (VikClipboardDataType));

	gtk_clipboard_request_targets(c, clip_determine_type, vcdt);
	ClipboardDataType answer = *vcdt;
	free(vcdt);
#endif
	return answer;
}




void Clipboard::append_string(GByteArray * byte_array, const char * string)
{
	clipboard_size_t size = (clipboard_size_t) (string ? strlen(string) + 1 : 0);
	g_byte_array_append(byte_array, (uint8_t *) &size, sizeof (size));
	if (string) {
		g_byte_array_append(byte_array, (uint8_t *) string, size);
	}
}




void Clipboard::append_object(GByteArray * byte_array, uint8_t * obj, clipboard_size_t obj_size)
{
	clipboard_size_t size = obj_size;
	g_byte_array_append(byte_array, (uint8_t *) &size, sizeof (size));
	g_byte_array_append(byte_array, obj, size);
}




void Clipboard::append_object_with_type(GByteArray * byte_array, uint8_t * obj, clipboard_size_t obj_size, int obj_type)
{
	int type = obj_type;
	clipboard_size_t size = obj_size;

	g_byte_array_append(byte_array, (uint8_t *) &size, sizeof (size));
	g_byte_array_append(byte_array, (uint8_t *) &type, sizeof (type));
	g_byte_array_append(byte_array, obj, size);
}




clipboard_size_t Clipboard::peek_size(uint8_t * data)
{
	return (*(clipboard_size_t *) data);
}




void Clipboard::move_to_next_object(uint8_t ** data, clipboard_size_t * data_size)
{
	const clipboard_size_t this_object_size = Clipboard::peek_size(*data);

	(*data_size) -= sizeof (clipboard_size_t) + this_object_size;
	(*data) += sizeof (clipboard_size_t) + this_object_size;
}




void Clipboard::take_object(void * target, uint8_t ** data)
{
	const clipboard_size_t this_object_size = Clipboard::peek_size(*data);

	memcpy(target, (*data) + sizeof (clipboard_size_t), this_object_size);
	(*data) += sizeof (clipboard_size_t) + this_object_size;
}




QString Clipboard::take_string(uint8_t ** data)
{
	QString result;

	const clipboard_size_t len = Clipboard::peek_size(*data);
	(*data) += sizeof (len);

	if (len) {
		result = QString((char *) *data);
	} else {
		// result = "";
	}
	(*data) += len;

	return result;
}
