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

#include <QDebug>




#include <cmath>

#include "measurements.h"
#include "preferences.h"




using namespace SlavGPS;




#define SG_MODULE "Measurements"
#define PREFIX ": Measurements:" << __FUNCTION__ << __LINE__ << ">"
#define INVALID_RESULT_STRING "--"




QString Measurements::get_altitude_string(double value, int precision)
{
	const HeightUnit height_unit = Preferences::get_unit_height();

	QString buffer;

	switch (height_unit) {
	case HeightUnit::Metres:
		buffer = QObject::tr("%1 m").arg(value, 0, 'f', precision);
		break;
	case HeightUnit::Feet:
		buffer = QObject::tr("%1 feet").arg(value, 0, 'f', precision);
		break;
	default:
		buffer = "???";
		qDebug() << "EE" PREFIX << "invalid height unit" << (int) height_unit;
		break;
	}

	return buffer;
}




QString Measurements::get_altitude_string_recalculate(double value, int precision)
{
	const HeightUnit height_unit = Preferences::get_unit_height();

	QString buffer;

	switch (height_unit) {
	case HeightUnit::Metres:
		buffer = QObject::tr("%1 m").arg(value, 0, 'f', precision);
		break;
	case HeightUnit::Feet:
		buffer = QObject::tr("%1 feet").arg(VIK_METERS_TO_FEET(value), 0, 'f', precision);
		break;
	default:
		buffer = "???";
		qDebug() << "EE" PREFIX << "invalid height unit" << (int) height_unit;
		break;
	}

	return buffer;
}




QString Measurements::get_distance_string(double value, int precision)
{
	const DistanceUnit distance_unit = Preferences::get_unit_distance();

	QString buffer;

	switch (distance_unit) {
	case DistanceUnit::Kilometres:
		buffer = QObject::tr("%1 m").arg(value, 0, 'f', precision);
		break;
	case DistanceUnit::Miles:
		buffer = QObject::tr("%1 miles").arg(value, 0, 'f', precision);
		break;
	case DistanceUnit::NauticalMiles: /* TODO_REALLY: verify this case. */
		buffer = QObject::tr("%1 NM").arg(value, 0, 'f', precision);
		break;
	default:
		buffer = "???";
		qDebug() << "EE" PREFIX << "invalid distance unit" << (int) distance_unit;
		break;
	}

	return buffer;
}




QString Measurements::get_distance_string_short(double value, int precision)
{
	const DistanceUnit distance_unit = Preferences::get_unit_distance();

	QString buffer;

	switch (distance_unit) {
	case DistanceUnit::Kilometres:
		buffer = QObject::tr("%1 m").arg(value, 0, 'f', precision);
		break;
	case DistanceUnit::Miles:
	case DistanceUnit::NauticalMiles:
		buffer = QObject::tr("%1 yards").arg(value * 1.0936133, 0, 'f', precision); /* Miles -> yards. TODO_REALLY: verify this calculation. */
		break;
	default:
		buffer = "???";
		qDebug() << "EE" PREFIX << "invalid distance unit" << (int) distance_unit;
		break;
	}

	return buffer;
}






QString Measurements::get_speed_string(double value, int precision)
{
	if (std::isnan(value)) {
		return INVALID_RESULT_STRING;
	}

	const SpeedUnit speed_unit = Preferences::get_unit_speed();

	QString buffer;

	switch (speed_unit) {
	case SpeedUnit::KilometresPerHour:
		buffer = QObject::tr("%1 km/h").arg(VIK_MPS_TO_KPH (value), 0, 'f', precision);
		break;
	case SpeedUnit::MilesPerHour:
		buffer = QObject::tr("%1 mph").arg(VIK_MPS_TO_MPH (value), 0, 'f', precision);
		break;
	case SpeedUnit::MetresPerSecond:
		buffer = QObject::tr("%1 m/s").arg(value, 0, 'f', precision);
		break;
	case SpeedUnit::Knots:
		buffer = QObject::tr("%1 knots").arg(VIK_MPS_TO_KNOTS (value), 0, 'f', precision);
		break;
	default:
		buffer = "???";
		qDebug() << "EE:" PREFIX << "invalid speed unit" << (int) speed_unit;
		break;
	}

	return buffer;
}




