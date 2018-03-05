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




QString Measurements::get_altitude_string(double value, int precision)
{
	const HeightUnit unit = Preferences::get_unit_height();

	QString buffer;

	switch (unit) {
	case HeightUnit::METRES:
		buffer = QObject::tr("%1 m").arg(value, 0, 'f', precision);
		break;
	case HeightUnit::FEET:
		buffer = QObject::tr("%1 feet").arg(value, 0, 'f', precision);
		break;
	default:
		buffer = "???";
		qDebug() << "EE: Measurements: invalid altitude unit" << (int) unit << "in" << __FUNCTION__ << __LINE__;
	}

	return buffer;
}




QString Measurements::get_altitude_string_recalculate(double value, int precision)
{
	const HeightUnit unit = Preferences::get_unit_height();

	QString buffer;

	switch (unit) {
	case HeightUnit::METRES:
		buffer = QObject::tr("%1 m").arg(value, 0, 'f', precision);
		break;
	case HeightUnit::FEET:
		buffer = QObject::tr("%1 feet").arg(VIK_METERS_TO_FEET(value), 0, 'f', precision);
		break;
	default:
		buffer = "???";
		qDebug() << "EE: Measurements: invalid altitude unit" << (int) unit << "in" << __FUNCTION__ << __LINE__;
	}

	return buffer;
}




QString Measurements::get_distance_string(double value, int precision)
{
	const DistanceUnit unit = Preferences::get_unit_distance();

	QString buffer;

	switch (unit) {
	case DistanceUnit::KILOMETRES:
		buffer = QObject::tr("%1 m").arg(value, 0, 'f', precision);
		break;
	case DistanceUnit::MILES:
		buffer = QObject::tr("%1 miles").arg(value, 0, 'f', precision);
		break;
	case DistanceUnit::NAUTICAL_MILES: /* TODO: verify this case. */
		buffer = QObject::tr("%1 NM").arg(value, 0, 'f', precision);
		break;
	default:
		buffer = "???";
		qDebug() << "EE: Measurements: invalid distance unit" << (int) unit << "in" << __FUNCTION__ << __LINE__;
	}

	return buffer;
}




QString Measurements::get_distance_string_short(double value, int precision)
{
	const DistanceUnit unit = Preferences::get_unit_distance();

	QString buffer;

	switch (unit) {
	case DistanceUnit::KILOMETRES:
		buffer = QObject::tr("%1 m").arg(value, 0, 'f', precision);
		break;
	case DistanceUnit::MILES:
	case DistanceUnit::NAUTICAL_MILES:
		buffer = QObject::tr("%1 yards").arg(value * 1.0936133, 0, 'f', precision); /* Miles -> yards. TODO: verify this calculation. */
		break;
	default:
		buffer = "???";
		qDebug() << "EE: Measurements: invalid distance unit" << (int) unit << "in" << __FUNCTION__ << __LINE__;
	}

	return buffer;
}






QString Measurements::get_speed_string(double value, int precision)
{
	if (std::isnan(value)) {
		return "--";
	}

	const SpeedUnit unit = Preferences::get_unit_speed();

	QString buffer;

	switch (unit) {
	case SpeedUnit::KILOMETRES_PER_HOUR:
		buffer = QObject::tr("%1 km/h").arg(VIK_MPS_TO_KPH (value), 0, 'f', precision);
		break;
	case SpeedUnit::MILES_PER_HOUR:
		buffer = QObject::tr("%1 mph").arg(VIK_MPS_TO_MPH (value), 0, 'f', precision);
		break;
	case SpeedUnit::KNOTS:
		buffer = QObject::tr("%1 knots").arg(VIK_MPS_TO_KNOTS (value), 0, 'f', precision);
		break;
	case SpeedUnit::METRES_PER_SECOND:
		buffer = QObject::tr("%1 m/s").arg(value, 0, 'f', precision);
		break;
	default:
		buffer = "???";
		qDebug() << "EE: Measurements: invalid speed unit" << (int) unit << "in" << __FUNCTION__ << __LINE__;
	}

	return buffer;
}




QString Measurements::get_speed_string_dont_recalculate(double value, int precision)
{
	if (std::isnan(value)) {
		return "--";
	}

	const SpeedUnit unit = Preferences::get_unit_speed();

	QString buffer;

	switch (unit) {
	case SpeedUnit::KILOMETRES_PER_HOUR:
		buffer = QObject::tr("%1 km/h").arg(value, 0, 'f', precision);
		break;
	case SpeedUnit::MILES_PER_HOUR:
		buffer = QObject::tr("%1 mph").arg(value, 0, 'f', precision);
		break;
	case SpeedUnit::KNOTS:
		buffer = QObject::tr("%1 knots").arg(value, 0, 'f', precision);
		break;
	case SpeedUnit::METRES_PER_SECOND:
		buffer = QObject::tr("%1 m/s").arg(value, 0, 'f', precision);
		break;
	default:
		buffer = "???";
		qDebug() << "EE: Measurements: invalid speed unit" << (int) unit << "in" << __FUNCTION__ << __LINE__;
	}

	return buffer;
}





QString Measurements::get_course_string(double value, int precision)
{
	if (std::isnan(value)) {
		return "--";
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
	const int hours = (int) (duration / 3600);
	const int minutes = (int) (((int) round(duration / 60.0)) % 60);

	return QObject::tr("%1 hrs %2 mins").arg(hours).arg(minutes, 2, 10, (QChar) '0');
}
