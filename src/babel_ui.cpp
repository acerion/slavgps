/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include <cstdlib>

#include <QCheckBox>
#include <QTranslator>
#include <QDebug>

#include "babel.h"
#include "babel_ui.h"




extern std::map<int, BabelFileType *> a_babel_file_types;




void a_babel_ui_type_selector_dialog_sensitivity_cb(QComboBox * combo, void * user_data)
{
	/* Retrieve selected file type. */
	BabelFileType * file_type = a_babel_ui_file_type_selector_get(combo);

#ifdef K
	/* user_data is the GtkDialog */
	QDialog * dialog = (QDialog *) user_data;
	if (file_type) {
		/* Not NULL => valid selection */
		gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT, true);
	} else {
		/* NULL => invalid selection */
		gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT, false);
	}
#endif
}




/**
   \brief Create a list of gpsbabel file types

   \param mode: the mode to filter the file types

   \return list of file types
*/
QComboBox * a_babel_ui_file_type_selector_new(BabelMode mode)
{
	QComboBox * combo = new QComboBox();

	/* Add a first label to invite user to select a file type.
	   user data == -1 distinguishes this entry. This entry can be also recognized by index == 0. */
	combo->addItem("Select a file type", -1);

	/* Add all known and compatible file types */
	if (mode.tracks_read
	    && mode.routes_read
	    && mode.waypoints_read
	    && !mode.tracks_write
	    && !mode.routes_write
	    && !mode.waypoints_write) {

		/* Run a function on all file types with any kind of read method
		   (which is almost all but not quite - e.g. with GPSBabel v1.4.4
		   - PalmDoc is write only waypoints). */
		for (auto iter = a_babel_file_types.begin(); iter != a_babel_file_types.end(); iter++) {

			BabelFileType * file_type = iter->second;
			/* Call function when any read mode found. */
			if (file_type->mode.waypoints_read
			    || file_type->mode.tracks_read
			    || file_type->mode.routes_read) {

				combo->addItem(QString(file_type->label), iter->first);
			}
		}
	} else {
		/* Run a function on all file types supporting a given mode. */
		for (auto iter = a_babel_file_types.begin(); iter != a_babel_file_types.end(); iter++) {
			BabelFileType * file_type = iter->second;
			/* Check compatibility of modes. */
			bool compat = true;
			if (mode.waypoints_read  && !file_type->mode.waypoints_read)  compat = false;
			if (mode.waypoints_write && !file_type->mode.waypoints_write) compat = false;
			if (mode.tracks_read     && !file_type->mode.tracks_read)     compat = false;
			if (mode.tracks_write    && !file_type->mode.tracks_write)    compat = false;
			if (mode.routes_read     && !file_type->mode.routes_read)     compat = false;
			if (mode.routes_write    && !file_type->mode.routes_write)    compat = false;
			/* Do call. */
			if (compat) {
				combo->addItem(QString(file_type->label), iter->first);
			}
		}
	}

	/* Initialize the selection with the really first entry */
	combo->setCurrentIndex(0);

	return combo;
}




/**
   \brief Retrieve the selected file type

   \param combo: list with gpsbabel file types

   \return the selected BabelFileType or NULL
*/
BabelFileType * a_babel_ui_file_type_selector_get(QComboBox * combo)
{
	/* ID that was used in combo->addItem(<file type>, id);
	   A special item has been added with id == -1.
	   All other items have been added with id >= 0. */
	int i = combo->currentData().toInt();
	if (i == -1) {
		qDebug() << "II: Babel: selected file type: NONE";
		return NULL;
	} else {
		BabelFileType * file_type = a_babel_file_types.at(i);
		qDebug() << "II: Babel: selected file type:" << file_type->name << file_type->label;
		return file_type;
	}
}




/**
   \brief Create a selector for babel modes. This selector is based on 3 checkboxes.

   \param tracks:
   \param routes:
   \param waypoints:

   \return a layout widget packing all checkboxes
*/
QHBoxLayout * a_babel_ui_modes_new(bool tracks, bool routes, bool waypoints)
{
	QHBoxLayout * hbox = new QHBoxLayout();
	QCheckBox * checkbox = NULL;

	checkbox = new QCheckBox(QObject::tr("Tracks"));
	checkbox->setChecked(tracks);
	hbox->addWidget(checkbox);

	checkbox = new QCheckBox(QObject::tr("Routes"));
	checkbox->setChecked(routes);
	hbox->addWidget(checkbox);

	checkbox = new QCheckBox(QObject::tr("Waypoints"));
	checkbox->setChecked(waypoints);
	hbox->addWidget(checkbox);

	return hbox;
}




/**
   \brief Retrieve state of checkboxes

   \param hbox:
   \param tracks: return value
   \param routes: return value
   \param waypoints: return value
*/
void a_babel_ui_modes_get(QHBoxLayout * hbox, bool * tracks, bool * routes, bool * waypoints)
{
	QWidget * widget = NULL;
	QCheckBox * checkbox = NULL;

	widget = hbox->itemAt(0)->widget();
	if (!widget) {
		qDebug() << "EE: Babel UI: failed to get widget 0";
		return;
	}
	*tracks = ((QCheckBox *) widget)->isChecked();

	widget = hbox->itemAt(1)->widget();
	if (!widget) {
		qDebug() << "EE: Babel UI: failed to get widget 1";
		return;
	}
	*routes = ((QCheckBox *) widget)->isChecked();

	widget = hbox->itemAt(2)->widget();
	if (!widget) {
		qDebug() << "EE: Babel UI: failed to get widget 2";
		return;
	}
	*waypoints = ((QCheckBox *) widget)->isChecked();

	return;
}
