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
#include <cstdio>




#include <QString>
#include <QColor>




#include "measurements.h"
#include "lat_lon.h"
#include "ui_util.h"




namespace SlavGPS {




	class Pickle;




	/* id is index. */
	enum class SGVariantType {
		Empty = 0,
		Double,
		Int,

		Enumeration, /* Underlying, machine type shall be "int". */
		String,
		Boolean,
		Color,
		StringList,
		Pointer,

		/* These types are more abstract, closer to application domain than to machine language. */
		Timestamp,
		DurationType, /* Time span. */
		Latitude,
		Longitude,
		AltitudeType,
		ImageAlphaType,

	};

	QDebug operator<<(QDebug debug, const SGVariantType type_id);



	union SGVariantPODFields {
		double val_double;
		int32_t val_int;
		int val_enumeration;
		bool val_bool;
		void * val_pointer; /* For internal usage - don't save this value in a file! */
	};




	class SGVariant {
	public:
		/* Construct value of given type using data (text
		   representation of the value) from given string. */
		SGVariant(SGVariantType type_id, const char * str);
		SGVariant(SGVariantType type_id, const QString & str);

		SGVariant(const SGVariant & val); /* Copy constructor. */

		void marshall(Pickle & pickle, SGVariantType type_id) const;
		static SGVariant unmarshall(Pickle & pickle, SGVariantType expected_type_id);

		SGVariant()                           { type_id = SGVariantType::Empty; }
		SGVariant(double d,                   SGVariantType type_id = SGVariantType::Double);

		/* This constructor can be used to construct both
		   SGVariantType::Int and SGVariantType::Enumeration.
		   Don't set default value of second argument, force
		   client code to explicitly specify type id of
		   created variant. */
		SGVariant(int32_t i,                  SGVariantType type_id);

		/* If type_id == SGVariantType::StringList, these two
		   constructors treat '\n' characters in @param s as
		   string separators, and split @param s into list of
		   strings accordingly. */
		SGVariant(const QString & s,          SGVariantType type_id = SGVariantType::String);
		SGVariant(const char * s,             SGVariantType type_id = SGVariantType::String);

		SGVariant(bool b,                     SGVariantType type_id = SGVariantType::Boolean);
		SGVariant(int r, int g, int b, int a, SGVariantType type_id = SGVariantType::Color);
		SGVariant(const QColor & color,       SGVariantType type_id = SGVariantType::Color);
		SGVariant(const QStringList & sl,     SGVariantType type_id = SGVariantType::StringList);
		SGVariant(const Latitude & lat,       SGVariantType type_id = SGVariantType::Latitude);
		SGVariant(const Longitude & lon,      SGVariantType type_id = SGVariantType::Longitude);
		SGVariant(const Altitude & a,         SGVariantType type_id = SGVariantType::AltitudeType);
		SGVariant(const Time & timestamp,     SGVariantType type_id = SGVariantType::Timestamp);
		SGVariant(const Duration & duration,  SGVariantType type_id = SGVariantType::DurationType);
		SGVariant(const ImageAlpha & alpha,   SGVariantType type_id = SGVariantType::ImageAlphaType);

		~SGVariant();

		Time get_timestamp(void) const;
		Duration get_duration(void) const;
		Latitude get_latitude(void) const;
		Longitude get_longitude(void) const;
		Altitude get_altitude(void) const;
		ImageAlpha alpha(void) const;

		QString to_string() const;

		void write(FILE * file, const QString & param_name) const;

		SGVariant & operator=(const SGVariant & other);

		bool is_valid(void) const { return this->type_id != SGVariantType::Empty; }

		static bool unit_tests(void);

		SGVariantType type_id;

		SGVariantPODFields u;
		QString val_string;
		QColor val_color;
		QStringList val_string_list;
	private:
		Time val_timestamp;     /* SGVariantType::Timestamp */
		Duration val_duration;  /* SGVariantType::DurationType */
		Latitude lat;
		Longitude lon;
		Altitude altitude;
		ImageAlpha m_alpha;
	};


	QDebug operator<<(QDebug debug, const SGVariant & value);


	SGVariant sg_variant_true(void);
	SGVariant sg_variant_false(void);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VARIANT_H_ */
