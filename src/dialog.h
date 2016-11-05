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
 *
 */

#ifndef _SG_DIALOG_H_
#define _SG_DIALOG_H_




#include <cstdint>

#include <QString>
#include <QMessageBox>

#include <glib.h>
#if 0
#include <gtk/gtk.h>
#endif

#include "globals.h"
#include "coords.h"




/* Most of this file is an architechtural flaw. */

void a_dialog_msg(enum QMessageBox::Icon type, const QString & text, const QString & title);

#define a_dialog_info_msg(text, title) a_dialog_msg(QMessageBox::Information, text, title)
#define a_dialog_warning_msg(text, title) a_dialog_msg(QMessageBox::Warning, text, title)
#define a_dialog_error_msg(text, title) a_dialog_msg(QMessageBox::Critical, text, title)

QString a_dialog_new_track(QWidget * parent, QString const & default_name, bool is_route);

#if 0

#define a_dialog_info_msg_extra(win,info,extra) a_dialog_msg(win,GTK_MESSAGE_INFO,info,extra)
#define a_dialog_error_msg_extra(win,info,extra) a_dialog_msg(win,GTK_MESSAGE_ERROR,info,extra)

GtkWidget *a_dialog_create_label_vbox(char **texts, int label_count, int spacing, int padding);

void a_dialog_msg(GtkWindow *parent, int type, const char *info, const char *extra);

void a_dialog_response_accept(GtkDialog *dialog);

void a_dialog_list(GtkWindow *parent, const char *title, GArray *array, int padding);

void a_dialog_about(GtkWindow *parent);

/* Okay, everthing below here is an architechtural flaw. */
bool a_dialog_goto_latlon(GtkWindow *parent, struct LatLon *ll, const struct LatLon *old);
bool a_dialog_goto_utm(GtkWindow *parent, struct UTM *utm, const struct UTM *old);

char *a_dialog_get_date(GtkWindow *parent, const char *title);
#endif
bool a_dialog_yes_or_no(QWidget * parent, QString const & message, QString const & title = SG_APPLICATION_NAME);
#if 0
bool a_dialog_custom_zoom(GtkWindow *parent, double *xmpp, double *ympp);
bool a_dialog_time_threshold(GtkWindow *parent, char *title_text, char *label_text, unsigned int *thr);

unsigned int a_dialog_get_positive_number(GtkWindow *parent, char *title_text, char *label_text, unsigned int default_num, unsigned int min, unsigned int max, unsigned int step);

void a_dialog_choose_dir(GtkWidget *entry);

bool a_dialog_map_n_zoom(GtkWindow *parent, char *mapnames[], int default_map, char *zoom_list[], int default_zoom, int *selected_map, int *selected_zoom);

GList *a_dialog_select_from_list(GtkWindow *parent, GList *names, bool multiple_selection_allowed, const char *title, const char *msg);

void a_dialog_license(GtkWindow *parent, const char *map, const char *license, const char *url);
#endif



#endif /* #ifndef _SG_DIALOG_H_ */
