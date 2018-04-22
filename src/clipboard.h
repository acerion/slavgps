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

#ifndef _SG_CLIPBOARD_H_
#define _SG_CLIPBOARD_H_




namespace SlavGPS {




	class LayersPanel;
	enum class LayerType;




	enum class ClipboardDataType {
		NONE = 0,
		LAYER,
		SUBLAYER,
		TEXT,
	};




	class Clipboard {
	public:
		static void copy(ClipboardDataType  type, LayerType layer_type, const QString & type_id, unsigned int len, const QString & text, uint8_t * data);
		static void copy_selected(LayersPanel * panel);
		static bool paste(LayersPanel * panel);
		static ClipboardDataType get_current_type();
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_CLIPBOARD_H_ */
