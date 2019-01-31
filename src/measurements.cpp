/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
#include <ctime>




#include <QDebug>




#include "measurements.h"
#include "preferences.h"
#include "vikutils.h"




using namespace SlavGPS;




#define SG_MODULE "Measurements"
#define INVALID_RESULT_STRING "--"
#define INVALID_UNIT_STRING   "??"




static QString time_string_adjusted(time_t time, int offset_s);
static QString time_string_tz(time_t time, Qt::DateFormat format, const QTimeZone & tz);




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
		qDebug() << SG_PREFIX_W << "Operands invalid";
		return *this;
	}
	if (this->use_supplementary_distance_unit != rhs.use_supplementary_distance_unit) {
		qDebug() << SG_PREFIX_E << "Unit mismatch";
		return *this;
	}
	if (this->use_supplementary_distance_unit) {
		if (this->supplementary_distance_unit != rhs.supplementary_distance_unit) {
			qDebug() << SG_PREFIX_E << "Unit mismatch";
			return *this;
		}
	} else {
		if (this->distance_unit != rhs.distance_unit) {
			qDebug() << SG_PREFIX_E << "Unit mismatch";
			return *this;
		}
	}


	this->value += rhs.value;
	this->valid = !std::isnan(this->value) && this->value >= 0.0;
	return *this;
}




