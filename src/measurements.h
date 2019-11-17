/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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
#include <climits>




#include <QString>
#include <QTimeZone>




#include "globals.h"




namespace SlavGPS {



#define SG_PRECISION_ALTITUDE           2
#define SG_PRECISION_ALTITUDE_WIDE      5
#define SG_ALTITUDE_PRECISION           2
#define SG_ALTITUDE_RANGE_MIN       -5000 /* [meters] */
#define SG_ALTITUDE_RANGE_MAX       25000 /* [meters] */

#define SG_MEASUREMENT_PRECISION_FOR_FILE_MAX   16


#define SG_PRECISION_DISTANCE   2
#define SG_PRECISION_SPEED      2
#define SG_PRECISION_COURSE     1
#define SG_PRECISION_GRADIENT   2


#define DEGREE_SYMBOL "\302\260"     /* "\u00b0" */
#define SG_MEASUREMENT_INVALID_VALUE_STRING "--"
#define SG_MEASUREMENT_INVALID_UNIT_STRING  "??"




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

#define VIK_DEFAULT_DOP 0.0




	class Coord;
	class Duration;




	/* Coord display format. */
	enum class DegreeFormat {
		DDD, /* "Decimal degrees", degrees as float: 74.244534 */
		DMM, /* "Degrees and decimal minutes", degrees, followed by minutes as float: 19°33.2435' */
		DMS, /* "Degrees, minutes and seconds", degrees, followed by minutes, followed by seconds as float: 30°17'44.3545' */
		Raw,
	};




	enum class DistanceUnit {
		Kilometres,
		Miles,
		NauticalMiles,

		/* TODO: numerical values of these two enums should be
		   somehow synchronized with Viking project. */
		Meters,
		Yards,
	};
#define SG_MEASUREMENT_INTERNAL_UNIT_DISTANCE DistanceUnit::Meters




	enum class SpeedUnit {
		KilometresPerHour,
		MilesPerHour,
		MetresPerSecond,
		Knots
	};
#define SG_MEASUREMENT_INTERNAL_UNIT_SPEED SpeedUnit::MetresPerSecond




	enum class HeightUnit {
		Metres,
		Feet,
	};
#define SG_MEASUREMENT_INTERNAL_UNIT_HEIGHT HeightUnit::Metres




	/* TODO: if you ever add another time unit, you will have to
	   review all places where a time unit is used and update them
	   accordingly. */
	enum class TimeUnit {
		Seconds, /* Default, internal unit. */
	};
#define SG_MEASUREMENT_INTERNAL_UNIT_TIME TimeUnit::Seconds




	/* TODO: if you ever add another gradient unit, you will have
	   to review all places where a gradient unit is used and
	   update them accordingly. */
	enum class GradientUnit {
		Percents, /* Default, internal unit. */
	};
#define SG_MEASUREMENT_INTERNAL_UNIT_GRADIENT GradientUnit::Percents




	enum class AngleUnit {
		Radians, /* Default, internal unit. TODO: verify. */
		Degrees
	};
#define SG_MEASUREMENT_INTERNAL_UNIT_ANGLE AngleUnit::Radians




	/* Low-level types corresponding to measurement classes. */
	typedef time_t Time_ll;
	typedef double Distance_ll;
	typedef double Angle_ll;
	typedef double Altitude_ll;
	typedef double Speed_ll;
	typedef double Gradient_ll;



	double c_to_double(const QString & string);



	template <typename Tu, typename Tll>
	class Measurement {
	public:
		Measurement() {}
		Measurement(Tll new_value, Tu new_unit) : Measurement()
		{
			this->m_ll_value = new_value;
			this->m_valid = Measurement::ll_value_is_valid(new_value);
			this->m_unit = new_unit;
		}

		Measurement(const Measurement & other)
		{
			this->m_ll_value = other.m_ll_value;
			this->m_unit  = other.m_unit;
			this->m_valid = other.m_valid;
		}

		static Tu get_user_unit(void);
		static Tu get_internal_unit(void);

		/* Get/set current unit of measurement. */
		Tu get_unit(void) const { return this->m_unit; }
		void set_unit(Tu unit) { this->m_unit = unit; }

		static bool ll_value_is_valid(Tll value);
		bool is_valid(void) const { return this->m_valid; }
		void invalidate(void)
		{
			this->m_valid = false;
			this->m_ll_value = 0;
		}


