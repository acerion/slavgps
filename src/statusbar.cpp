/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2011, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include <cmath>
#include <cstdlib>

#include "statusbar.h"
#include "background.h"
#include "globals.h"




using namespace SlavGPS;




#if 0




struct _VikStatusbar {
	GtkHBox hbox;
	GtkWidget * status[VIK_STATUSBAR_NUM_TYPES];
	bool empty[VIK_STATUSBAR_NUM_TYPES];
};




static bool button_release_event(GtkWidget * widget, GdkEvent * event, void ** user_data)
{
	if (((GdkEventButton *) event)->button == 3) {
		int type = KPOINTER_TO_INT (g_object_get_data(G_OBJECT(widget), "type"));
		VikStatusbar * vs = VIK_STATUSBAR (user_data);
		/* Right Click: so copy the text in the INFO buffer only ATM. */
		if (type == VIK_STATUSBAR_INFO) {
			const char* msg = gtk_button_get_label(GTK_BUTTON(vs->status[VIK_STATUSBAR_INFO]));
			if (msg) {
				GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
				gtk_clipboard_set_text(clipboard, msg, -1);
			}
		}
		/* We've handled the event. */
		return true;
	}
	/* Otherwise carry on with other event handlers - i.e. ensure forward_signal() is called. */
	return false;
}




static void vik_statusbar_init(VikStatusbar * vs)
{
	for (int i = 0; i < VIK_STATUSBAR_NUM_TYPES; i++) {
		vs->empty[i] = true;

		if (i == VIK_STATUSBAR_ITEMS || i == VIK_STATUSBAR_ZOOM || i == VIK_STATUSBAR_INFO) {
			vs->status[i] = gtk_button_new();
		} else {
			vs->status[i] = gtk_statusbar_new();
			gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(vs->status[i]), false);
		}
		g_object_set_data(G_OBJECT (vs->status[i]), "type", KINT_TO_POINTER(i));
	}


	/* Set minimum overall size.
	   Otherwise the individual size_requests above create an implicit overall size,
	   and so one can't downsize horizontally as much as may be desired when the statusbar is on. */
	gtk_widget_set_size_request(GTK_WIDGET(vs), 50, -1);
}




#endif




StatusBar::StatusBar(QWidget * parent) : QStatusBar(parent)
{
	this->fields.assign((int) StatusBarField::NUM_FIELDS, NULL);

	QLabel * label = NULL;
	QString tooltip;

	label = new QLabel("tool");
	label->minimumSize().rwidth() = 120;
	label->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	label->setToolTip("Currently selected tool");
	this->fields[(int) StatusBarField::TOOL] = label;
	this->addPermanentWidget(label);

	label = new QLabel("zoom level");
	label->minimumSize().rwidth() = 100;
	label->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	tooltip = QString("Current zoom level. Click to select a new one.");
	label->setToolTip(tooltip);
	this->fields[(int) StatusBarField::ZOOM] = label;
	this->addPermanentWidget(label);

	label = new QLabel("tasks");
	label->minimumSize().rwidth() = 100;
	label->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	tooltip = QString("Current number of background tasks. Click to see the background jobs.");
	label->setToolTip(tooltip);
	this->fields[(int) StatusBarField::ITEMS] = label;
	this->addPermanentWidget(label);

	label = new QLabel("position");
	label->minimumSize().rwidth() = 275;
	label->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	tooltip = QString("Current position");
	label->setToolTip(tooltip);
	this->fields[(int) StatusBarField::POSITION] = label;
	this->addPermanentWidget(label);

	label = new QLabel("info");
	label->minimumSize().rwidth() = 275;
	label->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	tooltip = QString("Left click to clear the message. Right click to copy the message.");
	label->setToolTip(tooltip);
	this->fields[(int) StatusBarField::INFO] = label;
	this->addPermanentWidget(label);
}






/**
 * @field: the field to update
 * @message: the message to use
 *
 * Update the message of the given field.
 **/
void StatusBar::set_message(StatusBarField field, QString const & message)
{
	switch (field) {
	case StatusBarField::ITEMS:
	case StatusBarField::ZOOM:
	case StatusBarField::INFO:
	case StatusBarField::POSITION:
	case StatusBarField::TOOL: {

		/* Label. */
		((QLabel *) this->fields[(int) field])->setText(message);
	}
		break;

	default:
		qDebug() << "EE: Status Bar: unhandled field number" << (int) field;
		break;
	}
}



StatusBar::~StatusBar()
{
	this->fields.clear();
}
