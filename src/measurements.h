/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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




	/* Coord display format. */
	enum class DegreeFormat {
		DDD, /* "Decimal degrees", degrees as float: 74.244534 */
		DMM, /* "Degrees and decimal minutes", degrees, followed by minutes as float: 19°33.2435' */
		DMS, /* "Degrees, minutes and seconds", degrees, followed by minutes, followed by seconds as float: 30°17'44.3545' */
		Raw,
	};




	double c_to_double(const QString & string);




	class Altitude;
	class AltitudeType
	{
	public:
		class Unit {
		public:
			enum class E {
				Metres,
				Feet,
			};

			Unit() {}
			Unit(Unit::E unit) { this->u = unit; }

			static Unit internal_unit(void) { return AltitudeType::Unit(); }
			static Unit user_unit(void);

			Unit::E u = Unit::E::Metres; /* Default/internal unit. */

			bool operator==(const AltitudeType::Unit & rhs) const { return this->u == rhs.u; }
			bool operator!=(const AltitudeType::Unit & rhs) const { return !(*this == rhs);  }
		};

		typedef double LL;
		typedef Altitude Meas;
	};
	QDebug operator<<(QDebug debug, const AltitudeType::Unit & unit);




	class Gradient;
	class GradientType {
	public:
		class Unit {
		public:
			/* TODO: if you ever add another gradient unit, you
			   will have to review all places where a gradient
			   unit is used and update them accordingly. */
			enum class E {
				Percents, /* Default, internal unit. */
			};

			Unit() {}
			Unit(Unit::E unit) { this->u = unit; }

			static Unit internal_unit(void) { return GradientType::Unit(); }
			static Unit user_unit(void);

			Unit::E u = Unit::E::Percents; /* Default/internal unit. */

			bool operator==(const GradientType::Unit & rhs) const { return this->u == rhs.u; }
			bool operator!=(const GradientType::Unit & rhs) const { return !(*this == rhs);  }
		};

		typedef double LL;
		typedef Gradient Meas;
	};
	QDebug operator<<(QDebug debug, const GradientType::Unit & unit);




	class Time;
	class TimeType {
	public:

		class Unit {
		public:
			/* TODO: if you ever add another time unit, you will
			   have to review all places where a time unit is used
			   and update them accordingly. */
			enum class E {
				Seconds, /* Default, internal unit. */
			};

			Unit() {}
			Unit(Unit::E unit) { this->u = unit; }

			static Unit internal_unit(void) { return TimeType::Unit(); }
			static Unit user_unit(void);

			Unit::E u = Unit::E::Seconds; /* Default/internal unit. */

			bool operator==(const TimeType::Unit & rhs) const { return this->u == rhs.u; }
			bool operator!=(const TimeType::Unit & rhs) const { return !(*this == rhs);  }
		};

		typedef time_t LL;
		typedef Time Meas;
	};
	QDebug operator<<(QDebug debug, const TimeType::Unit & unit);




	class Duration;
	class DurationType {
	public:
		class Unit {
		public:
			enum class E {
				Seconds, /* Default, internal unit. */
				Minutes,
				Hours,
				Days
			};

			Unit() {}
			Unit(Unit::E unit) { this->u = unit; }

			static Unit internal_unit(void) { return DurationType::Unit(); }
			static Unit user_unit(void);

			Unit::E u = Unit::E::Seconds; /* Default/internal unit. */

			bool operator==(const DurationType::Unit & rhs) const { return this->u == rhs.u; }
			bool operator!=(const DurationType::Unit & rhs) const { return !(*this == rhs);  }
		};

		typedef time_t LL;
		typedef Duration Meas;
	};
	QDebug operator<<(QDebug debug, const DurationType::Unit & unit);




	class Distance;
	class DistanceType {
	public:
		class Unit {
		public:
			enum class E {
				Kilometres,
				Miles,
				NauticalMiles,

				/* TODO: numerical values of these two enums should be
				   somehow synchronized with Viking project. */
				Meters,
				Yards,
			};

			Unit() {}
			Unit(Unit::E unit) { this->u = unit; }

			static Unit internal_unit(void) { return DistanceType::Unit(); }
			static Unit user_unit(void);

			Unit::E u = Unit::E::Meters; /* Default/internal unit. */

			bool operator==(const DistanceType::Unit & rhs) const { return this->u == rhs.u; }
			bool operator!=(const DistanceType::Unit & rhs) const { return !(*this == rhs);  }
		};

		typedef double LL;
		typedef Distance Meas;
	};
	QDebug operator<<(QDebug debug, const DistanceType::Unit & unit);




	class Speed;
	class SpeedType {
	public:
		class Unit {
		public:
			enum class E {
				KilometresPerHour,
				MilesPerHour,
				MetresPerSecond,
				Knots
			};

			Unit() {}
			Unit(SpeedType::Unit::E unit) { this->u = unit; }

			static SpeedType::Unit internal_unit(void) { return SpeedType::Unit(); }
			static SpeedType::Unit user_unit(void);

			SpeedType::Unit::E u = SpeedType::Unit::E::MetresPerSecond; /* Default/internal unit. */

			bool operator==(const SpeedType::Unit & rhs) const { return this->u == rhs.u; }
			bool operator!=(const SpeedType::Unit & rhs) const { return !(*this == rhs);  }
		};

		typedef double LL;
		typedef Speed Meas;
	};
	QDebug operator<<(QDebug debug, const SpeedType::Unit & unit);




	class Angle;
	class AngleType {
	public:
		class Unit {
		public:
			enum class E {
				Radians, /* Default, internal unit. TODO: verify. */
				Degrees
			};

			Unit() {}
			Unit(Unit::E unit) { this->u = unit; }

			static Unit internal_unit(void) { return AngleType::Unit(); }
			static Unit user_unit(void);

			Unit::E u = Unit::E::Radians; /* Default/internal unit. */

			bool operator==(const AngleType::Unit & rhs) const { return this->u == rhs.u; }
			bool operator!=(const AngleType::Unit & rhs) const { return !(*this == rhs);  }
		};

		typedef double LL;
		typedef Angle Meas;
	};
	QDebug operator<<(QDebug debug, const AngleType::Unit & unit);




	template <typename T>
	class Measurement {

	public:
		using LL = typename T::LL;

		Measurement<T>() {}
		Measurement<T>(typename T::LL new_value, const typename T::Unit & new_unit) : Measurement<T>()
		{
			this->m_ll_value = new_value;
			this->m_valid = T::Meas::ll_value_is_valid(new_value);
			this->m_unit = new_unit;
		}

		Measurement<T>(const Measurement<T> & other)
		{
			this->m_ll_value = other.m_ll_value;
			this->m_unit  = other.m_unit;
			this->m_valid = other.m_valid;
		}

		/* Get/set current unit of measurement. */
		const typename T::Unit & unit(void) const { return this->m_unit; }
		void set_unit(const typename T::Unit & unit) { this->m_unit = unit; }

		static bool ll_value_is_valid(typename T::LL value);
		bool is_valid(void) const { return this->m_valid; }
		void invalidate(void)
		{
			this->m_valid = false;
			this->m_ll_value = 0;
		}


		/* Set value, don't change unit. */
		void set_ll_value(typename T::LL new_value)
		{
			this->m_ll_value = new_value;
			this->m_valid = T::Meas::ll_value_is_valid(new_value);
			/* this->m_unit = ; ... Don't change unit. */
		}

		/*
		  This function always returns verbatim value. It may
		  be invalid. It's up to caller to check if
		  measurement is invalid before calling this
		  getter.
		*/
		typename T::LL ll_value(void) const { return this->m_ll_value; }


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

		typename T::Meas convert_to_unit(const typename T::Unit & new_unit) const;

		static typename T::LL convert_to_unit(typename T::LL value, const typename T::Unit & from, const typename T::Unit & to);

		sg_ret convert_to_unit_in_place(const typename T::Unit & new_unit)
		{
			this->m_ll_value = Measurement<T>::convert_to_unit(this->m_ll_value, this->m_unit, new_unit);
			this->m_valid = Measurement<T>::ll_value_is_valid(this->m_ll_value);
			this->m_unit = new_unit;
			return this->m_valid ? sg_ret::ok : sg_ret::err;
		}

		/* Get string representing measurement unit in
		   abbreviated form, e.g. "km/h". */
		static QString unit_string(const typename T::Unit & unit);

		/* Return "kilometers" or "miles" string.
		   This is a full string, not "km" or "mi". */
		static QString unit_full_string(const typename T::Unit & unit);

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
			this->m_unit = T::Unit::internal_unit();

			return this->m_valid ? sg_ret::ok : sg_ret::err;
		}


		/* Will return INT_MIN or NAN if value is invalid. */
		typename T::LL floor(void) const;



		/* Generate string with value and unit. Value
		   (magnitude) of distance does not influence units
		   used to present the value. E.g. "0.01" km/h will
		   always be presented as "0.01 km/h", never as "10
		   m/h". */
		QString to_string(void) const;
		QString to_string(int precision) const;

		static QString ll_value_to_string(typename T::LL value, const typename T::Unit & unit);

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

		Measurement<T> & operator=(const Measurement<T> & rhs)
		{
			if (this == &rhs) {
				return *this;
			}

			this->m_ll_value = rhs.m_ll_value;
			this->m_valid = rhs.m_valid;
			this->m_unit = rhs.m_unit;

			return *this;
		}


		typename T::Meas operator+(double rhs) const { typename T::Meas result = *this; result += rhs; return result; }
		typename T::Meas operator-(double rhs) const { typename T::Meas result = *this; result -= rhs; return result; }


		typename T::Meas operator+(const typename T::Meas & rhs) const
		{
			typename T::Meas result;
			if (!this->m_valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid lhs";
				return result;
			}
			if (!rhs.m_valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid rhs";
				return result;
			}
			if (this->m_unit != rhs.m_unit) {
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << this->m_unit << rhs.m_unit;
				return result;
			}

			result = *this;
			result += rhs;
			return result;
		}


		typename T::Meas operator-(const typename T::Meas & rhs) const
		{
			typename T::Meas result;
			if (!this->m_valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid lhs";
				return result;
			}
			if (!rhs.m_valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid rhs";
				return result;
			}
			if (this->m_unit != rhs.m_unit) {
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << this->m_unit << rhs.m_unit;
				return result;
			}

			result = *this;
			result -= rhs;
			return result;
		}


		Measurement<T> & operator+=(const Measurement<T> & rhs)
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
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << this->m_unit << rhs.m_unit;
				return *this;
			}

			this->m_ll_value += rhs.m_ll_value;
			this->m_valid = !std::isnan(this->m_ll_value);
			return *this;
		}


		Measurement<T> & operator-=(const Measurement<T> & rhs)
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
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << this->m_unit << rhs.m_unit;
				return *this;
			}

			this->m_ll_value -= rhs.m_ll_value;
			this->m_valid = !std::isnan(this->m_ll_value);
			return *this;
		}

		Measurement<T> & operator+=(double rhs)
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

		Measurement<T> & operator-=(double rhs)
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

		Measurement<T> & operator*=(double rhs)
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

		Measurement<T> & operator/=(double rhs)
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




		bool operator==(const Measurement<T> & rhs) const
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
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << this->m_unit << rhs.m_unit;
				return false;
			}

			return this->m_ll_value == rhs.m_ll_value;
		}

		bool operator!=(const Measurement<T> & rhs) const
		{
			return !(*this == rhs);
		}

	protected:
		typename T::LL m_ll_value;
		typename T::Unit m_unit;
		bool m_valid;
	};
	template<typename T>
	QDebug operator<<(QDebug debug, const typename T::Unit & unit);

	template<typename T>
	double operator/(const T & lhs, const T & rhs); /* For getting proportion of two values. */

	template<typename T>
	T operator*(T lhs, double rhs) { lhs *= rhs; return lhs; }

	template<typename T>
	T operator/(T lhs, double rhs) { lhs /= rhs; return lhs; }

	template<typename T>
	bool operator<(const T & lhs, const T & rhs)
	{
		if (lhs.unit() != rhs.unit()) {
			qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << lhs.unit() << rhs.unit();
			return false;
		}
		return lhs.ll_value() < rhs.ll_value();
	}
	template<typename T>
	bool operator>(const T & lhs, const T & rhs)
	{
		return rhs < lhs;
	}
	template<typename T>
	bool operator<=(const T & lhs, const T & rhs)
	{
		return !(lhs > rhs);
	}
	template<typename T>
	bool operator>=(const T & lhs, const T & rhs)
	{
		return !(lhs < rhs);
	}

	/*
	  For getting proportion of two values.
	  Implemented in class declaration because of this: https://stackoverflow.com/a/10787730
	*/
	template<typename T>
	double operator/(const T & lhs, const T & rhs)
	{
		if (!lhs.is_valid()) {
			qDebug() << "WW    " << __FUNCTION__ << "Invalid 'lhs' operand";
			return NAN;
		}
		if (!rhs.is_valid()) {
			qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
			return NAN;
		}
		if (lhs.unit() != rhs.unit()) {
			qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << lhs.unit() << rhs.unit();
			return NAN;
		}
		if (rhs.is_zero()) {
			qDebug() << "WW    " << __FUNCTION__ << "rhs is zero";
			return NAN;
		}

		return (1.0 * lhs.ll_value()) / rhs.ll_value();
	}




	class Altitude : public Measurement<AltitudeType> {
	public:
		typedef AltitudeType::LL LL;
		typedef AltitudeType::Unit Unit;

		Altitude() {};
		Altitude(const Measurement<AltitudeType> & other) : Measurement<AltitudeType>(other) {}
		Altitude(AltitudeType::LL value, const AltitudeType::Unit & unit) : Measurement<AltitudeType>(value, unit) {}

		friend QDebug operator<<(QDebug debug, const Altitude & altitude);

		//using Measurement<AltitudeType>::Measurement;
		using Measurement<AltitudeType>::operator-;
		using Measurement<AltitudeType>::operator+;
		using Measurement<AltitudeType>::operator-=;
		using Measurement<AltitudeType>::operator=;
	};




	class Gradient : public Measurement<GradientType> {
	public:
		Gradient() {};
		Gradient(const Measurement<GradientType> & other) : Measurement<GradientType>(other) {}
		Gradient(GradientType::LL value, const GradientType::Unit & unit) : Measurement<GradientType>(value, unit) {}

		friend QDebug operator<<(QDebug debug, const Gradient & gradient);

		typedef GradientType::LL LL;
		typedef GradientType::Unit Unit;

		//using Measurement<GradientType>::Measurement;
		using Measurement<GradientType>::operator-;
		using Measurement<GradientType>::operator=;
	};




	class Time : public Measurement<TimeType> {
	public:
		Time() {};
		Time(const Measurement<TimeType> & other) : Measurement<TimeType>(other) {}
		Time(TimeType::LL value, const TimeType::Unit & unit) : Measurement<TimeType>(value, unit) {}

		friend QDebug operator<<(QDebug debug, const Time & time);

		typedef TimeType::LL LL;
		typedef TimeType::Unit Unit;

		//using Measurement<TimeType>::Measurement;
		using Measurement<TimeType>::operator-;
		using Measurement<TimeType>::operator=;

		QString to_timestamp_string(Qt::TimeSpec time_spec = Qt::LocalTime) const;
		QString strftime_local(const char * format) const;
		QString strftime_utc(const char * format) const;
		QString get_time_string(Qt::DateFormat format) const;
		QString get_time_string(Qt::DateFormat format, const Coord & coord) const;
		QString get_time_string(Qt::DateFormat format, const Coord & coord, const QTimeZone * tz) const;

		/*
		  Set value from a string that contains value only
		  (does not contain any token representing unit).
		  Assign default internal value to to measurement.
		*/
		sg_ret set_timestamp_from_char_string(const char * str);
		sg_ret set_timestamp_from_string(const QString & str);
	};




	class Duration : public Measurement<DurationType> {
	public:
		Duration() {};
		Duration(const Measurement<DurationType> & other) : Measurement<DurationType>(other) {}
		Duration(DurationType::LL value, const DurationType::Unit & unit) : Measurement<DurationType>(value, unit) {}

		friend QDebug operator<<(QDebug debug, const Duration & duration);

		typedef DurationType::LL LL;
		typedef DurationType::Unit Unit;

		//using Measurement<DurationType>::Measurement;
		using Measurement<DurationType>::operator-;
		using Measurement<DurationType>::operator=;

		static Duration get_abs_duration(const Time & later, const Time & earlier);

		/*
		  Set value from a string that contains value only
		  (does not contain any token representing unit).
		  Assign default internal value to to measurement.
		*/
		sg_ret set_duration_from_char_string(const char * str);
		sg_ret set_duration_from_string(const QString & str);
	};




	class Distance : public Measurement<DistanceType> {
	public:
		Distance() {};
		Distance(const Measurement<DistanceType> & other) : Measurement<DistanceType>(other) {}
		Distance(DistanceType::LL value, const DistanceType::Unit & unit) : Measurement<DistanceType>(value, unit) {}

		friend QDebug operator<<(QDebug debug, const Distance & distance);

		typedef DistanceType::LL LL;
		typedef DistanceType::Unit Unit;

		//using Measurement<DistanceType>::Measurement;
		using Measurement<DistanceType>::operator-;
		using Measurement<DistanceType>::operator=;
	};




	class Speed : public Measurement<SpeedType> {
	public:
		Speed() {};
		Speed(const Measurement<SpeedType> & other) : Measurement<SpeedType>(other) {}
		Speed(SpeedType::LL value, const SpeedType::Unit & unit) : Measurement<SpeedType>(value, unit) {}

		friend QDebug operator<<(QDebug debug, const Speed & speed);

		//using Measurement<SpeedType>::Measurement;
		using Measurement<SpeedType>::operator-;
		using Measurement<SpeedType>::operator=;
		//using Measurement<SpeedType>::user_unit;

		typedef SpeedType::LL LL;
		typedef SpeedType::Unit Unit;

		sg_ret make_speed(const Distance & distance, const Duration & duration);
		sg_ret make_speed(const Altitude & altitude, const Duration & duration);
	};




	class Angle : public Measurement<AngleType> {
	public:
		Angle() {};
		Angle(const Measurement<AngleType> & other) : Measurement<AngleType>(other) {}
		Angle(AngleType::LL value, const AngleType::Unit & unit) : Measurement<AngleType>(value, unit) {}

		friend QDebug operator<<(QDebug debug, const Angle & angle);

		typedef AngleType::LL LL;
		typedef AngleType::Unit Unit;

		//using Measurement<AngleType>::Measurement;
		//using Measurement<AngleType>::operator-;
		//using Measurement<AngleType>::operator=;

		static Angle get_vector_sum(const Angle & meas1, const Angle & meas2);
		/* Ensure that value is in range of 0-2pi (if the
		   value is valid). */
		void normalize(void);
	};




	class Measurements {
	public:
		static QString get_file_size_string(size_t file_size);
		static bool unit_tests(void);
	};




}




#endif /* #ifndef _SG_MEASUREMENTS_H_ */
