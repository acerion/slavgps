/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
template<>
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
template<>
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




template<>
TimeUnit Time::get_user_unit(void)
{
	return TimeUnit::Seconds;
}




template<>
QString Time::to_timestamp_string(Qt::TimeSpec time_spec) const
{
	QString result;
	if (this->is_valid()) {
		 /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		result = QDateTime::fromTime_t(this->get_ll_value(), time_spec).toString(Qt::ISODate);
	} else {
		result = QObject::tr("No Data");
	}
	return result;
}




template<>
QString Time::to_string(void) const
{
	return this->to_timestamp_string();
}




template<>
bool Time::is_zero(void) const
{
	if (!this->m_valid) {
		return true;
	}
	return this->m_ll_value == 0;
}




template<>
TimeUnit Time::get_internal_unit(void)
{
	return SG_MEASUREMENT_INTERNAL_UNIT_TIME;
}




template<>
sg_ret Time::set_timestamp_from_string(const QString & str)
{
	this->m_ll_value = (Time_ll) str.toULong(&this->m_valid);

	if (!this->m_valid) {
		qDebug() << SG_PREFIX_W << "Setting invalid value of timestamp from string" << str;
	}

	return this->m_valid ? sg_ret::ok : sg_ret::err;
}



template<>
sg_ret Time::set_timestamp_from_char_string(const char * str)
{
	if (NULL == str) {
		qDebug() << SG_PREFIX_E << "Attempting to set invalid value of timestamp from NULL string";
		this->m_valid = false;
		return sg_ret::err_arg;
	} else {
		return this->set_value_from_string(QString(str));
	}
}




template<>
bool Time::ll_value_is_valid(Time_ll value)
{
	return true; /* TODO: improve for Time_ll data type. */
}




template<>
const QString Time::value_to_string_for_file(int precision) const
{
	QString result;
	if (this->m_valid) {
		result.setNum(this->m_ll_value);
	}
	return result;
}




#if 0
bool SlavGPS::operator<(const Time & lhs, const Time & rhs)
{
	/* TODO: shouldn't this be difftime()? */
	return lhs.m_ll_value < rhs.m_ll_value;
}
#endif




template<>
QString Time::strftime_utc(const char * format) const
{
	char timestamp_string[32] = { 0 };
	struct tm tm;
	std::strftime(timestamp_string, sizeof (timestamp_string), format, gmtime_r(&this->m_ll_value, &tm));

	return QString(timestamp_string);
}




template<>
QString Time::strftime_local(const char * format) const
{
	char timestamp_string[32] = { 0 };
	struct tm tm;
	std::strftime(timestamp_string, sizeof (timestamp_string), format, localtime_r(&this->m_ll_value, &tm));

	return QString(timestamp_string);
}




template<>
QString Time::get_time_string(Qt::DateFormat format) const
{
	QDateTime date_time;
	date_time.setTime_t(this->m_ll_value);
	const QString result = date_time.toString(format);

	return result;
}








QString SlavGPS::Duration::to_string(void) const
{
	QString result;

	const int seconds = this->m_ll_value % 60;
	const int minutes = (this->m_ll_value / 60) % 60;
	const int hours   = (this->m_ll_value / (60 * 60)) % 60;

	return QObject::tr("%1 h %2 m %3 s").arg(hours).arg(minutes, 2, 10, (QChar) '0').arg(seconds, 2, 10, (QChar) '0');
}




template<>
Duration Time::get_abs_duration(const Time & later, const Time & earlier)
{
	Duration result;
	if (later.m_unit != earlier.m_unit) {
		qDebug() << SG_PREFIX_E << "Arguments have different units:" << (int) later.m_unit << (int) earlier.m_unit;
	} else {
		if (later.m_ll_value >= earlier.m_ll_value) {
			result.m_ll_value = later.m_ll_value - earlier.m_ll_value;
		} else {
			result.m_ll_value = earlier.m_ll_value - later.m_ll_value;
		}
		result.m_valid = Measurement::ll_value_is_valid(result.m_ll_value);
		result.m_unit = later.m_unit;
	}
	return result;
}








