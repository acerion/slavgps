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
#include <QTableView>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QCheckBox>

#include "globals.h"
#include "coords.h"
#include "window.h"
#include "ui_util.h"
#include "widget_radio_group.h"




void dialog_info(QString const & message, QWidget * parent);
void dialog_warning(QString const & message, QWidget * parent);
void dialog_error(QString const & message, QWidget * parent);
bool dialog_yes_or_no(QString const & message, QWidget * parent = NULL, QString const & title = SG_APPLICATION_NAME);

QString a_dialog_new_track(QWidget * parent, QString const & default_name, bool is_route);

void a_dialog_list(const QString & title, const QStringList & items, int padding, QWidget * parent = NULL);

void a_dialog_about(SlavGPS::Window * parent);



/* Okay, everthing below here is an architechtural flaw. */

char * a_dialog_get_date(const QString & title, QWidget * parent = NULL);
bool a_dialog_custom_zoom(double * xmpp, double * ympp, QWidget * parent);
bool a_dialog_time_threshold(const QString & title, const QString & label, uint32_t * thr, QWidget * parent = NULL);

int a_dialog_get_positive_number(SlavGPS::Window * parent, QString const & title, QString const & label, int default_num, int min, int max, int step);

bool a_dialog_map_and_zoom(const QStringList & map_labels, unsigned int default_map_idx, const QStringList & zoom_labels, unsigned int default_zoom_idx, unsigned int * selected_map_idx, unsigned int * selected_zoom_idx, QWidget * parent);

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




void a_dialog_license(const char * map, const char * license, const char * url, QWidget * parent);




class ViewportZoomDialog : public QDialog {
	Q_OBJECT
public:
	ViewportZoomDialog() {};
	ViewportZoomDialog(double * xmpp, double * ympp, QWidget * a_parent = NULL);
	~ViewportZoomDialog() {};

	void get_values(double * xmpp, double * ympp);

private slots:
	void spin_changed_cb(double new_value);

private:
	QDialogButtonBox button_box;
	QVBoxLayout * vbox = NULL;

	QLabel main_label;
	QLabel xlabel;
	QLabel ylabel;

	QDoubleSpinBox xspin;
	QDoubleSpinBox yspin;

	QGridLayout * grid = NULL;

	QCheckBox checkbox;
};




class TimeThresholdDialog : public QDialog {
	Q_OBJECT
public:
	TimeThresholdDialog() {};
	TimeThresholdDialog(const QString & title, const QString & label, uint32_t custom_threshold, QWidget * parent = NULL);
	~TimeThresholdDialog() {};

	void get_value(uint32_t * custom_threshold);

private slots:
	void spin_changed_cb(int unused);

private:
	QDialogButtonBox button_box;
	QSpinBox custom_spin;
	QVBoxLayout * vbox = NULL;
	SlavGPS::SGRadioGroup * radio_group = NULL;
};




#endif /* #ifndef _SG_DIALOG_H_ */
