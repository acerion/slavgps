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

#include "babel.h"
#include "babel_ui.h"




extern std::vector<BabelFile *> a_babel_file_list;

static void babel_ui_selector_add_entry(BabelFile * file, GtkWidget * combo)
{
#ifdef K
	GList * formats = (GList *) g_object_get_data(G_OBJECT(combo), "formats");
	formats = g_list_append ( formats, file );
	g_object_set_data(G_OBJECT(combo), "formats", formats);

	const char *label = file->label;
	vik_combo_box_text_append(combo, label);
#endif
}




void a_babel_ui_type_selector_dialog_sensitivity_cb(QComboBox * widget, void * user_data )
{
#ifdef K
	/* user_data is the GtkDialog */
	GtkDialog * dialog = GTK_DIALOG(user_data);

	/* Retrieve the associated file format descriptor */
	BabelFile * file = a_babel_ui_file_type_selector_get(GTK_WIDGET(widget));

	if (file) {
		/* Not NULL => valid selection */
		gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT, true);
	} else {
		/* NULL => invalid selection */
		gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT, false);
	}
#endif
}




/**
 * @mode: the mode to filter the file types
 *
 * Create a file type selector.
 *
 * This widget relies on a combo box listing labels of file formats.
 * We store in the "data" of the GtkWidget a list with the BabelFile
 * entries, in order to retrieve the selected file format.
 *
 * Returns: a GtkWidget
 */
QComboBox * a_babel_ui_file_type_selector_new(BabelMode mode)
{
	QComboBox * combo = new QComboBox();

	/* Add a first label to invite user to select a file format.
	   id == -1 distinguishes this entry. */
	int i = -1;
	combo->addItem("Select a file format", i);
	i++;

	/* Add all known and compatible file formats */
	if (mode.tracksRead && mode.routesRead && mode.waypointsRead
	    && !mode.tracksWrite && !mode.routesWrite && !mode.waypointsWrite) {

		/* Run a function on all file formats with any kind of read method
		   (which is almost all but not quite - e.g. with GPSBabel v1.4.4
		   - PalmDoc is write only waypoints). */
		for (auto iter = a_babel_file_list.begin(); iter != a_babel_file_list.end(); iter++) {

			BabelFile * currentFile = *iter;
			/* Call function when any read mode found. */
			if (currentFile->mode.waypointsRead
			    || currentFile->mode.tracksRead
			    || currentFile->mode.routesRead) {

				combo->addItem(QString(currentFile->label), i);
				i++;
				//babel_ui_selector_add_entry(currentFile, combo);
			}
		}
	} else {
		/* Run a function on all file formats supporting a given mode. */
		for (auto iter = a_babel_file_list.begin(); iter != a_babel_file_list.end(); iter++) {
			BabelFile * currentFile = *iter;
			/* Check compatibility of modes. */
			bool compat = true;
			if (mode.waypointsRead  && ! currentFile->mode.waypointsRead)  compat = false;
			if (mode.waypointsWrite && ! currentFile->mode.waypointsWrite) compat = false;
			if (mode.tracksRead     && ! currentFile->mode.tracksRead)     compat = false;
			if (mode.tracksWrite    && ! currentFile->mode.tracksWrite)    compat = false;
			if (mode.routesRead     && ! currentFile->mode.routesRead)     compat = false;
			if (mode.routesWrite    && ! currentFile->mode.routesWrite)    compat = false;
			/* Do call. */
			if (compat) {
				combo->addItem(QString(currentFile->label), i);
				i++;
				//babel_ui_selector_add_entry(currentFile, combo);
			}
		}
	}

	/* Initialize the selection with the really first entry */
	combo->setCurrentIndex(0);

	return combo;
}




/**
 * @selector: the selector to destroy
 *
 * Destroy the selector and any related data.
 */
void a_babel_ui_file_type_selector_destroy ( GtkWidget *selector )
{
#ifdef K
	GList *formats = (GList *) g_object_get_data ( G_OBJECT(selector), "formats" );
	free( formats );
#endif
}




/**
   \brief Retrieve the selected file type

   \param selector: the selector

   \return the selected BabelFile or NULL
*/
BabelFile * a_babel_ui_file_type_selector_get(GtkWidget * selector)
{
#ifdef K
	int active = gtk_combo_box_get_active(GTK_COMBO_BOX(selector));
	if (active >= 0) {
		GList *formats = (GList *) g_object_get_data ( G_OBJECT(selector), "formats" );
		return (BabelFile*) g_list_nth_data(formats, active);
	} else {
		return NULL;
	}
#endif
}




/**
 * @tracks:
 * @routes:
 * @waypoints:
 *
 * Creates a selector for babel modes.
 * This selector is based on 3 checkboxes.
 *
 * Returns: a GtkWidget packing all checkboxes.
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
 * a_babel_ui_modes_get:
 * @container:
 * @tracks: return value
 * @routes: return value
 * @waypoints: return value
 *
 * Retrieve state of checkboxes.
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