#if 0
Distance Distance::operator+(const Distance & rhs)
{
	/* TODO_LATER: make operator work for arguments with different units. */
	Distance result;

	if (!this->valid || !rhs.valid) {
		qDebug() << SG_PREFIX_W << "Operands invalid";
		return result;
	}
	if (this->use_supplementary_distance_unit != rhs.use_supplementary_distance_unit) {
		qDebug() << SG_PREFIX_E << "Unit mismatch";
		return result;
	}
	if (this->use_supplementary_distance_unit) {
		if (this->supplementary_distance_unit != rhs.supplementary_distance_unit) {
			qDebug() << SG_PREFIX_E << "Unit mismatch";
			return result;
		}
	} else {
		if (this->distance_unit != rhs.distance_unit) {
			qDebug() << SG_PREFIX_E << "Unit mismatch";
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
#endif




Distance SlavGPS::operator+(const Distance & lhs, const Distance & rhs)
{
	Distance result;

	if (!lhs.valid) {
		qDebug() << SG_PREFIX_W << "Operating on invalid lhs";
		return result;
	}

	if (!rhs.valid) {
		qDebug() << SG_PREFIX_W << "Operating on invalid rhs";
		return result;
	}

	result.value = lhs.value + rhs.value;
	result.valid = result.value >= 0;

	return result;
}




Distance SlavGPS::operator-(const Distance & lhs, const Distance & rhs)
{
	Distance result;

	if (!lhs.valid) {
		qDebug() << SG_PREFIX_W << "Operating on invalid lhs";
		return result;
	}

	if (!rhs.valid) {
		qDebug() << SG_PREFIX_W << "Operating on invalid rhs";
		return result;
	}

	result.value = lhs.value - rhs.value;
	result.valid = result.value >= 0;
	return result;
}




#if 0
Distance Distance::operator-(const Distance & rhs)
{
	/* TODO_LATER: make operator work for arguments with different units. */
	Distance result;

	if (!this->valid || !rhs.valid) {
		qDebug() << SG_PREFIX_E << "Operands invalid";
		return result;
	}
	if (this->use_supplementary_distance_unit != rhs.use_supplementary_distance_unit) {
		qDebug() << SG_PREFIX_E << "Unit mismatch";
		return result;
	}
	if (this->use_supplementary_distance_unit) {
		if (this->supplementary_distance_unit != rhs.supplementary_distance_unit) {
			qDebug() << SG_PREFIX_E << "Unit mismatch";
			return result;
		}
	} else {
		if (this->distance_unit != rhs.distance_unit) {
			qDebug() << SG_PREFIX_E << "Unit mismatch";
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
#endif



Distance & Distance::operator*=(double rhs)
{
	if (this->valid) {
		this->value *= rhs;
		this->valid = !std::isnan(this->value);
		return *this;
	} else {
		return *this;
	}
}




Distance & Distance::operator/=(double rhs)
{
	if (this->valid) {
		this->value /= rhs;
		this->valid = !std::isnan(this->value);
		return *this;
	} else {
		return *this;
	}
}




double SlavGPS::operator/(const Distance & lhs, const Distance & rhs)
{
	if (lhs.valid && rhs.valid && !rhs.is_zero()) {
		return lhs.value / rhs.value;
	} else {
		return NAN;
	}
}




bool SlavGPS::operator<(const Distance & lhs, const Distance & rhs)
{
	return lhs.value < rhs.value;
}




bool SlavGPS::operator>(const Distance & lhs, const Distance & rhs)
{
	return rhs < lhs;
}




bool SlavGPS::operator<=(const Distance & lhs, const Distance & rhs)
{
	return !(lhs > rhs);
}




bool SlavGPS::operator>=(const Distance & lhs, const Distance & rhs)
{
	return !(lhs < rhs);
}




QDebug SlavGPS::operator<<(QDebug debug, const Distance & distance)
{
	debug << "Distance:" << distance.value;
	return debug;
}



bool Distance::operator==(const Distance & rhs) const
{
	if (!this->valid) {
		qDebug() << SG_PREFIX_W << "Comparing invalid timestamp";
		return false;
	}

	if (!rhs.valid) {
		qDebug() << SG_PREFIX_W << "Comparing invalid timestamp";
		return false;
	}

	return this->value == rhs.value;
}




bool Distance::operator!=(const Distance & rhs) const
{
	return !(*this == rhs);
}




QString Distance::get_unit_full_string(DistanceUnit distance_unit)
{
	QString result;

	switch (distance_unit) {
	case DistanceUnit::Kilometres:
		result = QObject::tr("kilometers");
		break;
	case DistanceUnit::Miles:
		result = QObject::tr("miles)");
		break;
	case DistanceUnit::NauticalMiles:
		result = QObject::tr("nautical miles)");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) distance_unit;
		result = INVALID_UNIT_STRING;
		break;
	}

	return result;
}




double Distance::convert_meters_to(double distance, DistanceUnit distance_unit)
{
	switch (distance_unit) {
	case DistanceUnit::Kilometres:
		return distance / 1000.0;
	case DistanceUnit::Miles:
		return VIK_METERS_TO_MILES(distance);
	case DistanceUnit::NauticalMiles:
		return VIK_METERS_TO_NAUTICAL_MILES(distance);
	default:
		qDebug() << SG_PREFIX_E << "Invalid distance unit" << (int) distance_unit;
		return distance;
	}
}




bool Distance::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->valid) {
		return true;
	}
	return std::abs(this->value) < epsilon;
}




Altitude::Altitude(double new_value, HeightUnit height_unit)
{
	this->value = new_value;
	this->valid = !std::isnan(new_value);
	this->unit = height_unit;
}




bool Altitude::is_valid(void) const
{
	return this->valid;
}




void Altitude::set_valid(bool new_valid)
{
	this->valid = new_valid;
	if (!this->valid) {
		this->value = NAN;
	}
}




bool Altitude::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->valid) {
		return true;
	}
	return std::abs(this->value) < epsilon;
}



const QString Altitude::value_to_string_for_file(void) const
{
	return SGUtils::double_to_c(this->value);
}




const QString Altitude::value_to_string(void) const
{
	QString result;
	if (!this->valid) {
		result = INVALID_RESULT_STRING;
	} else {
		result = QObject::tr("%1").arg(this->value, 0, 'f', SG_PRECISION_ALTITUDE);
	}

	return result;
}




QString Altitude::to_string(void) const
{
	QString result;
	if (!this->valid) {
		result = INVALID_RESULT_STRING;
		return result;
	}

	switch (this->unit) {
	case HeightUnit::Metres:
		result = QObject::tr("%1 m").arg(this->value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	case HeightUnit::Feet:
		result = QObject::tr("%1 ft").arg(this->value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid altitude unit" << (int) this->unit;
		break;
	}

	return result;
}




QString Altitude::to_nice_string(void) const
{
	QString result;
	if (!this->valid) {
		result = INVALID_RESULT_STRING;
		return result;
	}

	/* TODO_LATER: implement magnitude-dependent recalculations. */

	switch (this->unit) {
	case HeightUnit::Metres:
		result = QObject::tr("%1 m").arg(this->value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	case HeightUnit::Feet:
		result = QObject::tr("%1 ft").arg(this->value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid altitude unit" << (int) this->unit;
		break;
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
	this->valid = !std::isnan(new_value);
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
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid source altitude unit" << (int) this->unit;
		output.value = NAN;
		break;
	}

	output.valid = !std::isnan(output.value);

	return output;
}



Altitude & Altitude::operator=(const Altitude & rhs)
{
	if (this == &rhs) {
		return *this;
	}

	this->value = rhs.value;
	this->valid = rhs.valid;
	this->unit = rhs.unit;

	return *this;
}



Altitude & Altitude::operator+=(double rhs)
{
	if (this->valid) {
		this->value += rhs;
		this->valid = !std::isnan(this->value);
	} else {
		this->value = rhs;
		this->valid = !std::isnan(this->value);
		return *this;
	}

	return *this;
}




Altitude & Altitude::operator+=(const Altitude & rhs)
{
	if (!rhs.valid) {
		return *this;
	}


	if (!this->valid || !rhs.valid) {
		qDebug() << SG_PREFIX_W << "Invalid operands";
		return *this;
	}
	if (this->unit != rhs.unit) {
		qDebug() << SG_PREFIX_E << "Unit mismatch";
		return *this;
	}

	this->value += rhs.value;
	this->valid = !std::isnan(this->value) && this->value >= 0.0;
	return *this;
}




Altitude & Altitude::operator-=(double rhs)
{
	if (this->valid) {
		this->value -= rhs;
		this->valid = !std::isnan(this->value);
	} else {
		this->value = rhs;
		this->valid = !std::isnan(this->value);
		return *this;
	}

	return *this;
}




Altitude & Altitude::operator-=(const Altitude & rhs)
{
	if (!rhs.valid) {
		return *this;
	}


	if (!this->valid || !rhs.valid) {
		qDebug() << SG_PREFIX_W << "Invalid operands";
		return *this;
	}
	if (this->unit != rhs.unit) {
		qDebug() << SG_PREFIX_E << "Unit mismatch";
		return *this;
	}

	this->value += rhs.value;
	this->valid = !std::isnan(this->value);
	return *this;
}




Altitude & Altitude::operator*=(double rhs)
{
	if (this->valid) {
		this->value *= rhs;
		this->valid = !std::isnan(this->value);
		return *this;
	} else {
		return *this;
	}
}




Altitude & Altitude::operator/=(double rhs)
{
	if (this->valid) {
		this->value /= rhs;
		this->valid = !std::isnan(this->value);
		return *this;
	} else {
		return *this;
	}
}




double SlavGPS::operator/(const Altitude & lhs, const Altitude & rhs)
{
	if (lhs.valid && rhs.valid && !rhs.is_zero()) {
		return lhs.value / rhs.value;
	} else {
		return NAN;
	}
}




bool SlavGPS::operator<(const Altitude & lhs, const Altitude & rhs)
{
	return lhs.value < rhs.value;
}




bool SlavGPS::operator>(const Altitude & lhs, const Altitude & rhs)
{
	return rhs < lhs;
}




bool SlavGPS::operator<=(const Altitude & lhs, const Altitude & rhs)
{
	return !(lhs > rhs);
}




bool SlavGPS::operator>=(const Altitude & lhs, const Altitude & rhs)
{
	return !(lhs < rhs);
}




int Altitude::floor(void) const
{
	if (!this->valid) {
		return INT_MIN;
	}
	return std::floor(this->value);
}




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
		result = INVALID_UNIT_STRING;
		break;
	}

	return result;
}



sg_ret Altitude::set_from_string(const char * str)
{
	if (NULL == str) {
		qDebug() << SG_PREFIX_E << "Attempting to set invalid value of altitude from NULL string";
		this->valid = false;
		return sg_ret::err_arg;
	} else {
		return this->set_from_string(QString(str));
	}
}




sg_ret Altitude::set_from_string(const QString & str)
{
	this->value = SGUtils::c_to_double(str);
	this->valid = !std::isnan(this->value);
	this->unit = HeightUnit::Metres;

	return this->valid ? sg_ret::ok : sg_ret::err;
}




Speed::Speed(double new_value, SpeedUnit speed_unit)
{
	this->value = new_value;
	this->valid = !std::isnan(new_value); /* We don't test if value is less than zero, because in some contexts the speed can be negative. */
	this->unit = speed_unit;
}




bool Speed::is_valid(void) const
{
	return this->valid;
}




bool Speed::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->valid) {
		return true;
	}
	return std::abs(this->value) < epsilon;
}




#if 0
const QString Speed::value_to_string_for_file(void) const
{
	return SGUtils::double_to_c(this->value);
}
#endif




const QString Speed::value_to_string(void) const
{
	QString result;
	if (!this->valid) {
		result = INVALID_RESULT_STRING;
	} else {
		result = QObject::tr("%1").arg(this->value, 0, 'f', SG_PRECISION_ALTITUDE);
	}

	return result;
}




QString Speed::to_string(void) const
{
	QString result;
	if (!this->valid) {
		result = INVALID_RESULT_STRING;
		return result;
	}

	switch (this->unit) {
	case SpeedUnit::KilometresPerHour:
		result = QObject::tr("%1 km/h").arg(VIK_MPS_TO_KPH (this->value), 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::MilesPerHour:
		result = QObject::tr("%1 mph").arg(VIK_MPS_TO_MPH (this->value), 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::MetresPerSecond:
		result = QObject::tr("%1 m/s").arg(this->value, 0, 'f', SG_PRECISION_SPEED);
		break;
	case SpeedUnit::Knots:
		result = QObject::tr("%1 knots").arg(VIK_MPS_TO_KNOTS (this->value), 0, 'f', SG_PRECISION_SPEED);
		break;
	default:
		result = INVALID_RESULT_STRING;
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) this->unit;
		break;
	}

	return result;
}



#if 0
QString Speed::to_nice_string(void) const
{
	QString result;
	if (!this->valid) {
		result = INVALID_RESULT_STRING;
		return result;
	}

	/* TODO_LATER: implement magnitude-dependent recalculations. */

	switch (this->unit) {
	case SpeedUnit::Metres:
		result = QObject::tr("%1 m").arg(this->value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	case SpeedUnit::Feet:
		result = QObject::tr("%1 ft").arg(this->value, 0, 'f', SG_PRECISION_ALTITUDE);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid altitude unit" << (int) this->unit;
		break;
	}

	return result;
}
#endif




QString Speed::to_string(double value, int precision)
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
		buffer = INVALID_RESULT_STRING;
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) speed_unit;
		break;
	}

	return buffer;
}