template<>
bool Gradient::ll_value_is_valid(Gradient_ll value)
{
	return !std::isnan(value);
}




template<>
GradientUnit Gradient::get_user_unit(void)
{
	return GradientUnit::Percents;
}




template<>
QString Gradient::ll_value_to_string(Gradient_ll value, GradientUnit unit)
{
	QString result;
	if (!Gradient::ll_value_is_valid(value)) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	const int precision = SG_PRECISION_GRADIENT;
	switch (unit) {
	case GradientUnit::Percents:
		result = QObject::tr("%1%").arg(value, 0, 'f', precision);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Invalid gradient unit" << (int) unit;
		break;
	}

	return result;
}




template<>
QString Gradient::to_string(void) const
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
GradientUnit Gradient::get_internal_unit(void)
{
	return SG_MEASUREMENT_INTERNAL_UNIT_GRADIENT;
}




template<>
QString Gradient::get_unit_string(GradientUnit unit)
{
	QString result;

	switch (unit) {
	case GradientUnit::Percents:
		result = QString("%");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected gradient unit" << (int) unit;
		break;
	}

	return result;
}




template<>
const QString Gradient::value_to_string(void) const
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
const QString Gradient::value_to_string_for_file(int precision) const
{
	QString result;
	if (this->m_valid) {
		result = SGUtils::double_to_c(this->m_ll_value, precision);
	}
	return result;
}




template<>
bool Gradient::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->m_valid) {
		return true;
	}
	return std::abs(this->m_ll_value) < epsilon;
}








template<>
bool Speed::ll_value_is_valid(Speed_ll value)
{
	return !std::isnan(value);
}




template<>
SpeedUnit Speed::get_internal_unit(void)
{
	return SG_MEASUREMENT_INTERNAL_UNIT_SPEED;
}




template<>
SpeedUnit Speed::get_user_unit(void)
{
	return Preferences::get_unit_speed();
}




template<>
QString Speed::to_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	switch (this->m_unit) {
	case SpeedUnit::KilometresPerHour:
		result = QObject::tr("%1 km/h").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::MilesPerHour:
		result = QObject::tr("%1 mph").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::MetresPerSecond:
		result = QObject::tr("%1 m/s").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::Knots:
		result = QObject::tr("%1 knots").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) this->m_unit;
		break;
	}

	return result;
}




template<>
QString Speed::ll_value_to_string(Speed_ll value, SpeedUnit unit)
{
	QString result;
	if (!Speed::ll_value_is_valid(value)) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	const int precision = SG_PRECISION_SPEED;
	switch (unit) {
	case SpeedUnit::KilometresPerHour:
		result = QObject::tr("%1 km/h").arg(value, 0, 'f', precision);
		break;
	case SpeedUnit::MilesPerHour:
		result = QObject::tr("%1 mph").arg(value, 0, 'f', precision);
		break;
	case SpeedUnit::MetresPerSecond:
		result = QObject::tr("%1 m/s").arg(value, 0, 'f', precision);
		break;
	case SpeedUnit::Knots:
		result = QObject::tr("%1 knots").arg(value, 0, 'f', precision);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) unit;
		break;
	}

	return result;
}




template<>
QString Speed::to_nice_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	/* TODO_LATER: implement magnitude-dependent calculations for
	   the values to be "nice". */

	switch (this->m_unit) {
	case SpeedUnit::KilometresPerHour:
		result = QObject::tr("%1 km/h").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::MilesPerHour:
		result = QObject::tr("%1 mph").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::MetresPerSecond:
		result = QObject::tr("%1 m/s").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::Knots:
		result = QObject::tr("%1 knots").arg(this->m_ll_value, 0, 'f', SG_PRECISION_SPEED);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) this->m_unit;
		break;
	}

	return result;
}




