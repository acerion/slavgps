/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2010-2015, Rob Norris <rw_norris@hotmail.com>
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

#if 0
#include "degrees_converters.h"
#include "garminsymbols.h"
#ifdef VIK_CONFIG_GEOTAG
#include "geotag_exif.h"
#endif
#include "thumbnails.h"
#include "goto.h"
#include "vikutils.h"
#include "widget_file_entry.h"
#endif
#include "waypoint_properties.h"
#include "dialog.h"
#include "globals.h"
#include "ui_builder.h"
#include "date_time_dialog.h"
#include "layer_trw.h"
#include "waypoint.h"




using namespace SlavGPS;




Parameter wp_params[] = {
	{ SG_WP_PARAM_NAME,     "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  "Name",         WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_LAT,      "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  "Latitude",     WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_LON,      "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  "Longitude",    WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_TIME,     "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  "Time",         WidgetType::DATETIME,    NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_ALT,      "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  "Altitude",     WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_COMMENT,  "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  "Comment",      WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_DESC,     "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  "Description",  WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_IMAGE,    "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  "Image",        WidgetType::FILEENTRY,   NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_SYMBOL,   "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  "Symbol",       WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	/* TODO: where is guard item? */
};




#if 0
static void update_time(GtkWidget * widget, Waypoint * wp)
{
	char * msg = vu_get_time_string(&(wp->timestamp), "%c", &(wp->coord), NULL);
	gtk_button_set_label(GTK_BUTTON(widget), msg);
	free(msg);
}




static Waypoint * edit_wp;




static void time_edit_click(GtkWidget * widget, GdkEventButton * event, Waypoint * wp)
{
	if (event->button == MouseButton::RIGHT) {
		/* On right click and when a time is available, allow a method to copy the displayed time as text. */
		if (!gtk_button_get_image(GTK_BUTTON(widget))) {
			vu_copy_label_menu(widget, event->button);
		}
		return;
	} else if (event->button == MouseButton::MIDDLE) {
		return;
	}

	time_t mytime = date_time_dialog(parent,
					 ("Date/Time Edit"),
					 wp->timestamp);

	/* Was the dialog cancelled?. */
	if (mytime == 0) {
		return;
	}

	/* Otherwise use new value in the edit buffer. */
	edit_wp->timestamp = mytime;

	/* Clear the previous 'Add' image as now a time is set. */
	if (gtk_button_get_image(GTK_BUTTON(widget))) {
		gtk_button_set_image(GTK_BUTTON(widget), NULL);
	}

	update_time(widget, edit_wp);
}




static void symbol_entry_changed_cb(GtkWidget * combo, GtkListStore * store)
{
	GtkTreeIter iter;
	char * sym;

	if (!gtk_combo_box_get_active_iter(combo, &iter)) {
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, (void *)&sym, -1);
	/* Note: symm is NULL when "(none)" is select (first cell is empty). */
	combo->setToolTip(sym);
	free(sym);
}
#endif



/* Specify if a new waypoint or not. */
/* If a new waypoint then it uses the default_name for the suggested name allowing the user to change it.
   The name to use is returned.
*/
/* TODO: less on this side, like add track. */
char * a_dialog_waypoint(Window * parent, char * default_name, LayerTRW * trw, Waypoint * wp, CoordMode coord_mode, bool is_new, bool * updated)
{
#if 0
	GtkWidget * dialog = gtk_dialog_new_with_buttons(_("Waypoint Properties"),
							 parent,
							 (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
							 GTK_STOCK_CANCEL,
							 GTK_RESPONSE_REJECT,
							 GTK_STOCK_OK,
							 GTK_RESPONSE_ACCEPT,
							 NULL);

	GtkWidget *symbolentry;
	SGFileEntry * imageentry = NULL;
	GtkWidget *sourcelabel = NULL, *sourceentry = NULL;
	GtkWidget *typelabel = NULL, *typeentry = NULL;
	GtkWidget *timevaluebutton = NULL;
	GtkWidget *hasGeotagCB = NULL;
	GtkWidget *consistentGeotagCB = NULL;
	GtkListStore *store;


	struct LatLon ll = wp->coord.get_latlon();

	QString alt;
	const Qstring lat = QObject::tr("%1").arg(ll.lat);
	const QString lon = QObject::tr("%1").arg(ll.lon);
	vik_units_height_t height_units = a_vik_get_units_height();
	switch (height_units) {
	case VIK_UNITS_HEIGHT_METRES:
		alt = QObject::tr("%1").arg(wp->altitude);
		break;
	case VIK_UNITS_HEIGHT_FEET:
		alt = QObject::tr("%1").arg(VIK_METERS_TO_FEET(wp->altitude));
		break;
	default:
		alt = QObject::tr("%1").arg(wp->altitude);
		qDebug() << "EE: Waypoint Properties: dialog: invalid height units:" << (int) height_units;
	}

	*updated = false;

	QLabel * namelabel = new QLabel(QObject::tr("Name:"));
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), namelabel, false, false, 0);
	/* Name is now always changeable. */
	QLineEdit * nameentry = new QLineEdit();
	if (default_name) {
		nameentry->setText(default_name);
	}
	QObject::connect(nameentry, SIGNAL("activate"), dialog, SLOT (accept));
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), nameentry, false, false, 0);



	QLabel * latlabel = new QLabel(QObject::tr("Latitude:"));
	QLineEdit * latentry = new QLineEdit(lat);

	QLabel * lonlabel = new QLabel(QObject::tr("Longitude:"));
	QLineEdit * lonentry = new QLineEdit(lon);

	QLabel * altlabel = new QLabel(QObject::tr("Altitude:"));
	QLineEdit * altentry = new QLineEdit(alt);




	QLabel * commentlabel = NULL;
	if (wp->comment && !strncmp(wp->comment, "http", 4)) {
		commentlabel = gtk_link_button_new_with_label(wp->comment, _("Comment:"));
	} else {
		commentlabel = new QLabel(QObject::tr("Comment:"));
	}
	QLineEdit * commententry = new QLineEdit();

	/* Auto put in some kind of 'name' as a comment if one previously 'goto'ed this exact location. */
	const QString cmt = a_vik_goto_get_search_string_for_this_location(trw->get_window());
	if (!cmt.isEmpty()) {
		commententry->setText(cmt);
	}

	QLabel * descriptionlabel = NULL;
	if (wp->description && !strncmp(wp->description, "http", 4)) {
		descriptionlabel = gtk_link_button_new_with_label(wp->description, _("Description:"));
	} else {
		descriptionlabel = new QLabel(QObject::tr("Description:"));
	}
	QLineEdit * descriptionentry = new QLineEdit();

	QLabel * imagelabel = new QLabel(QObject::tr("Image:"));
	SGFileEntry * imageentry = new SGFileEntry(enum QFileDialog::Option options, enum QFileDialog::FileMode mode, SGFileTypeFilter file_type_filter, QString & title, QWidget * parent); vik_file_entry_new(GTK_FILE_CHOOSER_ACTION_OPEN, SGFileTypeFilter::IMAGE, NULL, NULL);

	QLabel * symbollabel = NULL;
	{
		GtkCellRenderer *r;
		symbollabel = new QLabel(QObject::tr("Symbol:"));
		GtkTreeIter iter;

		store = gtk_list_store_new(3, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING);
		symbolentry = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
		gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(symbolentry), 6);

		QObject::connect(symbolentry, SIGNAL("changed"), store, SLOT (symbol_entry_changed_cb));
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, NULL, 1, NULL, 2, _("(none)"), -1);
		a_populate_sym_list(store);

		r = gtk_cell_renderer_pixbuf_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (symbolentry), r, false);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT (symbolentry), r, "pixbuf", 1, NULL);

		r = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (symbolentry), r, false);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT (symbolentry), r, "text", 2, NULL);

		if (!is_new && wp->symbol) {
			bool ok;
			char *sym;
			for (ok = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter); ok; ok = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter)) {
				gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, (void *)&sym, -1);
				if (sym && !strcmp(sym, wp->symbol)) {
					free(sym);
					break;
				} else {
					free(sym);
				}
			}
			/* Ensure is it a valid symbol in the given symbol set (large vs small).
			   Not all symbols are available in both.
			   The check prevents a Gtk Critical message. */
			if (iter.stamp) {
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(symbolentry), &iter);
			}
		}
	}

	if (!is_new && wp->comment) {
		commententry->setText(wp->comment);
	}

	if (!is_new && wp->description) {
		descriptionentry->setText(wp->description);
	}

	if (!is_new && wp->image) {
		imageentry->set_filename(wp->image);

#ifdef VIK_CONFIG_GEOTAG
		/* Geotag Info [readonly]. */
		hasGeotagCB = gtk_check_button_new_with_label(_("Has Geotag"));
		gtk_widget_set_sensitive(hasGeotagCB, false);
		bool hasGeotag;
		char *ignore = a_geotag_get_exif_date_from_file(wp->image, &hasGeotag);
		free(ignore);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hasGeotagCB), hasGeotag);

		consistentGeotagCB = gtk_check_button_new_with_label(_("Consistent Position"));
		gtk_widget_set_sensitive(consistentGeotagCB, false);
		if (hasGeotag) {
			struct LatLon ll = a_geotag_get_position(wp->image);
			Coord coord(ll, coord_mode);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(consistentGeotagCB), coord == wp->coord);
		}
