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




#include <cstdint>




#include <glib.h>




#include <QString>




namespace SlavGPS {




	class LayersPanel;
	enum class LayerType;




	typedef size_t pickle_size_t;




	enum class ClipboardDataType {
		NONE = 0,
		LAYER,
		SUBLAYER,
		TEXT,
	};




	class Pickle {
	public:
		~Pickle();

		pickle_size_t peek_size(pickle_size_t offset = 0) const;
		pickle_size_t take_size(void);

		QString peek_string(pickle_size_t offset = 0) const;
		QString take_string(void);

		void put_object(void * object, pickle_size_t object_size);
		void take_object(void * target);

		void move_to_next_object(void);

		void clear(void);

		uint8_t * data = NULL;
		pickle_size_t data_size = 0;
	};




	class Clipboard {
	public:
		static void copy(ClipboardDataType  type, LayerType layer_type, const QString & type_id, unsigned int len, const QString & text, uint8_t * data);
		static void copy_selected(LayersPanel * panel);
		static bool paste(LayersPanel * panel);
		static ClipboardDataType get_current_type();



		/* This allocates space for variant sized strings and copies that
		   amount of data from the string to byte array. */
		static void append_string(GByteArray * byte_array, const char * string);
		static void append_object(GByteArray * byte_array, uint8_t * obj, pickle_size_t obj_size);

		/*
		  Store:
		  the length of the item
		  the sublayer type of item
		  the the actual item
		*/
		static void append_object_with_type(GByteArray * byte_array, Pickle & pickle, pickle_size_t obj_size, int obj_type);
	};





} /* namespace SlavGPS */




#endif /* #ifndef _SG_CLIPBOARD_H_ */