template<>
Speed_ll Speed::convert_to_unit(Speed_ll value, SpeedUnit from, SpeedUnit to)
{
	Speed_ll result = NAN;

	switch (from) {
	case SpeedUnit::KilometresPerHour:
		qDebug() << SG_PREFIX_E << "Unhandled case";
		break;
	case SpeedUnit::MilesPerHour:
		qDebug() << SG_PREFIX_E << "Unhandled case";
		break;
	case SpeedUnit::MetresPerSecond:
		switch (to) {
		case SpeedUnit::KilometresPerHour:
			result = VIK_MPS_TO_KPH(value);
			break;
		case SpeedUnit::MilesPerHour:
			result = VIK_MPS_TO_MPH(value);
			break;
		case SpeedUnit::MetresPerSecond:
			result = value;
			break;
		case SpeedUnit::Knots:
			result = VIK_MPS_TO_KNOTS(value);
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid target speed unit" << (int) to;
			break;
		}
		break;
	case SpeedUnit::Knots:
		qDebug() << SG_PREFIX_E << "Unhandled case";
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid source speed unit" << (int) from;
		break;
	}

	return value;
}





template<>
Speed Speed::convert_to_unit(SpeedUnit target_unit) const
{
	Speed result;
	result.m_ll_value = Speed::convert_to_unit(this->m_ll_value, this->m_unit, target_unit);
	result.m_unit = target_unit;
	result.m_valid = Speed::ll_value_is_valid(result.m_ll_value);
	return result;
}




template<>
QString Speed::get_unit_string(SpeedUnit speed_unit)
{
	QString result;

	switch (speed_unit) {
	case SpeedUnit::KilometresPerHour:
		result = QObject::tr("km/h");
		break;
	case SpeedUnit::MilesPerHour:
		result = QObject::tr("mph");
		break;
	case SpeedUnit::MetresPerSecond:
		result = QObject::tr("m/s");
		break;
	case SpeedUnit::Knots:
		result = QObject::tr("knots");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) speed_unit;
		break;
	}

	return result;
}




template<>
QString Speed::get_unit_full_string(SpeedUnit unit)
{
	QString result;

	switch (unit) {
	case SpeedUnit::KilometresPerHour:
		result = QObject::tr("kilometers per hour");
		break;
	case SpeedUnit::MilesPerHour:
		result = QObject::tr("miles per hour");
		break;
	case SpeedUnit::MetresPerSecond:
		result = QObject::tr("meters per second");
		break;
	case SpeedUnit::Knots:
		result = QObject::tr("knots");
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) unit;
		break;
	}

	return result;
}




template<>
const QString Speed::value_to_string(void) const
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
const QString Speed::value_to_string_for_file(int precision) const
{
	QString result;
	if (this->m_valid) {
		result = SGUtils::double_to_c(this->m_ll_value, precision);
	}
	return result;
}




template<>
bool Speed::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->m_valid) {
		return true;
	}
	return std::abs(this->m_ll_value) < epsilon;
}




template<>
sg_ret Speed::make_speed(const Measurement<DistanceUnit, Distance_ll> & distance, const Duration & duration)
{
	if (distance.get_unit() != DistanceUnit::Meters) {
		qDebug() << SG_PREFIX_E << "Unhandled distance unit" << (int) distance.get_unit();
		return sg_ret::err;
	}
	if (duration.get_unit() != TimeUnit::Seconds) {
		qDebug() << SG_PREFIX_E << "Unhandled duration unit" << (int) duration.get_unit();
		return sg_ret::err;
	}

	this->m_ll_value = distance.get_ll_value() / duration.get_ll_value();
	this->m_unit = SpeedUnit::MetresPerSecond;
	this->m_valid = Speed::ll_value_is_valid(this->m_ll_value);

	return this->m_valid ? sg_ret::ok : sg_ret::err;
}




template<>
sg_ret Speed::make_speed(const Measurement<HeightUnit, Altitude_ll> & altitude, const Duration & duration)
{
	if (altitude.get_unit() != HeightUnit::Metres) {
		qDebug() << SG_PREFIX_E << "Unhandled altitude unit" << (int) altitude.get_unit();
		return sg_ret::err;
	}
	if (duration.get_unit() != TimeUnit::Seconds) {
		qDebug() << SG_PREFIX_E << "Unhandled duration unit" << (int) duration.get_unit();
		return sg_ret::err;
	}

	this->m_ll_value = altitude.get_ll_value() / duration.get_ll_value();
	this->m_unit = SpeedUnit::MetresPerSecond;
	this->m_valid = Speed::ll_value_is_valid(this->m_ll_value);

	return this->m_valid ? sg_ret::ok : sg_ret::err;
}








