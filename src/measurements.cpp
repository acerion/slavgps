/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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




#include <cmath>
#include <ctime>
#include <cassert>




#include <QDebug>




#include "measurements.h"
#include "preferences.h"
#include "vikutils.h"




using namespace SlavGPS;




#define SG_MODULE "Measurements"




static QString time_string_adjusted(time_t time, int offset_s);
static QString time_string_tz(time_t time, Qt::DateFormat format, const QTimeZone & tz);




namespace SlavGPS {




/**
   @timestamp - The time of which the string is wanted
   @format - The format of the time string
   @coord - Position of object for the time output (only applicable for format == SGTimeReference::World)

   @return a string of the time according to the time display property
*/
QString Time::get_time_string(Qt::DateFormat format, const Coord & coord) const
{
	QString result;

	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	const QTimeZone * tz_from_location = NULL;

	const SGTimeReference ref = Preferences::get_time_ref_frame();
	switch (ref) {
	case SGTimeReference::UTC:
		result = QDateTime::fromTime_t(this->m_ll_value, QTimeZone::utc()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "UTC: timestamp =" << this->m_ll_value << "-> time string" << result;
		break;
	case SGTimeReference::World:
		/* No timezone specified so work it out. */
		tz_from_location = TZLookup::get_tz_at_location(coord);
		if (tz_from_location) {
			result = time_string_tz(this->m_ll_value, format, *tz_from_location);
			qDebug() << SG_PREFIX_D << "World (from location): timestamp =" << this->m_ll_value << "-> time string" << result;
		} else {
			/* No results (e.g. could be in the middle of a sea).
			   Fallback to simplistic method that doesn't take into account Timezones of countries. */
			const LatLon lat_lon = coord.get_lat_lon();
			result = time_string_adjusted(this->m_ll_value, round (lat_lon.lon / 15.0) * 3600);
			qDebug() << SG_PREFIX_D << "World (fallback): timestamp =" << this->m_ll_value << "-> time string" << result;
		}
		break;
	case SGTimeReference::Locale:
		result = QDateTime::fromTime_t(this->m_ll_value, QTimeZone::systemTimeZone()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "Locale: timestamp =" << this->m_ll_value << "-> time string" << result;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected SGTimeReference value" << (int) ref;
		break;
	}

	return result;
}




/**
   @timestamp - The time of which the string is wanted
   @format - The format of the time string
   @coord - Position of object for the time output (only applicable for format == SGTimeReference::World)
   @tz - TimeZone string - maybe NULL (only applicable for SGTimeReference::World). Useful to pass in the cached value from TZLookup::get_tz_at_location() to save looking it up again for the same position.

   @return a string of the time according to the time display property
*/
QString Time::get_time_string(Qt::DateFormat format, const Coord & coord, const QTimeZone * tz) const
{
	QString result;

	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	const SGTimeReference ref = Preferences::get_time_ref_frame();
	switch (ref) {
	case SGTimeReference::UTC:
		result = QDateTime::fromTime_t(this->m_ll_value, QTimeZone::utc()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "UTC: timestamp =" << this->m_ll_value << "-> time string" << result;
		break;
	case SGTimeReference::World:
		if (tz) {
			/* Use specified timezone. */
			result = time_string_tz(this->m_ll_value, format, *tz);
			qDebug() << SG_PREFIX_D << "World (from timezone): timestamp =" << this->m_ll_value << "-> time string" << result;
		} else {
			/* No timezone specified so work it out. */
			QTimeZone const * tz_from_location = TZLookup::get_tz_at_location(coord);
			if (tz_from_location) {
				result = time_string_tz(this->m_ll_value, format, *tz_from_location);
				qDebug() << SG_PREFIX_D << "World (from location): timestamp =" << this->m_ll_value << "-> time string" << result;
			} else {
				/* No results (e.g. could be in the middle of a sea).
				   Fallback to simplistic method that doesn't take into account Timezones of countries. */
				const LatLon lat_lon = coord.get_lat_lon();
				result = time_string_adjusted(this->m_ll_value, round (lat_lon.lon / 15.0) * 3600);
				qDebug() << SG_PREFIX_D << "World (fallback): timestamp =" << this->m_ll_value << "-> time string" << result;
			}
		}
		break;
	case SGTimeReference::Locale:
		result = QDateTime::fromTime_t(this->m_ll_value, QTimeZone::systemTimeZone()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "Locale: timestamp =" << this->m_ll_value << "-> time string" << result;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected SGTimeReference value" << (int) ref;
		break;
	}

	return result;
}




QString Time::to_timestamp_string(Qt::TimeSpec time_spec) const
{
	QString result;
	if (this->is_valid()) {
		 /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		result = QDateTime::fromTime_t(this->ll_value(), time_spec).toString(Qt::ISODate);
	} else {
		result = QObject::tr("No Data");
	}
	return result;
}




template<>
QString Measurement<TimeType>::to_string(void) const
{
	QString result;
	if (this->is_valid()) {
		/* TODO_LATER: this can't be hardcoded 'local time'. */
		Qt::TimeSpec time_spec = Qt::LocalTime;
		/* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		result = QDateTime::fromTime_t(this->ll_value(), time_spec).toString(Qt::ISODate);
	} else {
		result = QObject::tr("No Data");
	}
	return result;
}




template<>
bool Measurement<TimeType>::is_zero(void) const
{
	if (!this->m_valid) {
		return true;
	}
	return this->m_ll_value == 0;
}




TimeType::Unit TimeType::Unit::user_unit(void)
{
	return TimeType::Unit::E::Seconds; /* TODO_LATER: get this from preferences. */
}




sg_ret Time::set_timestamp_from_string(const QString & str)
{
	this->m_ll_value = (TimeType::LL) str.toULong(&this->m_valid);

	if (!this->m_valid) {
		qDebug() << SG_PREFIX_W << "Setting invalid value of timestamp from string" << str;
	}

	return this->m_valid ? sg_ret::ok : sg_ret::err;
}



sg_ret Time::set_timestamp_from_char_string(const char * str)
{
	if (NULL == str) {
		qDebug() << SG_PREFIX_E << "Attempting to set invalid value of timestamp from NULL string";
		this->m_valid = false;
		return sg_ret::err_arg;
	} else {
		return this->set_timestamp_from_string(QString(str));
	}
}




template<>
bool Measurement<TimeType>::ll_value_is_valid(__attribute__((unused)) TimeType::LL value)
{
	return true; /* TODO_LATER: improve for Time::LL data type. */
}




template<>
const QString Measurement<TimeType>::value_to_string_for_file(int precision) const
{
	QString result;
	if (this->m_valid) {
		result.setNum(this->m_ll_value);
	}
	return result;
}




template<>
QString Measurement<TimeType>::unit_string(const TimeType::Unit & unit)
{
	QString result;

	switch (unit.u) {
	case TimeType::Unit::E::Seconds:
		result = QString("s");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Uhandled unit" << unit;
		break;
	}

	return result;
}




#if 0
bool SlavGPS::operator<(const Time & lhs, const Time & rhs)
{
	/* TODO_LATER: shouldn't this be difftime()? */
	return lhs.m_ll_value < rhs.m_ll_value;
}
#endif




QString Time::strftime_utc(const char * format) const
{
	char timestamp_string[32] = { 0 };
	struct tm tm;
	std::strftime(timestamp_string, sizeof (timestamp_string), format, gmtime_r(&this->m_ll_value, &tm));

	return QString(timestamp_string);
}




QString Time::strftime_local(const char * format) const
{
	char timestamp_string[32] = { 0 };
	struct tm tm;
	std::strftime(timestamp_string, sizeof (timestamp_string), format, localtime_r(&this->m_ll_value, &tm));

	return QString(timestamp_string);
}




QString Time::get_time_string(Qt::DateFormat format) const
{
	QDateTime date_time;
	date_time.setTime_t(this->m_ll_value);
	const QString result = date_time.toString(format);

	return result;
}




template<>
TimeType::LL Measurement<TimeType>::convert_to_unit(__attribute__((unused)) TimeType::LL value, __attribute__((unused)) const TimeType::Unit & from, __attribute__((unused)) const TimeType::Unit & to)
{
	return value; /* There is only one Time unit, so the conversion is simple. */
}




template<>
Time Measurement<TimeType>::convert_to_unit(__attribute__((unused)) const TimeType::Unit & target_unit) const
{
	return *this; /* There is only one Time unit, so the conversion is simple. */
}




QDebug operator<<(QDebug debug, const TimeType::Unit & unit)
{
	debug << QString("time unit %1").arg((int) unit.u);
	return debug;
}




QDebug operator<<(QDebug debug, const Time & time)
{
	debug << QString("Time %1 %2").arg(time.m_ll_value).arg((int) time.m_unit.u);
	return debug;
}








/**
   @reviewed-on 2019-11-27
*/
template<>
QString Measurement<DurationType>::to_string(void) const
{
	QString result;

	switch (this->m_unit.u) {
	case DurationType::Unit::E::Seconds:
		result = QObject::tr("%1 s").arg(this->m_ll_value);
		break;
	case DurationType::Unit::E::Minutes:
		result = QObject::tr("%1 m").arg(this->m_ll_value);
		break;
	case DurationType::Unit::E::Hours:
		result = QObject::tr("%1 h").arg(this->m_ll_value);
		break;
	case DurationType::Unit::E::Days:
		result = QObject::tr("%1 d").arg(this->m_ll_value);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled unit" << this->m_unit;
		break;
	}
	return result;
}




template<>
const QString Measurement<DurationType>::value_to_string_for_file(int precision) const
{
	QString result;
	if (this->m_valid) {
		result.setNum(this->m_ll_value);
	}
	return result;
}




sg_ret Duration::set_duration_from_string(const QString & str)
{
	this->m_ll_value = str.toLong(&this->m_valid); /* DurationType::LL is signed. */

	if (!this->m_valid) {
		qDebug() << SG_PREFIX_W << "Setting invalid value of duration from string" << str;
	}

	return this->m_valid ? sg_ret::ok : sg_ret::err;
}




sg_ret Duration::set_duration_from_char_string(const char * str)
{
	if (NULL == str) {
		qDebug() << SG_PREFIX_E << "Attempting to set invalid value of duation from NULL string";
		this->m_valid = false;
		return sg_ret::err_arg;
	} else {
		return this->set_duration_from_string(QString(str));
	}
}




template<>
bool Measurement<DurationType>::is_zero(void) const
{
	if (!this->m_valid) {
		return true;
	}
	return this->m_ll_value == 0;
}




template<>
bool Measurement<DurationType>::ll_value_is_valid(__attribute__((unused)) TimeType::LL value)
{
	return true; /* TODO_LATER: improve for DurationType::LL data type. */
}




template<>
DurationType::LL Measurement<DurationType>::convert_to_unit(DurationType::LL value, const DurationType::Unit & from, const DurationType::Unit & to)
{
	DurationType::LL result = 0; /* TODO_LATER: this should be some form of NAN. */

	switch (from.u) {
	case DurationType::Unit::E::Seconds:
		qDebug() << SG_PREFIX_E << "Unhandled case";
		break;

	case DurationType::Unit::E::Minutes:
		switch (to.u) {
		case DurationType::Unit::E::Seconds:
			result = 60 * value;
			break;
		case DurationType::Unit::E::Minutes:
			result = value;
			break;
		case DurationType::Unit::E::Hours:
			qDebug() << SG_PREFIX_E << "Unhandled case";
			break;
		case DurationType::Unit::E::Days:
			qDebug() << SG_PREFIX_E << "Unhandled case";
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target unit" << to;
			break;
		}
		break;

	case DurationType::Unit::E::Hours:
		switch (to.u) {
		case DurationType::Unit::E::Seconds:
			result = 60 * 60 * value;
			break;
		case DurationType::Unit::E::Minutes:
			result = 60 * value;
			break;
		case DurationType::Unit::E::Hours:
			result = value;
			break;
		case DurationType::Unit::E::Days:
			qDebug() << SG_PREFIX_E << "Unhandled case";
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target unit" << to;
			break;
		}
		break;

	case DurationType::Unit::E::Days:
		switch (to.u) {
		case DurationType::Unit::E::Seconds:
			result = 24 * 60 * 60 * value;
			break;
		case DurationType::Unit::E::Minutes:
			result = 60 * 60 * value;
			break;
		case DurationType::Unit::E::Hours:
			result = 60 * value;
			break;
		case DurationType::Unit::E::Days:
			result = value;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target unit" << to;
			break;
		}
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unhandled source unit" << from;
		break;
	}

	return result;
}




template<>
Duration Measurement<DurationType>::convert_to_unit(const DurationType::Unit & target_unit) const
{
	Duration result;
	result.m_ll_value = Duration::convert_to_unit(this->m_ll_value, this->unit(), target_unit);
	result.m_unit = target_unit;
	result.m_valid = Duration::ll_value_is_valid(result.m_ll_value);
	return result;
}




Duration Duration::get_abs_duration(const Time & later, const Time & earlier)
{
	Duration result;
	if (later.unit() != earlier.unit()) {
		qDebug() << SG_PREFIX_E << "Arguments have different units:" << later.unit() << earlier.unit();
	} else {
		DurationType::LL diff = 0;
		if (later.ll_value() >= earlier.ll_value()) {
			diff = later.ll_value() - earlier.ll_value();
		} else {
			diff = earlier.ll_value() - later.ll_value();
		}
		result.set_ll_value(diff);

		switch (later.unit().u) {
		case TimeType::Unit::E::Seconds:
			result.set_unit(DurationType::Unit::E::Seconds);
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled unit" << later.unit();
			result.invalidate();
			break;
		}
	}
	return result;
}




template<>
QString Measurement<DurationType>::unit_string(const DurationType::Unit & unit)
{
	QString result;

	switch (unit.u) {
	case DurationType::Unit::E::Seconds:
		result = QString("s");
		break;
	case DurationType::Unit::E::Minutes:
		result = QString("m");
		break;
	case DurationType::Unit::E::Hours:
		result = QString("h");
		break;
	case DurationType::Unit::E::Days:
		result = QString("d");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Uhandled unit" << unit;
		break;
	}

	return result;
}




DurationType::Unit DurationType::Unit::user_unit(void)
{
	return DurationType::Unit::E::Seconds; /* TODO_LATER: get this from Preferences. */
}




QDebug operator<<(QDebug debug, const DurationType::Unit & unit)
{
	debug << QString("duration unit %1").arg((int) unit.u);
	return debug;
}




QDebug operator<<(QDebug debug, const Duration & duration)
{
	debug << QString("Duration %1 %2").arg(duration.m_ll_value).arg((int) duration.m_unit.u);
	return debug;
}








template<>
bool Measurement<GradientType>::ll_value_is_valid(GradientType::LL value)
{
	return !std::isnan(value);
}




template<>
QString Measurement<GradientType>::ll_value_to_string(GradientType::LL value, const GradientType::Unit & unit)
{
	QString result;
	if (!Gradient::ll_value_is_valid(value)) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	const int precision = SG_PRECISION_GRADIENT;
	switch (unit.u) {
	case GradientType::Unit::E::Percents:
		result = QObject::tr("%1%").arg(value, 0, 'f', precision);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Unhandled unit" << unit;
		break;
	}

	return result;
}




template<>
QString Measurement<GradientType>::to_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
	} else {
		result = QObject::tr("%1%").arg(this->m_ll_value, 0, 'f', SG_PRECISION_GRADIENT);
	}
	return result;
}




template<>
QString Measurement<GradientType>::unit_string(const GradientType::Unit & unit)
{
	QString result;

	switch (unit.u) {
	case GradientType::Unit::E::Percents:
		result = QString("%");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Uhandled gradient unit" << unit;
		break;
	}

	return result;
}




template<>
const QString Measurement<GradientType>::value_to_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
	} else {
		result = QObject::tr("%1").arg(this->m_ll_value, 0, 'f', SG_PRECISION_GRADIENT);
	}

	return result;
}




template<>
const QString Measurement<GradientType>::value_to_string_for_file(int precision) const
{
	QString result;
	if (this->m_valid) {
		result = SGUtils::double_to_c(this->m_ll_value, precision);
	}
	return result;
}




template<>
bool Measurement<GradientType>::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->m_valid) {
		return true;
	}
	return std::abs(this->m_ll_value) < epsilon;
}




