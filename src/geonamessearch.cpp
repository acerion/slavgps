/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Hein Ragas
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <QDebug>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

#include "dialog.h"
#include "geonamessearch.h"
#include "download.h"
#include "util.h"
#include "globals.h"
#include "util.h"
#include "widget_list_selection.h"
#include "layer_trw.h"




using namespace SlavGPS;



#ifdef K
/* Compatibility. */
#if ! GLIB_CHECK_VERSION(2,22,0)
#define g_mapped_file_unref g_mapped_file_free
#endif
#endif


/**
 * See http://www.geonames.org/export/wikipedia-webservice.html#wikipediaBoundingBox
 */
/* Translators may wish to change this setting as appropriate to get Wikipedia articles in that language. */
#define GEONAMES_LANG QObject::tr("en")
/* TODO - offer configuration of this value somewhere.
   ATM decided it's not essential enough to warrant putting in the preferences. */
#define GEONAMES_MAX_ENTRIES 20

//#define GEONAMES_WIKIPEDIA_URL_FMT "http://api.geonames.org/wikipediaBoundingBoxJSON?formatted=true&north=%s&south=%s&east=%s&west=%s&lang=%s&maxRows=%d&username=viking"
#define GEONAMES_WIKIPEDIA_URL_FMT "http://api.geonames.org/wikipediaBoundingBoxJSON?formatted=true&north=%1&south=%2&east=%3&west=%4&lang=%5&maxRows=%6&username=viking"

#define GEONAMES_FEATURE_PATTERN "\"feature\": \""
#define GEONAMES_LONGITUDE_PATTERN "\"lng\": "
#define GEONAMES_NAME_PATTERN "\"name\": \""
#define GEONAMES_LATITUDE_PATTERN "\"lat\": "
#define GEONAMES_ELEVATION_PATTERN "\"elevation\": "
#define GEONAMES_TITLE_PATTERN "\"title\": \""
#define GEONAMES_WIKIPEDIAURL_PATTERN "\"wikipediaUrl\": \""
#define GEONAMES_THUMBNAILIMG_PATTERN "\"thumbnailImg\": \""
#define GEONAMES_SEARCH_NOT_FOUND "not understand the location"




#if 0
static found_geoname * new_found_geoname()
{
	found_geoname * ret = (found_geoname *)malloc(sizeof(found_geoname));
	ret->name = NULL;
	ret->feature = NULL;
	ret->cmt = NULL;
	ret->desc = NULL;
	ret->ll.lat = 0.0;
	ret->ll.lon = 0.0;
	ret->elevation = VIK_DEFAULT_ALTITUDE;
	return ret;
}
#endif




Geoname::~Geoname()
{
	free(this->name);
	free(this->feature);
	free(this->cmt);
	free(this->desc);
}




static Geoname * copy_geoname(Geoname * src)
{
	Geoname * dest = new Geoname();
	dest->name = g_strdup(src->name);
	dest->feature = g_strdup(src->feature);
	dest->ll.lat = src->ll.lat;
	dest->ll.lon = src->ll.lon;
	dest->elevation = src->elevation;
	dest->cmt = g_strdup(src->cmt);
	dest->desc = g_strdup(src->desc);
	return dest;
}




static void free_geoname_list(std::list<Geoname *> & found_places)
{
	for (auto iter = found_places.begin(); iter != found_places.end(); iter++) {
		qDebug() << "DD: geoname" << (unsigned long) *iter;
#ifdef K
		//delete *iter;
#endif
	}
	qDebug() << "";
}