#endif
	}

	QLabel * timelabel = new QLabel(QObject::tr("Time:"));
	timevaluebutton = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(timevaluebutton), GTK_RELIEF_NONE);

	/* kamilFIXME: are we overwriting wp here? */
	if (!edit_wp) {
		edit_wp = new Waypoint();
	}
	edit_wp = new Waypoint(*wp);

	/* TODO: Consider if there should be a remove time button... */

	if (!is_new && wp->has_timestamp) {
		update_time(timevaluebutton, wp);
	} else {
		GtkWidget *img = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
		gtk_button_set_image(GTK_BUTTON(timevaluebutton), img);
		/* Initially use current time or otherwise whatever the last value used was. */
		if (edit_wp->timestamp == 0) {
			time(&edit_wp->timestamp);
		}
	}
	QObject::connect(timevaluebutton, SIGNAL("button-release-event"), edit_wp, SLOT (time_edit_click));


	if (hasGeotagCB) {
		GtkWidget *hbox =  gtk_hbox_new(false, 0);
		hbox->addWidget(hasGeotagCB);
		hbox->addWidget(consistentGeotagCB);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox, false, false, 0);
	}

	if (!is_new) {
		/* Shift left/right/up/down to try not to obscure the waypoint. */
		trw->dialog_shift(GTK_WINDOW(dialog), &wp->coord, false);
	}

	while (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		if (nameentry->text().length() == 0) {  /* TODO: other checks (isalpha or whatever). */
			Dialog::info(tr("Please enter a name for the waypoint."), parent);
		} else {
			/* NB: No check for unique names - this allows generation of same named entries. */
			char *entered_name = nameentry->text();

			/* Do It. */
			ll.lat = convert_dms_to_dec(latentry->text());
			ll.lon = convert_dms_to_dec(lonentry->text());
			wp->coord = Coord(ll, coord_mode);
			/* Always store in metres. */
			switch (height_units) {
			case HeightUnit::METRES:
				wp->altitude = atof(altentry->text());
				break;
			case HeightUnit::FEET:
				wp->altitude = VIK_FEET_TO_METERS(atof(altentry->text()));
				break;
			default:
				wp->altitude = atof(altentry->text());
				fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
			}
			if (g_strcmp0(wp->comment, commententry->text()))
				wp->set_comment(commententry->text());
			if (g_strcmp0(wp->description, descriptionentry->text()))
				wp->set_description(descriptionentry->text());
			if (g_strcmp0(wp->image, imageentry->get_filename()))
				wp->set_image(imageentry->get_filename());
			if (g_strcmp0(wp->source, sourceentry->text()))
				wp->set_source(sourceentry->text());
			if (g_strcmp0(wp->type, typeentry->text()))
				wp->set_type(typeentry->text());
			if (wp->image&& *(wp->image) && (!a_thumbnails_exists(wp->image)))
				a_thumbnails_create (wp->image);
			if (edit_wp->timestamp) {
				wp->timestamp = edit_wp->timestamp;
				wp->has_timestamp = true;
			}

			GtkTreeIter iter, first;
			gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &first);
			if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(symbolentry), &iter) || !memcmp(&iter, &first, sizeof(GtkTreeIter))) {
				wp->set_symbol(NULL);
			} else {
				char *sym;
				gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, (void *)&sym, -1);
				wp->set_symbol(sym);
				free(sym);
			}

			gtk_widget_destroy(dialog);
			if (is_new) {
				return entered_name;
			} else {
				*updated = true;
				/* See if name has been changed. */
				if (g_strcmp0(default_name, entered_name)) {
					return entered_name;
				} else {
					return NULL;
				}
			}
		}
	}
	gtk_widget_destroy(dialog);