QString Measurements::get_speed_string_dont_recalculate(double value, int precision)
{
	if (std::isnan(value)) {
		return INVALID_RESULT_STRING;
	}

	const SpeedUnit speed_unit = Preferences::get_unit_speed();

	QString buffer;

	switch (speed_unit) {
	case SpeedUnit::KilometresPerHour:
		buffer = QObject::tr("%1 km/h").arg(value, 0, 'f', precision);
		break;
	case SpeedUnit::MilesPerHour:
		buffer = QObject::tr("%1 mph").arg(value, 0, 'f', precision);
		break;
	case SpeedUnit::MetresPerSecond:
		buffer = QObject::tr("%1 m/s").arg(value, 0, 'f', precision);
		break;
	case SpeedUnit::Knots:
		buffer = QObject::tr("%1 knots").arg(value, 0, 'f', precision);
		break;
	default:
		buffer = "???";
		qDebug() << "EE:" PREFIX << "invalid speed unit" << (int) speed_unit;
		break;
	}

	return buffer;
}





QString Measurements::get_course_string(double value, int precision)
{
	if (std::isnan(value)) {
		return INVALID_RESULT_STRING;
	} else {
		/* Second arg is a degree symbol. */
		return QObject::tr("%1%2").arg(value, 5, 'f', precision, '0').arg("\u00b0");
	}
}




QString Measurements::get_file_size_string(size_t file_size)
{
	float size = (float) file_size;

	static const QStringList suffixes = { QObject::tr("KB"), QObject::tr("MB"), QObject::tr("GB"), QObject::tr("TB") };

	QStringListIterator iter(suffixes);
	QString unit = QObject::tr("B"); /* B == Bytes. */
	bool show_fractional = false; /* Show (or don't show) fractional part when displaying number of bytes. */

	while (size >= 1024.0 && iter.hasNext()) {
		unit = iter.next();
		size /= 1024.0;
		show_fractional = true;
	}

	return QObject::tr("%1%2").arg(size, 0, 'f', show_fractional * 2).arg(unit);
}




QString Measurements::get_duration_string(time_t duration)
{
	QString result;

	const int seconds = duration % 60;
	const int minutes = (duration / 60) % 60;
	const int hours   = (duration / (60 * 60)) % 60;

	return QObject::tr("%1 h %2 m %3 s").arg(hours).arg(minutes, 2, 10, (QChar) '0').arg(seconds, 2, 10, (QChar) '0');
}




QString Measurements::get_distance_string_for_ruler(double distance, DistanceUnit distance_unit)
{
	QString distance_label;

	switch (distance_unit) {
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
		qDebug() << "EE" PREFIX << "invalid distance unit" << (int) distance_unit;
		break;
	}

	return distance_label;
}




Distance::Distance(double new_value, DistanceUnit new_distance_unit)
{
	this->distance_unit = new_distance_unit;
	this->use_supplementary_distance_unit = false;

	this->value = new_value;

	if (!std::isnan(this->value) && this->value >= 0.0) {
		this->valid = true;
	}
}




Distance::Distance(double new_value, SupplementaryDistanceUnit new_supplementary_distance_unit)
{
	this->supplementary_distance_unit = new_supplementary_distance_unit;
	this->use_supplementary_distance_unit = false;

	this->value = new_value;

	if (!std::isnan(this->value) && this->value >= 0.0) {
		this->valid = true;
	}
}




