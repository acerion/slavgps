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
#include <climits>




#include <QString>
#include <QTimeZone>




#include "globals.h"




namespace SlavGPS {



#define SG_PRECISION_ALTITUDE           2
#define SG_PRECISION_ALTITUDE_WIDE      5
#define SG_ALTITUDE_PRECISION           2
#define SG_ALTITUDE_RANGE_MIN       -5000 /* [meters] */   /* See also VIK_VAL_MIN_ALT */
#define SG_ALTITUDE_RANGE_MAX       25000 /* [meters] */   /* See also VIK_VAL_MAX_ALT */


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
		DDD,
		DMM,
		DMS,
		Raw,
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
#define SG_MEASUREMENT_INTERNAL_UNIT_SPEED SpeedUnit::MetresPerSecond




	enum class HeightUnit {
		Metres,
		Feet,
	};
#define SG_MEASUREMENT_INTERNAL_UNIT_HEIGHT HeightUnit::Metres




	enum class TimeUnit {
		Seconds, /* Default, internal unit. */
	};
#define SG_MEASUREMENT_INTERNAL_UNIT_TIME TimeUnit::Seconds




	enum class GradientUnit {
		Degrees, /* Default, internal unit. TODO: verify. */
	};
#define SG_MEASUREMENT_INTERNAL_UNIT_GRADIENT GradientUnit::Degrees




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
			this->value = new_value;
			this->valid = Measurement::ll_value_is_valid(new_value);
			this->unit = new_unit;
		}

		static Tu get_user_unit(void);
		static Tu get_internal_unit(void);

		static bool ll_value_is_valid(Tll value);
		bool is_valid(void) const { return this->valid; }
		void invalidate(void)
		{
			this->valid = false;
			this->value = 0;
		}

		void set_value(Tll value);
		Tll get_value(void) const { return this->value; } /* TODO: what to do when this method is called on invalid value? Return zero? */


		static Measurement get_abs_diff(const Measurement & m1, const Measurement & m2)
		{
			/* TODO: check units. */
			Measurement result;
			result.value = std::abs(m1.value - m2.value);
			result.valid = true; /* TODO: validate. */
			result.unit = m1.unit;

			return result;
		}

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

		Measurement convert_to_unit(Tu new_unit) const;

		static Tll convert_to_unit(Tll value, Tu from, Tu to);

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

		/* Get current unit of measurement. */
		Tu get_unit(void) const { return this->unit; }

		/*
		  Set value from a string that contains value only
		  (does not contain any token representing unit).
		  Assign default internal value to to measurement.
		*/
		sg_ret set_value_from_char_string(const char * str)
		{
			if (NULL == str) {
				qDebug() << "EE    " << __FUNCTION__ << "Attempting to set invalid value of altitude from NULL string";
				this->valid = false;
				return sg_ret::err_arg;
			} else {
				return this->set_value_from_string(QString(str));
			}
		}
		sg_ret set_value_from_string(const QString & str)
		{
			this->value = c_to_double(str);
			this->valid = !std::isnan(this->value);
			this->unit = Measurement<Tu, Tll>::get_internal_unit();

			return this->valid ? sg_ret::ok : sg_ret::err;
		}



		/* Will return INT_MIN if value is invalid. */
		int floor(void) const;

		/* Specific to Time. Keeping it in Measurement class is problematic. */
		QString to_timestamp_string(Qt::TimeSpec time_spec = Qt::LocalTime) const;
		QString strftime_local(const char * format) const;
		QString strftime_utc(const char * format) const;
		QString to_duration_string(void) const;
		QString get_time_string(Qt::DateFormat format) const;
		QString get_time_string(Qt::DateFormat format, const Coord & coord) const;
		QString get_time_string(Qt::DateFormat format, const Coord & coord, const QTimeZone * tz) const;


		/* Generate string with value and unit. Value
		   (magnitude) of distance does not influence units
		   used to present the value. E.g. "0.01" km/h will
		   always be presented as "0.01 km/h", never as "10
		   m/h". */
		QString to_string(void) const;

		static QString to_string(Tll value);

		/* Is this measurement so small that it can be treated as zero? */
		bool is_zero(void) const
		{
			const double epsilon = 0.0000001;
			if (!this->valid) {
				return true;
			}
			return std::abs(this->value) < epsilon;
		}




		Measurement & operator=(const Measurement & rhs)
		{
			if (this == &rhs) {
				return *this;
			}

			this->value = rhs.value;
			this->valid = rhs.valid;
			this->unit = rhs.unit;

			return *this;
		}

		Measurement operator+(double rhs) const { Measurement result = *this; result += rhs; return result; }
		Measurement operator-(double rhs) const { Measurement result = *this; result -= rhs; return result; }

		Measurement operator+(const Measurement & rhs) const
		{
			Measurement result;
			if (!this->valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid lhs";
				return result;
			}
			if (!rhs.valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid rhs";
				return result;
			}

			result = *this;
			result += rhs;
			return result;
		}
		Measurement operator-(const Measurement & rhs) const
		{
			Measurement result;
			if (!this->valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid lhs";
				return result;
			}
			if (!rhs.valid) {
				qDebug() << "WW     " << __FUNCTION__ << "Operating on invalid rhs";
				return result;
			}

			result = *this;
			result -= rhs;
			return result;
		}


		Measurement & operator+=(const Measurement & rhs)
		{
			if (!this->valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}
			if (!rhs.valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}
			if (this->unit != rhs.unit) {
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch"; /* TODO: use this in more operators. */
				return *this;
			}

			this->value += rhs.value;
			this->valid = !std::isnan(this->value);
			return *this;
		}

		Measurement & operator-=(const Measurement & rhs)
		{
			if (!this->valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}
			if (!rhs.valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}
			if (this->unit != rhs.unit) {
				qDebug() << "EE    " << __FUNCTION__ << "Unit mismatch"; /* TODO: use this in more operators. */
				return *this;
			}

			this->value -= rhs.value;
			this->valid = !std::isnan(this->value);
			return *this;
		}

		Measurement & operator+=(double rhs)
		{
			if (!this->valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}

			if (std::isnan(rhs)) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}

			this->value += rhs;
			this->valid = !std::isnan(this->value);
			return *this;
		}

		Measurement & operator-=(double rhs)
		{
			if (!this->valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}

			if (std::isnan(rhs)) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}

			this->value -= rhs;
			this->valid = !std::isnan(this->value);
			return *this;
		}

		//friend Measurement operator+(Measurement lhs, const Measurement & rhs) { lhs += rhs; return lhs; }
		//friend Measurement operator-(Measurement lhs, const Measurement & rhs) { lhs -= rhs; return lhs; }

		Measurement & operator*=(double rhs)
		{
			if (!this->valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}

			if (std::isnan(rhs)) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}

			this->value *= rhs;
			this->valid = !std::isnan(this->value);
			return *this;
		}

		Measurement & operator/=(double rhs)
		{
			if (!this->valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'this' operand";
				return *this;
			}

			if (std::isnan(rhs)) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return *this;
			}

			this->value /= rhs;
			this->valid = !std::isnan(this->value);
			return *this;
		}

		friend Measurement operator*(Measurement lhs, double rhs) { lhs *= rhs; return lhs; }
		friend Measurement operator/(Measurement lhs, double rhs) { lhs /= rhs; return lhs; }

		friend bool operator<(const Measurement<Tu, Tll> & lhs, const Measurement<Tu, Tll> & rhs)
		{
			return lhs.value < rhs.value;
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
			if (!lhs.valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'lhs' operand";
				return NAN;
			}
			if (!rhs.valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Invalid 'rhs' operand";
				return NAN;
			}
			if (rhs.is_zero()) { /* TODO: introduce more checks of zero in division operators. */
				qDebug() << "WW    " << __FUNCTION__ << "rhs is zero";
				return NAN;
			}

			return (1.0 * lhs.value) / rhs.value;
		}

		bool operator==(const Measurement & rhs) const
		{
			if (!this->valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Comparing invalid value";
				return false;
			}

			if (!rhs.valid) {
				qDebug() << "WW    " << __FUNCTION__ << "Comparing invalid value";
				return false;
			}

			return this->value == rhs.value;
		}

		bool operator!=(const Measurement & rhs) const
		{
			return !(*this == rhs);
		}


		Tll value = 0;
		Tu unit = (Tu) 0;
		bool valid = false;
	};
	template<typename Tu, typename Tll>
	double operator/(const Measurement<Tu, Tll> & lhs, const Measurement<Tu, Tll> & rhs); /* For getting proportion of two values. */

	template<typename Tu, typename Tll>
	bool Measurement<Tu, Tll>::ll_value_is_valid(Tll new_value)
	{
		return !std::isnan(new_value);
	}

	template<typename Tu, typename Tll>
	void Measurement<Tu, Tll>::set_value(Tll new_value)
	{
		this->value = new_value;
		this->valid = !std::isnan(new_value);
		/* Don't change unit. */
	}

	template<typename Tu, typename Tll>
	QString Measurement<Tu, Tll>::to_timestamp_string(Qt::TimeSpec time_spec) const
	{
		QString result;
		return result;
	}

	template<typename Tu, typename Tll>
	QString Measurement<Tu, Tll>::strftime_local(const char * format) const
	{
		QString result;
		return result;
	}

	template<typename Tu, typename Tll>
	QString Measurement<Tu, Tll>::strftime_utc(const char * format) const
	{
		QString result;
		return result;
	}

	template<typename Tu, typename Tll>
	QString Measurement<Tu, Tll>::to_duration_string(void) const
	{
		QString result;
		return result;
	}

	template<typename Tu, typename Tll>
	QString Measurement<Tu, Tll>::get_time_string(Qt::DateFormat format) const
	{
		QString result;
		return result;
	}

	template<typename Tu, typename Tll>
	QString Measurement<Tu, Tll>::get_time_string(Qt::DateFormat format, const Coord & coord) const
	{
		QString result;
		return result;
	}

	template<typename Tu, typename Tll>
	QString Measurement<Tu, Tll>::get_time_string(Qt::DateFormat format, const Coord & coord, const QTimeZone * tz) const
	{
		QString result;
		return result;
	}




	typedef Measurement<HeightUnit, Altitude_ll> Altitude;
	typedef Measurement<SpeedUnit, Speed_ll> Speed;
	typedef Measurement<GradientUnit, Gradient_ll> Gradient;
	typedef Measurement<TimeUnit, Time_ll> Time;





	class Angle {
	public:
		Angle(double new_value = NAN) { this->set_value(new_value); }

		QString to_string(int precision = SG_PRECISION_COURSE) const;
		QString value_to_c_string(int precision = SG_PRECISION_COURSE) const;

		double get_value(void) const;
		bool set_value(double value);

		bool is_valid(void) const;
		void invalidate(void);

		/* Ensure that value is in range of 0-2pi (if the
		   value is valid). */
		void normalize(void);

		static Angle get_vector_sum(const Angle & angle1, const Angle & angle2);

	private:
		double value = NAN;
		bool valid = false;
	};




	class Measurements {
	public:
		static QString get_file_size_string(size_t file_size);
		static bool unit_tests(void);
	};




	class Distance {
	public:
		Distance(double new_value = 0.0, SupplementaryDistanceUnit new_supplementary_distance_unit = SupplementaryDistanceUnit::Meters);
		Distance(double value, DistanceUnit distance_unit);

		static DistanceUnit get_user_unit(void);
		static DistanceUnit get_internal_unit(void) { return DistanceUnit::Kilometres; } /* FIXME: this should be metres */

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

		friend Distance operator+(const Distance & lhs, const Distance & rhs);
		friend Distance operator-(const Distance & lhs, const Distance & rhs);

		Distance & operator*=(double rhs);
		Distance & operator/=(double rhs);
		friend Distance operator*(Distance lhs, double rhs) { lhs *= rhs; return lhs; }
		friend Distance operator/(Distance lhs, double rhs) { lhs /= rhs; return lhs; }

		/* For calculating proportion of values. */
		friend double operator/(const Distance & rhs, const Distance & lhs);

		bool operator==(const Distance & rhs) const;
		bool operator!=(const Distance & rhs) const;

		friend bool operator<(const Distance & lhs, const Distance & rhs);
		friend bool operator>(const Distance & lhs, const Distance & rhs);
		friend bool operator<=(const Distance & lhs, const Distance & rhs);
		friend bool operator>=(const Distance & lhs, const Distance & rhs);

		/* Return "kilometers" or "miles" string.
		   This is a full string, not "km" or "mi". */
		static QString get_unit_full_string(DistanceUnit distance_unit);

		static double convert_meters_to(double distance, DistanceUnit distance_unit);

		bool is_valid(void) const;

		/* Is this measurement so small that it can be treated as zero? */
		bool is_zero(void) const;

		friend QDebug operator<<(QDebug debug, const Distance & distance);

		double value = 0.0;
	private:
		bool valid = false;

		DistanceUnit distance_unit;
		SupplementaryDistanceUnit supplementary_distance_unit;
		bool use_supplementary_distance_unit = false;
	};
	QDebug operator<<(QDebug debug, const Distance & distance);
	bool operator<(const Distance & lhs, const Distance & rhs);
	bool operator>(const Distance & lhs, const Distance & rhs);
	bool operator<=(const Distance & lhs, const Distance & rhs);
	bool operator>=(const Distance & lhs, const Distance & rhs);
	Distance operator+(const Distance & lhs, const Distance & rhs);
	Distance operator-(const Distance & lhs, const Distance & rhs);
	double operator/(const Distance & rhs, const Distance & lhs);