/*
  TODO: this function builds a table with three columns, but only one
  of them (Name) is filled with details from geonames.  Extend/improve
  list selection widget so that it can display properties of items in
  N columns.
*/
std::list<Geoname *> a_select_geoname_from_list(const QString & title, const QStringList & headers, std::list<Geoname *> & geonames, bool multiple_selection, Window * parent)
{
	QStringList headers2;
	headers2 << QObject::tr("Name") << QObject::tr("Feature") << QObject::tr("Lat/Lon");

	std::list<Geoname *> selected_geonames = a_dialog_select_from_list(geonames, multiple_selection, title, headers2, parent);
#ifdef K
	GtkTreeIter iter;
	GtkCellRenderer * renderer;
	GtkWidget *view;
	Geoname * geoname;
	char * latlon_string;
	int column_runner;

	GtkWidget *dialog = gtk_dialog_new_with_buttons(title,
							parent,
							(GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);
	/* When something is selected then OK. */
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
	/* Default to not apply - as initially nothing is selected! */
	response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
#endif

	QLabel * label = new QLabel(msg);
	GtkTreeStore *store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	GList *geoname_runner = geonames;
	while (geoname_runner) {
		geoname = (Geoname *) geoname_runner->data;
		latlon_string = g_strdup_printf("(%f,%f)", geoname->ll.lat, geoname->ll.lon);
		gtk_tree_store_append(store, &iter, NULL);
		gtk_tree_store_set(store, &iter, 0, geoname->name, 1, geoname->feature, 2, latlon_string, -1);
		geoname_runner = g_list_next(geoname_runner);
		free(latlon_string);
	}

	view = gtk_tree_view_new();
	renderer = gtk_cell_renderer_text_new();
	column_runner = 0;
	GtkTreeViewColumn *column;
	/* NB could allow columns to be shifted around by doing this after each new
	   gtk_tree_view_column_set_reorderable(column, true);
	   However I don't think is that useful, so I haven't put it in. */
	column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id(column, column_runner);
	gtk_tree_view_append_column(GTK_TREE_VIEW (view), column);

	column_runner++;
	column = gtk_tree_view_column_new_with_attributes(_("Feature"), renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id(column, column_runner);
	gtk_tree_view_append_column(GTK_TREE_VIEW (view), column);

	column_runner++;
	column = gtk_tree_view_column_new_with_attributes(_("Lat/Lon"), renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id(column, column_runner);
	gtk_tree_view_append_column(GTK_TREE_VIEW (view), column);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
				    multiple_selection_allowed ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_BROWSE);
	g_object_unref(store);

	GtkWidget *scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolledwindow), view);

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), scrolledwindow, true, true, 0);

	/* Ensure a reasonable number of items are shown, but let the width be automatically sized. */
	gtk_widget_set_size_request(dialog, -1, 400) ;
	gtk_widget_show_all(dialog);

	if (response_w) {
		gtk_widget_grab_focus(response_w);
	}

	while (gtk_dialog_run(GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
		selected_geonames.erase(); /* TODO: Erase? Clear? Something else? */

		/* Possibily not the fastest method but we don't have thousands of entries to process... */
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
			do {
				if (gtk_tree_selection_iter_is_selected(selection, &iter)) {
					/* For every selected item,
					   compare the name from the displayed view to every geoname entry to find the geoname this selection represents. */
					char* name;
					gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, &name, -1);
					/* I believe the name of these items to be always unique. */
					geoname_runner = geonames;
					while (geoname_runner) {
						if (!strcmp(((Geoname *) geoname_runner->data)->name, name)) {
							Geoname * copied = copy_geoname((Geoname *) geoname_runner->data);
							selected_geonames.push_front(copied);
							break;
						}
						geoname_runner = g_list_next(geoname_runner);
					}
					free(name);
				}
			}
			while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
		}

		if (selected_geonames.size()) {
			gtk_widget_destroy(dialog);
			return selected_geonames; /* Hopefully Named Return Value Optimization will work here. */
		}
		Dialog::error(tr("Nothing was selected"), parent);
	}
	gtk_widget_destroy(dialog);
#endif
	return selected_geonames; /* Hopefully Named Return Value Optimization will work here. */
}