template<>
GradientType::LL Measurement<GradientType>::convert_to_unit(__attribute__((unused)) GradientType::LL value, __attribute__((unused)) const GradientType::Unit & from, __attribute__((unused)) const GradientType::Unit & to)
{
	return value; /* There is only one Gradient unit, so the conversion is simple. */
}




template<>
Gradient Measurement<GradientType>::convert_to_unit(__attribute__((unused)) const GradientType::Unit & target_unit) const
{
	return *this; /* There is only one Gradient unit, so the conversion is simple. */
}




GradientType::Unit GradientType::Unit::user_unit(void)
{
	return GradientType::Unit::E::Percents;
}




QDebug operator<<(QDebug debug, const GradientType::Unit & unit)
{
	debug << QString("gradient unit %1").arg((int) unit.u);
	return debug;
}




QDebug operator<<(QDebug debug, const Gradient & gradient)
{
	debug << QString("Gradient %1 %2").arg(gradient.m_ll_value).arg((int) gradient.m_unit.u);
	return debug;
}








template<>
bool Measurement<SpeedType>::ll_value_is_valid(SpeedType::LL value)
{
	return !std::isnan(value);
}




template<>
QString Measurement<SpeedType>::to_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	switch (this->m_unit.u) {
	case SpeedType::Unit::E::KilometresPerHour:
		result = QObject::tr("%1 km/h").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedType::Unit::E::MilesPerHour:
		result = QObject::tr("%1 mph").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedType::Unit::E::MetresPerSecond:
		result = QObject::tr("%1 m/s").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedType::Unit::E::Knots:
		result = QObject::tr("%1 knots").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Unhandled unit" << this->m_unit;
		break;
	}

	return result;
}