double Speed::get_value(void) const
{
	return this->value;
}




void Speed::set_value(double new_value)
{
	this->value = new_value;
	this->valid = !std::isnan(new_value);
	/* Don't change unit. */
}




Speed Speed::convert_to_unit(SpeedUnit target_speed_unit) const
{
	Speed output;
	output.unit = target_speed_unit;

	 /* TODO_LATER: implement missing calculations. */

	switch (this->unit) {
	case SpeedUnit::KilometresPerHour:
		qDebug() << SG_PREFIX_E << "Unhandled case";
		break;
	case SpeedUnit::MilesPerHour:
		qDebug() << SG_PREFIX_E << "Unhandled case";
		break;
	case SpeedUnit::MetresPerSecond:
		switch (target_speed_unit) {
		case SpeedUnit::KilometresPerHour:
			output.value = VIK_MPS_TO_KPH(this->value);
			break;
		case SpeedUnit::MilesPerHour:
			output.value = VIK_MPS_TO_MPH(this->value);
			break;
		case SpeedUnit::MetresPerSecond:
			output.value = this->value;
			break;
		case SpeedUnit::Knots:
			output.value = VIK_MPS_TO_KNOTS(this->value);
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid target speed unit" << (int) target_speed_unit;
			break;
		}

	case SpeedUnit::Knots:
		qDebug() << SG_PREFIX_E << "Unhandled case";
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) this->unit;
		break;
	}

	output.valid = !std::isnan(this->value);

	return output;
}



