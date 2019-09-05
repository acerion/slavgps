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




#include <QDebug>




#include "layer_trw_waypoint_properties.h"
#include "layer_trw_waypoint.h"
#include "dialog.h"
#include "ui_builder.h"
#include "date_time_dialog.h"
#include "lat_lon.h"
#include "preferences.h"
#include "thumbnails.h"
#include "garmin_symbols.h"
#include "widget_coord_display.h"
#include "widget_file_entry.h"
#include "widget_timestamp.h"




using namespace SlavGPS;




#define SG_MODULE "Waypoint Poperties"




/**
   Dialog displays \p default_wp_name as name of waypoint.
   For existing waypoints you should pass wp->name as value of this argument.
   For new waypoints you should pass some auto-generated name as value of this argument.

   Final name of the waypoint (accepted in the dialog) is returned. If
   user rejected the dialog (e.g by pressing Cancel button), the
   returned string is empty.

   Return tuple:
   SG_WP_DIALOG_OK: Dialog returns OK, values were correctly set/edited.
   SG_WP_DIALOG_NAME: Waypoint's name has been edited and/or is different than @default_name
*/
std::tuple<bool, bool> SlavGPS::waypoint_properties_dialog(Waypoint * wp, const QString & default_wp_name, CoordMode coord_mode, QWidget * parent)
{
	/* This function may be called on existing waypoint with
	   existing name.  In that case @default_name argument refers
	   to waypoint's member.  At the end of this function we
	   compare value of default name with waypoint's name, so the
	   two variables can't refer to the same thing. We need to
	   make a copy here. */
	const QString default_wp_name_copy = default_wp_name;


	WpPropertiesDialog dialog(wp, parent);;
	QString name = wp->name;
	if (name.isEmpty()) {
		name = QObject::tr("New Waypoint");
	}
	dialog.set_dialog_data(wp, name);

	std::tuple<bool, bool> result(false, false);

#if 0
	QObject::connect(timevaluebutton, SIGNAL("button-release-event"), edit_wp, SLOT (time_edit_click));
#endif


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
		const QString entered_name = dialog.name_entry->text();
		if (entered_name.isEmpty()) { /* TODO_MAYBE: other checks (isalpha or whatever). */
			Dialog::info(QObject::tr("Please enter a name for the waypoint."), parent);
			continue;
		}

		dialog.save_from_dialog(wp);

		std::get<SG_WP_DIALOG_OK>(result) = true;
		std::get<SG_WP_DIALOG_NAME>(result) = default_wp_name_copy != wp->name;


		return result;
	}

	return result;
}




WpPropertiesDialog::WpPropertiesDialog(Waypoint * wp, QWidget * parent_widget) : QDialog(parent_widget)
{
	this->current_object = wp;
 	this->setWindowTitle(tr("Waypoint Properties"));

	this->button_box = new QDialogButtonBox();
	this->button_box->addButton(tr("&Close"), QDialogButtonBox::AcceptRole);
	this->button_box->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);


	 /* Without this connection the dialog wouldn't close.  Button
	    box is sending accepted() signal thanks to AcceptRole of
	    "Close" button, configured above. */
	connect(this->button_box, SIGNAL (accepted()), this, SLOT (accept()));

	this->vbox = new QVBoxLayout;
	this->grid = new QGridLayout();
	this->vbox->addLayout(this->grid);
	this->vbox->addWidget(this->button_box);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);


	this->build_widgets(this);
}




WpPropertiesDialog::~WpPropertiesDialog()
{
}