template<>
QString Measurement<SpeedType>::ll_value_to_string(SpeedType::LL value, const SpeedType::Unit & unit)
{
	QString result;
	if (!Speed::ll_value_is_valid(value)) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	const int precision = SG_PRECISION_SPEED;
	switch (unit.u) {
	case SpeedType::Unit::E::KilometresPerHour:
		result = QObject::tr("%1 km/h").arg(value, 0, 'f', precision);
		break;
	case SpeedType::Unit::E::MilesPerHour:
		result = QObject::tr("%1 mph").arg(value, 0, 'f', precision);
		break;
	case SpeedType::Unit::E::MetresPerSecond:
		result = QObject::tr("%1 m/s").arg(value, 0, 'f', precision);
		break;
	case SpeedType::Unit::E::Knots:
		result = QObject::tr("%1 knots").arg(value, 0, 'f', precision);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Unhandled unit" << unit;
		break;
	}

	return result;
}




template<>
QString Measurement<SpeedType>::to_nice_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	/* TODO_LATER: implement magnitude-dependent calculations for
	   the values to be "nice". */

	switch (this->m_unit.u) {
	case SpeedType::Unit::E::KilometresPerHour:
		result = QObject::tr("%1 km/h").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedType::Unit::E::MilesPerHour:
		result = QObject::tr("%1 mph").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedType::Unit::E::MetresPerSecond:
		result = QObject::tr("%1 m/s").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedType::Unit::E::Knots:
		result = QObject::tr("%1 knots").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Unhandled unit" << this->m_unit;
		break;
	}

	return result;
}




