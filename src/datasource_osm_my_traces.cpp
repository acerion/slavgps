/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012-2015, Rob Norris <rw_norris@hotmail.com>
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




#include <cstdlib>
#include <cassert>




#ifdef HAVE_EXPAT_H
#include <expat.h>
#endif




#include <QPushButton>




#include "window.h"
#include "datasource_osm_my_traces.h"
#include "layer_trw.h"
#include "layer_aggregate.h"
#include "layers_panel.h"
#include "viewport_internal.h"
#include "gpx.h"
#include "acquire.h"
#include "osm_traces.h"
#include "datasource_gps.h"
#include "bbox.h"
#include "dialog.h"
#include "util.h"
#include "statusbar.h"




using namespace SlavGPS;




/**
   See http://wiki.openstreetmap.org/wiki/API_v0.6#GPS_Traces
*/
#define DS_OSM_TRACES_GPX_URL_FMT "api.openstreetmap.org/api/0.6/gpx/%1/data"
#define DS_OSM_TRACES_GPX_FILES "api.openstreetmap.org/api/0.6/user/gpx_files"




DataSourceOSMMyTraces::DataSourceOSMMyTraces(GisViewport * new_gisview)
{
	this->gisview = new_gisview;

	this->window_title = QObject::tr("OSM My Traces");
	this->layer_title = QObject::tr("OSM My Traces");
	this->mode = DataSourceMode::ManualLayerManagement; /* We'll do this ourselves. */
	this->input_type = DataSourceInputType::None;
	this->autoview = true;
	this->keep_dialog_open = true; /* true = keep dialog open after success. */
}





/* Comment from constructor function from viking: */
/* Reuse GPS functions.
   Haven't been able to get the thread method to work reliably (or get progress feedback).
   So thread version is disabled ATM. */
/*
  if (datasource_osm_my_traces_interface.is_thread) {
  datasource_osm_my_traces_interface.progress_func = datasource_gps_progress;
  datasource_osm_my_traces_interface.create_progress_dialog_func = datasource_gps_add_progress_widgets;
  }
*/




int DataSourceOSMMyTraces::run_config_dialog(AcquireContext * acquire_context)
{
	DataSourceOSMMyTracesDialog config_dialog(this->window_title, this->gisview);

	/* Keep reference to viewport. */
	config_dialog.gisview = this->gisview;


	QLabel * user_label = new QLabel(QObject::tr("Username:"));
	config_dialog.user_entry.setToolTip(QObject::tr("The email or username used to login to OSM"));
	config_dialog.grid->addWidget(user_label, 0, 0);
	config_dialog.grid->addWidget(&config_dialog.user_entry, 0, 1);


	QLabel * password_label = new QLabel(QObject::tr("Password:"));
	config_dialog.password_entry.setToolTip(QObject::tr("The password used to login to OSM"));
	config_dialog.grid->addWidget(password_label, 1, 0);
	config_dialog.grid->addWidget(&config_dialog.password_entry, 1, 1);

	OSMTraces::fill_credentials_widgets(config_dialog.user_entry, config_dialog.password_entry);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}



AcquireOptions * DataSourceOSMMyTracesDialog::create_acquire_options(AcquireContext * acquire_context)
{
	AcquireOptions * babel_options = new AcquireOptions(AcquireOptions::Mode::FromURL);

	/* Overwrite authentication info. */
	OSMTraces::save_current_credentials(this->user_entry.text(), this->password_entry.text());

	/* If going to use the values passed back into the process function parameters then they need to be set.
	   But ATM we aren't. */
	/* dl_options = NULL; */

	return babel_options;
}




enum class XTagID {
	Unknown = 0,
	OSM,
	GPXFile,
	GPXFileDesc,
	GPXFileTag,
};

typedef struct {
	XTagID tag_id;
	const QString tag_name;     /* xpath-ish tag name. */
} xtag_mapping;

class SlavGPS::GPXMetaData {
public:
	GPXMetaData() {};
	~GPXMetaData() {};

	unsigned int id = 0;
	QString name;
	QString visibility;
	QString description;
	LatLon ll;
	bool in_current_view = false; /* Is the track LatLon start within the current viewport.
				 This is useful in deciding whether to download a track or not. */
	/* ATM Only used for display - may want to convert to a time_t for other usage. */
	QString timestamp;
	/* user made up tags - not being used yet - would be nice to sort/select on these but display will get complicated. */
	// GList *tag_list;
};




