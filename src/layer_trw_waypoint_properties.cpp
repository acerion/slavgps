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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




#include <QDebug>




#ifdef K_INCLUDES
#ifdef VIK_CONFIG_GEOTAG
#include "geotag_exif.h"
#endif
#include "goto.h"
#include "vikutils.h"
#include "widget_file_entry.h"
#endif

#include "layer_trw_waypoint_properties.h"
#include "layer_trw_waypoint.h"
#include "dialog.h"
#include "ui_builder.h"
#include "date_time_dialog.h"
#include "degrees_converters.h"
#include "preferences.h"
#include "thumbnails.h"
#include "garmin_symbols.h"




using namespace SlavGPS;




#define SG_MODULE "Waypoint Poperties"




static ParameterSpecification wp_param_specs[] = {
	{ SG_WP_PARAM_NAME,     "trw_wp_prop_name",     SGVariantType::String,     PARAMETER_GROUP_GENERIC,  QObject::tr("Name"),         WidgetType::Entry,         NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_LAT,      "trw_wp_prop_lat",      SGVariantType::Latitude,   PARAMETER_GROUP_GENERIC,  QObject::tr("Latitude"),     WidgetType::Entry,         NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_LON,      "trw_wp_prop_lon",      SGVariantType::Longitude,  PARAMETER_GROUP_GENERIC,  QObject::tr("Longitude"),    WidgetType::Entry,         NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_TIME,     "trw_wp_prop_time",     SGVariantType::Timestamp,  PARAMETER_GROUP_GENERIC,  QObject::tr("Time"),         WidgetType::DateTime,      NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_ALT,      "trw_wp_prop_alt",      SGVariantType::Altitude,   PARAMETER_GROUP_GENERIC,  QObject::tr("Altitude"),     WidgetType::Entry,         NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_COMMENT,  "trw_wp_prop_comment",  SGVariantType::String,     PARAMETER_GROUP_GENERIC,  QObject::tr("Comment"),      WidgetType::Entry,         NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_DESC,     "trw_wp_prop_desc",     SGVariantType::String,     PARAMETER_GROUP_GENERIC,  QObject::tr("Description"),  WidgetType::Entry,         NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_IMAGE,    "trw_wp_prop_image",    SGVariantType::String,     PARAMETER_GROUP_GENERIC,  QObject::tr("Image"),        WidgetType::FileSelector,  NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_SYMBOL,   "trw_wp_prop_symbol",   SGVariantType::String,     PARAMETER_GROUP_GENERIC,  QObject::tr("Symbol"),       WidgetType::ComboBox,      NULL, NULL, NULL, NULL },

	{ SG_WP_PARAM_MAX,      "",                     SGVariantType::Empty,      PARAMETER_GROUP_GENERIC,  "",                          WidgetType::Entry,         NULL, NULL, NULL, NULL },
};