template<>
SpeedType::LL Measurement<SpeedType>::convert_to_unit(SpeedType::LL value, const SpeedType::Unit & from, const SpeedType::Unit & to)
{
	SpeedType::LL result = NAN;

	switch (from.u) {
	case SpeedType::Unit::E::KilometresPerHour:
		qDebug() << SG_PREFIX_E << "Unhandled case";
		break;
	case SpeedType::Unit::E::MilesPerHour:
		qDebug() << SG_PREFIX_E << "Unhandled case";
		break;
	case SpeedType::Unit::E::MetresPerSecond:
		switch (to.u) {
		case SpeedType::Unit::E::KilometresPerHour:
			result = VIK_MPS_TO_KPH(value);
			break;
		case SpeedType::Unit::E::MilesPerHour:
			result = VIK_MPS_TO_MPH(value);
			break;
		case SpeedType::Unit::E::MetresPerSecond:
			result = value;
			break;
		case SpeedType::Unit::E::Knots:
			result = VIK_MPS_TO_KNOTS(value);
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target unit" << to;
			break;
		}
		break;
	case SpeedType::Unit::E::Knots:
		qDebug() << SG_PREFIX_E << "Unhandled case";
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled source unit" << from;
		break;
	}

	return result;
}




template<>
Speed Measurement<SpeedType>::convert_to_unit(const SpeedType::Unit & target_unit) const
{
	Speed result;
	result.m_ll_value = Speed::convert_to_unit(this->m_ll_value, this->m_unit, target_unit);
	result.m_unit = target_unit;
	result.m_valid = Speed::ll_value_is_valid(result.m_ll_value);
	return result;
}




template<>
QString Measurement<SpeedType>::unit_string(const SpeedType::Unit & speed_unit)
{
	QString result;

	switch (speed_unit.u) {
	case SpeedType::Unit::E::KilometresPerHour:
		result = QObject::tr("km/h");
		break;
	case SpeedType::Unit::E::MilesPerHour:
		result = QObject::tr("mph");
		break;
	case SpeedType::Unit::E::MetresPerSecond:
		result = QObject::tr("m/s");
		break;
	case SpeedType::Unit::E::Knots:
		result = QObject::tr("knots");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled unit" << speed_unit;
		break;
	}

	return result;
}




template<>
QString Measurement<SpeedType>::unit_full_string(const SpeedType::Unit & unit)
{
	QString result;

	switch (unit.u) {
	case SpeedType::Unit::E::KilometresPerHour:
		result = QObject::tr("kilometers per hour");
		break;
	case SpeedType::Unit::E::MilesPerHour:
		result = QObject::tr("miles per hour");
		break;
	case SpeedType::Unit::E::MetresPerSecond:
		result = QObject::tr("meters per second");
		break;
	case SpeedType::Unit::E::Knots:
		result = QObject::tr("knots");
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Unhandled unit" << unit;
		break;
	}

	return result;
}




template<>
const QString Measurement<SpeedType>::value_to_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
	} else {
		result = QObject::tr("%1").arg(this->m_ll_value, 0, 'f', SG_PRECISION_ALTITUDE);
	}

	return result;
}




template<>
const QString Measurement<SpeedType>::value_to_string_for_file(int precision) const
{
	QString result;
	if (this->m_valid) {
		result = SGUtils::double_to_c(this->m_ll_value, precision);
	}
	return result;
}




template<>
bool Measurement<SpeedType>::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->m_valid) {
		return true;
	}
	return std::abs(this->m_ll_value) < epsilon;
}




sg_ret Speed::make_speed(const Distance & distance, const Duration & duration)
{
	if (distance.unit().u != DistanceType::Unit::E::Meters) {
		qDebug() << SG_PREFIX_E << "Unhandled distance unit" << distance.unit();
		return sg_ret::err;
	}
	if (duration.unit().u != DurationType::Unit::E::Seconds) {
		qDebug() << SG_PREFIX_E << "Unhandled duration unit" << duration.unit();
		return sg_ret::err;
	}

	this->m_ll_value = distance.ll_value() / duration.ll_value();
	this->m_unit.u = SpeedType::Unit::E::MetresPerSecond;
	this->m_valid = Speed::ll_value_is_valid(this->m_ll_value);

	return this->m_valid ? sg_ret::ok : sg_ret::err;
}




sg_ret Speed::make_speed(const Altitude & altitude, const Duration & duration)
{
	if (altitude.unit() != AltitudeType::Unit::E::Metres) {
		qDebug() << SG_PREFIX_E << "Unhandled altitude unit" << altitude.unit();
		return sg_ret::err;
	}
	if (duration.unit() != DurationType::Unit::E::Seconds) {
		qDebug() << SG_PREFIX_E << "Unhandled duration unit" << duration.unit();
		return sg_ret::err;
	}

	this->m_ll_value = altitude.ll_value() / duration.ll_value();
	this->m_unit.u = SpeedType::Unit::E::MetresPerSecond;
	this->m_valid = Speed::ll_value_is_valid(this->ll_value());

	return this->m_valid ? sg_ret::ok : sg_ret::err;
}




SpeedType::Unit SpeedType::Unit::user_unit(void)
{
	return Preferences::get_unit_speed();
}




QDebug operator<<(QDebug debug, const SpeedType::Unit & unit)
{
	debug << QString("speed unit %1").arg((int) unit.u);
	return debug;
}




QDebug operator<<(QDebug debug, const Speed & speed)
{
	debug << QString("Speed %1 %2").arg(speed.m_ll_value).arg((int) speed.m_unit.u);
	return debug;
}








template<>
bool Measurement<AltitudeType>::ll_value_is_valid(AltitudeType::LL value)
{
	return !std::isnan(value);
}




