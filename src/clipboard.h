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

#ifndef _VIKING_CLIPBOARD_H
#define _VIKING_CLIPBOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "viklayerspanel.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	VIK_CLIPBOARD_DATA_NONE = 0,
	VIK_CLIPBOARD_DATA_LAYER,
	VIK_CLIPBOARD_DATA_SUBLAYER,
	VIK_CLIPBOARD_DATA_TEXT,
} VikClipboardDataType;

void a_clipboard_copy(VikClipboardDataType  type, uint16_t layer_type, int subtype, unsigned int len, const char * text, uint8_t * data);
void a_clipboard_copy_selected(SlavGPS::LayersPanel * panel);
bool a_clipboard_paste(SlavGPS::LayersPanel * panel);
VikClipboardDataType a_clipboard_type();

#ifdef __cplusplus
}
#endif

#endif
