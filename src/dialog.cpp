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
#if 0
#include "authors.h"
#include "documenters.h"
#endif
#include "globals.h"




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




#if 0




static void get_selected_foreach_func(QStandardItemModel * model,
                                      GtkTreePath *path,
                                      GtkTreeIter *iter,
                                      void * data)
{
	GList **list = (GList **) data;
	char *name;
	gtk_tree_model_get(model, iter, 0, &name, -1);
	*list = g_list_prepend(*list, name);
}




#endif



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



#if 0



static void today_clicked(GtkWidget *cal)
{
	GDateTime *now = g_date_time_new_now_local();
	gtk_calendar_select_month(GTK_CALENDAR(cal), g_date_time_get_month(now)-1, g_date_time_get_year(now));
	gtk_calendar_select_day(GTK_CALENDAR(cal), g_date_time_get_day_of_month(now));
	g_date_time_unref(now);
}



#endif


/**
 * Returns: a date as a string - always in ISO8601 format (YYYY-MM-DD).
 * This string can be NULL (especially when the dialog is cancelled).
 * Free the string after use.
 */
char * a_dialog_get_date(Window * parent, char const * title)
{
	time_t mytime = date_dialog(parent,
				    QString(title),
				    time(NULL));

	if (!mytime) {
		return NULL;
	}

	struct tm * out = localtime(&mytime);
	size_t size = strlen("YYYY-MM-DD") + 1;
	char * date_str = (char *) malloc(size);
	snprintf(date_str, size, "%04d-%02d-%02d", 1900 + out->tm_year, out->tm_mon + 1, out->tm_mday);
	qDebug() << "II: Dialog:" << __FUNCTION__ << date_str;

	return date_str;
}




bool dialog_yes_or_no(QString const & message, QWidget * parent, QString const & title)
{
	return QMessageBox::Yes == QMessageBox::question(parent, title, message);
}


#ifdef K


static void zoom_spin_changed(QSpinBox * spin, GtkWidget * pass_along[3])
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pass_along[2])))
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(pass_along[GTK_WIDGET(spin) == pass_along[0] ? 1 : 0]),
					  spin.value());
}




bool a_dialog_custom_zoom(Window * parent, double * xmpp, double * ympp)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Zoom Factors..."),
							parent,
							(GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);
	GtkWidget *table, *samecheck;
	GtkWidget *pass_along[3];
	QSpinBox * xspin = NULL;
	QSpinBox * yxpin = NULL;

	table = gtk_table_new(4, 2, false);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), table, true, true, 0);

	QLabel * label = new QLabel(QObject::tr("Zoom factor (in meters per pixel):"));
	QLabel * xlabel = new QLabel(QObject::tr("X (easting): "));
	QLabel * ylabel = new QLabel(QObject::tr("Y (northing): "));

	pass_along[0] = xspin = new QSpinBox();
	xspin->setValue(*xmpp);
	xspin->setMinimum(VIK_VIEWPORT_MIN_ZOOM);
	xspin->setMaximum(VIK_VIEWPORT_MAX_ZOOM);
	xspin->setSingleStep(1);

	pass_along[1] = yspin = new QSpinBox();
	yspin->setValue(*ympp);
	yspin->setMinimum(VIK_VIEWPORT_MIN_ZOOM);
	yspin->setMaximum(VIK_VIEWPORT_MAX_ZOOM);
	yspin->setSingleStep(1);


	pass_along[2] = samecheck = gtk_check_button_new_with_label(_("X and Y zoom factors must be equal"));
	/* TODO -- same factor. */
	/* samecheck = gtk_check_button_new_with_label ("Same x/y zoom factor"); */

	if (*xmpp == *ympp) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(samecheck), true);
	}

	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), xlabel, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), xspin, 1, 2, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), ylabel, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(table), yspin, 1, 2, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(table), samecheck, 0, 2, 3, 4);

	gtk_widget_show_all(table);

	QObject::connect(xspin, SIGNAL("value-changed"), pass_along, SLOT (zoom_spin_changed));
	QObject::connect(yspin, SIGNAL("value-changed"), pass_along, SLOT (zoom_spin_changed));

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		*xmpp = xspin.value();
		*ympp = yspin.value();
		gtk_widget_destroy(dialog);
		return true;
	}
	gtk_widget_destroy(dialog);
	return false;
}




static void split_spin_focused(QSpinBox * spin, GtkWidget *pass_along[1])
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pass_along[0]), 1);
}