template<>
QString Measurement<AltitudeType>::to_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	switch (this->m_unit.u) {
	case AltitudeType::Unit::E::Metres:
		result = QObject::tr("%1 m").arg(this->m_ll_value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	case AltitudeType::Unit::E::Feet:
		result = QObject::tr("%1 ft").arg(this->m_ll_value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled unit" << this->m_unit;
		break;
	}

	return result;
}




template<>
QString Measurement<AltitudeType>::ll_value_to_string(AltitudeType::LL value, const AltitudeType::Unit & unit)
{
	QString result;
	if (!Altitude::ll_value_is_valid(value)) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	const int precision = SG_PRECISION_ALTITUDE;
	switch (unit.u) {
	case AltitudeType::Unit::E::Metres:
		result = QObject::tr("%1 m").arg(value, 0, 'f', precision);
		break;
	case AltitudeType::Unit::E::Feet:
		result = QObject::tr("%1 ft").arg(value, 0, 'f', precision);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Unhandled unit" << unit;
		break;
	}

	return result;
}




template<>
QString Measurement<AltitudeType>::unit_string(const AltitudeType::Unit & unit)
{
	QString result;

	switch (unit.u) {
	case AltitudeType::Unit::E::Metres:
		result = QString("m");
		break;
 	case AltitudeType::Unit::E::Feet:
		result = QString("ft");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Uhandled unit" << unit;
		break;
	}

	return result;
}




template<>
AltitudeType::LL Measurement<AltitudeType>::convert_to_unit(AltitudeType::LL value, const AltitudeType::Unit & from, const AltitudeType::Unit & to)
{
	AltitudeType::LL result = NAN;

	switch (from.u) {
	case AltitudeType::Unit::E::Metres:
		switch (to.u) {
		case AltitudeType::Unit::E::Metres: /* No need to convert. */
			result = value;
			break;
		case AltitudeType::Unit::E::Feet:
			result = VIK_METERS_TO_FEET(value);
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target unit" << to;
			result = NAN;
			break;
		}
		break;
	case AltitudeType::Unit::E::Feet:
		switch (to.u) {
		case AltitudeType::Unit::E::Metres:
			result = VIK_FEET_TO_METERS(value);
			break;
		case AltitudeType::Unit::E::Feet:
			/* No need to convert. */
			result = value;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target unit" << to;
			result = NAN;
			break;
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled source unit" << from;
		result = NAN;
		break;
	}

	return result;
}




template<>
Altitude Measurement<AltitudeType>::convert_to_unit(const AltitudeType::Unit & target_unit) const
{
	Altitude result;
	result.m_ll_value = Altitude::convert_to_unit(this->m_ll_value, this->m_unit, target_unit);
	result.m_unit = target_unit;
	result.m_valid = Altitude::ll_value_is_valid(result.m_ll_value);
	return result;
}




#if 0
template<>
sg_ret Measurement<AltitudeType>::set_from_string(const char * str)
{
	if (NULL == str) {
		qDebug() << SG_PREFIX_E << "Attempting to set invalid value of altitude from NULL string";
		this->m_valid = false;
		return sg_ret::err_arg;
	} else {
		return this->set_from_string(QString(str));
	}
}




template<>
sg_ret Measurement<AltitudeType>::set_from_string(const QString & str)
{
	this->m_ll_value = SGUtils::c_to_double(str);
	this->m_valid = !std::isnan(this->m_ll_value);
	this->m_unit.u = AltitudeType::Unit::E::Metres;

	return this->m_valid ? sg_ret::ok : sg_ret::err;
}
#endif




template<>
QString Measurement<AltitudeType>::unit_full_string(const AltitudeType::Unit & height_unit)
{
	QString result;

	switch (height_unit.u) {
	case AltitudeType::Unit::E::Metres:
		result = QObject::tr("meters");
		break;
	case AltitudeType::Unit::E::Feet:
		result = QObject::tr("feet");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled unit" << height_unit;
		result = SG_MEASUREMENT_INVALID_UNIT_STRING;
		break;
	}

	return result;
}




template<>
AltitudeType::LL Measurement<AltitudeType>::floor(void) const
{
	if (!this->m_valid) {
		return INT_MIN;
	}
	return std::floor(this->m_ll_value);
}




template<>
QString Measurement<AltitudeType>::to_nice_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	/* TODO_LATER: implement magnitude-dependent calculations for
	   the values to be "nice". */

	switch (this->m_unit.u) {
	case AltitudeType::Unit::E::Metres:
		result = QObject::tr("%1 m").arg(this->m_ll_value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	case AltitudeType::Unit::E::Feet:
		result = QObject::tr("%1 ft").arg(this->m_ll_value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled unit" << this->m_unit;
		break;
	}

	return result;
}




template<>
const QString Measurement<AltitudeType>::value_to_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
	} else {
		result = QObject::tr("%1").arg(this->m_ll_value, 0, 'f', SG_PRECISION_ALTITUDE);
	}

	return result;
}




template<>
const QString Measurement<AltitudeType>::value_to_string_for_file(int precision) const
{
	QString result;
	if (this->m_valid) {
		result = SGUtils::double_to_c(this->m_ll_value, precision);
	}
	return result;
}




template<>
bool Measurement<AltitudeType>::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->m_valid) {
		return true;
	}
	return std::abs(this->m_ll_value) < epsilon;
}




AltitudeType::Unit AltitudeType::Unit::user_unit(void)
{
	return Preferences::get_unit_height();
}




QDebug operator<<(QDebug debug, const AltitudeType::Unit & unit)
{
	debug << QString("altitude unit %1").arg((int) unit.u);
	return debug;
}




QDebug operator<<(QDebug debug, const Altitude & altitude)
{
	debug << QString("Altitude %1 %2").arg(altitude.m_ll_value).arg((int) altitude.m_unit.u);
	return debug;
}








template<>
bool Measurement<AngleType>::ll_value_is_valid(AngleType::LL value)
{
	return !std::isnan(value);
}




template<>
QString Measurement<AngleType>::to_string(int precision) const
{
	if (this->is_valid()) {
		return QObject::tr("%1%2").arg(RAD2DEG(this->m_ll_value), 5, 'f', precision, '0').arg(DEGREE_SYMBOL);
	} else {
		return SG_MEASUREMENT_INVALID_VALUE_STRING;
	}
}




template<>
QString Measurement<AngleType>::unit_string(const AngleType::Unit & unit)
{
	QString result;

	switch (unit.u) {
	case AngleType::Unit::E::Degrees:
		result = QString("%1").arg(DEGREE_SYMBOL);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Uhandled unit" << unit;
		break;
	}

	return result;
}




