/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013 Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _VIKING_BABEL_UI_H
#define _VIKING_BABEL_UI_H

#include <cstdint>

#include <QComboBox>
#include <QHBoxLayout>

#include "babel.h"

QComboBox * a_babel_ui_file_type_selector_new(BabelMode mode);
BabelFileType * a_babel_ui_file_type_selector_get(QComboBox * combo);
void a_babel_ui_type_selector_dialog_sensitivity_cb(QComboBox * combo, void * user_data);

QHBoxLayout * a_babel_ui_modes_new(bool tracks, bool routes, bool waypoints);
void a_babel_ui_modes_get(QHBoxLayout * hbox, bool * tracks, bool * routes, bool * waypoints);


#endif
