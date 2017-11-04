/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_VARIANT_H_
#define _SG_VARIANT_H_




#include <cstdint>

#include <QString>
#include <QColor>




namespace SlavGPS {




	/* id is index. */
	enum class SGVariantType {
		EMPTY = 0,
		DOUBLE,
		UINT,
		INT,

		/* In my_layer_set_param, if you want to use the string, you should dup it.
		 * In my_layer_get_param, the string returned will NOT be free'd, you are responsible for managing it (I think). */
		STRING,
		BOOLEAN,
		COLOR,
		STRING_LIST,  /* SGVariant stores a copy of string list. */
		PTR, /* Not really a 'parameter' but useful to route to extended configuration (e.g. toolbar order). */
	};




	class SGVariant {
	public:
		SGVariant(const char * str, SGVariantType type_id); /* Construct value of given type using data from given string. */
		SGVariant(const SGVariant & val); /* Copy constructor. */

		SGVariant()                           { type_id = SGVariantType::EMPTY; }
		SGVariant(double d)                   { type_id = SGVariantType::DOUBLE; val_double = d; }
		SGVariant(uint32_t u)                 { type_id = SGVariantType::UINT; val_uint = u; }
		SGVariant(int32_t i)                  { type_id = SGVariantType::INT; val_int = i; }
		SGVariant(const QString & s)          { type_id = SGVariantType::STRING; val_string = s; }
		SGVariant(const char * s)             { type_id = SGVariantType::STRING; val_string = QString(s); }
		SGVariant(bool b)                     { type_id = SGVariantType::BOOLEAN; val_bool = b; }
		SGVariant(int r, int g, int b, int a) { type_id = SGVariantType::COLOR; val_color = QColor(r, g, b, a); }
		SGVariant(const QColor & color)       { type_id = SGVariantType::COLOR; val_color = color; }
		SGVariant(const QStringList & sl)     { type_id = SGVariantType::STRING_LIST; val_string_list = sl; }

		~SGVariant();

		SGVariantType type_id;

		double val_double = 0.0;
		uint32_t val_uint = 0;
		int32_t val_int = 0;
		bool val_bool = false;
		QString val_string;
		QColor val_color;
		QStringList val_string_list;
		void * val_pointer = NULL; /* For internal usage - don't save this value in a file! */
	};


	QDebug operator<<(QDebug debug, const SGVariant & value);


	SGVariant sg_variant_true(void);
	SGVariant sg_variant_false(void);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VARIANT_H_ */