Angle Angle::get_vector_sum(const Angle & angle1, const Angle & angle2)
{
	/*
	  TODO_MAYBE: there is still some room for improvement in this
	  function:
	  - can't we do it any easier way?
	  - is the epsilon value in if() correct?
	  - when do we accept returning 0, and when 2*pi?
	*/

	double angle = 0.0;
	Angle result;

	if (angle1.m_unit != angle2.m_unit) {
		qDebug() << SG_PREFIX_E << "Unit mismatch:" << angle1.m_unit << angle2.m_unit;
		return result;
	}
	if (!angle1.m_valid || !angle2.m_valid) {
		qDebug() << SG_PREFIX_E << "One of arguments is invalid:" << angle1.m_valid << angle2.m_valid;
		return result;
	}

	const double angle_min = std::min(angle1.m_ll_value, angle2.m_ll_value);
	const double angle_max = std::max(angle1.m_ll_value, angle2.m_ll_value);

	const double diff = angle_max - angle_min;
	if (std::abs(M_PI - diff) > 0.000000000001) { /* Check for two angles that are 180 degrees apart. */
		const double x1 = cos(angle1.m_ll_value);
		const double y1 = sin(angle1.m_ll_value);

		const double x2 = cos(angle2.m_ll_value);
		const double y2 = sin(angle2.m_ll_value);

		const double x = x1 + x2;
		const double y = y1 + y2;

		angle = x == 0.0 ? 0.0 : atan2(y, x);
		angle = angle < 0 ? ((2 * M_PI) + angle) : angle;
	}

	result.m_ll_value = angle;
	result.m_unit = angle1.m_unit;
	result.m_valid = Angle::ll_value_is_valid(result.m_ll_value);

	return result;
}




void Angle::normalize(void)
{
	if (!this->is_valid()) {
		return;
	}

	if (this->m_ll_value < 0) {
		this->m_ll_value += 2 * M_PI;
	}
	if (this->m_ll_value > 2 * M_PI) {
		this->m_ll_value -= 2 * M_PI;
	}

	return;
}




template<>
QString Measurement<AngleType>::to_string(void) const
{
	QString result;

	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	switch (this->m_unit.u) {
	case AngleType::Unit::E::Radians:
		result = QObject::tr("%1 rad").arg(this->m_ll_value, 0, 'f', SG_PRECISION_COURSE);
		break;
	case AngleType::Unit::E::Degrees:
		result = QObject::tr("%1%2").arg(this->m_ll_value, 0, 'f', SG_PRECISION_COURSE).arg(DEGREE_SYMBOL);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled unit" << this->m_unit;
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		break;
	}

	return result;
}




template<>
const QString Measurement<AngleType>::value_to_string_for_file(int precision) const
{
	QString result;
	if (this->m_valid) {
		result = SGUtils::double_to_c(this->m_ll_value, precision);
	}
	return result;
}




template<>
bool Measurement<AngleType>::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->m_valid) {
		return true;
	}
	return std::abs(this->m_ll_value) < epsilon;
}




AngleType::Unit AngleType::Unit::user_unit(void)
{
	return AngleType::Unit::E::Degrees; /* TODO_LATER: get this from preferences. */
}




QDebug operator<<(QDebug debug, const AngleType::Unit & unit)
{
	debug << QString("angle unit %1").arg((int) unit.u);
	return debug;
}




QDebug operator<<(QDebug debug, const Angle & angle)
{
	debug << QString("Angle %1 %2").arg(angle.m_ll_value).arg((int) angle.m_unit.u);
	return debug;
}








template<>
bool Measurement<DistanceType>::ll_value_is_valid(DistanceType::LL value)
{
	return !std::isnan(value);
}




template<>
DistanceType::LL Measurement<DistanceType>::convert_to_unit(DistanceType::LL input, const DistanceType::Unit & from, const DistanceType::Unit & to)
{
	DistanceType::LL result = NAN;

	switch (from.u) {
	case DistanceType::Unit::E::Kilometres:
		switch (to.u) {
		case DistanceType::Unit::E::Meters:
			/* Kilometers to meters. */
			result = input * 1000.0;
			break;
		case DistanceType::Unit::E::Yards:
			/* Kilometers to yards. */
			result = input * 1000 * 1.0936133;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target unit" << to;
			break;
		}
		break;

	case DistanceType::Unit::E::Miles:
		switch (to.u) {
		case DistanceType::Unit::E::Meters:
			/* Miles to meters. */
			result = VIK_MILES_TO_METERS(input);
			break;
		case DistanceType::Unit::E::Yards:
			/* Miles to yards. */
			result = input * 1760;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target unit" << to;
			break;
		}
		break;

	case DistanceType::Unit::E::NauticalMiles:
		switch (to.u) {
		case DistanceType::Unit::E::Meters:
			/* Nautical Miles to meters. */
			result = VIK_NAUTICAL_MILES_TO_METERS(input);
			break;
		case DistanceType::Unit::E::Yards:
			/* Nautical Miles to yards. */
			result = input * 2025.37183;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target unit" << to;
			break;
		}
		break;

	case DistanceType::Unit::E::Meters:
		switch (to.u) {
		case DistanceType::Unit::E::Kilometres:
			result = input / 1000.0;
			break;
		case DistanceType::Unit::E::Miles:
			result = VIK_METERS_TO_MILES(input);
			break;
		case DistanceType::Unit::E::NauticalMiles:
			result = VIK_METERS_TO_NAUTICAL_MILES(input);
			break;
		case DistanceType::Unit::E::Meters:
			/* Meters to meters. */
			result = input;
			break;
		case DistanceType::Unit::E::Yards:
			/* Meters to yards. */
			result = input * 1.0936133;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target unit" << to;
			break;
		}
		break;

	case DistanceType::Unit::E::Yards:
		switch (to.u) {
		case DistanceType::Unit::E::Meters:
			/* Yards to meters. */
			result = input * 0.9144;
			break;
		case DistanceType::Unit::E::Yards:
			/* Yards to yards. */
			result = input;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target unit" << to;
			break;
		}
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unhandled source unit" << from;
		break;
	}

	return result;
}




template<>
Distance Measurement<DistanceType>::convert_to_unit(const DistanceType::Unit & target_distance_unit) const
{
	Distance result;
	result.m_ll_value = Distance::convert_to_unit(this->m_ll_value, this->m_unit, target_distance_unit);
	result.m_unit = target_distance_unit;
	result.m_valid = Distance::ll_value_is_valid(result.m_ll_value);
	return result;
}