static std::list<Geoname *> get_entries_from_file(char * file_name)
{
	std::list<Geoname *> found_places;

	char lat_buf[32] = { 0 };
	char lon_buf[32] = { 0 };
	char elev_buf[32] = { 0 };
	char * wikipedia_url = NULL;
	char * thumbnail_url = NULL;

	GMappedFile * mf = NULL;
	if ((mf = g_mapped_file_new(file_name, false, NULL)) == NULL) {
		fprintf(stderr, _("CRITICAL: couldn't map temp file\n"));
		return found_places;
	}
	size_t len = g_mapped_file_get_length(mf);
	char * text = g_mapped_file_get_contents(mf);

	bool more = true;
	if (g_strstr_len(text, len, GEONAMES_SEARCH_NOT_FOUND) != NULL) {
		more = false;
	}

	char ** found_entries = g_strsplit(text, "},", 0);
	int entry_runner = 0;
	char * entry = found_entries[entry_runner];
	char * pat = NULL;
	char * s = NULL;
	int fragment_len;
	while (entry) {
		more = true;
		Geoname * geoname = new Geoname();
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_FEATURE_PATTERN))) {
			pat += strlen(GEONAMES_FEATURE_PATTERN);
			fragment_len = 0;
			s = pat;
			while (*pat != '"') {
				fragment_len++;
				pat++;
			}
			geoname->feature = g_strndup(s, fragment_len);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_LONGITUDE_PATTERN)) == NULL) {
			more = false;
		} else {
			pat += strlen(GEONAMES_LONGITUDE_PATTERN);
			s = lon_buf;
			if (*pat == '-')
				*s++ = *pat++;
			while ((s < (lon_buf + sizeof(lon_buf))) && (pat < (text + len)) &&
			       (g_ascii_isdigit(*pat) || (*pat == '.'))) {
				*s++ = *pat++;
			}
			*s = '\0';
			if ((pat >= (text + len)) || (lon_buf[0] == '\0')) {
				more = false;
			}
			geoname->ll.lon = g_ascii_strtod(lon_buf, NULL);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_ELEVATION_PATTERN))) {
			pat += strlen(GEONAMES_ELEVATION_PATTERN);
			s = elev_buf;
			if (*pat == '-') {
				*s++ = *pat++;
			}
			while ((s < (elev_buf + sizeof(elev_buf))) && (pat < (text + len)) &&
			       (g_ascii_isdigit(*pat) || (*pat == '.'))) {
				*s++ = *pat++;
			}
			*s = '\0';
			geoname->elevation = g_ascii_strtod(elev_buf, NULL);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_NAME_PATTERN))) {
			pat += strlen(GEONAMES_NAME_PATTERN);
			fragment_len = 0;
			s = pat;
			while (*pat != '"') {
				fragment_len++;
				pat++;
			}
			geoname->name = g_strndup(s, fragment_len);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_TITLE_PATTERN))) {
			pat += strlen(GEONAMES_TITLE_PATTERN);
			fragment_len = 0;
			s = pat;
			while (*pat != '"') {
				fragment_len++;
				pat++;
			}
			geoname->name = g_strndup(s, fragment_len);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_WIKIPEDIAURL_PATTERN))) {
			pat += strlen(GEONAMES_WIKIPEDIAURL_PATTERN);
			fragment_len = 0;
			s = pat;
			while (*pat != '"') {
				fragment_len++;
				pat++;
			}
			wikipedia_url = g_strndup(s, fragment_len);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_THUMBNAILIMG_PATTERN))) {
			pat += strlen(GEONAMES_THUMBNAILIMG_PATTERN);
			fragment_len = 0;
			s = pat;
			while (*pat != '"') {
				fragment_len++;
				pat++;
			}
			thumbnail_url = g_strndup(s, fragment_len);
		}
		if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_LATITUDE_PATTERN)) == NULL) {
			more = false;
		} else {
			pat += strlen(GEONAMES_LATITUDE_PATTERN);
			s = lat_buf;
			if (*pat == '-') {
				*s++ = *pat++;
			}

			while ((s < (lat_buf + sizeof(lat_buf))) && (pat < (text + len)) &&
			       (g_ascii_isdigit(*pat) || (*pat == '.'))) {
				*s++ = *pat++;
			}

			*s = '\0';
			if ((pat >= (text + len)) || (lat_buf[0] == '\0')) {
				more = false;
			}
			geoname->ll.lat = g_ascii_strtod(lat_buf, NULL);
		}
		if (!more) {
			if (geoname) {
				free(geoname);
			}
		} else {
			if (wikipedia_url) {
				/* Really we should support the GPX URL tag and then put that in there... */
				geoname->cmt = g_strdup_printf("http://%s", wikipedia_url);
				if (thumbnail_url) {
					geoname->desc = g_strdup_printf("<a href=\"http://%s\" target=\"_blank\"><img src=\"%s\" border=\"0\"/></a>", wikipedia_url, thumbnail_url);
				} else {
					geoname->desc = g_strdup_printf("<a href=\"http://%s\" target=\"_blank\">%s</a>", wikipedia_url, geoname->name);
				}
			}
			if (wikipedia_url) {
				free(wikipedia_url);
				wikipedia_url = NULL;
			}
			if (thumbnail_url) {
				free(thumbnail_url);
				thumbnail_url = NULL;
			}
			found_places.push_front(geoname);
		}
		entry_runner++;
		entry = found_entries[entry_runner];
	}
	g_strfreev(found_entries);
	found_places.reverse();
	g_mapped_file_unref(mf);

	return found_places; /* Hopefully Named Return Value Optimization will work here. */
}