template<>
bool Altitude::ll_value_is_valid(Altitude_ll value)
{
	return !std::isnan(value);
}




template<>
HeightUnit Altitude::get_internal_unit(void)
{
	return SG_MEASUREMENT_INTERNAL_UNIT_HEIGHT;
}




template<>
HeightUnit Altitude::get_user_unit(void)
{
	return Preferences::get_unit_height();
}




template<>
QString Altitude::to_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	switch (this->m_unit) {
	case HeightUnit::Metres:
		result = QObject::tr("%1 m").arg(this->m_ll_value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	case HeightUnit::Feet:
		result = QObject::tr("%1 ft").arg(this->m_ll_value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid altitude unit" << (int) this->m_unit;
		break;
	}

	return result;
}




template<>
QString Altitude::ll_value_to_string(Altitude_ll value, HeightUnit unit)
{
	QString result;
	if (!Altitude::ll_value_is_valid(value)) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	const int precision = SG_PRECISION_ALTITUDE;
	switch (unit) {
	case HeightUnit::Metres:
		result = QObject::tr("%1 m").arg(value, 0, 'f', precision);
		break;
	case HeightUnit::Feet:
		result = QObject::tr("%1 ft").arg(value, 0, 'f', precision);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Invalid altitude unit" << (int) unit;
		break;
	}

	return result;
}




template<>
Altitude_ll Altitude::convert_to_unit(Altitude_ll value, HeightUnit from, HeightUnit to)
{
	Altitude_ll result = NAN;

	switch (from) {
	case HeightUnit::Metres:
		switch (to) {
		case HeightUnit::Metres: /* No need to convert. */
			result = value;
			break;
		case HeightUnit::Feet:
			result = VIK_METERS_TO_FEET(value);
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid target altitude unit" << (int) to;
			result = NAN;
			break;
		}
		break;
	case HeightUnit::Feet:
		switch (to) {
		case HeightUnit::Metres:
			result = VIK_FEET_TO_METERS(value);
			break;
		case HeightUnit::Feet:
			/* No need to convert. */
			result = value;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid target altitude unit" << (int) to;
			result = NAN;
			break;
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid source altitude unit" << (int) from;
		result = NAN;
		break;
	}

	return result;
}




template<>
Altitude Altitude::convert_to_unit(HeightUnit target_unit) const
{
	Altitude result;
	result.m_ll_value = Altitude::convert_to_unit(this->m_ll_value, this->m_unit, target_unit);
	result.m_unit = target_unit;
	result.m_valid = Altitude::ll_value_is_valid(result.m_ll_value);
	return result;
}




#if 0
template<>
sg_ret Altitude::set_from_string(const char * str)
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
sg_ret Altitude::set_from_string(const QString & str)
{
	this->m_ll_value = SGUtils::c_to_double(str);
	this->m_valid = !std::isnan(this->m_ll_value);
	this->m_unit = HeightUnit::Metres;

	return this->m_valid ? sg_ret::ok : sg_ret::err;
}
#endif




template<>
QString Altitude::get_unit_full_string(HeightUnit height_unit)
{
	QString result;

	switch (height_unit) {
	case HeightUnit::Metres:
		result = QObject::tr("meters");
		break;
	case HeightUnit::Feet:
		result = QObject::tr("feet");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid height unit" << (int) height_unit;
		result = SG_MEASUREMENT_INVALID_UNIT_STRING;
		break;
	}

	return result;
}




template<>
Altitude_ll Altitude::floor(void) const
{
	if (!this->m_valid) {
		return INT_MIN;
	}
	return std::floor(this->m_ll_value);
}




template<>
QString Altitude::to_nice_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	/* TODO_LATER: implement magnitude-dependent calculations for
	   the values to be "nice". */

	switch (this->m_unit) {
	case HeightUnit::Metres:
		result = QObject::tr("%1 m").arg(this->m_ll_value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	case HeightUnit::Feet:
		result = QObject::tr("%1 ft").arg(this->m_ll_value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid altitude unit" << (int) this->m_unit;
		break;
	}

	return result;
}




template<>
const QString Altitude::value_to_string(void) const
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
const QString Altitude::value_to_string_for_file(int precision) const
{
	QString result;
	if (this->m_valid) {
		result = SGUtils::double_to_c(this->m_ll_value, precision);
	}
	return result;
}




template<>
bool Altitude::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->m_valid) {
		return true;
	}
	return std::abs(this->m_ll_value) < epsilon;
}








template<>
bool Angle::ll_value_is_valid(Angle_ll value)
{
	return !std::isnan(value);
}




template<>
QString Angle::to_string(int precision) const
{
	if (this->is_valid()) {
		return QObject::tr("%1%2").arg(RAD2DEG(this->m_ll_value), 5, 'f', precision, '0').arg(DEGREE_SYMBOL);
	} else {
		return SG_MEASUREMENT_INVALID_VALUE_STRING;
	}
}




template<>
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
		qDebug() << SG_PREFIX_E << "Unit mismatch:" << (int) angle1.m_unit << (int) angle2.m_unit;
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




template<>
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
QString Angle::to_string(void) const
{
	QString result;

	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	switch (this->m_unit) {
	case AngleUnit::Radians:
		result = QObject::tr("%1 rad").arg(this->m_ll_value, 0, 'f', SG_PRECISION_COURSE);
		break;
	case AngleUnit::Degrees:
		result = QObject::tr("%1%2").arg(this->m_ll_value, 0, 'f', SG_PRECISION_COURSE).arg(DEGREE_SYMBOL);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected angle unit" << (int) this->m_unit;
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		break;
	}

	return result;
}




template<>
const QString Angle::value_to_string_for_file(int precision) const
{
	QString result;
	if (this->m_valid) {
		result = SGUtils::double_to_c(this->m_ll_value, precision);
	}
	return result;
}




template<>
bool Angle::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->m_valid) {
		return true;
	}
	return std::abs(this->m_ll_value) < epsilon;
}