#if 0
	class Altitude {
	public:
		Altitude() {};
		Altitude(double value, HeightUnit height_unit = SG_MEASUREMENT_INTERNAL_UNIT_HEIGHT);

		static HeightUnit get_user_unit(void);
		static HeightUnit get_internal_unit(void) { return SG_MEASUREMENT_INTERNAL_UNIT_HEIGHT; }

		void set_value(double value); /* Set value, don't change unit. */
		double get_value(void) const;

		bool is_valid(void) const;
		void set_valid(bool valid);

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

		/* Will return INT_MIN if altitude is invalid. */
		int floor(void) const;

		Altitude & operator=(const Altitude & rhs)

		Altitude & operator+=(double rhs);
		Altitude & operator-=(double rhs);
		Altitude & operator+=(const Altitude & rhs);
		Altitude & operator-=(const Altitude & rhs);
		Altitude operator+(double rhs) const { Altitude result = *this; result += rhs; return result; }
		Altitude operator-(double rhs) const { Altitude result = *this; result -= rhs; return result; }
		Altitude operator+(const Altitude & rhs) const { Altitude result = *this; result += rhs; return result; }
		Altitude operator-(const Altitude & rhs) const { Altitude result = *this; result -= rhs; return result; }

		Altitude & operator*=(double rhs);
		Altitude & operator/=(double rhs);
		Altitude operator*(double rhs) const { Altitude result = *this; result *= rhs; return result; }
		Altitude operator/(double rhs) const { Altitude result = *this; result /= rhs; return result; }

		bool operator==(const Altitude & rhs) const;
		bool operator!=(const Altitude & rhs) const;

		/* For calculating proportion of values. */
		friend double operator/(const Altitude & rhs, const Altitude & lhs);

		friend bool operator<(const Altitude & lhs, const Altitude & rhs);
		friend bool operator>(const Altitude & lhs, const Altitude & rhs);
		friend bool operator<=(const Altitude & lhs, const Altitude & rhs);
		friend bool operator>=(const Altitude & lhs, const Altitude & rhs);

		HeightUnit get_unit(void) const { return this->unit; };

		/* Return "meters" or "feet" string.
		   This is a full string, not "m" or "ft". */
		static QString get_unit_full_string(HeightUnit height_unit);

		sg_ret set_from_string(const char * str);
		sg_ret set_from_string(const QString & str);

		/* Is this measurement so small that it can be treated as zero? */
		bool is_zero(void) const;

		Altitude_ll value = NAN;

	private:

		bool valid = false;
		HeightUnit unit;
	};
	bool operator<(const Altitude & lhs, const Altitude & rhs);
	bool operator>(const Altitude & lhs, const Altitude & rhs);
	bool operator<=(const Altitude & lhs, const Altitude & rhs);
	bool operator>=(const Altitude & lhs, const Altitude & rhs);
	double operator/(const Altitude & rhs, const Altitude & lhs);
	QDebug operator<<(QDebug debug, const Altitude & altitude);




	class Speed {
	public:
		Speed() {};
		Speed(double value, SpeedUnit speed_unit);

		static SpeedUnit get_user_unit(void);
		static SpeedUnit get_internal_unit(void) { return SpeedUnit::MetresPerSecond; }

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
		Speed & operator-=(const Speed & rhs);
	        friend Speed operator+(Speed lhs, const Speed & rhs) { lhs += rhs; return lhs; }
		friend Speed operator-(Speed lhs, const Speed & rhs) { lhs -= rhs; return lhs; }

		Speed & operator/=(double rhs);
		Speed & operator*=(double rhs);
	        friend Speed operator*(Speed lhs, double rhs) { lhs *= rhs; return lhs; }
		friend Speed operator/(Speed lhs, double rhs) { lhs /= rhs; return lhs; }

		friend bool operator<(const Speed & lhs, const Speed & rhs);
		friend bool operator>(const Speed & lhs, const Speed & rhs);
		friend bool operator<=(const Speed & lhs, const Speed & rhs);
		friend bool operator>=(const Speed & lhs, const Speed & rhs);

		/* For calculating proportion of values. */
		friend double operator/(const Speed & rhs, const Speed & lhs);

		bool operator==(const Speed & rhs) const;
		bool operator!=(const Speed & rhs) const;

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

		/* Is this measurement so small that it can be treated as zero? */
		bool is_zero(void) const;

		Speed_ll value = NAN;

	private:
		static bool operator_args_valid(const Speed & lhs, const Speed & rhs);

		bool valid = false;
		SpeedUnit unit;
	};
	double operator/(const Speed & rhs, const Speed & lhs);
	bool operator<(const Speed & lhs, const Speed & rhs);
	bool operator>(const Speed & lhs, const Speed & rhs);
	bool operator<=(const Speed & lhs, const Speed & rhs);
	bool operator>=(const Speed & lhs, const Speed & rhs);




	class Time {
	public:
		Time();
		Time(time_t value, TimeUnit time_unit = TimeUnit::Seconds);

		static TimeUnit get_user_unit(void);
		static TimeUnit get_internal_unit(void);

		time_t get_value(void) const;

		Time convert_to_unit(TimeUnit time_unit) const { return *this; }

		QString to_duration_string(void) const;
		QString to_string(void) const;
		QString to_timestamp_string(Qt::TimeSpec time_spec = Qt::LocalTime) const;
		QString get_time_string(Qt::DateFormat format) const;
		QString get_time_string(Qt::DateFormat format, const Coord & coord) const;
		QString get_time_string(Qt::DateFormat format, const Coord & coord, const QTimeZone * tz) const;

		bool is_valid(void) const;
		void invalidate(void);
		void set_valid(bool value);
		sg_ret set_from_unix_timestamp(const char * str);
		sg_ret set_from_unix_timestamp(const QString & str);


		static Time get_abs_diff(const Time & t1, const Time & t2);

		QString strftime_local(const char * format) const;
		QString strftime_utc(const char * format) const;

		/* Is this measurement so small that it can be treated as zero?
		   For Time class using this method has sense only if value of type Time represents duration.
		   There is no much sense to see whether a date is zero. */
		bool is_zero(void) const;

		time_t value = 0;

		friend Time operator+(const Time & lhs, const Time & rhs);
		friend Time operator-(const Time & lhs, const Time & rhs);
		friend bool operator<(const Time & lhs, const Time & rhs);
		friend bool operator>(const Time & lhs, const Time & rhs);
		friend bool operator<=(const Time & lhs, const Time & rhs);
		friend bool operator>=(const Time & lhs, const Time & rhs);
		friend QDebug operator<<(QDebug debug, const Time & timestamp);

		bool operator==(const Time & timestamp) const;
		bool operator!=(const Time & timestamp) const;

		Time & operator*=(double rhs);
		Time & operator/=(double rhs);
		Time operator*(double rhs) const { Time result = *this; result *= rhs; return result; }
		Time operator/(double rhs) const { Time result = *this; result /= rhs; return result; }

		/* For calculating proportion of values. */
		friend double operator/(const Time & rhs, const Time & lhs);

		Time & operator+=(const Time & rhs);

	private:
		bool valid = false;
	};
	Time operator+(const Time & lhs, const Time & rhs);
	Time operator-(const Time & lhs, const Time & rhs);
	bool operator<(const Time & lhs, const Time & rhs);
	bool operator>(const Time & lhs, const Time & rhs);
	bool operator<=(const Time & lhs, const Time & rhs);
	bool operator>=(const Time & lhs, const Time & rhs);
	QDebug operator<<(QDebug debug, const Time & timestamp);
	double operator/(const Time & rhs, const Time & lhs);




	class Gradient {
	public:
		Gradient() {};
		Gradient(double value, GradientUnit gradient_unit = SG_MEASUREMENT_INTERNAL_UNIT_GRADIENT);

		static GradientUnit get_user_unit(void);
		static GradientUnit get_internal_unit(void) { return SG_MEASUREMENT_INTERNAL_UNIT_GRADIENT; }

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

		/* Get string representing speed unit in abbreviated
		   form. */
		static QString get_unit_string(void);

		/* Generate string with value and unit. Value
		   (magnitude) of distance does not influence units
		   used to present the value. E.g. "0.01" km/h will
		   always be presented as "0.01 km/h", never as "10
		   m/h".

		   Unit is taken from global preferences. There is no
		   conversion of value from one unit to another. */
		static QString to_string(double value, int precision = SG_PRECISION_GRADIENT);

		/* Is this measurement so small that it can be treated as zero? */
		bool is_zero(void) const;


		Gradient & operator+=(const Gradient & rhs);
		Gradient & operator-=(const Gradient & rhs);
		Gradient & operator+=(double rhs);
		Gradient & operator-=(double rhs);
		friend Gradient operator+(Gradient lhs, const Gradient & rhs) { lhs += rhs; return lhs; }
		friend Gradient operator-(Gradient lhs, const Gradient & rhs) { lhs -= rhs; return lhs; }

		Gradient & operator*=(double rhs);
		Gradient & operator/=(double rhs);
		friend Gradient operator*(Gradient lhs, double rhs) { lhs *= rhs; return lhs; }
		friend Gradient operator/(Gradient lhs, double rhs) { lhs /= rhs; return lhs; }

		friend double operator/(const Gradient & rhs, const Gradient & lhs); /* For getting proportion of two values. */

		bool operator==(const Gradient & rhs) const;
		bool operator!=(const Gradient & rhs) const;

		Gradient_ll value = NAN;

	private:
		bool valid = false;
	};
	double operator/(const Gradient & rhs, const Gradient & lhs);
#endif



}




#endif /* #ifndef _SG_MEASUREMENTS_H_ */