static void free_gpx_meta_data_list(std::list<GPXMetaData *> & list)
{
	for (auto iter = list.begin(); iter != list.end(); iter++) {
		delete *iter;
	}
}




static GPXMetaData * copy_gpx_meta_data_t(GPXMetaData * src)
{
	GPXMetaData * dest = new GPXMetaData();

	dest->id = src->id;
	dest->name = src->name;
	dest->visibility  = src->visibility;
	dest->description = src->description;
	dest->ll.lat = src->ll.lat;
	dest->ll.lon = src->ll.lon;
	dest->in_current_view = src->in_current_view;
	dest->timestamp = src->timestamp;

	return dest;
}




typedef struct {
	//GString *xpath;
	GString *c_cdata;
	XTagID current_tag_id;
	GPXMetaData * current_gpx_meta_data = NULL;
	std::list<GPXMetaData *> list_of_gpx_meta_data;
} xml_data;




/* Same as the gpx.c function. */
static const char *get_attr(const char **attr, const char *key)
{
	while (*attr) {
		if (strcmp(*attr,key) == 0) {
			return *(attr + 1);
		}
		attr += 2;
	}
	return NULL;
}




/* ATM don't care about actual path as tags are all unique. */
static xtag_mapping xtag_path_map[] = {
	{ XTagID::OSM,           "osm" },
	{ XTagID::GPXFile,      "gpx_file" },
	{ XTagID::GPXFileDesc, "description" },
	{ XTagID::GPXFileTag,  "tag" },
};




static XTagID get_tag_id(const QString & tag_name)
{
	for (xtag_mapping * tm = xtag_path_map; tm->tag_id != XTagID::Unknown; tm++) {
		if (tm->tag_name == tag_name) {
			return tm->tag_id;
		}
	}
	return XTagID::Unknown;
}




static void gpx_meta_data_start(xml_data * xd, const char * element, const char ** attributes)
{
	const char *tmp;
	char buf[G_ASCII_DTOSTR_BUF_SIZE];
	buf[0] = '\0';

	/* Don't need to build a path - we can use the tag directly. */
	// g_string_append_c(xd->xpath, '/');
	// g_string_append(xd->xpath, element);
	// xd->current_tag_id = get_tag_id(xd->xpath->str);
	xd->current_tag_id = get_tag_id(element);
	switch (xd->current_tag_id) {
	case XTagID::GPXFile:
		if (xd->current_gpx_meta_data) {
			delete xd->current_gpx_meta_data;
		}
		xd->current_gpx_meta_data = new GPXMetaData();

		if ((tmp = get_attr(attributes, "id"))) {
			xd->current_gpx_meta_data->id = atoi(tmp);
		}

		if ((tmp = get_attr(attributes, "name"))) {
			xd->current_gpx_meta_data->name = tmp;
		}

		if ((tmp = get_attr(attributes, "lat"))) {
			strncpy(buf, tmp, sizeof (buf));
			buf[sizeof (buf) - 1] = '\0';
			xd->current_gpx_meta_data->ll.lat = SGUtils::c_to_double(buf);
		}

		if ((tmp = get_attr(attributes, "lon"))) {
			strncpy(buf, tmp, sizeof (buf));
			buf[sizeof (buf) - 1] = '\0';
			xd->current_gpx_meta_data->ll.lon = SGUtils::c_to_double(buf);
		}

		if ((tmp = get_attr(attributes, "visibility"))) {
			xd->current_gpx_meta_data->visibility = tmp;
		}

		if ((tmp = get_attr(attributes, "timestamp"))) {
			xd->current_gpx_meta_data->timestamp = tmp;
		}

		g_string_erase(xd->c_cdata, 0, -1); /* Clear the cdata buffer. */
		break;
	case XTagID::GPXFileDesc:
	case XTagID::GPXFileTag:
		g_string_erase(xd->c_cdata, 0, -1); /* Clear the cdata buffer. */
		break;
	default:
		g_string_erase(xd->c_cdata, 0, -1); /* Clear the cdata buffer. */
		break;
	}
}