		/* Set value, don't change unit. */
		void set_ll_value(Tll new_value)
		{
			this->m_ll_value = new_value;
			this->m_valid = Measurement<Tu, Tll>::ll_value_is_valid(new_value);
			/* this->m_unit = ; ... Don't change unit. */
		}

		/*
		  This function always returns verbatim value. It may
		  be invalid. It's up to caller to check if
		  measurement is invalid before calling this
		  getter.
		*/
		Tll get_ll_value(void) const { return this->m_ll_value; }


		/*
		  Generate string containing only value, without unit
		  and without magnitude-dependent conversions of
		  value.

		  Locale of the value in string is suitable for saving
		  the value in gpx or vik file.

		  Default precision is very large to make sure that we
		  don't lose too much information when saving value to
		  file.
		*/
		const QString value_to_string_for_file(int precision = SG_MEASUREMENT_PRECISION_FOR_FILE_MAX) const;

		/* Generate string containing only value, without unit
		   and without magnitude-dependent conversions of value.

		   Locale of the value in string is suitable for
		   presentation to user. */
		const QString value_to_string(void) const;

		Measurement convert_to_unit(Tu new_unit) const;

		static Tll convert_to_unit(Tll value, Tu from, Tu to);

		sg_ret convert_to_unit_in_place(Tu new_unit)
		{
			this->m_ll_value = Measurement::convert_to_unit(this->m_ll_value, this->m_unit, new_unit);
			this->m_valid = Measurement::ll_value_is_valid(this->m_ll_value);
			this->m_unit = new_unit;
			return this->m_valid ? sg_ret::ok : sg_ret::err;
		}

		/* Get string representing speed unit in abbreviated
		   form, e.g. "km/h". */
		static QString get_unit_string(Tu unit);

		/* Return "kilometers" or "miles" string.
		   This is a full string, not "km" or "mi". */
		static QString get_unit_full_string(Tu unit);

		/* Generate string with value and unit. Value
		   (magnitude) of distance may be used to decide how
		   the string will look like. E.g. "0.1 km" may be
		   presented as "100 m". */
		QString to_nice_string(void) const;

		/*
		  Set value from a string that contains value only
		  (does not contain any token representing unit).
		  Assign default internal unit to to measurement.

		  Use these two methods for non-Time classes.
		*/
		sg_ret set_value_from_char_string(const char * str)
		{
			if (NULL == str) {
				qDebug() << "EE    " << __FUNCTION__ << "Attempting to set invalid value of altitude from NULL string";
				this->m_valid = false;
				return sg_ret::err_arg;
			} else {
				return this->set_value_from_string(QString(str));
			}
		}
		sg_ret set_value_from_string(const QString & str)
		{
			this->m_ll_value = c_to_double(str);
			this->m_valid = !std::isnan(this->m_ll_value);
			this->m_unit = Measurement<Tu, Tll>::get_internal_unit();

			return this->m_valid ? sg_ret::ok : sg_ret::err;
		}


		/*
		  Set value from a string that contains value only
		  (does not contain any token representing unit).
		  Assign default internal value to to measurement.

		  To be used only with Time class.
		*/
		sg_ret set_timestamp_from_char_string(const char * str);
		sg_ret set_timestamp_from_string(const QString & str);



		/* Will return INT_MIN or NAN if value is invalid. */
		Tll floor(void) const;

		/* Specific to Time. Keeping it in Measurement class is problematic. */
		QString to_timestamp_string(Qt::TimeSpec time_spec = Qt::LocalTime) const;
		QString strftime_local(const char * format) const;
		QString strftime_utc(const char * format) const;
		QString get_time_string(Qt::DateFormat format) const;
		QString get_time_string(Qt::DateFormat format, const Coord & coord) const;
		QString get_time_string(Qt::DateFormat format, const Coord & coord, const QTimeZone * tz) const;
		static Duration get_abs_duration(const Measurement & later, const Measurement & earlier);


		/* Specific to Speed. */
		sg_ret make_speed(const Measurement<DistanceUnit, Distance_ll> & distance, const Duration & duration);
		sg_ret make_speed(const Measurement<HeightUnit, Altitude_ll> & distance, const Duration & duration);