void SlavGPS::a_geonames_wikipedia_box(Window * window, LayerTRW * trw, struct LatLon maxmin[2])
{
	QString north;
	QString south;
	QString east;
	QString west;
	CoordUtils::to_string(north, maxmin[0].lat);
	CoordUtils::to_string(south, maxmin[1].lat);
	CoordUtils::to_string(east, maxmin[0].lon);
	CoordUtils::to_string(west, maxmin[1].lon);

	/* Encode doubles in a C locale; kamilTODO: see viewport->get_bbox_strings(). */
	const QString uri = QString(GEONAMES_WIKIPEDIA_URL_FMT).arg(north).arg(south).arg(east).arg(west).arg(GEONAMES_LANG).arg(GEONAMES_MAX_ENTRIES);

	char * tmpname = Download::get_uri_to_tmp_file(uri, NULL);
	if (!tmpname) {
		Dialog::info(QObject::tr("No entries found!"), window);
		return;
	}

	std::list<Geoname *> wiki_places = get_entries_from_file(tmpname);
	(void) util_remove(tmpname);
	free(tmpname);

	if (wiki_places.size() == 0) {
		Dialog::info(QObject::tr("No entries found!"), window);
		return;
	}

	const QStringList headers = { QObject::tr("Select the articles you want to add.") };
	std::list<Geoname *> selected = a_select_geoname_from_list(QObject::tr("Select articles"), headers, wiki_places, true, window);

	for (auto iter = selected.begin(); iter != selected.end(); iter++) {
		const Geoname * wiki_geoname = *iter;

		Waypoint * wiki_wp = new Waypoint();
		wiki_wp->visible = true;
		wiki_wp->coord = Coord(wiki_geoname->ll, trw->get_coord_mode());
		wiki_wp->altitude = wiki_geoname->elevation;
		wiki_wp->set_comment(wiki_geoname->cmt);
		wiki_wp->set_description(wiki_geoname->desc);

		/* Use the featue type to generate a suitable waypoint icon
		   http://www.geonames.org/wikipedia/wikipedia_features.html
		   Only a few values supported as only a few symbols make sense. */
		if (wiki_geoname->feature) {
			if (!strcmp(wiki_geoname->feature, "city")) {
				wiki_wp->set_symbol("city (medium)");
			}

			if (!strcmp(wiki_geoname->feature, "edu")) {
				wiki_wp->set_symbol("school");
			}

			if (!strcmp(wiki_geoname->feature, "airport")) {
				wiki_wp->set_symbol("airport");
			}

			if (!strcmp(wiki_geoname->feature, "mountain")) {
				wiki_wp->set_symbol("summit");
			}
			if (!strcmp(wiki_geoname->feature, "forest")) {
				wiki_wp->set_symbol("forest");
			}
		}
		trw->filein_add_waypoint(wiki_wp, wiki_geoname->name);
	}

	free_geoname_list(wiki_places);
	/* TODO: 'selected' contains pointer to geonames that were
	   present on 'wiki_places'.  Freeing 'wiki_places' freed also
	   pointers stored in 'selected', so there is no need to call
	   free_geoname_list(selected). */
	free_geoname_list(selected);

	return;
}