/**
   Dialog displays \p default_name as name of waypoint.
   For existing waypoints you should pass wp->name as value of this argument.
   For new waypoints you should pass some auto-generated name as value of this argument.

   Final name of the waypoint (accepted in the dialog) is returned. If
   user rejected the dialog (e.g by pressing Cancel button), the
   returned string is empty.

   Return tuple:
   SG_WP_DIALOG_OK: Dialog returns OK, values were correctly set/edited.
   SG_WP_DIALOG_NAME: Waypoint's name has been edited and/or is different than @default_name
*/
std::tuple<bool, bool> SlavGPS::waypoint_properties_dialog(Waypoint * wp, const QString & default_name, CoordMode coord_mode, QWidget * parent)
{
	/* This function may be called on existing waypoint with
	   existing name.  In that case @default_name argument refers
	   to waypoint's member.  At the end of this function we
	   compare value of default name with waypoint's name, so the
	   two variables can't refer to the same thing. We need to
	   make a copy here. */
	const QString default_wp_name = default_name;

	std::tuple<bool, bool> result(false, false);

	std::map<param_id_t, ParameterSpecification *> param_specs;
	for (param_id_t param_id = 0; param_id < SG_WP_PARAM_MAX; param_id++) {
		param_specs.insert(std::pair<param_id_t, ParameterSpecification *>(param_id, &wp_param_specs[param_id]));
	}


	std::map<param_id_t, SGVariant> values;
	{
		const LatLon lat_lon = wp->coord.get_latlon();

		values.insert(std::pair<param_id_t, SGVariant>(SG_WP_PARAM_NAME, SGVariant(default_wp_name))); /* TODO_LATER: This should be somehow taken from param_specs->default */

		values.insert(std::pair<param_id_t, SGVariant>(SG_WP_PARAM_LAT, SGVariant(lat_lon.lat, SGVariantType::Latitude)));

		values.insert(std::pair<param_id_t, SGVariant>(SG_WP_PARAM_LON, SGVariant(lat_lon.lon, SGVariantType::Longitude)));

		values.insert(std::pair<param_id_t, SGVariant>(SG_WP_PARAM_TIME, SGVariant(wp->timestamp, SGVariantType::Timestamp)));
#ifdef K_FIXME_RESTORE
		QObject::connect(timevaluebutton, SIGNAL("button-release-event"), edit_wp, SLOT (time_edit_click));
#endif

		QString alt;
		const HeightUnit user_height_unit = Preferences::get_unit_height();
		Altitude alt_dialog;
		if (wp->altitude.is_valid()) {
			/* Waypoint stored altitude in meters. */
			alt_dialog = wp->altitude.convert_to_unit(user_height_unit);
		}
		values.insert(std::pair<param_id_t, SGVariant>(SG_WP_PARAM_ALT, SGVariant(alt)));

		/* TODO_MAYBE: comment may contain URL. Make the label or input field clickable. */
		values.insert(std::pair<param_id_t, SGVariant>(SG_WP_PARAM_COMMENT, SGVariant(wp->comment)));

		/* TODO_MAYBE: description may contain URL. Make the label or input field clickable. */
		values.insert(std::pair<param_id_t, SGVariant>(SG_WP_PARAM_DESC, SGVariant(wp->description)));

		/* TODO_MAYBE: perhaps add file filter for image files? */
		values.insert(std::pair<param_id_t, SGVariant>(SG_WP_PARAM_IMAGE, SGVariant(wp->image_full_path)));

		values.insert(std::pair<param_id_t, SGVariant>(SG_WP_PARAM_SYMBOL, SGVariant(wp->symbol_name)));
	}



	PropertiesDialogWaypoint dialog(wp, QObject::tr("Waypoint Properties"), parent);
	const std::vector<SGLabelID> empty_parameter_groups; /* No parameter groups -> dialog class will create only one tab with default label. */
	dialog.fill(param_specs, values, empty_parameter_groups);

	dialog.date_time_button = (SGDateTimeButton *) dialog.get_widget(wp_param_specs[SG_WP_PARAM_TIME]);
	if (wp->has_timestamp) {
		/* This should force drawing time label on date/time
		   button.  The label represents timestamp in specific
		   time reference system.  If the time reference
		   system (read from preferences) is World, the widget
		   will infer time zone from waypoint's coordinate and
		   use the time zone to generate the label. */
		dialog.date_time_button->set_coord(wp->coord);
	}
	QObject::connect(dialog.date_time_button, SIGNAL (value_is_set(time_t)), &dialog, SLOT (set_timestamp_cb(time_t)));
	QObject::connect(dialog.date_time_button, SIGNAL (value_is_reset(void)), &dialog, SLOT (clear_timestamp_cb(void)));

	dialog.symbol_combo = (QComboBox *) dialog.get_widget(wp_param_specs[SG_WP_PARAM_SYMBOL]);
	QObject::connect(dialog.symbol_combo, SIGNAL (currentIndexChanged(int)), &dialog, SLOT (symbol_entry_changed_cb(int)));
	GarminSymbols::populate_symbols_list(dialog.symbol_combo, wp->symbol_name);


	/*
	  TODO_LATER: changes to coordinates of waypoint need to be
	  translated "in real time" (without the need to close the
	  dialog window) to position of waypoint on viewport.

	  See how this is done for Trackpoint in Trackpoint properties
	  dialog.
	*/

	/*
	  TODO_LATER: changes in coordinates of waypoint need to be passed
	  to datetime button, because in some cases (in World time
	  reference system) the value of button label depends on
	  coordinates of waypoint.

	  So each change to the coordinates must result in call to
	  SGDateTimeButton::set_coord().
	*/

	while (QDialog::Accepted == dialog.exec()) {

		SGVariant param_value;

		param_value = dialog.get_param_value(wp_param_specs[SG_WP_PARAM_NAME]);
		const QString entered_name = param_value.val_string;

		if (entered_name.isEmpty()) { /* TODO_2_LATER: other checks (isalpha or whatever). */
			Dialog::info(QObject::tr("Please enter a name for the waypoint."), parent);
			continue;
		}

		/* We don't check for unique names: this allows generation of waypoints with the same name. */
		wp->set_name(entered_name);


		LatLon lat_lon;
		param_value = dialog.get_param_value(wp_param_specs[SG_WP_PARAM_LAT]);
		lat_lon.lat = param_value.get_latitude();
		param_value = dialog.get_param_value(wp_param_specs[SG_WP_PARAM_LON]);
		lat_lon.lon = param_value.get_longitude();
		wp->coord = Coord(lat_lon, coord_mode);


		param_value = dialog.get_param_value(wp_param_specs[SG_WP_PARAM_TIME]);
		wp->timestamp = param_value.get_timestamp();
		wp->has_timestamp = wp->timestamp != 0; /* TODO_LATER: zero value may still be a valid time stamp. */


		/* Always store Altitude in metres. */
		param_value = dialog.get_param_value(wp_param_specs[SG_WP_PARAM_ALT]);
		const HeightUnit current_height_unit = Preferences::get_unit_height();
		const HeightUnit target_height_unit = HeightUnit::Metres;
		const Altitude dialog_alt = Altitude(param_value.get_altitude(), current_height_unit);
		wp->altitude = dialog_alt.convert_to_unit(target_height_unit);


		param_value = dialog.get_param_value(wp_param_specs[SG_WP_PARAM_COMMENT]);
		wp->set_comment(param_value.val_string);

		param_value = dialog.get_param_value(wp_param_specs[SG_WP_PARAM_DESC]);
		wp->set_description(param_value.val_string);

		param_value = dialog.get_param_value(wp_param_specs[SG_WP_PARAM_IMAGE]);
		wp->set_image_full_path(param_value.val_string);
		if (!wp->image_full_path.isEmpty()) {
			Thumbnails::generate_thumbnail_if_missing(wp->image_full_path);
		}


		if (!wp->image_full_path.isEmpty()) {
			QImage image = QImage(wp->image_full_path);
		}

		param_value = dialog.get_param_value(wp_param_specs[SG_WP_PARAM_SYMBOL]);
		if (GarminSymbols::is_none_symbol_name(param_value.val_string)) {
			wp->set_symbol(""); /* Save empty string instead of actual "none" string. */
		} else {
			wp->set_symbol(param_value.val_string);
		}

#ifdef TODO_LATER
		if (wp->source != sourceentry->text()) {
			wp->set_source(sourceentry->text());
		}
		if (wp->type != typeentry->text()) {
			wp->set_type(typeentry->text());
		}
#endif

		std::get<SG_WP_DIALOG_OK>(result) = true;
		std::get<SG_WP_DIALOG_NAME>(result) = default_wp_name != wp->name;


		return result;
	}

	return result;
}