template<>
QString Measurement<DistanceType>::to_nice_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	/* TODO_LATER: implement magnitude-dependent calculations for
	   the values to be "nice". */

	switch (this->m_unit.u) {
	case DistanceType::Unit::E::Kilometres:
		result = QObject::tr("%1 km").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;

	case DistanceType::Unit::E::Miles:
		result = QObject::tr("%1 miles").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;

	case DistanceType::Unit::E::NauticalMiles:
		result = QObject::tr("%1 NM").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;

	case DistanceType::Unit::E::Meters:
		if (this->m_ll_value <= 1000.0) {
			result = QObject::tr("%1 m").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		} else {
			result = QObject::tr("%1 km").arg(this->m_ll_value / 1000.0, 0, 'f', SG_PRECISION_DISTANCE);
		}
		break;

	case DistanceType::Unit::E::Yards:
		result = QObject::tr("%1 yd").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unhandled unit" << this->m_unit;
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		break;
	}

	return result;
}




template<>
QString Measurement<DistanceType>::ll_value_to_string(DistanceType::LL value, const DistanceType::Unit & unit)
{
	QString result;
	if (std::isnan(value)) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	switch (unit.u) {
	case DistanceType::Unit::E::Kilometres:
		result = QObject::tr("%1 km").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceType::Unit::E::Miles:
		result = QObject::tr("%1 miles").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceType::Unit::E::NauticalMiles:
		result = QObject::tr("%1 NM").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceType::Unit::E::Meters:
		result = QObject::tr("%1 m").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceType::Unit::E::Yards:
		result = QObject::tr("%1 yd").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Unhandled unit" << unit;
		break;
	}

	return result;
}




template<>
QString Measurement<DistanceType>::to_string(void) const
{
	QString result;

	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	switch (this->m_unit.u) {
	case DistanceType::Unit::E::Kilometres:
		result = QObject::tr("%1 km").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceType::Unit::E::Miles:
		result = QObject::tr("%1 miles").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceType::Unit::E::NauticalMiles:
		result = QObject::tr("%1 NM").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceType::Unit::E::Meters:
		result = QObject::tr("%1 m").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceType::Unit::E::Yards:
		result = QObject::tr("%1 yd").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled unit" << this->m_unit;
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		break;
	}

	return result;
}




template<>
QString Measurement<DistanceType>::unit_string(const DistanceType::Unit & unit)
{
	QString result;

	switch (unit.u) {
	case DistanceType::Unit::E::Kilometres:
		result = QObject::tr("km");
		break;
	case DistanceType::Unit::E::Miles:
		result = QObject::tr("miles");
		break;
	case DistanceType::Unit::E::NauticalMiles:
		result = QObject::tr("NM");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled unit" << unit;
		result = SG_MEASUREMENT_INVALID_UNIT_STRING;
		break;
	}

	return result;
}




template<>
QString Measurement<DistanceType>::unit_full_string(const DistanceType::Unit & distance_unit)
{
	QString result;

	switch (distance_unit.u) {
	case DistanceType::Unit::E::Kilometres:
		result = QObject::tr("kilometers");
		break;
	case DistanceType::Unit::E::Miles:
		result = QObject::tr("miles");
		break;
	case DistanceType::Unit::E::NauticalMiles:
		result = QObject::tr("nautical miles");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled unit" << distance_unit;
		result = SG_MEASUREMENT_INVALID_UNIT_STRING;
		break;
	}

	return result;
}




template <>
bool Measurement<DistanceType>::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->m_valid) {
		return true;
	}
	return std::abs(this->m_ll_value) < epsilon;
}




DistanceType::Unit DistanceType::Unit::user_unit(void)
{
	return Preferences::get_unit_distance();
}




QDebug operator<<(QDebug debug, const DistanceType::Unit & unit)
{
	debug << QString("distance unit %1").arg((int) unit.u);
	return debug;
}




QDebug operator<<(QDebug debug, const Distance & distance)
{
	debug << QString("Distance %1 %2").arg(distance.m_ll_value).arg((int) distance.m_unit.u);
	return debug;
}




#if 0
 case DistanceType::Unit::E::Kilometres:
	if (distance >= 1000 && distance < 100000) {
		distance_label = QObject::tr("%1 km").arg(distance/1000.0, 0, 'f', 2);
	} else if (distance < 1000) {
		distance_label = QObject::tr("%1 m").arg((int) distance);
	} else {
		distance_label = QObject::tr("%1 km").arg((int) distance/1000);
	}
	break;
 case DistanceType::Unit::E::Miles:
	if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
		distance_label = QObject::tr("%1 miles").arg(VIK_METERS_TO_MILES(distance), 0, 'f', 2);
	} else if (distance < VIK_MILES_TO_METERS(1)) {
		distance_label = QObject::tr("%1 yards").arg((int) (distance * 1.0936133));
	} else {
		distance_label = QObject::tr("%1 miles").arg((int) VIK_METERS_TO_MILES(distance));
	}
	break;
 case DistanceType::Unit::E::NauticalMiles:
	if (distance >= VIK_NAUTICAL_MILES_TO_METERS(1) && distance < VIK_NAUTICAL_MILES_TO_METERS(100)) {
		distance_label = QObject::tr("%1 NM").arg(VIK_METERS_TO_NAUTICAL_MILES(distance), 0, 'f', 2);
	} else if (distance < VIK_NAUTICAL_MILES_TO_METERS(1)) {
		distance_label = QObject::tr("%1 yards").arg((int) (distance * 1.0936133));
	} else {
		distance_label = QObject::tr("%1 NM").arg((int) VIK_METERS_TO_NAUTICAL_MILES(distance));
	}
	break;
	default:
	qDebug() << SG_PREFIX_E << "Invalid target distance unit" << (int) target_distance_unit;
	break;




static QString distance_string(double distance)
{
	QString result;
	char str[128];

	/* Draw label with distance. */
	const DistanceType::Unit distance_unit = Preferences::get_unit_distance();
	switch (distance_unit) {
	case DistanceType::Unit::E::Kilometres:
		if (distance >= 1000 && distance < 100000) {
			result = QObject::tr("%1 km").arg(distance/1000.0, 6, 'f', 2); /* ""%3.2f" */
		} else if (distance < 1000) {
			result = QObject::tr("%1 m").arg((int) distance);
		} else {
			result = QObject::tr("%1 km").arg((int) distance/1000);
		}
		break;
	case DistanceType::Unit::E::Miles:
		if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
			result = QObject::tr("%1 miles").arg(VIK_METERS_TO_MILES(distance), 6, 'f', 2); /* "%3.2f" */
		} else if (distance < 1609.4) {
			result = QObject::tr("%1 yards").arg((int) (distance * 1.0936133));
		} else {
			result = QObject::tr("%1 miles").arg((int) VIK_METERS_TO_MILES(distance));
		}
		break;
	case DistanceType::Unit::E::NauticalMiles:
		if (distance >= VIK_NAUTICAL_MILES_TO_METERS(1) && distance < VIK_NAUTICAL_MILES_TO_METERS(100)) {
			result = QObject::tr("%1 NM").arg(VIK_METERS_TO_NAUTICAL_MILES(distance), 6, 'f', 2); /* "%3.2f" */
		} else if (distance < VIK_NAUTICAL_MILES_TO_METERS(1)) {
			result = QObject::tr("%1 yards").arg((int) (distance * 1.0936133));
		} else {
			result = QObject::tr("%1 NM").arg((int) VIK_METERS_TO_NAUTICAL_MILES(distance));
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) distance_unit;
		break;
	}
	return result;
}
#endif




}




