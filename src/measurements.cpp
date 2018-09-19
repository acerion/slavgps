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




#include <cmath>




#include <QDebug>




#include "measurements.h"
#include "preferences.h"
#include "vikutils.h"




using namespace SlavGPS;




#define SG_MODULE "Measurements"
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
		qDebug() << SG_PREFIX_E << "Invalid height unit" << (int) height_unit;
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
		qDebug() << SG_PREFIX_E << "Invalid height unit" << (int) height_unit;
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
		qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) distance_unit;
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
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) speed_unit;
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
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) speed_unit;
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




Distance Distance::convert_to_unit(DistanceUnit target_distance_unit) const
{
	Distance result;
	result.distance_unit = target_distance_unit;
	result.use_supplementary_distance_unit = false;
	result.valid = false;

	if (this->use_supplementary_distance_unit) {
		switch (this->supplementary_distance_unit) {
		case SupplementaryDistanceUnit::Meters:
			switch (target_distance_unit) {
			case DistanceUnit::Kilometres:
				result.value = this->value / 1000.0;
				break;
			case DistanceUnit::Miles:
				result.value = VIK_METERS_TO_MILES(this->value);
				break;
			case DistanceUnit::NauticalMiles:
				result.value = VIK_METERS_TO_NAUTICAL_MILES(this->value);
				break;
			default:
				qDebug() << SG_PREFIX_E << "Invalid target distance unit" << (int) target_distance_unit;
				break;
			}
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) this->supplementary_distance_unit;
			break;
		}
	} else {
		qDebug() << SG_PREFIX_E << "Unhandled situation" << (int) this->distance_unit << this->value; /* TODO_LATER: implement */
#if 0
		switch (this->distance_unit) {
		case DistanceUnit::Kilometres:
			result.value = ;
			break;
		case DistanceUnit::Miles:
			result.value = ;
			break;
		case DistanceUnit::NauticalMiles:
			result.value = ;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) this->distance_unit;
			break;
		}
#endif
	}

	result.valid = (!std::isnan(result.value)) && result.value >= 0.0;

	return result;
}





Distance Distance::convert_to_supplementary_unit(DistanceUnit target_distance_unit) const
{
	Distance result;

	switch (target_distance_unit) {
	case DistanceUnit::Kilometres:
		result = this->convert_to_unit(SupplementaryDistanceUnit::Meters);
		break;
	case DistanceUnit::Miles:
	case DistanceUnit::NauticalMiles:
		result = this->convert_to_unit(SupplementaryDistanceUnit::Yards);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) target_distance_unit;
		break;
	}

	return result;
}




