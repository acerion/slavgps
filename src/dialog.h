/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_DIALOG_H_
#define _SG_DIALOG_H_




#include <cstdint>

#include <QString>
#include <QMessageBox>

#include "globals.h"
#include "coords.h"
#include "window.h"
#include "ui_util.h"




void dialog_info(QString const & message, QWidget * parent);
void dialog_warning(QString const & message, QWidget * parent);
void dialog_error(QString const & message, QWidget * parent);
bool dialog_yes_or_no(QString const & message, QWidget * parent = NULL, QString const & title = SG_APPLICATION_NAME);

QString a_dialog_new_track(QWidget * parent, QString const & default_name, bool is_route);




#if 0

void a_dialog_list(GtkWindow *parent, const char *title, GArray *array, int padding);

#endif

void a_dialog_about(QWidget * parent);



/* Okay, everthing below here is an architechtural flaw. */
bool dialog_goto_latlon(SlavGPS::Window * parent, struct LatLon * ll, const struct LatLon * old);
bool dialog_goto_utm(SlavGPS::Window * parent, struct UTM * utm, const struct UTM * old);

char * a_dialog_get_date(SlavGPS::Window * parent, char const * title);
#ifdef K
bool a_dialog_custom_zoom(GtkWindow *parent, double *xmpp, double *ympp);
bool a_dialog_time_threshold(GtkWindow *parent, char *title_text, char *label_text, unsigned int *thr);
#endif

int a_dialog_get_positive_number(SlavGPS::Window * parent, QString const & title, QString const & label, int default_num, int min, int max, int step);

#ifdef K
bool a_dialog_map_n_zoom(GtkWindow *parent, char *mapnames[], int default_map, char *zoom_list[], int default_zoom, int *selected_map, int *selected_zoom);
#endif

void a_dialog_select_from_list_prepare(QDialog & dialog, QStandardItemModel & model, QTableView & view, QVBoxLayout & vbox, QDialogButtonBox & button_box, bool multiple_selection_allowed, QString const & title, QString const & msg);

template <typename T>
std::list<T> a_dialog_select_from_list(SlavGPS::Window * parent, std::list<T> const & elements, bool multiple_selection_allowed, QString const & title, QString const & msg)
{
	QDialog dialog(parent);
	QDialogButtonBox button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	QStandardItemModel model;
	QTableView view;
	QVBoxLayout vbox;
	QItemSelectionModel selection_model(&model);

	a_dialog_select_from_list_prepare(dialog, model, view, vbox, button_box, multiple_selection_allowed, title, msg);
	view.setSelectionModel(&selection_model);

	for (auto iter = elements.begin(); iter != elements.end(); iter++) {
		SlavGPS::SGItem * item = new SlavGPS::SGItem(*iter);
		item->setEditable(false);
		model.invisibleRootItem()->appendRow(item);
	}
	view.setVisible(false);
	view.resizeRowsToContents();
	view.resizeColumnsToContents();
	view.setVisible(true);
	view.show();


	std::list<T> result;
	if (dialog.exec() == QDialog::Accepted) {
		QModelIndexList selected = selection_model.selectedIndexes();
		for (auto iter = selected.begin(); iter != selected.end(); iter++) {
			QVariant variant = model.itemFromIndex(*iter)->data();
			result.push_back(variant.value<T>());
		}
	}

	return result;
}


#ifdef K
void a_dialog_license(GtkWindow *parent, const char *map, const char *license, const char *url);
#endif




#endif /* #ifndef _SG_DIALOG_H_ */