sg_ret WpPropertiesDialog::build_widgets(QWidget * parent_widget)
{
	int row = 0;
	const int left_col = 0;
	const int right_col = 1;


	this->name_entry = new QLineEdit("", this);
	this->grid->addWidget(new QLabel(tr("Name:")), row, left_col);
	this->grid->addWidget(this->name_entry, row, right_col);
	connect(this->name_entry, SIGNAL (textEdited(const QString &)), this, SLOT (sync_name_entry_to_current_object_cb(const QString &)));

	row++;

	this->coord_entry = new CoordEntryWidget(CoordMode::LatLon); /* TODO: argument to constructor must be somehow calculated. */
	this->grid->addWidget(this->coord_entry, row, left_col, 1, 2);
	connect(this->coord_entry, SIGNAL (value_changed(void)), this, SLOT (sync_coord_entry_to_current_object_cb(void)));

	row++;

	this->timestamp_widget = new TimestampWidget(parent_widget);
	this->grid->addWidget(this->timestamp_widget, row, left_col, 1, 2);
	connect(this->timestamp_widget, SIGNAL (value_is_set(const Time &)), this, SLOT (sync_timestamp_entry_to_current_object_cb(const Time &)));
	connect(this->timestamp_widget, SIGNAL (value_is_reset()), this, SLOT (sync_empty_timestamp_entry_to_current_object_cb(void)));

	row++;

	/* At this level every altitude info should be in internal units. */
	const HeightUnit height_unit = Altitude::get_internal_unit();
	MeasurementScale<Altitude> scale_alti(Altitude(SG_ALTITUDE_RANGE_MIN, height_unit),
					      Altitude(SG_ALTITUDE_RANGE_MAX, height_unit),
					      Altitude(0.0, height_unit),
					      Altitude(1, height_unit),
					      SG_ALTITUDE_PRECISION);
	this->altitude_entry = new MeasurementEntry_2<Altitude, HeightUnit>(Altitude(), &scale_alti, this);
	this->grid->addWidget(new QLabel(tr("Altitude:")), row, left_col);
	this->grid->addWidget(this->altitude_entry->me_widget, row, right_col);
	connect(this->altitude_entry->me_widget->spin, SIGNAL (valueChanged(double)), this, SLOT (sync_altitude_entry_to_current_object_cb(void)));

	row++;

	/* TODO_MAYBE: comment may contain URL. Make the label or input field clickable. */
	this->comment_entry = new QLineEdit("", this);
	this->grid->addWidget(new QLabel(tr("Comment:")), row, left_col);
	this->grid->addWidget(this->comment_entry, row, right_col);
	connect(this->comment_entry, SIGNAL (textEdited(const QString &)), this, SLOT (sync_comment_entry_to_current_object_cb(const QString &)));

	row++;

	/* TODO_MAYBE: description may contain URL. Make the label or input field clickable. */
	this->description_entry = new QLineEdit("", this);
	this->grid->addWidget(new QLabel(tr("Description:")), row, left_col);
	this->grid->addWidget(this->description_entry, row, right_col);
	connect(this->description_entry, SIGNAL (textEdited(const QString &)), this, SLOT (sync_description_entry_to_current_object_cb(const QString &)));

	row++;

	/* TODO_MAYBE: perhaps add file filter for image files? */
	this->file_selector = new FileSelectorWidget(QFileDialog::Option(0), QFileDialog::ExistingFile, tr("Select file"), this);
	this->file_selector->set_file_type_filter(FileSelectorWidget::FileTypeFilter::Any);
	this->grid->addWidget(new QLabel(tr("Image:")), row, left_col);
	this->grid->addWidget(this->file_selector, row, right_col);
	connect(this->file_selector, SIGNAL (textEdited(const QString &)), this, SLOT (sync_file_selector_to_current_object_cb(void)));

	row++;

	this->symbol_combo = new QComboBox(this);
	GarminSymbols::populate_symbols_list(this->symbol_combo, GarminSymbols::none_symbol_name);
	this->grid->addWidget(new QLabel(tr("Symbol:")), row, left_col);
	this->grid->addWidget(this->symbol_combo, row, right_col);
	QObject::connect(this->symbol_combo, SIGNAL (currentIndexChanged(int)), this, SLOT (symbol_entry_changed_cb(int)));

	return sg_ret::ok;
}




sg_ret WpPropertiesDialog::set_dialog_data(Waypoint * object, const QString & name)
{
	this->name_entry->setText(name);
	this->coord_entry->set_value(object->coord); /* TODO: ::set_value() should re-build the widget according to mode of object->coord or according to global setting of coord mode? */
	this->timestamp_widget->set_timestamp(object->get_timestamp(), object->coord);
	this->altitude_entry->set_value_iu(object->altitude);
	this->comment_entry->setText(object->comment);
	this->description_entry->setText(object->description);
	this->file_selector->preselect_file_full_path(object->image_full_path);

	const QString & symbol_name = object->symbol_name.isEmpty() ? GarminSymbols::none_symbol_name : object->symbol_name;
	const int selected_idx = this->symbol_combo->findText(object->symbol_name);
	if (selected_idx == -1) {
		qDebug() << SG_PREFIX_E << "Waypoint symbol not found in combo:" << symbol_name;
		this->symbol_combo->setCurrentIndex(0); /* Index of first added item, which should be "none" symbol. */
	} else {
		this->symbol_combo->setCurrentIndex(selected_idx);
	}

	return sg_ret::ok;
}




sg_ret WpPropertiesDialog::reset_dialog_data(void)
{
	Waypoint wp; /* Invalid, empty object. */
	return this->set_dialog_data(&wp, wp.name);
}