bool Speed::operator_args_valid(const Speed & lhs, const Speed & rhs)
{
	if (!lhs.valid || !rhs.valid) {
		qDebug() << SG_PREFIX_W << "Operands invalid";
		return false;
	}
	if (lhs.unit != rhs.unit) {
		/* TODO_LATER: make operator work for arguments with different units. */
		qDebug() << SG_PREFIX_E << "Unit mismatch";
		return false;
	}

	return true;
}




Speed & Speed::operator+=(const Speed & rhs)
{
	if (!Speed::operator_args_valid(*this, rhs)) {
		return *this;
	}

	this->value += rhs.value;
	this->valid = !std::isnan(this->value);
	return *this;
}




Speed & Speed::operator-=(const Speed & rhs)
{
	if (!Speed::operator_args_valid(*this, rhs)) {
		return *this;
	}

	this->value -= rhs.value;
	this->valid = !std::isnan(this->value);
	return *this;
}




Speed & Speed::operator*=(double x)
{
	if (!this->valid) {
		qDebug() << SG_PREFIX_E << "Operation on invalid arg";
		return *this;
	}

	this->value *= x;
	this->valid = !std::isnan(this->value);
	return *this;
}




Speed & Speed::operator/=(double x)
{
	if (!this->valid) {
		qDebug() << SG_PREFIX_E << "Operation on invalid arg";
		return *this;
	}

	this->value /= x;
	this->valid = !std::isnan(this->value);
	return *this;
}