PropertiesDialogWaypoint::PropertiesDialogWaypoint(Waypoint * wp_, QString const & title, QWidget * parent) : PropertiesDialog(title, parent)
{
	this->wp = wp_;
}




void PropertiesDialogWaypoint::set_timestamp_cb(time_t timestamp)
{
	this->date_time_button->set_label(timestamp, this->wp->coord);
}




void PropertiesDialogWaypoint::clear_timestamp_cb(void)
{
	this->date_time_button->clear_label();
}




void PropertiesDialogWaypoint::symbol_entry_changed_cb(int index)
{
#if 0
	GtkTreeIter iter;
	gchar *sym;

	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, (void *)&sym, -1 );
	/* Note: symm is NULL when "(none)" is select (first cell is empty) */
	gtk_widget_set_tooltip_text(combo, sym);
	g_free(sym);
#endif
}




#ifdef K_OLD_IMPLEMENTATION


/* Specify if a new waypoint or not. */
/* If a new waypoint then it uses the default_name for the suggested name allowing the user to change it.
   The name to use is returned.
*/
char * a_dialog_waypoint(Window * parent, char * default_name, Waypoint * wp, CoordMode coord_mode, bool is_new, bool * updated)
{

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
		gtk_list_store_set(store, &iter, 0, NULL, 1, NULL, 2, QObject::tr("(none)"), -1);
		GarminSymbols::populate_symbols_list(store);

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


#ifdef VIK_CONFIG_GEOTAG
	/* Geotag Info [readonly]. */
	hasGeotagCB = new QCheckBox(QObject::tr("Has Geotag"));
	hasGeotagCB->setEnabled(false);
	bool hasGeotag;
	GeotagExif::get_exif_date_from_file(wp->image, &hasGeotag);
	hasGeotagCB->setChecked(hasGeotag);

	consistentGeotagCB = new QCheckBox(QObject::tr("Consistent Position"));
	consistentGeotagCB->setEnabled(false);
	if (hasGeotag) {
		const Coord coord(GeotagExif::get_position(wp->image), coord_mode);
		consistentGeotagCB->setChecked(coord == wp->coord);
	}
#endif


	if (hasGeotagCB) {
		GtkWidget *hbox =  gtk_hbox_new(false, 0);
		hbox->addWidget(hasGeotagCB);
		hbox->addWidget(consistentGeotagCB);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox, false, false, 0);
	}

	if (!is_new) {
		/* Shift left/right/up/down to try not to obscure the waypoint. */
		const GlobalPoint point_to_expose = SGUtils::coord_to_global_point(wp->coord);
		Dialog::move_dialog(GTK_WINDOW(dialog), point_to_expose, false);
	}

	return NULL;
}


#endif
