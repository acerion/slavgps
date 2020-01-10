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



	class DistanceUnit {
	public:
		enum class Unit {
			Kilometres,
			Miles,
			NauticalMiles,

			/* TODO: numerical values of these two enums should be
			   somehow synchronized with Viking project. */
			Meters,
			Yards,
		};

		DistanceUnit() {}
		DistanceUnit(DistanceUnit::Unit unit) { this->u = unit; }

		static DistanceUnit internal_unit(void) { return DistanceUnit(); }
		static DistanceUnit user_unit(void);
		bool operator==(const DistanceUnit & rhs) const { return this->u == rhs.u; }
		bool operator!=(const DistanceUnit & rhs) const { return !(*this == rhs); }

		DistanceUnit::Unit u = DistanceUnit::Unit::Meters;
	};
	QDebug operator<<(QDebug debug, const DistanceUnit & unit);




	class SpeedUnit {
	public:
		enum class Unit {
			KilometresPerHour,
			MilesPerHour,
			MetresPerSecond,
			Knots
		};

		SpeedUnit() {}
		SpeedUnit(SpeedUnit::Unit unit) { this->u = unit; }

		static SpeedUnit internal_unit(void) { return SpeedUnit(); }
		static SpeedUnit user_unit(void);
		bool operator==(const SpeedUnit & rhs) const { return this->u == rhs.u; }
		bool operator!=(const SpeedUnit & rhs) const { return !(*this == rhs); }

		SpeedUnit::Unit u = SpeedUnit::Unit::MetresPerSecond;
	};
	QDebug operator<<(QDebug debug, const SpeedUnit & unit);




	class HeightUnit {
	public:
		enum class Unit {
			Metres,
			Feet,
		};

		HeightUnit() {}
		HeightUnit(HeightUnit::Unit unit) { this->u = unit; }

		static HeightUnit internal_unit(void) { return HeightUnit(); }
		static HeightUnit user_unit(void);
		bool operator==(const HeightUnit & rhs) const { return this->u == rhs.u; }
		bool operator!=(const HeightUnit & rhs) const { return !(*this == rhs); }

		HeightUnit::Unit u = HeightUnit::Unit::Metres;
	};
	QDebug operator<<(QDebug debug, const HeightUnit & unit);




	class TimeUnit {
	public:
		/* TODO: if you ever add another time unit, you will
		   have to review all places where a time unit is used
		   and update them accordingly. */
		enum class Unit {
			Seconds, /* Default, internal unit. */
		};

		TimeUnit() {}
		TimeUnit(TimeUnit::Unit unit) { this->u = unit; }

		static TimeUnit internal_unit(void) { return TimeUnit(); }
		static TimeUnit user_unit(void);
		bool operator==(const TimeUnit & rhs) const { return this->u == rhs.u; }
		bool operator!=(const TimeUnit & rhs) const { return !(*this == rhs); }

		TimeUnit::Unit u = TimeUnit::Unit::Seconds;
	};
	QDebug operator<<(QDebug debug, const TimeUnit & unit);




	class DurationUnit {
	public:
		enum class Unit {
			Seconds, /* Default, internal unit. */
			Minutes,
			Hours,
			Days
		};

		DurationUnit() {}
		DurationUnit(DurationUnit::Unit unit) { this->u = unit; }

		static DurationUnit internal_unit(void) { return DurationUnit(); }
		static DurationUnit user_unit(void);
		bool operator==(const DurationUnit & rhs) const { return this->u == rhs.u; }
		bool operator!=(const DurationUnit & rhs) const { return !(*this == rhs); }

		DurationUnit::Unit u = DurationUnit::Unit::Seconds;
	};
	QDebug operator<<(QDebug debug, const DurationUnit & unit);




	class GradientUnit {
	public:
		/* TODO: if you ever add another gradient unit, you
		   will have to review all places where a gradient
		   unit is used and update them accordingly. */
		enum class Unit {
			Percents, /* Default, internal unit. */
		};

		GradientUnit() {}
		GradientUnit(GradientUnit::Unit unit) { this->u = unit; }

		static GradientUnit internal_unit(void) { return GradientUnit(); }
		static GradientUnit user_unit(void);
		bool operator==(const GradientUnit & rhs) const { return this->u == rhs.u; }
		bool operator!=(const GradientUnit & rhs) const { return !(*this == rhs); }

		GradientUnit::Unit u = GradientUnit::Unit::Percents;
	};
	QDebug operator<<(QDebug debug, const GradientUnit & unit);




	class AngleUnit {
	public:
		enum class Unit {
			Radians, /* Default, internal unit. TODO: verify. */
			Degrees
		};

		AngleUnit() {}
		AngleUnit(AngleUnit::Unit unit) { this->u = unit; }

		static AngleUnit internal_unit(void) { return AngleUnit(); }
		static AngleUnit user_unit(void);
		bool operator==(const AngleUnit & rhs) const { return this->u == rhs.u; }
		bool operator!=(const AngleUnit & rhs) const { return !(*this == rhs); }

		AngleUnit::Unit u = AngleUnit::Unit::Radians;
	};
	QDebug operator<<(QDebug debug, const AngleUnit & unit);




	/* Low-level types corresponding to measurement classes. */
	typedef time_t Time_ll;
	typedef long int Duration_ll; /* Signed int allows negative durations. Maybe I will regret it later ;) */
	typedef double Distance_ll;
	typedef double Angle_ll;
	typedef double Altitude_ll;
	typedef double Speed_ll;
	typedef double Gradient_ll;



	double c_to_double(const QString & string);



	template <typename Tll, typename Tu>
	class Measurement {
	public:
		Measurement<Tll, Tu>() {}
		Measurement<Tll, Tu>(Tll new_value, const Tu & new_unit) : Measurement<Tll, Tu>()
		{
			this->m_ll_value = new_value;
			this->m_valid = Measurement<Tll, Tu>::ll_value_is_valid(new_value);
			this->m_unit = new_unit;
		}

		Measurement<Tll, Tu>(const Measurement<Tll, Tu> & other)
		{
			this->m_ll_value = other.m_ll_value;
			this->m_unit  = other.m_unit;
			this->m_valid = other.m_valid;
		}

		static Tu user_unit(void) { return Tu::user_unit(); };
		static Tu internal_unit(void) { return Tu::internal_unit(); };

		/* Get/set current unit of measurement. */
		const Tu & unit(void) const { return this->m_unit; }
		void set_unit(const Tu & unit) { this->m_unit = unit; }

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
			this->m_valid = Measurement<Tll, Tu>::ll_value_is_valid(new_value);
			/* this->m_unit = ; ... Don't change unit. */
		}

		/*
		  This function always returns verbatim value. It may
		  be invalid. It's up to caller to check if
		  measurement is invalid before calling this
		  getter.
		*/
		Tll ll_value(void) const { return this->m_ll_value; }


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

		Measurement<Tll, Tu> convert_to_unit(const Tu & new_unit) const;

		static Tll convert_to_unit(Tll value, const Tu & from, const Tu & to);

		sg_ret convert_to_unit_in_place(const Tu & new_unit)
		{
			this->m_ll_value = Measurement<Tll, Tu>::convert_to_unit(this->m_ll_value, this->m_unit, new_unit);
			this->m_valid = Measurement<Tll, Tu>::ll_value_is_valid(this->m_ll_value);
			this->m_unit = new_unit;
			return this->m_valid ? sg_ret::ok : sg_ret::err;
		}

		/* Get string representing measurement unit in
		   abbreviated form, e.g. "km/h". */
		static QString unit_string(const Tu & unit);

		/* Return "kilometers" or "miles" string.
		   This is a full string, not "km" or "mi". */
		static QString unit_full_string(const Tu & unit);

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
			this->m_unit = Measurement<Tll, Tu>::internal_unit();

			return this->m_valid ? sg_ret::ok : sg_ret::err;
		}


		/* Will return INT_MIN or NAN if value is invalid. */
		Tll floor(void) const;



		/* Generate string with value and unit. Value
		   (magnitude) of distance does not influence units
		   used to present the value. E.g. "0.01" km/h will
		   always be presented as "0.01 km/h", never as "10
		   m/h". */
		QString to_string(void) const;
		QString to_string(int precision) const;

		static QString ll_value_to_string(Tll value, const Tu & unit);

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

		Measurement<Tll, Tu> & operator=(const Measurement<Tll, Tu> & rhs)
		{
			if (this == &rhs) {
				return *this;
			}

			this->m_ll_value = rhs.m_ll_value;
			this->m_valid = rhs.m_valid;
			this->m_unit = rhs.m_unit;

			return *this;
		}


		Measurement<Tll, Tu> operator+(double rhs) const { Measurement<Tll, Tu> result = *this; result += rhs; return result; }
		Measurement<Tll, Tu> operator-(double rhs) const { Measurement<Tll, Tu> result = *this; result -= rhs; return result; }


		Measurement<Tll, Tu> operator+(const Measurement<Tll, Tu> & rhs) const
		{
			Measurement<Tll, Tu> result;
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


		Measurement<Tll, Tu> operator-(const Measurement<Tll, Tu> & rhs) const
		{
			Measurement<Tll, Tu> result;
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


		Measurement<Tll, Tu> & operator+=(const Measurement<Tll, Tu> & rhs)
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


		Measurement<Tll, Tu> & operator-=(const Measurement<Tll, Tu> & rhs)
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

		Measurement<Tll, Tu> & operator+=(double rhs)
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

		Measurement<Tll, Tu> & operator-=(double rhs)
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

		Measurement<Tll, Tu> & operator*=(double rhs)
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

		Measurement<Tll, Tu> & operator/=(double rhs)
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




		bool operator==(const Measurement<Tll, Tu> & rhs) const
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

		bool operator!=(const Measurement<Tll, Tu> & rhs) const
		{
			return !(*this == rhs);
		}

	protected:
		Tll m_ll_value = 0;
		Tu m_unit;
		bool m_valid = false;
	};
	template<typename Tll, typename Tu>
	double operator/(const Measurement<Tll, Tu> & lhs, const Measurement<Tll, Tu> & rhs); /* For getting proportion of two values. */

	template<typename Tll, typename Tu>
	Measurement<Tll, Tu> operator*(Measurement<Tll, Tu> lhs, double rhs) { lhs *= rhs; return lhs; }

	template<typename Tll, typename Tu>
	Measurement<Tll, Tu> operator/(Measurement<Tll, Tu> lhs, double rhs) { lhs /= rhs; return lhs; }

	template<typename Tll, typename Tu>
	QDebug operator<<(QDebug debug, const Measurement<Tll, Tu> & meas)
	{
		debug << meas.to_string();
		return debug;
	}

	template<typename Tll, typename Tu>
	bool operator<(const Measurement<Tll, Tu> & lhs, const Measurement<Tll, Tu> & rhs)
	{
		if (lhs.unit() != rhs.unit()) {
			qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch:" << lhs.unit() << rhs.unit();
			return false;
		}
		return lhs.ll_value() < rhs.ll_value();
	}
	template<typename Tll, typename Tu>
	bool operator>(const Measurement<Tll, Tu> & lhs, const Measurement<Tll, Tu> & rhs)
	{
		return rhs < lhs;
	}
	template<typename Tll, typename Tu>
	bool operator<=(const Measurement<Tll, Tu> & lhs, const Measurement<Tll, Tu> & rhs)
	{
		return !(lhs > rhs);
	}
	template<typename Tll, typename Tu>
	bool operator>=(const Measurement<Tll, Tu> & lhs, const Measurement<Tll, Tu> & rhs)
	{
		return !(lhs < rhs);
	}

	/*
	  For getting proportion of two values.
	  Implemented in class declaration because of this: https://stackoverflow.com/a/10787730
	*/
	template<typename Tll, typename Tu>
	double operator/(const Measurement<Tll, Tu> & lhs, const Measurement<Tll, Tu> & rhs)
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



	class Altitude : public  Measurement<Altitude_ll, HeightUnit>
	{
	public:
		Altitude(const Measurement<Altitude_ll, HeightUnit> & base) : Measurement<Altitude_ll, HeightUnit>(base) {}
		Altitude() : Measurement<Altitude_ll, HeightUnit>() {}
		Altitude(Altitude_ll value, HeightUnit unit) : Measurement<Altitude_ll, HeightUnit>(value, unit) {}

		//using Measurement<Altitude_ll, HeightUnit>::Measurement;
		using Measurement<Altitude_ll, HeightUnit>::operator-;
		using Measurement<Altitude_ll, HeightUnit>::operator=;
	};

	class Gradient : public Measurement<Gradient_ll, GradientUnit>
	{
	public:
		Gradient(const Measurement<Gradient_ll, GradientUnit> & base) : Measurement<Gradient_ll, GradientUnit>(base) {}
		Gradient() : Measurement<Gradient_ll, GradientUnit>() {}
		Gradient(Gradient_ll value, GradientUnit unit) : Measurement<Gradient_ll, GradientUnit>(value, unit) {}

		//using Measurement<Gradient_ll, GradientUnit>::Measurement;
		using Measurement<Gradient_ll, GradientUnit>::operator-;
		using Measurement<Gradient_ll, GradientUnit>::operator=;
	};

	class Time : public Measurement<Time_ll, TimeUnit>
	{
	public:
		Time(const Measurement<Time_ll, TimeUnit> & base) : Measurement<Time_ll, TimeUnit>(base) {}
		Time() : Measurement<Time_ll, TimeUnit>() {}
		Time(Time_ll value, TimeUnit unit) : Measurement<Time_ll, TimeUnit>(value, unit) {}

		//using Measurement<Time_ll, TimeUnit>::Measurement;
		using Measurement<Time_ll, TimeUnit>::operator-;
		using Measurement<Time_ll, TimeUnit>::operator=;

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

	class Duration : public Measurement<Duration_ll, DurationUnit>
	{
	public:
		Duration(const Measurement<Duration_ll, DurationUnit> & base) : Measurement<Duration_ll, DurationUnit>(base) {}
		Duration() : Measurement<Duration_ll, DurationUnit>() {}
		Duration(Duration_ll value, DurationUnit unit) : Measurement<Duration_ll, DurationUnit>(value, unit) {}

		//using Measurement<Duration_ll, DurationUnit>::Measurement;
		using Measurement<Duration_ll, DurationUnit>::operator-;
		using Measurement<Duration_ll, DurationUnit>::operator=;

		static Duration get_abs_duration(const Time & later, const Time & earlier);

		/*
		  Set value from a string that contains value only
		  (does not contain any token representing unit).
		  Assign default internal value to to measurement.
		*/
		sg_ret set_duration_from_char_string(const char * str);
		sg_ret set_duration_from_string(const QString & str);
	};


	class Distance : public Measurement<Distance_ll, DistanceUnit>
	{
	public:
		Distance(const Measurement<Distance_ll, DistanceUnit> & base) : Measurement<Distance_ll, DistanceUnit>(base) {}
		Distance() : Measurement<Distance_ll, DistanceUnit>() {}
		Distance(Distance_ll value, DistanceUnit unit) : Measurement<Distance_ll, DistanceUnit>(value, unit) {}

		//using Measurement<Distance_ll, DistanceUnit>::Measurement;
		using Measurement<Distance_ll, DistanceUnit>::operator-;
		using Measurement<Distance_ll, DistanceUnit>::operator=;
		//Distance() {}
		//Distance(Distance_ll value, DistanceUnit unit) : Measurement(value, unit) {}
	};



	class Speed : public Measurement<Speed_ll, SpeedUnit>
	{
	public:
		Speed(const Measurement<Speed_ll, SpeedUnit> & base) : Measurement<Speed_ll, SpeedUnit>(base) {}
		Speed() : Measurement<Speed_ll, SpeedUnit>() {}
		Speed(Speed_ll value, SpeedUnit unit) : Measurement<Speed_ll, SpeedUnit>(value, unit) {}
#if 0
		Speed() : Measurement<Speed_ll, SpeedUnit>() {}
		Speed(Speed_ll value, SpeedUnit unit) : Measurement<Speed_ll, SpeedUnit>(value, unit) {}
#else
		//using Measurement<Speed_ll, SpeedUnit>::Measurement;
		using Measurement<Speed_ll, SpeedUnit>::operator-;
		using Measurement<Speed_ll, SpeedUnit>::operator=;
#endif

		sg_ret make_speed(const Distance & distance, const Duration & duration);
		sg_ret make_speed(const Altitude & altitude, const Duration & duration);
	};


	class Angle : public Measurement<Angle_ll, AngleUnit>
	{
	public:
#if 1
		Angle(const Measurement<Angle_ll, AngleUnit> & base) : Measurement<Angle_ll, AngleUnit>(base) {}
		Angle() : Measurement<Angle_ll, AngleUnit>() {}
		Angle(Angle_ll value, AngleUnit unit) : Measurement<Angle_ll, AngleUnit>(value, unit) {}
#else
		using Measurement<Angle_ll, AngleUnit>::Measurement;
		using Measurement<Angle_ll, AngleUnit>::operator-;
		using Measurement<Angle_ll, AngleUnit>::operator=;
#endif
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
