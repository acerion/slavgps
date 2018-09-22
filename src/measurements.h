/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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
#ifndef _SG_MEASUREMENTS_H_
#define _SG_MEASUREMENTS_H_




#include <cmath>




#include <QString>




namespace SlavGPS {



#define SG_PRECISION_ALTITUDE           2
#define SG_PRECISION_ALTITUDE_WIDE      5
#define SG_ALTITUDE_PRECISION           2
#define SG_ALTITUDE_RANGE_MIN       -5000 /* [meters] */   /* See also VIK_VAL_MIN_ALT */
#define SG_ALTITUDE_RANGE_MAX       25000 /* [meters] */   /* See also VIK_VAL_MAX_ALT */


#define SG_PRECISION_DISTANCE   2
#define SG_PRECISION_SPEED      2
#define SG_PRECISION_COURSE     1
#define SG_PRECISION_LATITUDE   6
#define SG_PRECISION_LONGITUDE  6
#define SG_PRECISION_GRADIENT   2


#define DEGREE_SYMBOL "\302\260"




#define ALTI_TO_MPP 1.4017295
#define MPP_TO_ALTI 0.7134044

#define VIK_FEET_IN_METER 3.2808399
#define VIK_METERS_TO_FEET(X) ((X)*VIK_FEET_IN_METER)
#define VIK_FEET_TO_METERS(X) ((X)/VIK_FEET_IN_METER)

#define VIK_MILES_IN_METER 0.000621371192
#define VIK_METERS_TO_MILES(X) ((X)*VIK_MILES_IN_METER)
#define VIK_MILES_TO_METERS(X) ((X)/VIK_MILES_IN_METER)

#define VIK_NAUTICAL_MILES_IN_METER 0.000539957
#define VIK_METERS_TO_NAUTICAL_MILES(X) ((X)*VIK_NAUTICAL_MILES_IN_METER)
#define VIK_NAUTICAL_MILES_TO_METERS(X) ((X)/VIK_NAUTICAL_MILES_IN_METER)

/* MPS - Metres Per Second. */
/* MPH - Metres Per Hour. */
#define VIK_MPH_IN_MPS 2.23693629
#define VIK_MPS_TO_MPH(X) ((X)*VIK_MPH_IN_MPS)
#define VIK_MPH_TO_MPS(X) ((X)/VIK_MPH_IN_MPS)

/* KPH - Kilometres Per Hour. */
#define VIK_KPH_IN_MPS 3.6
#define VIK_MPS_TO_KPH(X) ((X)*VIK_KPH_IN_MPS)
#define VIK_KPH_TO_MPS(X) ((X)/VIK_KPH_IN_MPS)

#define VIK_KNOTS_IN_MPS 1.94384449
#define VIK_MPS_TO_KNOTS(X) ((X)*VIK_KNOTS_IN_MPS)
#define VIK_KNOTS_TO_MPS(X) ((X)/VIK_KNOTS_IN_MPS)

#define DEG2RAD(x) ((x)*(M_PI/180))
#define RAD2DEG(x) ((x)*(180/M_PI))

/* Mercator projection, latitude conversion (degrees). */
#define MERCLAT(x) (RAD2DEG(log(tan((0.25 * M_PI) + (0.5 * DEG2RAD(x))))))
#define DEMERCLAT(x) (RAD2DEG(atan(sinh(DEG2RAD(x)))))

#define VIK_DEFAULT_ALTITUDE 0.0
#define VIK_DEFAULT_DOP 0.0




	/* Coord display format. */
	enum class DegreeFormat {
		DDD,
		DMM,
		DMS,
		RAW,
	};




	enum class DistanceUnit {
		Kilometres,
		Miles,
		NauticalMiles,
	};




	enum class SupplementaryDistanceUnit {
		Meters,
		Yards,
	};



	enum class SpeedUnit {
		KilometresPerHour,
		MilesPerHour,
		MetresPerSecond,
		Knots
	};




	enum class HeightUnit {
		Metres,
		Feet,
	};




	class Angle {
	public:
		QString to_string(void) const;
		static QString get_course_string(double value, int precision = SG_PRECISION_COURSE);

		double value = 0.0;
	};




	class Measurements {
	public:
		static QString get_file_size_string(size_t file_size);

		static QString get_duration_string(time_t duration);
	};




	class Distance {
	public:
		Distance(double new_value = 0.0, SupplementaryDistanceUnit new_supplementary_distance_unit = SupplementaryDistanceUnit::Meters);
		Distance(double value, DistanceUnit distance_unit);


		Distance convert_to_unit(DistanceUnit distance_unit) const;
		Distance convert_to_unit(SupplementaryDistanceUnit supplementary_distance_unit) const;
		/* Recalculate to supplementary distance unit. Choose
		   the supplementary distance unit based on @distance_unit. */
		Distance convert_to_supplementary_unit(DistanceUnit distance_unit) const;