template<>
bool Distance::ll_value_is_valid(Distance_ll value)
{
	return !std::isnan(value);
}




template<>
DistanceUnit Distance::get_internal_unit(void)
{
	return SG_MEASUREMENT_INTERNAL_UNIT_DISTANCE;
}




template<>
DistanceUnit Distance::get_user_unit(void)
{
	return Preferences::get_unit_distance();
}




template<>
Distance_ll Distance::convert_to_unit(Distance_ll input, DistanceUnit from, DistanceUnit to)
{
	Distance_ll result = NAN;

	switch (from) {
	case DistanceUnit::Kilometres:
		switch (to) {
		case DistanceUnit::Meters:
			/* Kilometers to meters. */
			result = input * 1000.0;
			break;
		case DistanceUnit::Yards:
			/* Kilometers to yards. */
			result = input * 1000 * 1.0936133;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target distance unit" << (int) to;
			break;
		}
		break;

	case DistanceUnit::Miles:
		switch (to) {
		case DistanceUnit::Meters:
			/* Miles to meters. */
			result = VIK_MILES_TO_METERS(input);
			break;
		case DistanceUnit::Yards:
			/* Miles to yards. */
			result = input * 1760;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target distance unit" << (int) to;
			break;
		}
		break;

	case DistanceUnit::NauticalMiles:
		switch (to) {
		case DistanceUnit::Meters:
			/* Nautical Miles to meters. */
			result = VIK_NAUTICAL_MILES_TO_METERS(input);
			break;
		case DistanceUnit::Yards:
			/* Nautical Miles to yards. */
			result = input * 2025.37183;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target distance unit" << (int) to;
			break;
		}
		break;

	case DistanceUnit::Meters:
		switch (to) {
		case DistanceUnit::Kilometres:
			result = input / 1000.0;
			break;
		case DistanceUnit::Miles:
			result = VIK_METERS_TO_MILES(input);
			break;
		case DistanceUnit::NauticalMiles:
			result = VIK_METERS_TO_NAUTICAL_MILES(input);
			break;
		case DistanceUnit::Meters:
			/* Meters to meters. */
			result = input;
			break;
		case DistanceUnit::Yards:
			/* Meters to yards. */
			result = input * 1.0936133;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target distance unit" << (int) to;
			break;
		}
		break;

	case DistanceUnit::Yards:
		switch (to) {
		case DistanceUnit::Meters:
			/* Yards to meters. */
			result = input * 0.9144;
			break;
		case DistanceUnit::Yards:
			/* Yards to yards. */
			result = input;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled target distance unit" << (int) to;
			break;
		}
		break;

	default:
		qDebug() << SG_PREFIX_E << "Unhandled source distance unit" << (int) from;
		break;
	}

	return result;
}