bool a_dialog_time_threshold(Window * parent, char * title_text, char * label_text, unsigned int * thr)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(title_text,
							parent,
							(GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);
	GtkWidget *table, *t1, *t2, *t3, *t4;
	GtkWidget *pass_along[1];

	table = gtk_table_new(4, 2, false);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), table, true, true, 0);

	QLabel * label = new QLabel(label_text);

	t1 = gtk_radio_button_new_with_label(NULL, _("1 min"));
	t2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(t1), _("1 hour"));
	t3 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(t2), _("1 day"));
	t4 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(t3), _("Custom (in minutes):"));

	pass_along[0] = t4;

	QSpinBox * spin = new QSpinBox();
	spin->setValue(*thr);
	spin->setMinimum(0);
	spin->setMaximum(65536); /* kamilTODO: is it 65535 or 65536? Won't it cause any overflow? */
	spin->setSingleStep(1);


	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), t1, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), t2, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(table), t3, 0, 1, 3, 4);
	gtk_table_attach_defaults(GTK_TABLE(table), t4, 0, 1, 4, 5);
	gtk_table_attach_defaults(GTK_TABLE(table), spin, 1, 2, 4, 5);

	gtk_widget_show_all(table);

	QObject::connect(spin, SIGNAL("grab-focus"), pass_along, SLOT (split_spin_focused));

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(t1))) {
			*thr = 1;
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(t2))) {
			*thr = 60;
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(t3))) {
			*thr = 60 * 24;
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(t4))) {
			*thr = spin.value();
		}
		gtk_widget_destroy(dialog);
		return true;
	}
	gtk_widget_destroy(dialog);
	return false;
}


#endif


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


#if (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 24)
static void about_url_hook(GtkAboutDialog *about,
			   const char    *link,
			   void *        data)
{
	open_url(GTK_WINDOW(about), link);
}




static void about_email_hook(GtkAboutDialog *about,
			     const char    *email,
			     void *        data)
{
	new_email(GTK_WINDOW(about), email);
}
#endif




/**
 * Creates a dialog with list of text.
 * Mostly useful for longer messages that have several lines of information.
 */
void a_dialog_list(Window * parent, const char * title, GArray * array, int padding)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(title,
							parent,
							(GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
							GTK_STOCK_CLOSE,
							GTK_RESPONSE_CLOSE,
							NULL);

	GtkBox *vbox = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	GtkWidget *label;

	for (unsigned int i = 0; i < array->len; i++) {
		label = ui_label_new_selectable(NULL);
		gtk_label_set_markup(GTK_LABEL(label), g_array_index(array,char*,i));
		gtk_box_pack_start(GTK_BOX(vbox), label, false, true, padding);
	}

	gtk_widget_show_all(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


#endif


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
#if (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 24)
	gtk_about_dialog_set_url_hook(about_url_hook, NULL, NULL);
	gtk_about_dialog_set_email_hook(about_email_hook, NULL, NULL);
#endif

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


#ifdef K


bool a_dialog_map_n_zoom(Window * parent, char * mapnames[], int default_map, char * zoom_list[], int default_zoom, int * selected_map, int * selected_zoom)
{
	char **s;

	GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Download along track"), parent, (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
	response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
#endif

	QLabel * map_label = new QLabel(QObject::tr("Map type:"));
	QComboBox * map_combo = new QComboBox();
	for (s = mapnames; *s; s++) {
		vik_combo_box_text_append(map_combo, *s);
	}
	map_combo->setCurrentIndex(default_map);

	QLabel * zoom_label = new QLabel(QObject::tr("Zoom level:"));
	QComboBox * zoom_combo = new QComboBox();
	for (s = zoom_list; *s; s++) {
		vik_combo_box_text_append(zoom_combo, *s);
	}
	zoom_combo->setCurrentIndex(default_zoom);

	GtkTable *box = GTK_TABLE(gtk_table_new(2, 2, false));
	gtk_table_attach_defaults(box, map_label, 0, 1, 0, 1);
	gtk_table_attach_defaults(box, map_combo, 1, 2, 0, 1);
	gtk_table_attach_defaults(box, zoom_label, 0, 1, 1, 2);
	gtk_table_attach_defaults(box, zoom_combo, 1, 2, 1, 2);

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), GTK_WIDGET(box), false, false, 5);

	if (response_w) {
		gtk_widget_grab_focus(response_w);
	}

	gtk_widget_show_all(dialog);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(dialog);
		return false;
	}

	*selected_map = map_combo->currentIndex();
	*selected_zoom = zoom_combo->currentIndex();

	gtk_widget_destroy(dialog);
	return true;
}


#endif


/**
 * Display a dialog presenting the license of a map.
 * Allow to read the license by launching a web browser.
 */
void a_dialog_license(Window * parent, const char * map, const char * license, const char * url)
{
#ifdef K
	GtkWidget *dialog = gtk_message_dialog_new(parent,
						   GTK_DIALOG_DESTROY_WITH_PARENT,
						   GTK_MESSAGE_INFO,
						   GTK_BUTTONS_OK,
						   _("The map data is licensed: %s."),
						   license);
	gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG (dialog),
						   _("The data provided by '<b>%s</b>' are licensed under the following license: <b>%s</b>."),
						   map, license);
#define RESPONSE_OPEN_LICENSE 600
	if (url != NULL) {
		gtk_dialog_add_button(GTK_DIALOG (dialog), _("Open license"), RESPONSE_OPEN_LICENSE);
	}
	int response;
	do {
		response = gtk_dialog_run(GTK_DIALOG (dialog));
		if (response == RESPONSE_OPEN_LICENSE) {
			open_url(parent, url);
		}
	} while (response != GTK_RESPONSE_DELETE_EVENT && response != GTK_RESPONSE_OK);
	gtk_widget_destroy(dialog);
#endif
}