		/* Generate string with value and unit. Value
		   (magnitude) of distance does not influence units
		   used to present the value. E.g. "0.01" km will
		   always be presented as "0.01 km", never as "10 m". */
		QString to_string(void) const;

		/* Generate string with value and unit. Value
		   (magnitude) of distance may be used to decide how
		   the string will look like. E.g. "0.1 km" may be
		   presented as "100 m". */
		QString to_nice_string(void) const;

		Distance & operator+=(const Distance & rhs);
		Distance operator+(const Distance & rhs);
		Distance operator-(const Distance & rhs);

		/* Return "kilometers" or "miles" string.
		   This is a full string, not "km" or "mi". */
		static QString get_unit_full_string(DistanceUnit distance_unit);

		static double convert_meters_to(double distance, DistanceUnit distance_unit);

		bool is_valid(void) const;

		double value = 0.0;
	private:
		bool valid = false;

		DistanceUnit distance_unit;
		SupplementaryDistanceUnit supplementary_distance_unit;
		bool use_supplementary_distance_unit = false;
	};




	class Altitude {
	public:
		Altitude() {};
		Altitude(double value, HeightUnit height_unit);

		void set_value(double value); /* Set value, don't change unit. */
		double get_value(void) const;

		bool is_valid(void) const;

		/* Generate string containing only value, without unit
		   and without magnitude-dependent conversions of value.

		   Locale of the value in string is suitable for
		   saving the value in gpx or vik file. */
		const QString value_to_string_for_file(void) const;

		/* Generate string containing only value, without unit
		   and without magnitude-dependent conversions of value.

		   Locale of the value in string is suitable for
		   presentation to user. */
		const QString value_to_string(void) const;

		/* Generate string with value and unit. Value
		   (magnitude) of distance does not influence units
		   used to present the value. E.g. "0.01" km will
		   always be presented as "0.01 km", never as "10 m". */
		QString to_string(void) const;

		/* Generate string with value and unit. Value
		   (magnitude) of distance may be used to decide how
		   the string will look like. E.g. "0.1 km" may be
		   presented as "100 m". */
		QString to_nice_string(void) const;

		Altitude convert_to_unit(HeightUnit height_unit) const;

		Altitude & operator+=(const Altitude & rhs);
	        Altitude operator+(const Altitude & rhs);
		Altitude operator-(const Altitude & rhs);

		/* Return "meters" or "feet" string.
		   This is a full string, not "m" or "ft". */
		static QString get_unit_full_string(HeightUnit height_unit);

	private:
		double value = NAN;
		bool valid = false;
		HeightUnit unit;
	};




	class Speed {
	public:
		Speed() {};
		Speed(double value, SpeedUnit speed_unit);

		void set_value(double value); /* Set value, don't change unit. */
		double get_value(void) const;

		bool is_valid(void) const;

		/* Generate string containing only value, without unit
		   and without magnitude-dependent conversions of value.

		   Locale of the value in string is suitable for
		   saving the value in gpx or vik file. */
		//const QString value_to_string_for_file(void) const;

		/* Generate string containing only value, without unit
		   and without magnitude-dependent conversions of value.

		   Locale of the value in string is suitable for
		   presentation to user. */
		const QString value_to_string(void) const;

		/* Generate string with value and unit. Value
		   (magnitude) of distance does not influence units
		   used to present the value. E.g. "0.01" km/h will
		   always be presented as "0.01 km/h", never as "10
		   m/h". */
		QString to_string(void) const;

		/* Generate string with value and unit. Value
		   (magnitude) of distance may be used to decide how
		   the string will look like. E.g. "0.1 km/h" may be
		   presented as "100 m/h". */
		//QString to_nice_string(void) const;

		Speed convert_to_unit(SpeedUnit speed_unit) const;

		Speed & operator+=(const Speed & rhs);
	        Speed operator+(const Speed & rhs);
		Speed operator-(const Speed & rhs);

		/* Return "meters" or "feet" string.
		   This is a full string, not "m" or "ft". */
		//static QString get_unit_full_string(SpeedUnit speed_unit);

		/* Get string representing speed unit in abbreviated
		   form, e.g. "km/h". */
		static QString get_unit_string(SpeedUnit speed_unit);
		static double convert_mps_to(double speed_value, SpeedUnit speed_units);

		/* Generate string with value and unit. Value
		   (magnitude) of distance does not influence units
		   used to present the value. E.g. "0.01" km/h will
		   always be presented as "0.01 km/h", never as "10
		   m/h".

		   Unit is taken from global preferences. There is no
		   conversion of value from one unit to another. */
		static QString to_string(double value, int precision = SG_PRECISION_SPEED);

	private:
		double value = NAN;
		bool valid = false;
		SpeedUnit unit;
	};




}




#endif /* #ifndef _SG_MEASUREMENTS_H_ */
