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

		SGVariant()                   { type_id = SGVariantType::EMPTY; }
		SGVariant(double d_)          { type_id = SGVariantType::DOUBLE; d = d_; }
		SGVariant(uint32_t u_)        { type_id = SGVariantType::UINT; u = u_; }
		SGVariant(int32_t i_)         { type_id = SGVariantType::INT; i = i_; }
		SGVariant(const QString & s_) { type_id = SGVariantType::STRING; s = s_; }
		SGVariant(const char * s_)    { type_id = SGVariantType::STRING; s = QString(s_); }
		SGVariant(bool b_)            { type_id = SGVariantType::BOOLEAN; b = b_; }
		SGVariant(int r_, int g_, int b_, int a_) { type_id = SGVariantType::COLOR; c.r = r_; c.g = g_; c.b = b_; c.a = a_; }
		SGVariant(const QColor & color);
		SGVariant(const QStringList & sl_) { type_id = SGVariantType::STRING_LIST; sl = sl_; }

		~SGVariant();

		void to_qcolor(QColor & color) const;

		SGVariantType type_id;

		double d = 0.0;
		uint32_t u = 0;
		int32_t i = 0;
		bool b = false;
		QString s;
		struct { int r; int g; int b; int a; } c;
		QStringList sl;
		void * ptr = NULL; /* For internal usage - don't save this value in a file! */
	};


	QDebug operator<<(QDebug debug, const SGVariant & value);


	SGVariant sg_variant_true(void);
	SGVariant sg_variant_false(void);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VARIANT_H_ */
