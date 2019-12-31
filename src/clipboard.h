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




#include <QString>




#include "variant.h"
#include "tree_item.h"




namespace SlavGPS {




	class LayersPanel;
	enum class LayerKind;




	typedef size_t pickle_size_t;




	enum class ClipboardDataType {
		None = 0,
		Layer,
		Sublayer,
		Text,
	};




	class Pickle {
	public:
		Pickle();
		~Pickle();

		void put_pickle(const Pickle & pickle);

		pickle_size_t peek_size(pickle_size_t offset = 0) const;
		pickle_size_t take_size(void);

		void put_string(const QString & string);
		QString peek_string(pickle_size_t offset = 0) const;
		QString take_string(void);

		void put_raw_object(const char * object, pickle_size_t object_size);
		void take_raw_object(char * target, pickle_size_t size);
		void take_object(void * target);


		void clear(void);

		pickle_size_t data_size(void) const { return this->data_size_; };


		/* Convenience functions. We could use put/take_raw_object() instead. */
		void put_raw_int(int value);
		int take_raw_int(void);


		void put_pickle_tag(const char * tag);
		const char * take_pickle_tag(const char * expected_tag);

		void put_pickle_length(pickle_size_t length);
		pickle_size_t take_pickle_length(void);

		void print_bytes(const char * label) const;

	private:
		int read_iter = 0;
		QByteArray byte_array;
		pickle_size_t data_size_ = 0;
	};




	class Clipboard {
	public:
		static void copy(ClipboardDataType type, LayerKind layer_kind, const SGObjectTypeID & type_id, Pickle & pickle, const QString & text);
		static void copy_selected(LayersPanel * panel);

		/* @param pasted indicates whether pasting has been
		   made (a tree item where pasting was made has been
		   modified). */
		static sg_ret paste(LayersPanel * panel, bool & pasted);

		static ClipboardDataType get_current_type();
	};





} /* namespace SlavGPS */




#endif /* #ifndef _SG_CLIPBOARD_H_ */
