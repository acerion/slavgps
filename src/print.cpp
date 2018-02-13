/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 *
 * print.c
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
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




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




#include <QPrinter>
#include <QPrintPreviewDialog>




#include "window.h"
#include "viewport_internal.h"
#include "print.h"




using namespace SlavGPS;




void SlavGPS::a_print(Window * parent, Viewport * viewport)
{
	QPrinter printer;
	QPrintPreviewDialog dialog(&printer, parent);
	QObject::connect(&dialog, SIGNAL (paintRequested(QPrinter *)), viewport, SLOT (print_cb(QPrinter *)));
	if (dialog.exec() == QDialog::Accepted) {
		// print ...
	}
}