Distance::Distance(double new_value, SupplementaryDistanceUnit new_supplementary_distance_unit)
{
	this->supplementary_distance_unit = new_supplementary_distance_unit;
	this->use_supplementary_distance_unit = true;

	this->value = new_value;

	if (!std::isnan(this->value) && this->value >= 0.0) {
		this->valid = true;
	}
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




Distance Distance::convert_to_unit(SupplementaryDistanceUnit target_supplementary_distance_unit) const
{
	Distance output;
	output.use_supplementary_distance_unit = true; /* Because we are converting to supplementary unit. */
	output.supplementary_distance_unit = target_supplementary_distance_unit;
	output.valid = false;

	if (!this->valid) {
		return output;
	}


	if (this->use_supplementary_distance_unit) { /* This variable is in meters or yards, to be converted into yards or meters. */
		switch (this->supplementary_distance_unit) {
		case SupplementaryDistanceUnit::Meters:
			switch (target_supplementary_distance_unit) {
			case SupplementaryDistanceUnit::Meters:
				/* Meters to meters. */
				output.value = this->value;
				break;
			case SupplementaryDistanceUnit::Yards:
				/* Meters to yards. */
				output.value = this->value * 1.0936133;
				break;
			default:
				qDebug() << SG_PREFIX_E << "Invalid target supplementary distance unit" << (int) target_supplementary_distance_unit;
				break;
			}
		case SupplementaryDistanceUnit::Yards:
			switch (target_supplementary_distance_unit) {
			case SupplementaryDistanceUnit::Meters:
				/* Yards to meters. */
				output.value = this->value * 0.9144;
				break;
			case SupplementaryDistanceUnit::Yards:
				/* Yards to yards. */
				output.value = this->value;
				break;
			default:
				qDebug() << SG_PREFIX_E << "Invalid target supplementary distance unit" << (int) target_supplementary_distance_unit;
				break;
			}
		default:
			qDebug() << SG_PREFIX_E << "Invalid supplementary distance unit" << (int) this->supplementary_distance_unit;
			break;
		}
	} else {
		switch (this->distance_unit) {
		case DistanceUnit::Kilometres:
			switch (target_supplementary_distance_unit) {
			case SupplementaryDistanceUnit::Meters:
				/* Kilometers to meters. */
				output.value = this->value * 1000.0;
				break;
			case SupplementaryDistanceUnit::Yards:
				/* Kilometers to yards. */
				output.value = this->value * 1000 * 1.0936133;
				break;
			default:
				qDebug() << SG_PREFIX_E << "Invalid target supplementary distance unit" << (int) target_supplementary_distance_unit;
				break;
			}
		case DistanceUnit::Miles:
			switch (target_supplementary_distance_unit) {
			case SupplementaryDistanceUnit::Meters:
				/* Miles to meters. */
				output.value = VIK_MILES_TO_METERS(this->value);
				break;
			case SupplementaryDistanceUnit::Yards:
				/* Miles to yards. */
				output.value = this->value * 1760;
				break;
			default:
				qDebug() << SG_PREFIX_E << "Invalid target supplementary distance unit" << (int) target_supplementary_distance_unit;
				break;
			}
		case DistanceUnit::NauticalMiles:
			switch (target_supplementary_distance_unit) {
			case SupplementaryDistanceUnit::Meters:
				/* Nautical Miles to meters. */
				output.value = VIK_NAUTICAL_MILES_TO_METERS(this->value);
				break;
			case SupplementaryDistanceUnit::Yards:
				/* Nautical Miles to yards. */
				output.value = this->value * 2025.37183;
				break;
			default:
				qDebug() << SG_PREFIX_E << "Invalid target supplementary distance unit" << (int) target_supplementary_distance_unit;
				break;
			}
		default:
			qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) this->distance_unit;
			break;
		}
	}

	output.valid = !std::isnan(output.value) && output.value >= 0.0;

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




Distance & Distance::operator+=(const Distance & rhs)
{
	if (!rhs.valid) {
		return *this;
	}


	if (!this->valid || !rhs.valid) {
		qDebug() << SG_PREFIX_E << "Error" << __LINE__;
		return *this;
	}
	if (this->use_supplementary_distance_unit != rhs.use_supplementary_distance_unit) {
		qDebug() << SG_PREFIX_E << "Error" << __LINE__;
		return *this;
	}
	if (this->use_supplementary_distance_unit) {
		if (this->supplementary_distance_unit != rhs.supplementary_distance_unit) {
			qDebug() << SG_PREFIX_E << "Error" << __LINE__;
			return *this;
		}
	} else {
		if (this->distance_unit != rhs.distance_unit) {
			qDebug() << SG_PREFIX_E << "Error" << __LINE__;
			return *this;
		}
	}


	this->value += rhs.value;
	this->valid = !std::isnan(this->value) && this->value >= 0.0;
	return *this;
}




Distance Distance::operator+(const Distance & rhs)
{
	/* TODO_LATER: make operator work for arguments with different units. */
	Distance result;

	if (!this->valid || !rhs.valid) {
		qDebug() << SG_PREFIX_E << "Error" << __LINE__;
		return result;
	}
	if (this->use_supplementary_distance_unit != rhs.use_supplementary_distance_unit) {
		qDebug() << SG_PREFIX_E << "Error" << __LINE__;
		return result;
	}
	if (this->use_supplementary_distance_unit) {
		if (this->supplementary_distance_unit != rhs.supplementary_distance_unit) {
			qDebug() << SG_PREFIX_E << "Error" << __LINE__;
			return result;
		}
	} else {
		if (this->distance_unit != rhs.distance_unit) {
			qDebug() << SG_PREFIX_E << "Error" << __LINE__;
			return result;
		}
	}


	result.use_supplementary_distance_unit = this->use_supplementary_distance_unit;
	result.supplementary_distance_unit = this->supplementary_distance_unit;
	result.distance_unit = this->distance_unit;

	result.value = this->value + rhs.value;
	result.valid = !std::isnan(result.value) && result.value >= 0.0;

	return result;
}




Distance Distance::operator-(const Distance & rhs)
{
	/* TODO_LATER: make operator work for arguments with different units. */
	Distance result;

	if (!this->valid || !rhs.valid) {
		qDebug() << SG_PREFIX_E << "Error" << __LINE__;
		return result;
	}
	if (this->use_supplementary_distance_unit != rhs.use_supplementary_distance_unit) {
		qDebug() << SG_PREFIX_E << "Error" << __LINE__;
		return result;
	}
	if (this->use_supplementary_distance_unit) {
		if (this->supplementary_distance_unit != rhs.supplementary_distance_unit) {
			qDebug() << SG_PREFIX_E << "Error" << __LINE__;
			return result;
		}
	} else {
		if (this->distance_unit != rhs.distance_unit) {
			qDebug() << SG_PREFIX_E << "Error" << __LINE__;
			return result;
		}
	}


	result.use_supplementary_distance_unit = this->use_supplementary_distance_unit;
	result.supplementary_distance_unit = this->supplementary_distance_unit;
	result.distance_unit = this->distance_unit;

	result.value = this->value - rhs.value;
	result.valid = !std::isnan(result.value) && result.value >= 0.0;

	return result;
}




Altitude::Altitude(double new_value, HeightUnit height_unit)
{
	this->value = new_value;
	this->valid = (!std::isnan(new_value)) && new_value != VIK_DEFAULT_ALTITUDE;
	this->unit = height_unit;
}




bool Altitude::is_valid(void) const
{
	return this->valid;
}




const QString Altitude::value_to_string_for_file(void) const
{
	return SGUtils::double_to_c(this->value);
}




const QString Altitude::value_to_string(void) const
{
	QString result;
	if (this->valid) {
		result = QObject::tr("%1").arg(this->value, 0, 'f', SG_PRECISION_ALTITUDE);
	}
	return result;
}




double Altitude::get_value(void) const
{
	return this->value;
}




void Altitude::set_value(double new_value)
{
	this->value = new_value;
	this->valid = (!std::isnan(new_value)) && new_value != VIK_DEFAULT_ALTITUDE;
	/* Don't change unit. */
}




Altitude Altitude::convert_to_unit(HeightUnit target_height_unit) const
{
	Altitude output;
	output.unit = target_height_unit;

	switch (this->unit) {
	case HeightUnit::Metres:
		switch (target_height_unit) {
		case HeightUnit::Metres: /* No need to convert. */
			output.value = this->value;
			break;
		case HeightUnit::Feet:
			output.value = VIK_METERS_TO_FEET(this->value);
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid target altitude unit" << (int) target_height_unit;
			output.value = NAN;
			break;
		}
		break;
	case HeightUnit::Feet:
		switch (target_height_unit) {
		case HeightUnit::Metres:
			output.value = VIK_FEET_TO_METERS(this->value);
			break;
		case HeightUnit::Feet:
			/* No need to convert. */
			output.value = this->value;
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid target altitude unit" << (int) target_height_unit;
			output.value = NAN;
			break;
		}
	default:
		qDebug() << SG_PREFIX_E << "Invalid source altitude unit" << (int) this->unit;
		output.value = NAN;
		break;
	}

	output.valid = (!std::isnan(output.value)) && output.value != VIK_DEFAULT_ALTITUDE;

	return output;
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
			}
			break;
		default:
			qDebug() << SG_PREFIX_E << "Unhandled situation"; /* TODO_LATER: implement */
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