double SlavGPS::operator/(const Speed & lhs, const Speed & rhs)
{
	if (lhs.valid && rhs.valid && !rhs.is_zero()) {
		return lhs.value / rhs.value;
	} else {
		return NAN;
	}
}




#if 0
QString Speed::get_unit_full_string(SpeedUnit speed_unit)
{
	QString result;

	switch (speed_unit) {
	case SpeedUnit::Metres:
		result = QObject::tr("meters");
		break;
	case SpeedUnit::Feet:
		result = QObject::tr("feet");
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) speed_unit;
		result = INVALID_UNIT_STRING;
		break;
	}

	return result;
}
#endif



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




double Speed::convert_mps_to(double speed, SpeedUnit speed_unit)
{
	switch (speed_unit) {
	case SpeedUnit::KilometresPerHour:
		speed = VIK_MPS_TO_KPH(speed);
		break;
	case SpeedUnit::MilesPerHour:
		speed = VIK_MPS_TO_MPH(speed);
		break;
	case SpeedUnit::MetresPerSecond:
		/* Already in m/s so nothing to do. */
		break;
	case SpeedUnit::Knots:
		speed = VIK_MPS_TO_KNOTS(speed);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Invalid speed unit" << (int) speed_unit;
		break;
	}

	return speed;
}




QString Angle::to_string(void) const
{
	return QObject::tr("%1°").arg(RAD2DEG(this->value), 0, 'f', 1);
}




