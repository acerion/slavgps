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

#include <QDebug>

#include "degrees_converters.h"
#include "preferences.h"
#if 0
#include "garminsymbols.h"
#ifdef VIK_CONFIG_GEOTAG
#include "geotag_exif.h"
#endif
#include "thumbnails.h"
#include "goto.h"
#include "vikutils.h"
#include "widget_file_entry.h"
#endif
#include "layer_trw_waypoint_properties.h"
#include "layer_trw_waypoint.h"
#include "dialog.h"
#include "globals.h"
#include "ui_builder.h"
#include "date_time_dialog.h"




using namespace SlavGPS;




ParameterSpecification wp_param_specs[] = {
	{ SG_WP_PARAM_NAME,     NULL, "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  QObject::tr("Name"),         WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_LAT,      NULL, "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  QObject::tr("Latitude"),     WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_LON,      NULL, "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  QObject::tr("Longitude"),    WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_TIME,     NULL, "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  QObject::tr("Time"),         WidgetType::DATETIME,    NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_ALT,      NULL, "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  QObject::tr("Altitude"),     WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_COMMENT,  NULL, "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  QObject::tr("Comment"),      WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_DESC,     NULL, "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  QObject::tr("Description"),  WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_IMAGE,    NULL, "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  QObject::tr("Image"),        WidgetType::FILEENTRY,   NULL, NULL, NULL, NULL },
	{ SG_WP_PARAM_SYMBOL,   NULL, "",  SGVariantType::STRING,  PARAMETER_GROUP_GENERIC,  QObject::tr("Symbol"),       WidgetType::ENTRY,       NULL, NULL, NULL, NULL },
	/* TODO: where is guard item? */
};




/**
   Dialog displays \p default_name as name of waypoint.
   For existing waypoints you should pass wp->name as value of this argument.
   For new waypoints you should pass some auto-generated name as value of this argument.

   Final name of the waypoint (accepted in the dialog) is returned. If
   user rejected the dialog (e.g by pressing Cancel button), the
   returned string is empty.
*/
QString SlavGPS::waypoint_properties_dialog(QWidget * parent, const QString & default_name, Waypoint * wp, CoordMode coord_mode, bool is_new, bool * updated)
{
	PropertiesDialog dialog(QObject::tr("Waypoint Properties"), parent);
	dialog.fill(wp, wp_param_specs, default_name);

	while (QDialog::Accepted == dialog.exec()) {

		bool conversion_ok;
		SGVariant param_value;

		param_value = dialog.get_param_value(SG_WP_PARAM_NAME, &wp_param_specs[SG_WP_PARAM_NAME]);
		const QString entered_name = param_value.val_string;

		if (entered_name.isEmpty()) { /* TODO: other checks (isalpha or whatever). */
			Dialog::info(QObject::tr("Please enter a name for the waypoint."), parent);
			continue;
		}

		/* We don't check for unique names: this allows generation of waypoints with the same name. */
		wp->set_name(entered_name);


		struct LatLon lat_lon;
		param_value = dialog.get_param_value(SG_WP_PARAM_LAT, &wp_param_specs[SG_WP_PARAM_LAT]);
		lat_lon.lat = convert_dms_to_dec(param_value.val_string.toUtf8().constData());
		param_value = dialog.get_param_value(SG_WP_PARAM_LON, &wp_param_specs[SG_WP_PARAM_LON]);
		lat_lon.lon = convert_dms_to_dec(param_value.val_string.toUtf8().constData());
		wp->coord = Coord(lat_lon, coord_mode);


		param_value = dialog.get_param_value(SG_WP_PARAM_TIME, &wp_param_specs[SG_WP_PARAM_TIME]);
		wp->timestamp = param_value.val_string.toULong(&conversion_ok);
		wp->has_timestamp = conversion_ok && wp->timestamp;


		/* Always store Altitude in metres. */
		param_value = dialog.get_param_value(SG_WP_PARAM_ALT, &wp_param_specs[SG_WP_PARAM_ALT]);
		const HeightUnit height_unit = Preferences::get_unit_height();
		switch (height_unit) {
		case HeightUnit::METRES:
			wp->altitude = param_value.val_string.toFloat();
			break;
		case HeightUnit::FEET:
			wp->altitude = VIK_FEET_TO_METERS(param_value.val_string.toFloat());
			break;
		default:
			wp->altitude = param_value.val_string.toFloat();
			qDebug() << "EE: Waypoint Properties" << __FUNCTION__ << "unknown height unit" << (int) height_unit;
		}



		param_value = dialog.get_param_value(SG_WP_PARAM_COMMENT, &wp_param_specs[SG_WP_PARAM_COMMENT]);
		wp->set_comment(param_value.val_string);

		param_value = dialog.get_param_value(SG_WP_PARAM_DESC, &wp_param_specs[SG_WP_PARAM_DESC]);
		wp->set_description(param_value.val_string);

		param_value = dialog.get_param_value(SG_WP_PARAM_IMAGE, &wp_param_specs[SG_WP_PARAM_IMAGE]);
		wp->set_image(param_value.val_string);

		param_value = dialog.get_param_value(SG_WP_PARAM_SYMBOL, &wp_param_specs[SG_WP_PARAM_SYMBOL]);
		wp->set_symbol_name(param_value.val_string);
#ifdef K
		if (g_strcmp0(wp->source, sourceentry->text())) {
			wp->set_source(sourceentry->text());
		}
		if (g_strcmp0(wp->type, typeentry->text())) {
			wp->set_type(typeentry->text());
		}
		if (wp->image&& *(wp->image) && (!Thumbnails::thumbnail_exists(wp->image))) {
			Thumbnails::generate_thumbnail(wp->image);
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
#endif

		if (is_new) {
			return entered_name;
		} else {
			*updated = true;
			/* See if name has been changed. */
			if (default_name != entered_name) {
				return entered_name;
			} else {
				return "";
			}
		}
	}

	return "";
}




#if 0

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

	time_t new_timestamp = 0;
	if (!date_time_dialog(QObject::tr("Date/Time Edit"), wp->timestamp, new_timestamp, parent)) {
		/* The dialog was cancelled. */
		return;
	} else {
		edit_wp->timestamp = new_timestamp;
	}

	/* Clear the previous 'Add' image as now a time is set. */
	if (gtk_button_get_image(GTK_BUTTON(widget))) {
		gtk_button_set_image(GTK_BUTTON(widget), NULL);
	}

	const QString msg = vu_get_time_string(&wp->timestamp, "%c", &wp->coord, NULL);
	gtk_button_set_label(GTK_BUTTON(widget), msg);
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




/* Specify if a new waypoint or not. */
/* If a new waypoint then it uses the default_name for the suggested name allowing the user to change it.
   The name to use is returned.
*/
/* TODO: less on this side, like add track. */
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
		gtk_list_store_set(store, &iter, 0, NULL, 1, NULL, 2, _("(none)"), -1);
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
	char *ignore = a_geotag_get_exif_date_from_file(wp->image, &hasGeotag);
	free(ignore);
	hasGeotagCB->setChecked(hasGeotag);

	consistentGeotagCB = new QCheckBox(QObject::tr("Consistent Position"));
	consistentGeotagCB->setEnabled(false);
	if (hasGeotag) {
		struct LatLon ll = a_geotag_get_position(wp->image);
		Coord coord(ll, coord_mode);
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
		trw->dialog_shift(GTK_WINDOW(dialog), &wp->coord, false);
	}

	return NULL;
}
#endif
