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
		Empty = 0,
		Double,
		Uint,
		Int,

		String,
		Boolean,
		Color,
		StringList,
		Pointer,

		/* These types are more abstract, closer to application domain than to machine language. */
		Timestamp,
		Latitude,
		Longitude,
		Altitude
	};

	QDebug operator<<(QDebug debug, const SGVariantType type_id);




	class SGVariant {
	public:
		/* Construct value of given type using data (text
		   representation of the value) from given string. */
		SGVariant(SGVariantType type_id, const char * str);
		SGVariant(SGVariantType type_id, const QString & str);

		SGVariant(const SGVariant & val); /* Copy constructor. */

		SGVariant()                           { type_id = SGVariantType::Empty; }
		SGVariant(double d,                   SGVariantType type_id = SGVariantType::Double);
		SGVariant(uint32_t u,                 SGVariantType type_id = SGVariantType::Uint);
		SGVariant(int32_t i,                  SGVariantType type_id = SGVariantType::Int);
		SGVariant(const QString & s,          SGVariantType type_id = SGVariantType::String);
		SGVariant(const char * s,             SGVariantType type_id = SGVariantType::String);
		SGVariant(bool b,                     SGVariantType type_id = SGVariantType::Boolean);
		SGVariant(int r, int g, int b, int a, SGVariantType type_id = SGVariantType::Color);
		SGVariant(const QColor & color,       SGVariantType type_id = SGVariantType::Color);
		SGVariant(const QStringList & sl,     SGVariantType type_id = SGVariantType::StringList);
		SGVariant(time_t timestamp,           SGVariantType type_id);
		/* Notice that there is no separate one-argument constructor for Timestamp data type.
		   It would be too similar to SGVariant(uint32_t u). Use SGVariant(time_t, SGVariantType) instead. */

		~SGVariant();

		time_t get_timestamp() const;
		double get_latitude() const;
		double get_longitude() const;
		double get_altitude() const;

		QString to_string() const;


		SGVariantType type_id;
		double val_double = 0.0;
		uint32_t val_uint = 0;
		int32_t val_int = 0;
		bool val_bool = false;
		QString val_string;
		QColor val_color;
		QStringList val_string_list;
		void * val_pointer = NULL; /* For internal usage - don't save this value in a file! */

	private:
		time_t val_timestamp;    /* SGVariantType::TIMESTAMP */
		double val_lat_lon_alt;  /* SGVariantType::LATITUDE/LONGITUDE/ALTITUDE */
	};


	QDebug operator<<(QDebug debug, const SGVariant & value);


	SGVariant sg_variant_true(void);
	SGVariant sg_variant_false(void);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VARIANT_H_ */