template<>
Distance Distance::convert_to_unit(DistanceUnit target_distance_unit) const
{
	Distance result;
	result.m_ll_value = Distance::convert_to_unit(this->m_ll_value, this->m_unit, target_distance_unit);
	result.m_unit = target_distance_unit;
	result.m_valid = Distance::ll_value_is_valid(result.m_ll_value);
	return result;
}




template<>
QString Distance::to_nice_string(void) const
{
	QString result;
	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	/* TODO_LATER: implement magnitude-dependent calculations for
	   the values to be "nice". */

	switch (this->m_unit) {
	case DistanceUnit::Kilometres:
		result = QObject::tr("%1 km").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;

	case DistanceUnit::Miles:
		result = QObject::tr("%1 miles").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;

	case DistanceUnit::NauticalMiles:
		result = QObject::tr("%1 NM").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;

	case DistanceUnit::Meters:
		if (this->m_ll_value <= 1000.0) {
			result = QObject::tr("%1 m").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		} else {
			result = QObject::tr("%1 km").arg(this->m_ll_value / 1000.0, 0, 'f', SG_PRECISION_DISTANCE);
		}
		break;

	case DistanceUnit::Yards:
		result = QObject::tr("%1 yd").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;

	default:
		qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) this->m_unit;
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		break;
	}

	return result;
}




template<>
QString Distance::ll_value_to_string(Distance_ll value, DistanceUnit unit)
{
	QString result;
	if (std::isnan(value)) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	switch (unit) {
	case DistanceUnit::Kilometres:
		result = QObject::tr("%1 km").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceUnit::Miles:
		result = QObject::tr("%1 miles").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceUnit::NauticalMiles:
		result = QObject::tr("%1 NM").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceUnit::Meters:
		result = QObject::tr("%1 m").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceUnit::Yards:
		result = QObject::tr("%1 yd").arg(value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	default:
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) unit;
		break;
	}

	return result;
}




template<>
QString Distance::to_string(void) const
{
	QString result;

	if (!this->m_valid) {
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		return result;
	}

	switch (this->m_unit) {
	case DistanceUnit::Kilometres:
		result = QObject::tr("%1 km").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceUnit::Miles:
		result = QObject::tr("%1 miles").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceUnit::NauticalMiles:
		result = QObject::tr("%1 NM").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceUnit::Meters:
		result = QObject::tr("%1 m").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	case DistanceUnit::Yards:
		result = QObject::tr("%1 yd").arg(this->m_ll_value, 0, 'f', SG_PRECISION_DISTANCE);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) this->m_unit;
		result = SG_MEASUREMENT_INVALID_VALUE_STRING;
		break;
	}

	return result;
}




template<>
QString Distance::get_unit_full_string(DistanceUnit distance_unit)
{
	QString result;

	switch (distance_unit) {
	case DistanceUnit::Kilometres:
		result = QObject::tr("kilometers");
		break;
	case DistanceUnit::Miles:
		result = QObject::tr("miles");
		break;
	case DistanceUnit::NauticalMiles:
		result = QObject::tr("nautical miles");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) distance_unit;
		result = SG_MEASUREMENT_INVALID_UNIT_STRING;
		break;
	}

	return result;
}




template<>
bool Distance::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->m_valid) {
		return true;
	}
	return std::abs(this->m_ll_value) < epsilon;
}