#endif
	return NULL;
}


/**
   Dialog displays \p default_name as name of waypoint.
   For existing waypoints you should pass wp->name as value of this argument.
   For new waypoints you should pass some auto-generated name as value of this argument.

   Final name of the waypoint (accepted in the dialog) is returned. If
   user rejected the dialog (e.g by pressing Cancel button), the
   returned string is empty.
*/
QString SlavGPS::waypoint_properties_dialog(QWidget * parent, const QString & default_name, LayerTRW * trw, Waypoint * wp, CoordMode coord_mode, bool is_new, bool * updated)
{
	PropertiesDialog dialog(QObject::tr("Waypoint Properties"), parent);
	dialog.fill(wp, wp_params, default_name);
	int dialog_code = dialog.exec();

	QString entered_name;

	if (dialog_code == QDialog::Accepted) {

		SGVariant param_value;

		param_value = dialog.get_param_value(SG_WP_PARAM_NAME, &wp_params[SG_WP_PARAM_NAME]);
		entered_name = param_value.s;
		wp->set_name(entered_name);


		param_value = dialog.get_param_value(SG_WP_PARAM_LAT, &wp_params[SG_WP_PARAM_LAT]);

		param_value = dialog.get_param_value(SG_WP_PARAM_LON, &wp_params[SG_WP_PARAM_LON]);


		param_value = dialog.get_param_value(SG_WP_PARAM_TIME, &wp_params[SG_WP_PARAM_TIME]);
		wp->timestamp = param_value.u;

		param_value = dialog.get_param_value(SG_WP_PARAM_ALT, &wp_params[SG_WP_PARAM_ALT]);
		//wp->alt = ;

		param_value = dialog.get_param_value(SG_WP_PARAM_COMMENT, &wp_params[SG_WP_PARAM_COMMENT]);
		wp->set_comment(param_value.s);

		param_value = dialog.get_param_value(SG_WP_PARAM_DESC, &wp_params[SG_WP_PARAM_DESC]);
		wp->set_description(param_value.s);

		param_value = dialog.get_param_value(SG_WP_PARAM_IMAGE, &wp_params[SG_WP_PARAM_IMAGE]);
		wp->set_image(param_value.s);

		param_value = dialog.get_param_value(SG_WP_PARAM_SYMBOL, &wp_params[SG_WP_PARAM_SYMBOL]);
		wp->set_symbol_name(param_value.s);
	}

	return entered_name;
}