static void gpx_meta_data_end(xml_data *xd, const char * element)
{
	// g_string_truncate(xd->xpath, xd->xpath->len - strlen(element) - 1);
	// switch (xd->current_tag) {
	switch (get_tag_id(element)) {
	case XTagID::GPXFile: {
		/* End of the individual file metadata, thus save what we have read in to the list.
		   Copy it so we can reference it. */
		GPXMetaData * current = copy_gpx_meta_data_t(xd->current_gpx_meta_data);
		/* Stick in the list. */
		xd->list_of_gpx_meta_data.push_front(current);
		g_string_erase(xd->c_cdata, 0, -1);
		break;
	}
	case XTagID::GPXFileDesc:
		/* Store the description: */
		if (xd->current_gpx_meta_data) {
			/* NB Limit description size as it's displayed on a single line.
			   Hopefully this will prevent the dialog getting too wide... */
			xd->current_gpx_meta_data->description = QString(xd->c_cdata->str).left(63);
		}
		g_string_erase(xd->c_cdata, 0, -1);
		break;
	case XTagID::GPXFileTag:
		/* One day do something with this... */
		g_string_erase(xd->c_cdata, 0, -1);
		break;
	default:
		break;
	}
}




static void gpx_meta_data_cdata(xml_data * xd, const XML_Char *s, int len)
{
	switch (xd->current_tag_id) {
	case XTagID::GPXFileDesc:
	case XTagID::GPXFileTag:
		g_string_append_len(xd->c_cdata, s, len);
		break;
	default:
		break;  /* Ignore cdata from other things. */
	}
}



/*
  @file passed to this function is an opened QTemporaryFile.
*/
static LoadStatus read_gpx_files_metadata_xml(QFile & file, xml_data *xd)
{
	FILE *ff = fopen(file.fileName().toUtf8().constData(), "r");
	if (!ff) {
		return LoadStatus::Code::FileAccess;
	}

	XML_Parser parser = XML_ParserCreate(NULL);
	enum XML_Status status = XML_STATUS_ERROR;

	XML_SetElementHandler(parser, (XML_StartElementHandler) gpx_meta_data_start, (XML_EndElementHandler) gpx_meta_data_end);
	XML_SetUserData(parser, xd);
	XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler) gpx_meta_data_cdata);

	char buf[4096];

	int done=0, len;
	while (!done) {
		len = fread(buf, 1, sizeof(buf)-7, ff);
		done = feof(ff) || !len;
		status = XML_Parse(parser, buf, len, done);
	}

	XML_ParserFree(parser);

	fclose(ff);

	return status != XML_STATUS_ERROR ? LoadStatus::Code::Success : LoadStatus::Code::Error;
}