		/* Specific to Angle. */
		static Measurement get_vector_sum(const Measurement & meas1, const Measurement & meas2);
		/* Ensure that value is in range of 0-2pi (if the
		   value is valid). */
		void normalize(void);


		/* Generate string with value and unit. Value
		   (magnitude) of distance does not influence units
		   used to present the value. E.g. "0.01" km/h will
		   always be presented as "0.01 km/h", never as "10
		   m/h". */
		QString to_string(void) const;
		QString to_string(int precision) const;

		static QString ll_value_to_string(Tll value, Tu unit);

		/* Is this measurement so small that it can be treated as zero?

		   For Time class using this method has sense only if
		   value of type Time represents duration.  There is
		   no much sense to see whether a date is zero. */
		bool is_zero(void) const;

		/*
		  I don't want to create '<' '<=' and similar
		  operators with 'double' argument because comparing
		  Measurement and double makes sense in very few
		  cases. One of them is comparing against zero, and
		  for those limited cases we have three methods:
		  is_zero(), is_positive() and is_negative().
		*/
		bool is_negative(void) const
		{
			if (this->m_valid) {
				return this->m_ll_value < 0;
			} else {
				return false;
			}
		}
		bool is_positive(void) const
		{
			if (this->m_valid) {
				return this->m_ll_value > 0;
			} else {
				return false;
			}
		}

		Measurement & operator=(const Measurement & rhs)
		{
			if (this == &rhs) {
				return *this;
			}

			this->m_ll_value = rhs.m_ll_value;
			this->m_valid = rhs.m_valid;
			this->m_unit = rhs.m_unit;

			return *this;
		}


		Measurement operator+(double rhs) const { Measurement result = *this; result += rhs; return result; }
		Measurement operator-(double rhs) const { Measurement result = *this; result -= rhs; return result; }


		Measurement operator+(const Measurement & rhs) const
		{
			Measurement result;
			if (!this->m_valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid lhs";
				return result;
			}
			if (!rhs.m_valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid rhs";
				return result;
			}
			if (this->m_unit != rhs.m_unit) {
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << (int) this->m_unit << (int) rhs.m_unit;
				return result;
			}

			result = *this;
			result += rhs;
			return result;
		}


		Measurement operator-(const Measurement & rhs) const
		{
			Measurement result;
			if (!this->m_valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid lhs";
				return result;
			}
			if (!rhs.m_valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid rhs";
				return result;
			}
			if (this->m_unit != rhs.m_unit) {
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << (int) this->m_unit << (int) rhs.m_unit;
				return result;
			}

			result = *this;
			result -= rhs;
			return result;
		}


		Measurement & operator+=(const Measurement & rhs)
		{
			if (!this->m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}
			if (!rhs.m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}
			if (this->m_unit != rhs.m_unit) {
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << (int) this->m_unit << (int) rhs.m_unit;
				return *this;
			}

			this->m_ll_value += rhs.m_ll_value;
			this->m_valid = !std::isnan(this->m_ll_value);
			return *this;
		}


		Measurement & operator-=(const Measurement & rhs)
		{
			if (!this->m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}
			if (!rhs.m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}
			if (this->m_unit != rhs.m_unit) {
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << (int) this->m_unit << (int) rhs.m_unit;
				return *this;
			}

			this->m_ll_value -= rhs.m_ll_value;
			this->m_valid = !std::isnan(this->m_ll_value);
			return *this;
		}

		Measurement & operator+=(double rhs)
		{
			if (!this->m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}
			if (std::isnan(rhs)) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}

			this->m_ll_value += rhs;
			this->m_valid = !std::isnan(this->m_ll_value);
			return *this;
		}

		Measurement & operator-=(double rhs)
		{
			if (!this->m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}
			if (std::isnan(rhs)) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}