QString Angle::get_course_string(double value, int precision)
{
	if (std::isnan(value)) {
		return INVALID_RESULT_STRING;
	} else {
		/* Second arg is a degree symbol. */
		return QObject::tr("%1%2").arg(value, 5, 'f', precision, '0').arg("\u00b0");
	}
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




Time::Time()
{
}




Time::Time(time_t new_value)
{
	this->value = new_value;
	this->valid = !std::isnan(this->value);
}




bool Time::is_valid(void) const
{
	return this->valid;
}




time_t Time::get_value(void) const
{
	return this->value;
}




Time SlavGPS::operator+(const Time & lhs, const Time & rhs)
{
	Time result;

	if (!lhs.valid) {
		qDebug() << SG_PREFIX_W << "Operating on invalid lhs";
		return result;
	}

	if (!rhs.valid) {
		qDebug() << SG_PREFIX_W << "Operating on invalid rhs";
		return result;
	}

	result.value = lhs.value + rhs.value;
	result.valid = result.value >= 0;

	return result;
}




Time SlavGPS::operator-(const Time & lhs, const Time & rhs)
{
	Time result;

	if (!lhs.valid) {
		qDebug() << SG_PREFIX_W << "Operating on invalid lhs";
		return result;
	}

	if (!rhs.valid) {
		qDebug() << SG_PREFIX_W << "Operating on invalid rhs";
		return result;
	}

	result.value = lhs.value - rhs.value;
	result.valid = result.value >= 0;
	return result;
}




bool SlavGPS::operator<(const Time & lhs, const Time & rhs)
{
	/* TODO: shouldn't this be difftime()? */
	return lhs.value < rhs.value;
}




bool SlavGPS::operator>(const Time & lhs, const Time & rhs)
{
	return rhs < lhs;
}



bool SlavGPS::operator<=(const Time & lhs, const Time & rhs)
{
	return !(lhs > rhs);
}




bool SlavGPS::operator>=(const Time & lhs, const Time & rhs)
{
	return !(lhs < rhs);
}




QDebug SlavGPS::operator<<(QDebug debug, const Time & timestamp)
{
	debug << "Time:" << timestamp.value;
	return debug;
}




bool Time::operator==(const Time & rhs) const
{
	if (!this->valid) {
		qDebug() << SG_PREFIX_W << "Comparing invalid timestamp";
		return false;
	}

	if (!rhs.valid) {
		qDebug() << SG_PREFIX_W << "Comparing invalid timestamp";
		return false;
	}

	return this->value == rhs.value;
}




bool Time::operator!=(const Time & rhs) const
{
	return !(*this == rhs);
}




Time & Time::operator+=(const Time & rhs)
{
	if (!rhs.valid) {
		return *this;
	}

	if (!this->valid) {
		return *this;
	}

	this->value += rhs.value;
	this->valid = !std::isnan(this->value);
	return *this;
}




Time & Time::operator*=(double rhs)
{
	if (this->valid) {
		this->value *= rhs;
		this->valid = !std::isnan(this->value);
		return *this;
	} else {
		return *this;
	}
}




Time & Time::operator/=(double rhs)
{
	if (this->valid) {
		this->value /= rhs;
		this->valid = !std::isnan(this->value);
		return *this;
	} else {
		return *this;
	}
}




double SlavGPS::operator/(const Time & lhs, const Time & rhs)
{
	if (lhs.valid && rhs.valid && !rhs.is_zero()) {
		return lhs.value / rhs.value;
	} else {
		return NAN;
	}
}




Time Time::get_abs_diff(const Time & t1, const Time & t2)
{
	Time result;
	result.value = std::abs(t1.value - t2.value);
	result.valid = true;

	return result;
}




QString Time::strftime_utc(const char * format) const
{
	char timestamp_string[32] = { 0 };
	struct tm tm;
	std::strftime(timestamp_string, sizeof (timestamp_string), format, gmtime_r(&this->value, &tm));

	return QString(timestamp_string);
}




QString Time::strftime_local(const char * format) const
{
	char timestamp_string[32] = { 0 };
	struct tm tm;
	std::strftime(timestamp_string, sizeof (timestamp_string), format, localtime_r(&this->value, &tm));

	return QString(timestamp_string);
}




QString Time::to_timestamp_string(Qt::TimeSpec time_spec) const
{
	QString result;
	if (this->is_valid()) {
		 /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		result = QDateTime::fromTime_t(this->get_value(), time_spec).toString(Qt::ISODate);
	} else {
		result = QObject::tr("No Data");
	}
	return result;
}




QString Time::to_duration_string(void) const
{
	QString result;

	const int seconds = this->value % 60;
	const int minutes = (this->value / 60) % 60;
	const int hours   = (this->value / (60 * 60)) % 60;

	return QObject::tr("%1 h %2 m %3 s").arg(hours).arg(minutes, 2, 10, (QChar) '0').arg(seconds, 2, 10, (QChar) '0');
}




void Time::set_valid(bool new_value)
{
	this->valid = new_value;
}




QString Time::get_time_string(Qt::DateFormat format) const
{
	QDateTime date_time;
	date_time.setTime_t(this->value);
	const QString result = date_time.toString(format);

	return result;
}




/**
   @timestamp - The time of which the string is wanted
   @format - The format of the time string
   @coord - Position of object for the time output (only applicable for format == SGTimeReference::World)

   @return a string of the time according to the time display property
*/
QString Time::get_time_string(Qt::DateFormat format, const Coord & coord) const
{
	QString result;

	if (!this->valid) {
		result = INVALID_RESULT_STRING;
		return result;
	}

	const QTimeZone * tz_from_location = NULL;

	const SGTimeReference ref = Preferences::get_time_ref_frame();
	switch (ref) {
	case SGTimeReference::UTC:
		result = QDateTime::fromTime_t(this->value, QTimeZone::utc()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "UTC: timestamp =" << this->value << "-> time string" << result;
		break;
	case SGTimeReference::World:
		/* No timezone specified so work it out. */
		tz_from_location = TZLookup::get_tz_at_location(coord);
		if (tz_from_location) {
			result = time_string_tz(this->value, format, *tz_from_location);
			qDebug() << SG_PREFIX_D << "World (from location): timestamp =" << this->value << "-> time string" << result;
		} else {
			/* No results (e.g. could be in the middle of a sea).
			   Fallback to simplistic method that doesn't take into account Timezones of countries. */
			const LatLon ll = coord.get_latlon();
			result = time_string_adjusted(this->value, round (ll.lon / 15.0) * 3600);
			qDebug() << SG_PREFIX_D << "World (fallback): timestamp =" << this->value << "-> time string" << result;
		}
		break;
	case SGTimeReference::Locale:
		result = QDateTime::fromTime_t(this->value, QTimeZone::systemTimeZone()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "Locale: timestamp =" << this->value << "-> time string" << result;
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

	if (!this->valid) {
		result = INVALID_RESULT_STRING;
		return result;
	}

	const SGTimeReference ref = Preferences::get_time_ref_frame();
	switch (ref) {
	case SGTimeReference::UTC:
		result = QDateTime::fromTime_t(this->value, QTimeZone::utc()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "UTC: timestamp =" << this->value << "-> time string" << result;
		break;
	case SGTimeReference::World:
		if (tz) {
			/* Use specified timezone. */
			result = time_string_tz(this->value, format, *tz);
			qDebug() << SG_PREFIX_D << "World (from timezone): timestamp =" << this->value << "-> time string" << result;
		} else {
			/* No timezone specified so work it out. */
			QTimeZone const * tz_from_location = TZLookup::get_tz_at_location(coord);
			if (tz_from_location) {
			        result = time_string_tz(this->value, format, *tz_from_location);
				qDebug() << SG_PREFIX_D << "World (from location): timestamp =" << this->value << "-> time string" << result;
			} else {
				/* No results (e.g. could be in the middle of a sea).
				   Fallback to simplistic method that doesn't take into account Timezones of countries. */
				const LatLon ll = coord.get_latlon();
			        result = time_string_adjusted(this->value, round (ll.lon / 15.0) * 3600);
				qDebug() << SG_PREFIX_D << "World (fallback): timestamp =" << this->value << "-> time string" << result;
			}
		}
		break;
	case SGTimeReference::Locale:
		result = QDateTime::fromTime_t(this->value, QTimeZone::systemTimeZone()).toString(format); /* TODO_MAYBE: use fromSecsSinceEpoch() after migrating to Qt 5.8 or later. */
		qDebug() << SG_PREFIX_D << "Locale: timestamp =" << this->value << "-> time string" << result;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected SGTimeReference value" << (int) ref;
		break;
	}

	return result;
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




sg_ret Time::set_from_unix_timestamp(const char * str)
{
	if (NULL == str) {
		qDebug() << SG_PREFIX_E << "Attempting to set invalid value of timestamp from NULL string";
		this->valid = false;
		return sg_ret::err_arg;
	} else {
		return this->set_from_unix_timestamp(QString(str));
	}
}




sg_ret Time::set_from_unix_timestamp(const QString & str)
{
	this->value = (time_t) str.toULong(&this->valid);

	if (!this->valid) {
		qDebug() << SG_PREFIX_W << "Setting invalid value of timestamp from string" << str;
	}

	return this->valid ? sg_ret::ok : sg_ret::err;
}




bool Time::is_zero(void) const
{
	if (!this->valid) {
		return true;
	}
	return this->value == 0;
}




Gradient::Gradient(double new_value)
{
	this->value = new_value;
	this->valid = !std::isnan(new_value);
}




void Gradient::set_value(double new_value)
{
	this->value = new_value;
	this->valid = !std::isnan(new_value);
}




double Gradient::get_value(void) const
{
	return this->value;
}




bool Gradient::is_valid(void) const
{
	return this->valid;
}




const QString Gradient::value_to_string(void) const
{
	QString result;
	if (!this->valid) {
		result = INVALID_RESULT_STRING;
	} else {
		result = QObject::tr("%1").arg(this->value, 0, 'f', SG_PRECISION_GRADIENT);
	}

	return result;
}




QString Gradient::to_string(void) const
{
	QString result;
	if (!this->valid) {
		result = INVALID_RESULT_STRING;
	} else {
		result = QObject::tr("%1%").arg(this->value, 0, 'f', SG_PRECISION_GRADIENT);
	}
	return result;
}




QString Gradient::get_unit_string(void)
{
	return QString("%");
}




QString Gradient::to_string(double value, int precision)
{
	QString result;
	if (std::isnan(value)) {
		result = INVALID_RESULT_STRING;
	} else {
		result = QObject::tr("%1%").arg(value, 0, 'f', SG_PRECISION_GRADIENT);
	}
	return result;
}




bool Gradient::is_zero(void) const
{
	const double epsilon = 0.0000001;
	if (!this->valid) {
		return true;
	}
	return std::abs(this->value) < epsilon;
}