static std::list<GPXMetaData *> * select_from_list(Window * parent, std::list<GPXMetaData *> & list, const char *title, const char *msg)
{
	BasicDialog * dialog = new BasicDialog(parent);

	dialog->setWindowTitle(title);
	/* When something is selected then OK. */
	dialog->button_box->button(QDialogButtonBox::Ok)->setDefault(true);

	/* Default to not apply - as initially nothing is selected! */
	QPushButton * cancel_button = dialog->button_box->button(QDialogButtonBox::Cancel);

	QLabel * label = new QLabel(msg);

#ifdef K_FIXME_RESTORE

	GtkTreeStore *store = gtk_tree_store_new(6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	for (auto iter = list.begin(); iter != list.end(); iter++) {
		GPXMetaData * gpx_meta_data = *iter;
		gtk_tree_store_append(store, &iter, NULL);
		gtk_tree_store_set(store, &iter,
				   0, gpx_meta_data->name,
				   1, gpx_meta_data->descrition,
				   2, gpx_meta_data->timestamp,
				   3, gpx_meta_data->ll.to_string_raw(),
				   4, gpx_meta_data->visibility,
				   5, gpx_meta_data->in_current_view,
				   -1);
	}

	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	GtkWidget *view;
	int column_runner;

	view = gtk_tree_view_new();
	renderer = gtk_cell_renderer_text_new();
	column_runner = 0;
	GtkTreeViewColumn *column;

	column = gtk_tree_view_column_new_with_attributes(QObject::tr("Name"), renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	column_runner++;
	column = gtk_tree_view_column_new_with_attributes(QObject::tr("Description"), renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	column_runner++;
	column = gtk_tree_view_column_new_with_attributes(QObject::tr("Time"), renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	column_runner++;
	column = gtk_tree_view_column_new_with_attributes(QObject::tr("Lat/Lon"), renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	column_runner++;
	column = gtk_tree_view_column_new_with_attributes(QObject::tr("Privacy"), renderer, "text", column_runner, NULL); // AKA Visibility
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	GtkCellRenderer *renderer_toggle = gtk_cell_renderer_toggle_new ();
	g_object_set (G_OBJECT (renderer_toggle), "activatable", false, NULL); // No user action - value is just for display
	column_runner++;
	column = gtk_tree_view_column_new_with_attributes(QObject::tr("Within Current View"), renderer_toggle, "active", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
	gtk_tree_selection_set_mode(view->selectionModel(), GTK_SELECTION_MULTIPLE);

	GtkWidget *scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER(scrolledwindow), view);

	gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), label, false, false, 0);
	gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), scrolledwindow, true, true, 0);

	/* Ensure a reasonable number of items are shown, but let the width be automatically sized. */
	gtk_widget_set_size_request (dialog, -1, 400) ;
	gtk_widget_show_all (dialog);

	if (cancel_button) {
		cancel_button->setFocus();
	}

	while (dialog.exec() == QDialog::Accepted) {

		/* Possibily not the fastest method but we don't have thousands of entries to process... */
		QItemSelectionModel * selection = view.selectionModel();
		std::list<GPXMetaData *> * selected = new std::list<GPXMetaData *>;

		/* Because we don't store the full data in the gtk model, we have to scan & look it up. */
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
			do {
				if (gtk_tree_selection_iter_is_selected(selection, &iter)) {
					/* For every selected item,
					   compare the name from the displayed view to every gpx entry to find the gpx this selection represents. */
					char* name;
					gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, &name, -1);
					/* I believe the name of these items to be always unique. */
					for (auto iter = list.begin(); iter != list.end(); iter++) {
						if (!strcmp ((*iter)->name, name)) {
							GPXMetaData * copied = copy_gpx_meta_data_t(*iter);
							selected->push_front(copied);
							break;
						}
					}
					free(name);
				}
			}
			while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
		}

		if (selected.size()) {
			gtk_widget_destroy(dialog);
			return selected;
		}
		Dialog::error(tr("Nothing was selected"), parent);
	}
#endif

	delete dialog;

	return NULL;
}




/**
   For each track - mark whether the start is in within the viewport.
*/
void DataSourceOSMMyTracesDialog::set_in_current_view_property(std::list<GPXMetaData *> & list)
{
	const LatLonBBox viewport_bbox = this->gisview->get_bbox();

	for (auto iter = list.begin(); iter != list.end(); iter++) {
		GPXMetaData * gmd = *iter;
		gmd->in_current_view = viewport_bbox.contains_point(gmd->ll);
	}
}




