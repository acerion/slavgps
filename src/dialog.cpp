/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Hein Ragas <viking@ragas.nl>
 * Copyright (C) 2010-2014, Rob Norris <rw_norris@hotmail.com>
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include <QDialog>
#include <QInputDialog>
#include <QObject>
#include <QHeaderView>
#include <QDebug>

#include "dialog.h"
#include "viewport.h"
#include "ui_util.h"
#include "date_time_dialog.h"
#include "degrees_converters.h"
#include "authors.h"
#include "documenters.h"
#include "globals.h"
#include "viewport_zoom.h"




using namespace SlavGPS;




void dialog_info(QString const & message, QWidget * parent)
{
	QMessageBox box(parent);
	box.setText(message);
	box.setIcon(QMessageBox::Information);
	box.exec();
	return;
}




void dialog_warning(QString const & message, QWidget * parent)
{
	QMessageBox box(parent);
	box.setText(message);
	box.setIcon(QMessageBox::Warning);
	box.exec();
	return;
}




void dialog_error(QString const & message, QWidget * parent)
{
	QMessageBox box(parent);
	box.setText(message);
	box.setIcon(QMessageBox::Critical);
	box.exec();
	return;
}




void a_dialog_select_from_list_prepare(QDialog & dialog, QStandardItemModel & model, QTableView & view, QVBoxLayout & vbox, QDialogButtonBox & button_box, bool multiple_selection_allowed, QString const & title, QString const & msg)
{
	dialog.setWindowTitle(title);
	dialog.setMinimumHeight(400);

	QObject::connect(&button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(&button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	model.setHorizontalHeaderItem(0, new QStandardItem(msg));
	model.setItemPrototype(new SGItem());

	view.horizontalHeader()->setStretchLastSection(true);
	view.verticalHeader()->setVisible(false);
	view.setWordWrap(false);
	view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	view.setTextElideMode(Qt::ElideRight);
	if (multiple_selection_allowed) {
		view.setSelectionMode(QAbstractItemView::ExtendedSelection);
	} else {
		view.setSelectionMode(QAbstractItemView::SingleSelection);
	}
	view.setShowGrid(false);
	view.setModel(&model);

	view.horizontalHeader()->setSectionHidden(0, false);
	view.horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);

	vbox.addWidget(&view);
	vbox.addWidget(&button_box);

	QLayout * old = dialog.layout();
	delete old;
	dialog.setLayout(&vbox);
}




QString a_dialog_new_track(QWidget * parent, QString const & default_name, bool is_route)
{
	QString text;
	bool ok;
	do {
		text = QInputDialog::getText(parent,
					     is_route ? QString("Add Route") : QString("Add Track"),
					     is_route ? QString("Route Name:") : QString("Track Name:"),
					     QLineEdit::Normal,
					     QString(default_name), &ok);

		if (ok && text.isEmpty()) {
			QMessageBox::information(parent,
						 is_route ? QString("Route Name") : QString("Track Name"),
						 is_route ? QString("Please enter a name for the route.") : QString("Please enter a name for the track."));
		}

	} while (ok && text.isEmpty());

	return text;
}




/**
 * Returns: a date as a string - always in ISO8601 format (YYYY-MM-DD).
 * This string can be NULL (especially when the dialog is cancelled).
 * Free the string after use.
 */
char * a_dialog_get_date(const QString & title, QWidget * parent)
{
	time_t mytime = date_dialog(parent, title, time(NULL));

	if (!mytime) {
		return NULL;
	}

	struct tm * out = localtime(&mytime);
	size_t size = strlen("YYYY-MM-DD") + 1;
	char * date_str = (char *) malloc(size);
	snprintf(date_str, size, "%04d-%02d-%02d", 1900 + out->tm_year, out->tm_mon + 1, out->tm_mday);
	qDebug() << "II: Dialog: get date:" << date_str;

	return date_str;
}




bool dialog_yes_or_no(QString const & message, QWidget * parent, QString const & title)
{
	return QMessageBox::Yes == QMessageBox::question(parent, title, message);
}




bool a_dialog_custom_zoom(double * xmpp, double * ympp, QWidget * parent)
{
	ViewportZoomDialog dialog(xmpp, ympp, parent);
	if (QDialog::Accepted == dialog.exec()) {
		dialog.get_values(xmpp, ympp);
		/* There is something strange about argument to qSetRealNumberPrecision().  The precision for
		   fractional part is not enough, I had to add few places for leading digits and decimal dot. */
		qDebug() << qSetRealNumberPrecision(5 + 1 + SG_VIEWPORT_ZOOM_PRECISION) << "DD: Dialog: Saving custom zoom as" << *xmpp << *ympp;
		return true;
	} else {
		return false;
	}
}




ViewportZoomDialog::ViewportZoomDialog(double * xmpp, double * ympp, QWidget * parent)
{
	this->setWindowTitle(QObject::tr("Zoom Factors..."));


	this->vbox = new QVBoxLayout();


	this->main_label.setText(QObject::tr("Zoom factor (in meters per pixel):"));
	this->xlabel.setText(QObject::tr("X (easting): "));
	this->ylabel.setText(QObject::tr("Y (northing): "));


	/* TODO: add some kind of validation and indication for values out of range. */
	this->xspin.setMinimum(SG_VIEWPORT_ZOOM_MIN);
	this->xspin.setMaximum(SG_VIEWPORT_ZOOM_MAX);
	this->xspin.setSingleStep(1);
	this->xspin.setDecimals(SG_VIEWPORT_ZOOM_PRECISION);
	this->xspin.setValue(*xmpp);


	/* TODO: add some kind of validation and indication for values out of range. */
	this->yspin.setMinimum(SG_VIEWPORT_ZOOM_MIN);
	this->yspin.setMaximum(SG_VIEWPORT_ZOOM_MAX);
	this->yspin.setSingleStep(1);
	this->yspin.setDecimals(SG_VIEWPORT_ZOOM_PRECISION);
	this->yspin.setValue(*ympp);


	this->grid = new QGridLayout();
	this->grid->addWidget(&this->xlabel, 0, 0);
	this->grid->addWidget(&this->xspin, 0, 1);
	this->grid->addWidget(&this->ylabel, 1, 0);
	this->grid->addWidget(&this->yspin, 1, 1);


	this->checkbox.setText(QObject::tr("X and Y zoom factors must be equal"));
	if (*xmpp == *ympp) {
		this->checkbox.setChecked(true);
	}


	this->vbox->addWidget(&this->main_label);
	this->vbox->addLayout(this->grid);
	this->vbox->addWidget(&this->checkbox);
	this->vbox->addWidget(&this->button_box);

	QObject::connect(&this->xspin, SIGNAL (valueChanged(double)), this, SLOT (spin_changed_cb(double)));
	QObject::connect(&this->yspin, SIGNAL (valueChanged(double)), this, SLOT (spin_changed_cb(double)));


	this->button_box.addButton(QDialogButtonBox::Ok);
	this->button_box.addButton(QDialogButtonBox::Cancel);
	QObject::connect(&this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	QObject::connect(&this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox); /* setLayout takes ownership of vbox. */
}




void ViewportZoomDialog::get_values(double * xmpp, double * ympp)
{
	*xmpp = this->xspin.value();
	*ympp = this->yspin.value();
}




void ViewportZoomDialog::spin_changed_cb(double new_value)
{
	if (this->checkbox.isChecked()) {
		if (new_value == this->xspin.value()) {
			this->yspin.setValue(new_value);
		} else {
			this->xspin.setValue(new_value);
		}
	}
}




TimeThresholdDialog::TimeThresholdDialog(const QString & title, const QString & label, uint32_t custom_threshold, QWidget * a_parent)
{
	this->setWindowTitle(title);


	this->vbox = new QVBoxLayout();


	QLabel main_label(label);


	QStringList labels;
	labels << QObject::tr("1 min");
	labels << QObject::tr("1 hour");
	labels << QObject::tr("1 day");
	labels << QObject::tr("Custom (in minutes):");
	this->radio_group = new SGRadioGroup(QString(""), labels, NULL); /* kamilTODO: delete this widget in destructor? */


	/* TODO: add some kind of validation and indication for values out of range. */
	this->custom_spin.setMinimum(1); /* [minutes] */
	this->custom_spin.setMaximum(60 * 24 * 366); /* [minutes] */
	this->custom_spin.setValue(custom_threshold);
	this->custom_spin.setSingleStep(1);


	this->vbox->addWidget(&main_label);
	this->vbox->addWidget(this->radio_group);
	this->vbox->addWidget(&this->custom_spin);
	this->vbox->addWidget(&this->button_box);

	QObject::connect(&this->custom_spin, SIGNAL (valueChanged(int)), this, SLOT (spin_changed_cb(int)));

	this->button_box.addButton(QDialogButtonBox::Ok);
	this->button_box.addButton(QDialogButtonBox::Cancel);
	QObject::connect(&this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	QObject::connect(&this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox); /* setLayout takes ownership of vbox. */
}



void TimeThresholdDialog::get_value(uint32_t * custom_threshold)
{
	const uint32_t selection = this->radio_group->get_selected();
	switch (selection) {
	case 0:
		*custom_threshold = 1;
		break;
	case 1:
		*custom_threshold = 60;
		break;
	case 2:
		*custom_threshold = 60 * 24;
		break;
	case 3:
		*custom_threshold = (uint32_t) this->custom_spin.value();
		break;
	default:
		qDebug() << "EE: Dialog: Time Threshold Dialog: invalid selection value" << selection;
		break;
	}
}



void TimeThresholdDialog::spin_changed_cb(__attribute__((unused)) int new_value)
{
	/* Enable "custom value" checkbox. */
	this->radio_group->set_selected(3);
}




bool a_dialog_time_threshold(const QString & title, const QString & label, uint32_t * thr, QWidget * parent)
{
	TimeThresholdDialog dialog(title, label, *thr, parent);

	if (QDialog::Accepted == dialog.exec()) {
		dialog.get_value(thr);
		/* There is something strange about argument to qSetRealNumberPrecision().  The precision for
		   fractional part is not enough, I had to add few places for leading digits and decimal dot. */
		qDebug() << "DD: Dialog: Time Threshold Dialog: Saving time threshold as" << *thr;
		return true;
	} else {
		return false;
	}
}




/**
 * Dialog to return a positive number via a spinbox within the supplied limits.
 *
 * Returns: A value of zero indicates the dialog was cancelled.
 */
int a_dialog_get_positive_number(Window * parent, QString const & title, QString const & label, int default_num, int min, int max, int step)
{
	int result = QInputDialog::getInt(parent, title, label,
					  default_num, min, max, step);

	return result;
}




#ifdef K
static void about_url_hook(GtkAboutDialog * about, const char * link, void * data)
{
	open_url(GTK_WINDOW(about), link);
}




static void about_email_hook(GtkAboutDialog *about, const char * email,  void * data)
{
	new_email(GTK_WINDOW(about), email);
}
#endif




/**
   Creates a dialog with list of text.
   Mostly useful for longer messages that have several lines of information.
*/
void a_dialog_list(const QString & title, const QStringList & items, int padding, QWidget * parent)
{
	QMessageBox box(parent);
	//box.setTitle(title);
	box.setIcon(QMessageBox::Information);

	QVBoxLayout vbox;
	QLayout * old = box.layout();
	delete old;
	box.setLayout(&vbox);

	for (int i = 0; i < items.size(); i++) {
		QLabel * label = ui_label_new_selectable(items.at(i), &box);
		vbox.addWidget(label);
		// gtk_box_pack_start(GTK_BOX(vbox), label, false, true, padding);
	}

	box.exec();
}




void a_dialog_about(SlavGPS::Window * parent)
{
#ifdef K
	const char *program_name = PACKAGE_NAME;
	const char *version = VIKING_VERSION;
	const char *website = VIKING_URL;
	const char *copyright = "2003-2008, Evan Battaglia\n2008-" THEYEAR", Viking's contributors";
	const char *comments = _("GPS Data and Topo Analyzer, Explorer, and Manager.");
	const char *license = _("This program is free software; you can redistribute it and/or modify "
				"it under the terms of the GNU General Public License as published by "
				"the Free Software Foundation; either version 2 of the License, or "
				"(at your option) any later version."
				"\n\n"
				"This program is distributed in the hope that it will be useful, "
				"but WITHOUT ANY WARRANTY; without even the implied warranty of "
				"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
				"GNU General Public License for more details."
				"\n\n"
				"You should have received a copy of the GNU General Public License "
				"along with this program; if not, write to the Free Software "
				"Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA");

	/* Would be nice to use gtk_about_dialog_add_credit_section (), but that requires gtk 3.4. */
	/* For now shove it in the 'artists' section so at least the information is easily visible. */
	/* Something more advanced might have proper version information too... */
	const char *libs[] = {
		"Compiled in libraries:",
		/* Default libs. */
		"libglib-2.0",
		"libgthread-2.0",
		"libgtk+-2.0",
		"libgio-2.0",
		/* Potentially optional libs (but probably couldn't build without them). */
#ifdef HAVE_LIBM
		"libm",
#endif
#ifdef HAVE_LIBZ
		"libz",
#endif
#ifdef HAVE_LIBCURL
		"libcurl",
#endif
#ifdef HAVE_EXPAT_H
		"libexpat",
#endif
		/* Actually optional libs. */
#ifdef HAVE_LIBGPS
		"libgps",
#endif
#ifdef HAVE_LIBGEXIV2
		"libgexiv2",
#endif
#ifdef HAVE_LIBEXIF
		"libexif",
#endif
#ifdef HAVE_LIBX11
		"libX11",
#endif
#ifdef HAVE_LIBMAGIC
		"libmagic",
#endif
#ifdef HAVE_LIBBZ2
		"libbz2",
#endif
#ifdef HAVE_LIBZIP
		"libzip",
#endif
#ifdef HAVE_LIBSQLITE3
		"libsqlite3",
#endif
#ifdef HAVE_LIBMAPNIK
		"libmapnik",
#endif
		NULL
	};
	/* Newer versions of GTK 'just work', calling gtk_show_uri() on the URL or email and opens up the appropriate program.
	   This is the old method: */
	gtk_about_dialog_set_url_hook(about_url_hook, NULL, NULL);
	gtk_about_dialog_set_email_hook(about_email_hook, NULL, NULL);

	gtk_show_about_dialog(parent,
			      /* TODO do not set program-name and correctly set info for g_get_application_name. */
			      "program-name", program_name,
			      "version", version,
			      "website", website,
			      "comments", comments,
			      "copyright", copyright,
			      "license", license,
			      "wrap-license", true,
			      /* Logo automatically retrieved via gtk_window_get_default_icon_list. */
			      "authors", AUTHORS,
			      "documenters", DOCUMENTERS,
			      "translator-credits", _("Translation is coordinated on http://launchpad.net/viking"),
			      "artists", libs,
			      NULL);
#endif
}




bool a_dialog_map_and_zoom(const QStringList & map_labels, unsigned int default_map_idx, const QStringList & zoom_labels, unsigned int default_zoom_idx, unsigned int * selected_map_idx, unsigned int * selected_zoom_idx, QWidget * parent)
{
	QDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Download along track"));


	QLabel map_label(QObject::tr("Map type:"));
	QComboBox map_combo;
	for (int i = 0; i < map_labels.size(); i++) {
		map_combo.addItem(map_labels.at(i));
	}
	map_combo.setCurrentIndex(default_map_idx);

	QLabel zoom_label(QObject::tr("Zoom level:"));
	QComboBox zoom_combo;
	for (int i = 0; i < zoom_labels.size(); i++) {
		zoom_combo.addItem(zoom_labels.at(i));
	}
	zoom_combo.setCurrentIndex(default_zoom_idx);


	QGridLayout * grid = new QGridLayout();
	grid->addWidget(&map_label, 0, 0);
	grid->addWidget(&map_combo, 0, 1);
	grid->addWidget(&zoom_label, 1, 0);
	grid->addWidget(&zoom_combo, 1, 1);


	QDialogButtonBox button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
#if 0
	this->button_box.addButton(QDialogButtonBox::Ok);
	this->button_box.addButton(QDialogButtonBox::Cancel);
	QObject::connect(&this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	QObject::connect(&this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
#endif


	QVBoxLayout * vbox = new QVBoxLayout();
	vbox->addLayout(grid);
	vbox->addWidget(&button_box);


	QLayout * old = dialog.layout();
	delete old;
	dialog.setLayout(vbox); /* setLayout takes ownership of vbox. */


	if (QDialog::Accepted == dialog.exec()) {
		*selected_map_idx = map_combo.currentIndex();
		*selected_zoom_idx = zoom_combo.currentIndex();
		/* There is something strange about argument to qSetRealNumberPrecision().  The precision for
		   fractional part is not enough, I had to add few places for leading digits and decimal dot. */
		qDebug() << "DD: Dialog: Map and Zoom: map index:" << *selected_map_idx << "zoom index:" << *selected_zoom_idx;
		return true;
	} else {
		return false;
	}
}




/**
 * Display a dialog presenting the license of a map.
 * Allow to read the license by launching a web browser.
 */
void a_dialog_license(const char * map, const char * license, const char * url, QWidget * parent)
{
	const QString primary_text = QString(QObject::tr("The map data is licensed: %1.")).arg(license);
	const QString secondary_text = QString(QObject::tr("The data provided by '<b>%1</b>' are licensed under the following license: <b>%1</b>."))
		.arg(map)
		.arg(license);

	QMessageBox box;
	box.setText(primary_text);
	box.setInformativeText(secondary_text);
	box.setStandardButtons(QMessageBox::Ok);

	if (url != NULL) {
		box.addButton(QObject::tr("Open license"), QMessageBox::HelpRole);
	}
	int response;
	do {
		response = box.exec();
		if (response == QMessageBox::Help) {
#ifdef K
			open_url(url, parent);
#endif
		}
	} while (response != QMessageBox::Cancel && response != QMessageBox::Ok);
}