Distance Distance::to_meters(void) const
{
	Distance output;
	output.use_supplementary_distance_unit = true; /* Because we are converting to meters. */
	output.supplementary_distance_unit = SupplementaryDistanceUnit::Meters;

	if (!this->valid) {
		return output;
	}

	if (this->use_supplementary_distance_unit) {
		switch (this->supplementary_distance_unit) {
		case SupplementaryDistanceUnit::Meters:
			output.value = this->value;
			output.valid = true;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid supplementary distance unit" << (int) this->supplementary_distance_unit;
			output.value = -1.0;
			break;
		}
	} else {
		switch (this->distance_unit) {
		case DistanceUnit::Kilometres:
			output.value = this->value * 1000.0;
			output.valid = true;
			break;
		case DistanceUnit::Miles:
			output.value = VIK_MILES_TO_METERS(this->value);
			output.valid = true;
			break;
		case DistanceUnit::NauticalMiles:
			output.value = VIK_NAUTICAL_MILES_TO_METERS(this->value);
			output.valid = true;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) this->distance_unit;
			output.value = -1.0;
			break;
		}
	}

	return output;
}




QString Distance::to_string(void) const
{
	QString result;

	if (!this->valid) {
		result = INVALID_RESULT_STRING;
		return result;
	}

	if (this->use_supplementary_distance_unit) {
		switch (this->supplementary_distance_unit) {
		case SupplementaryDistanceUnit::Meters:
			result = QObject::tr("%1 m").arg(this->value, 0, 'f', SG_PRECISION_DISTANCE);
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) this->distance_unit;
			result = INVALID_RESULT_STRING;
			break;
		}
	} else {
		switch (this->distance_unit) {
		case DistanceUnit::Kilometres:
			result = QObject::tr("%1 km").arg(this->value, 0, 'f', SG_PRECISION_DISTANCE);
			break;
		case DistanceUnit::Miles:
			result = QObject::tr("%1 miles").arg(this->value, 0, 'f', SG_PRECISION_DISTANCE);
			break;
		case DistanceUnit::NauticalMiles:
			result = QObject::tr("%1 NM").arg(this->value, 0, 'f', SG_PRECISION_DISTANCE);
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) this->distance_unit;
			result = INVALID_RESULT_STRING;
			break;
		}
	}

	return result;
}




QString Distance::to_nice_string(void) const
{
	QString result;

	if (!this->valid) {
		result = INVALID_RESULT_STRING;
		return result;
	}

	if (this->use_supplementary_distance_unit) {
		switch (this->supplementary_distance_unit) {
		case SupplementaryDistanceUnit::Meters:
			if (this->value <= 1000.0) {
				result = QObject::tr("%1 m").arg(this->value, 0, 'f', SG_PRECISION_DISTANCE);
			} else {
				result = QObject::tr("%1 km").arg(this->value / 1000.0, 0, 'f', SG_PRECISION_DISTANCE);
			}
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) this->distance_unit;
			result = INVALID_RESULT_STRING;
			break;
		}
	} else {
		/* TODO_LATER: Since the result should be "nice"
		   string, we should check if this->value is above or
		   below some threshold and either leave the value and
		   unit string, or convert the value (e.g. multiply by
		   1000.0) and use sub-unit string (e.g. meters). */
		switch (this->distance_unit) {
		case DistanceUnit::Kilometres:
			result = QObject::tr("%1 km").arg(this->value, 0, 'f', SG_PRECISION_DISTANCE);
			break;
		case DistanceUnit::Miles:
			result = QObject::tr("%1 miles").arg(this->value, 0, 'f', SG_PRECISION_DISTANCE);
			break;
		case DistanceUnit::NauticalMiles:
			result = QObject::tr("%1 NM").arg(this->value, 0, 'f', SG_PRECISION_DISTANCE);
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) this->distance_unit;
			result = INVALID_RESULT_STRING;
			break;
		}
	}

	return result;
}




bool Distance::is_valid(void) const
{
	return this->valid;
}