LoadStatus DataSourceOSMMyTraces::acquire_into_layer(LayerTRW * trw, AcquireContext * acquire_context, AcquireProgressDialog * progr_dialog)
{
	// datasource_osm_my_traces_t *data = (datasource_osm_my_traces_t *) acquiring_context->user_data;

	LoadStatus result = LoadStatus::Code::Error;

	/* Support .zip + bzip2 files directly. */
	DownloadOptions local_dl_options(2); /* Allow a couple of redirects. */
	local_dl_options.convert_file = a_try_decompress_file;
	local_dl_options.user_pass = OSMTraces::get_current_credentials();

	DownloadHandle dl_handle(&local_dl_options);

	QTemporaryFile tmp_file;
	if (!dl_handle.download_to_tmp_file(tmp_file, DS_OSM_TRACES_GPX_FILES)) {
		return LoadStatus::Code::IntermediateFileAccess;
	}

	xml_data *xd = (xml_data *) malloc(sizeof (xml_data));
	//xd->xpath = g_string_new("");
	xd->c_cdata = g_string_new("");
	xd->current_tag_id = XTagID::Unknown;
	xd->current_gpx_meta_data = new GPXMetaData();
	xd->list_of_gpx_meta_data.clear();

	LoadStatus metadata_load_result = read_gpx_files_metadata_xml(tmp_file, xd);
	/* Test already downloaded metadata file: eg: */
	// result = read_gpx_files_metadata_xml("/tmp/viking-download.GI47PW", xd);
	Util::remove(tmp_file);

	if (LoadStatus::Code::Success != metadata_load_result) {
		free(xd);
		return LoadStatus::Code::IntermediateFileAccess;
	}

	if (xd->list_of_gpx_meta_data.size() == 0) {
#ifdef FIXME_RESTORE
		if (!this->is_thread) {
			Dialog::info(QObject::tr("No GPS Traces found"), acquire_context->window);
		}
#endif
		free(xd);
		return LoadStatus::Code::Error;
	}

	xd->list_of_gpx_meta_data.reverse();
#ifdef FIXME_RESTORE
	((DataSourceOSMMyTracesDialog *) acquiring_context->parent_data_source_dialog)->set_in_current_view_property(xd->list_of_gpx_meta_data);
#endif

	std::list<GPXMetaData *> * selected = select_from_list(acquire_context->window, xd->list_of_gpx_meta_data, "Select GPS Traces", "Select the GPS traces you want to add.");

#ifdef FIXME_RESTORE
	/* If non thread - show program is 'doing something...' */
	if (!this->is_thread) {
		acquire_context->window->set_busy_cursor();
	}
#endif

	/* If passed in on an existing layer - we will create everything into that.
	   thus with many differing gpx's - this will combine all waypoints into this single layer!
	   Hence the preference is to create multiple layers
	   and so this creation of the layers must be managed here. */

	bool create_new_layer = (!trw);

	/* Only update the screen on the last layer acquired. */
	LayerTRW * vtl_last = trw;
	bool got_something = false;

	if (selected) {
		for (auto iter = selected->begin(); iter != selected->end(); iter++) {

			LayerTRW * target_layer = NULL;

			if (create_new_layer) {
				/* Have data but no layer - so create one. */
				target_layer = new LayerTRW();
				target_layer->set_coord_mode(acquire_context->gisview->get_coord_mode());
				if (!(*iter)->name.isEmpty()) {
					target_layer->set_name((*iter)->name);
				} else {
					target_layer->set_name(QObject::tr("My OSM Traces"));
				}
			} else {
				target_layer = trw;
			}

			LoadStatus convert_result = LoadStatus::Code::Error;
			int gpx_id = (*iter)->id;
			if (gpx_id) {
				/* Download type is GPX (or a compressed version). */
				this->acquire_options->source_url = QString(DS_OSM_TRACES_GPX_URL_FMT).arg(gpx_id);

				convert_result = this->acquire_options->import_from_url(target_layer, &local_dl_options, NULL);
				/* TODO_MAYBE investigate using a progress bar:
				   http://developer.gnome.org/gtk/2.24/GtkProgressBar.html */

				got_something = got_something || (LoadStatus::Code::Success == convert_result);
				if (LoadStatus::Code::Success != convert_result) {
					/* Report errors to the status bar. */
					acquire_context->window->statusbar_update(StatusBarField::Info, QObject::tr("Unable to get trace: %1").arg(this->acquire_options->source_url));
				}
			}

			if (LoadStatus::Code::Success == convert_result) {
				/* Can use the layer. */
				acquire_context->top_level_layer->add_layer(target_layer, true);
				/* Move to area of the track. */
				target_layer->post_read(acquire_context->gisview, true);
				target_layer->move_viewport_to_show_all(acquire_context->gisview);
				vtl_last = target_layer;
			} else {
				if (create_new_layer) {
					/* Layer not needed as no data has been acquired. */
					target_layer->unref_layer();
				}
			}
		}
	}

	/* Free memory. */
	if (xd->current_gpx_meta_data) {
		delete xd->current_gpx_meta_data;
	}
	free(xd->current_gpx_meta_data);
	free_gpx_meta_data_list(xd->list_of_gpx_meta_data);
	if (selected) {
		free_gpx_meta_data_list(*selected);
		delete selected;
	}
	free(xd);

	/* Would prefer to keep the update in acquire.c,
	   however since we may create the layer - need to do the update here. */
	if (got_something) {
		Layer * layer_last = (Layer *) vtl_last;
		layer_last->emit_tree_item_changed("OSM My Traces - acquire into layer");
	}

	/* ATM The user is only informed if all getting *all* of the traces failed. */
	if (selected) {
		result = got_something ? LoadStatus::Code::Success : LoadStatus::Code::Error;
	} else {
		/* Process was cancelled but need to return that it proceeded as expected. */
		result = LoadStatus::Code::Success;
	}

#ifdef FIXME_RESTORE
	if (!this->is_thread) {
		acquire_context->window->clear_busy_cursor();
	}
#endif

	return result;
}