			this->m_ll_value -= rhs;
			this->m_valid = !std::isnan(this->m_ll_value);
			return *this;
		}

		//friend Measurement operator+(Measurement lhs, const Measurement & rhs) { lhs += rhs; return lhs; }
		//friend Measurement operator-(Measurement lhs, const Measurement & rhs) { lhs -= rhs; return lhs; }

		Measurement & operator*=(double rhs)
		{
			if (!this->m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}
			if (std::isnan(rhs)) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}

			this->m_ll_value *= rhs;
			this->m_valid = !std::isnan(this->m_ll_value);
			return *this;
		}

		Measurement & operator/=(double rhs)
		{
			if (!this->m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}
			if (std::isnan(rhs)) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}
			if (0.0 == rhs) {
				qDebug() << "WW    " << __FUNCTION__ << "rhs is zero";
				return *this;
			}

			this->m_ll_value /= rhs;
			this->m_valid = !std::isnan(this->m_ll_value);
			return *this;
		}

		friend Measurement operator*(Measurement lhs, double rhs) { lhs *= rhs; return lhs; }
		friend Measurement operator/(Measurement lhs, double rhs) { lhs /= rhs; return lhs; }

		friend bool operator<(const Measurement<Tu, Tll> & lhs, const Measurement<Tu, Tll> & rhs)
		{
			if (lhs.m_unit != rhs.m_unit) {
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << (int) lhs.m_unit << (int) rhs.m_unit;
				return false;
			}
			return lhs.m_ll_value < rhs.m_ll_value;
		}
		friend bool operator>(const Measurement<Tu, Tll> & lhs, const Measurement<Tu, Tll> & rhs)
		{
			return rhs < lhs;
		}
		friend bool operator<=(const Measurement<Tu, Tll> & lhs, const Measurement<Tu, Tll> & rhs)
		{
			return !(lhs > rhs);
		}
		friend bool operator>=(const Measurement<Tu, Tll> & lhs, const Measurement<Tu, Tll> & rhs)
		{
			return !(lhs < rhs);
		}
		friend QDebug operator<<(QDebug debug, const Measurement<Tu, Tll> & meas)
		{
			debug << meas.to_string();
			return debug;
		}

		/*
		  For getting proportion of two values.
		  Implemented in class declaration because of this: https://stackoverflow.com/a/10787730
		*/
		friend double operator/(const Measurement<Tu, Tll> & lhs, const Measurement<Tu, Tll> & rhs)
		{
			if (!lhs.m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'lhs' operand";
				return NAN;
			}
			if (!rhs.m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return NAN;
			}
			if (lhs.m_unit != rhs.m_unit) {
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << (int) lhs.m_unit << (int) rhs.m_unit;
				return NAN;
			}
			if (rhs.is_zero()) {
				qDebug() << "WW    " << __FUNCTION__ << "rhs is zero";
				return NAN;
			}

			return (1.0 * lhs.m_ll_value) / rhs.m_ll_value;
		}

		bool operator==(const Measurement & rhs) const
		{
			if (!this->m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Comparing invalid value";
				return false;
			}
			if (!rhs.m_valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Comparing invalid value";
				return false;
			}
			if (this->m_unit != rhs.m_unit) {
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << (int) this->m_unit << (int) rhs.m_unit;
				return false;
			}

			return this->m_ll_value == rhs.m_ll_value;
		}

		bool operator!=(const Measurement & rhs) const
		{
			return !(*this == rhs);
		}

		Tll m_ll_value = 0;

	private:
		Tu m_unit = (Tu) 565; /* Invalid unit. */
		bool m_valid = false;
	};
	template<typename Tu, typename Tll>
	double operator/(const Measurement<Tu, Tll> & lhs, const Measurement<Tu, Tll> & rhs); /* For getting proportion of two values. */




	typedef Measurement<HeightUnit, Altitude_ll> Altitude;
	typedef Measurement<SpeedUnit, Speed_ll> Speed;
	typedef Measurement<GradientUnit, Gradient_ll> Gradient;
	typedef Measurement<TimeUnit, Time_ll> Time;
	typedef Measurement<AngleUnit, Angle_ll> Angle;
	typedef Measurement<DistanceUnit, Distance_ll> Distance;




	/*
	  TODO_DEFINITELY_LATER: perhaps Duration_ll data type should
	  be able to hold negative values. If Duration will be used to
	  store difference between two Time points, this may be
	  necessary for Duration_ll to have this property.
	*/
	class Duration : public Time {
	public:
		Duration() : Time() {}
		Duration(Time_ll new_value, TimeUnit new_unit) : Time(new_value, new_unit) {}

		QString to_string(void) const;
	};




	class Measurements {
	public:
		static QString get_file_size_string(size_t file_size);
		static bool unit_tests(void);
	};




}




#endif /* #ifndef _SG_MEASUREMENTS_H_ */