static QString time_string_adjusted(time_t time, int offset_s)
{
	time_t mytime = time + offset_s;
	char * str = (char *) malloc(64);
	struct tm tm;
	/* Append asterisks to indicate use of simplistic model (i.e. no TZ). */
	strftime(str, 64, "%a %X %x **", gmtime_r(&mytime, &tm));

	QString result(str);
	free(str);
	return result;
}




static QString time_string_tz(time_t time, Qt::DateFormat format, const QTimeZone & tz)
{
	QDateTime utc = QDateTime::fromTime_t(time, Qt::OffsetFromUTC, 0);  /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
	QDateTime local = utc.toTimeZone(tz);

	return local.toString(format);
}




QString Measurements::get_file_size_string(size_t file_size)
{
	float size = (float) file_size;

	static const QStringList suffixes = { QObject::tr("B"), QObject::tr("KB"), QObject::tr("MB"), QObject::tr("GB"), QObject::tr("TB") };

	QStringListIterator iter(suffixes);
	QString unit = iter.next(); /* Initially B/Bytes. */
	bool show_fractional = false; /* Show (or don't show) fractional part when displaying number of bytes. */

	while (size >= 1024.0 && iter.hasNext()) {
		unit = iter.next();
		size /= 1024.0;
		show_fractional = true;
	}

	return QObject::tr("%1%2").arg(size, 0, 'f', show_fractional * 2).arg(unit);
}




bool Measurements::unit_tests(void)
{
	const double epsilon = 0.0001;

	{
		const Angle a1(DEG2RAD(0.0), AngleType::Unit::E::Radians);
		const Angle a2(DEG2RAD(0.0), AngleType::Unit::E::Radians);
		const double expected = DEG2RAD(0.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(360.0), AngleType::Unit::E::Radians);
		const Angle a2(DEG2RAD(360.0), AngleType::Unit::E::Radians);
		const double expected = DEG2RAD(360.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(70.0), AngleType::Unit::E::Radians);
		const Angle a2(DEG2RAD(70.0), AngleType::Unit::E::Radians);
		const double expected = DEG2RAD(70.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(184.0), AngleType::Unit::E::Radians);
		const Angle a2(DEG2RAD(186.0), AngleType::Unit::E::Radians);
		const double expected = DEG2RAD(185.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(185.0), AngleType::Unit::E::Radians);
		const Angle a2(DEG2RAD(185.0), AngleType::Unit::E::Radians);
		const double expected = DEG2RAD(185.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(350.0), AngleType::Unit::E::Radians);
		const Angle a2(DEG2RAD(20.0), AngleType::Unit::E::Radians);
		const double expected = DEG2RAD(5.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(0.0), AngleType::Unit::E::Radians);
		const Angle a2(DEG2RAD(180.0), AngleType::Unit::E::Radians);
		const double expected = DEG2RAD(0.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(-180.0), AngleType::Unit::E::Radians);
		const Angle a2(DEG2RAD(+180.0), AngleType::Unit::E::Radians);
		const double expected = DEG2RAD(180.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(90), AngleType::Unit::E::Radians);
		const Angle a2(DEG2RAD(270.0), AngleType::Unit::E::Radians);
		const double expected = DEG2RAD(0.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.ll_value() - expected));
	}

	{
		Distance smaller(0.1, DistanceType::Unit::E::Meters);
		Distance larger(0.2, DistanceType::Unit::E::Meters);

		assert(smaller < larger);
		assert(smaller <= larger);
		assert(!(smaller > larger));
		assert(!(smaller >= larger));
	}

	{
		Distance larger(1000, DistanceType::Unit::E::Meters);
		Distance smaller(999.99, DistanceType::Unit::E::Meters);

		assert(larger > smaller);
		assert(larger >= smaller);
		assert(!(larger < smaller));
		assert(!(larger <= smaller));
	}

	{
		Distance equal1(4.0, DistanceType::Unit::E::Meters);
		Distance equal2(4.0, DistanceType::Unit::E::Meters);

		assert(!(equal1 > equal2));
		assert(equal1 >= equal2);
		assert(!(equal1 < equal2));
		assert(equal1 <= equal2);
	}

	return true;
}




#if 0




bool Measurement::operator_args_valid(const Measurement & lhs, const Measurement & rhs)
{
	if (!lhs.m_valid || !rhs.m_valid) {
		qDebug() << SG_PREFIX_W << "Operands invalid";
		return false;
	}
	if (lhs.m_unit != rhs.m_unit) {
		qDebug() << SG_PREFIX_E << "Unit mismatch";
		return false;
	}

	return true;
}




QString get_time_grid_label(const Time & interval_value, const Time & value)
{
	QString result;

	const time_t val = value.get_value();
	const time_t interval = interval_value.get_value();

	switch (interval) {
	case 60:
	case 120:
	case 300:
	case 900:
		/* Minutes. */
		result = QObject::tr("%1 m").arg((int) (val / 60));
		break;
	case 1800:
	case 3600:
	case 10800:
	case 21600:
		/* Hours. */
		result = QObject::tr("%1 h").arg(((double) (val / (60 * 60))), 0, 'f', 1);
		break;
	case 43200:
	case 86400:
	case 172800:
		/* Days. */
		result = QObject::tr("%1 d").arg(((double) val / (60 *60 * 24)), 0, 'f', 1);
		break;
	case 604800:
	case 1209600:
		/* Weeks. */
		result = QObject::tr("%1 w").arg(((double) val / (60 * 60 * 24 * 7)), 0, 'f', 1);
		break;
	case 2419200:
		/* Months. */
		result = QObject::tr("%1 M").arg(((double) val / (60 * 60 * 24 * 28)), 0, 'f', 1);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled value" << val;
		break;
	}

	return result;
}




#endif