void WpPropertiesDialog::save_from_dialog(Waypoint * saved_object)
{
	/* We don't check for unique names: this allows generation of
	   objects with the same name. */
	saved_object->set_name(this->name_entry->text());

	saved_object->coord = this->coord_entry->get_value();

	saved_object->set_timestamp(this->timestamp_widget->get_timestamp());

	/* Always store Altitude in wp in in internal units. */
	saved_object->altitude = this->altitude_entry->get_value_iu();

	saved_object->set_comment(this->comment_entry->text());

	saved_object->set_description(this->description_entry->text());


	const QString path = this->file_selector->get_selected_file_full_path();
	saved_object->set_image_full_path(path);
	if (!saved_object->image_full_path.isEmpty()) {
		Thumbnails::generate_thumbnail_if_missing(saved_object->image_full_path);
	}
	if (!saved_object->image_full_path.isEmpty()) {
#if K_TODO
		saved_object->image = QImage(saved_object->image_full_path);
#endif
	}


	const QString symbol_name = this->symbol_combo->currentText();
	if (GarminSymbols::is_none_symbol_name(symbol_name)) {
		saved_object->set_symbol(""); /* Save empty string instead of actual "none" string. */
	} else {
		saved_object->set_symbol(symbol_name);
	}


#ifdef TODO_LATER
	if (saved_object->source != sourceentry->text()) {
		saved_object->set_source(sourceentry->text());
	}
	if (saved_object->type != typeentry->text()) {
		saved_object->set_type(typeentry->text());
	}
#endif
}





void WpPropertiesDialog::symbol_entry_changed_cb(int index)
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




void WpPropertiesDialog::set_title(const QString & title)
{
	this->setWindowTitle(title);
}



void WpPropertiesDialog::sync_coord_entry_to_current_object_cb(void) /* Slot. */
{
	if (!this->current_object) {
		return;
	}
	if (this->sync_to_current_object_block) {
		return;
	}

	//// this->current_object->coord.get_coord_mode()

	const Coord new_coord = this->coord_entry->get_value();
	const bool redraw = Coord::distance(this->current_object->coord, new_coord) > 0.05; /* May not be exact due to rounding. */
	this->current_object->coord = new_coord;


	this->timestamp_widget->set_coord(new_coord);


	/* Don't redraw unless we really have to. */
	if (redraw) {
		/* Tell parent code that a edited object has changed
		   its coordinates. */
		emit this->object_coordinates_changed();
	}
}




void WpPropertiesDialog::sync_altitude_entry_to_current_object_cb(void) /* Slot. */
{
	if (!this->current_object) {
		return;
	}
	if (this->sync_to_current_object_block) {
		return;
	}

	/* Always store internally in metres. */
	this->current_object->altitude = this->altitude_entry->get_value_iu();
}




/* Set timestamp of current waypoint. */
void WpPropertiesDialog::sync_timestamp_entry_to_current_object_cb(const Time & timestamp)
{
	qDebug() << SG_PREFIX_SLOT << "Slot received new timestamp" << timestamp;

	this->set_timestamp_of_current_object(timestamp);
}




/* Clear timestamp of current waypoint. */
void WpPropertiesDialog::sync_empty_timestamp_entry_to_current_object_cb(void)
{
	qDebug() << SG_PREFIX_SLOT << "Slot received zero timestamp";

	this->set_timestamp_of_current_object(Time()); /* Invalid value - this should indicate that timestamp is cleared from the tp. */
}




void WpPropertiesDialog::sync_comment_entry_to_current_object_cb(const QString & comment)
{
	/* TODO: implement. */
}




void WpPropertiesDialog::sync_description_entry_to_current_object_cb(const QString & description)
{
	/* TODO: implement. */
}




void WpPropertiesDialog::sync_file_selector_to_current_object_cb(void)
{
	/* TODO: implement. */
}




bool WpPropertiesDialog::set_timestamp_of_current_object(const Time & timestamp)
{
	if (!this->current_object) {
		return false;
	}
	if (this->sync_to_current_object_block) {
		/* TODO_LATER: indicate to user that operation has failed. */
		return false;
	}

	/* TODO_LATER: we are changing a timestamp of tp somewhere in
	   the middle of a track, so the timestamps may now not have
	   consecutive values.  Should we now warn user about unsorted
	   timestamps in consecutive trackpoints? */

	this->current_object->set_timestamp(timestamp);

	return true;
}




bool WpPropertiesDialog::sync_name_entry_to_current_object_cb(const QString & new_name) /* Slot. */
{
	if (!this->current_object) {
		return false;
	}
	if (this->sync_to_current_object_block) {
		return false;
	}

	this->current_object->set_name(new_name);

	return true;
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
	const bool has_gps_info = GeotagExif::object_has_gps_info(wp->image);;
	hasGeotagCB = new QCheckBox(QObject::tr("Has Geotag"));
	hasGeotagCB->setEnabled(false);
	hasGeotagCB->setChecked(has_gps_info);

	consistentGeotagCB = new QCheckBox(QObject::tr("Consistent Position"));
	consistentGeotagCB->setEnabled(false);
	if (has_gps_info) {
		const Coord coord(GeotagExif::get_object_lat_lon(wp->image), coord_mode);
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
		const ScreenPos point_to_expose = SGUtils::coord_to_point(wp->coord);
		Dialog::move_dialog(GTK_WINDOW(dialog), point_to_expose, false);
	}

	return NULL;
}


#endif
