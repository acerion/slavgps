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




#include <QObject>
#include <QString>
#include <QMessageBox>
#include <QTableView>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QCheckBox>
#include <QDialog>
#include <QSpinBox>




#include "globals.h"
#include "ui_util.h"
#include "vikutils.h"
#include "measurements.h"




namespace SlavGPS {




	class RadioGroupWidget;




	class Dialog {
	public:
		static void info(const QString & message, QWidget * parent = NULL);

		/* Header will be displayed using slightly larger
		   font. Message will be split into separate strings,
		   separated by newline. */
		static void info(const QString & header, const QStringList & message, QWidget * parent = NULL);

		static void warning(QString const & message, QWidget * parent);
		static void error(QString const & message, QWidget * parent);
		static bool yes_or_no(QString const & message, QWidget * parent = NULL, QString const & title = PROJECT);
		static void about(QWidget * parent);
		static void map_license(const QString & map_name, const QString & map_license, const QString & map_license_url, QWidget * parent);

		static int get_int(const QString & title, const QString & label, int default_num, int min, int max, int step, bool * ok, QWidget * parent);

		static void move_dialog(QDialog * dialog, const ScreenPos & exposed_point, bool move_vertically);

		static bool duration(const QString & title, const QString & label, Duration & duration, QWidget * parent = NULL);
	};




	/* Dialog with "OK" and "Cancel" buttons, allowing a basic decision to be taken by user. */
	class BasicDialog : public QDialog {
		Q_OBJECT
	public:
		BasicDialog(QWidget * parent = NULL);
		BasicDialog(const QString & title, QWidget * parent = NULL);
		~BasicDialog();

	public slots:
		void set_central_widget_cb(QWidget * widget);

	signals:
		void set_central_widget(QWidget * widget);

	public:
		QVBoxLayout * vbox = NULL;
		QGridLayout * grid = NULL;
		QDialogButtonBox * button_box = NULL;
	};




	/* Dialog with "OK" button, only for presenting data.
	   Single button doesn't provide means to user to make decision. */
	class BasicMessage : public QDialog {
		Q_OBJECT
	public:
		BasicMessage(QWidget * parent = NULL);
		~BasicMessage();

	/* protected: */
		QVBoxLayout * vbox = NULL;
		QGridLayout * grid = NULL;
		QDialogButtonBox * button_box = NULL;
	};




	class DurationDialog : public QDialog {
		Q_OBJECT
	public:
		DurationDialog() {};
		DurationDialog(const QString & title, const QString & label, const Duration & duration, QWidget * parent = NULL);
		~DurationDialog() {};

		sg_ret get_value(Duration & duration);

	private slots:
		void spin_changed_cb(int unused);

	private:
		QDialogButtonBox button_box;
		QSpinBox spinbox;
		QVBoxLayout * vbox = NULL;
		RadioGroupWidget * radio_group = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DIALOG_H_ */