#if 0
	case DistanceUnit::Kilometres:
	if (distance >= 1000 && distance < 100000) {
		distance_label = QObject::tr("%1 km").arg(distance/1000.0, 0, 'f', 2);
	} else if (distance < 1000) {
		distance_label = QObject::tr("%1 m").arg((int) distance);
	} else {
		distance_label = QObject::tr("%1 km").arg((int) distance/1000);
	}
	break;
	case DistanceUnit::Miles:
	if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
		distance_label = QObject::tr("%1 miles").arg(VIK_METERS_TO_MILES(distance), 0, 'f', 2);
	} else if (distance < VIK_MILES_TO_METERS(1)) {
		distance_label = QObject::tr("%1 yards").arg((int) (distance * 1.0936133));
	} else {
		distance_label = QObject::tr("%1 miles").arg((int) VIK_METERS_TO_MILES(distance));
	}
	break;
	case DistanceUnit::NauticalMiles:
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
	const DistanceUnit distance_unit = Preferences::get_unit_distance();
	switch (distance_unit) {
	case DistanceUnit::Kilometres:
		if (distance >= 1000 && distance < 100000) {
			result = QObject::tr("%1 km").arg(distance/1000.0, 6, 'f', 2); /* ""%3.2f" */
		} else if (distance < 1000) {
			result = QObject::tr("%1 m").arg((int) distance);
		} else {
			result = QObject::tr("%1 km").arg((int) distance/1000);
		}
		break;
	case DistanceUnit::Miles:
		if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
			result = QObject::tr("%1 miles").arg(VIK_METERS_TO_MILES(distance), 6, 'f', 2); /* "%3.2f" */
		} else if (distance < 1609.4) {
			result = QObject::tr("%1 yards").arg((int) (distance * 1.0936133));
		} else {
			result = QObject::tr("%1 miles").arg((int) VIK_METERS_TO_MILES(distance));
		}
		break;
	case DistanceUnit::NauticalMiles:
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
		const Angle a1(DEG2RAD(0.0), AngleUnit::Radians);
		const Angle a2(DEG2RAD(0.0), AngleUnit::Radians);
		const double expected = DEG2RAD(0.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.get_ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(360.0), AngleUnit::Radians);
		const Angle a2(DEG2RAD(360.0), AngleUnit::Radians);
		const double expected = DEG2RAD(360.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.get_ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(70.0), AngleUnit::Radians);
		const Angle a2(DEG2RAD(70.0), AngleUnit::Radians);
		const double expected = DEG2RAD(70.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.get_ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(184.0), AngleUnit::Radians);
		const Angle a2(DEG2RAD(186.0), AngleUnit::Radians);
		const double expected = DEG2RAD(185.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.get_ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(185.0), AngleUnit::Radians);
		const Angle a2(DEG2RAD(185.0), AngleUnit::Radians);
		const double expected = DEG2RAD(185.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.get_ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(350.0), AngleUnit::Radians);
		const Angle a2(DEG2RAD(20.0), AngleUnit::Radians);
		const double expected = DEG2RAD(5.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.get_ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(0.0), AngleUnit::Radians);
		const Angle a2(DEG2RAD(180.0), AngleUnit::Radians);
		const double expected = DEG2RAD(0.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.get_ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(-180.0), AngleUnit::Radians);
		const Angle a2(DEG2RAD(+180.0), AngleUnit::Radians);
		const double expected = DEG2RAD(180.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.get_ll_value() - expected));
	}
	{
		const Angle a1(DEG2RAD(90), AngleUnit::Radians);
		const Angle a2(DEG2RAD(270.0), AngleUnit::Radians);
		const double expected = DEG2RAD(0.0);

		const Angle result = Angle::get_vector_sum(a1, a2);
		qDebug() << SG_PREFIX_D << a1 << a2 << "-->" << result << "(expected =" << expected << ")";
		assert (epsilon > std::fabs(result.get_ll_value() - expected));
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
		qDebug() << SG_PREFIX_E << "Unhandled time interval value" << val;
		break;
	}

	return result;
}




#endif
