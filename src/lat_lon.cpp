/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2006-2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#include <cstring>
#include <cstdio>
#include <cstdlib>




#include "lat_lon.h"
#include "coords.h"
#include "measurements.h"
#include "preferences.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "LatLon"

#define SG_MEASUREMENT_DEGREE_CHAR      '\u00B0' /* "Degree" */
#define SG_MEASUREMENT_DEGREE_STRING    "\u00B0" /* "Degree" */
#define SG_MEASUREMENT_ARCMINUTE_CHAR   '\u2032' /* "Prime" */
#define SG_MEASUREMENT_ARCMINUTE_STRING "\u2032" /* "Prime" */
#define SG_MEASUREMENT_ARCSECOND_CHAR   '\u2033' /* "Double prime" */
#define SG_MEASUREMENT_ARCSECOND_STRING "\u2033" /* "Double prime" */



/*
  N79.01234567890123456789°'

  Length of "01234567890123456789" part is arbitrarily large, much
  larger than SG_LATITUDE_PRECISION.
*/
#define DDD_LAT_STRING_REPRESENTATION_LENGTH				\
	(sizeof ("N79.01234567890123456789") - 1) +			\
	(sizeof (SG_MEASUREMENT_DEGREE_STRING) - 1)



/*
  E111.01234567890123456789°'

  Length of "01234567890123456789" part is arbitrarily large, much
  larger than SG_LONGITUDE_PRECISION.
*/
#define DDD_LON_STRING_REPRESENTATION_LENGTH				\
	(sizeof ("E111.01234567890123456789") - 1) +			\
	(sizeof (SG_MEASUREMENT_DEGREE_STRING) - 1)



/*
  N79° 31.01234567890123456789'

  Length of "01234567890123456789" part is arbitrarily large, much
  larger than SG_LATITUDE_PRECISION.

  Notice a space that may be put after degrees for readability.
*/
#define DMM_LAT_STRING_REPRESENTATION_LENGTH				\
	(sizeof ("N79") - 1) +						\
	(sizeof (SG_MEASUREMENT_DEGREE_STRING) - 1) +			\
	(sizeof (" ") - 1) +						\
	(sizeof ("31.01234567890123456789") - 1) +			\
	(sizeof (SG_MEASUREMENT_ARCMINUTE_STRING) - 1)



/*
  E111° 19.01234567890123456789'

  Length of "01234567890123456789" part is arbitrarily large, much
  larger than SG_LONGITUDE_PRECISION.

  Notice a space that may be put after degrees for readability.
*/
#define DMM_LON_STRING_REPRESENTATION_LENGTH				\
	(sizeof ("E111") - 1) +						\
	(sizeof (SG_MEASUREMENT_DEGREE_STRING) - 1) +			\
	(sizeof (" ") - 1) +						\
	(sizeof ("19.01234567890123456789") - 1) +			\
	(sizeof (SG_MEASUREMENT_ARCMINUTE_STRING) - 1)





static void convert_dec_to_ddd(char * str, size_t size, int precision, double dec, char pos_c, char neg_c);
static void convert_dec_to_dmm(char * str, size_t size, double dec, char pos_c, char neg_c);
static void convert_dec_to_dms(char * str, size_t size, double dec, char pos_c, char neg_c);





/**
 * @param pos_c char for positive value
 * @param neg_c char for negative value
 */
void convert_dec_to_ddd(char * str, size_t size, int precision, double dec, char pos_c, char neg_c)
{
	char sign_c = ' ';

	if (dec > 0) {
		sign_c = pos_c;
	} else if (dec < 0) {
		sign_c = neg_c;
	} else { /* Nul value */
		sign_c = ' ';
	}

	/* Degree. */
	const double val_d = std::fabs(dec);

	/* Format. */
	snprintf(str, size, "%c%.*f%s", sign_c,
		 precision,
		 val_d, SG_MEASUREMENT_DEGREE_STRING);
}




void SlavGPS::convert_lat_dec_to_ddd(QString & lat_string, double lat)
{
	char c_str[DDD_LAT_STRING_REPRESENTATION_LENGTH + 1] = { 0 };
	convert_dec_to_ddd(c_str, sizeof (c_str), SG_LATITUDE_PRECISION, lat, 'N', 'S');
	fprintf(stderr, "kamil '%s'\n", c_str);
	lat_string = QString(c_str);
}




void SlavGPS::convert_lon_dec_to_ddd(QString & lon_string, double lon)
{
	char c_str[DDD_LON_STRING_REPRESENTATION_LENGTH + 1] = { 0 };
	convert_dec_to_ddd(c_str, sizeof (c_str), SG_LONGITUDE_PRECISION, lon, 'E', 'W');
	fprintf(stderr, "kamil '%s'\n", c_str);
	lon_string = QString(c_str);
}




/**
 * @param pos_c char for positive value
 * @param neg_c char for negative value
 */
void convert_dec_to_dmm(char * str, size_t size, double dec, char pos_c, char neg_c)
{
	char sign_c = ' ';

	if (dec > 0) {
		sign_c = pos_c;
	} else if (dec < 0) {
		sign_c = neg_c;
	} else { /* Nul value. */
		sign_c = ' ';
	}

	/* Degree. */
	double tmp = std::fabs(dec);
	int val_d = (int) tmp;

	/* Minutes. */
	double val_m = (tmp - val_d) * 60;

	/* Format. */
	snprintf(str, size, "%c%d%s"  "%f%s",
		 sign_c, val_d, SG_MEASUREMENT_DEGREE_STRING,
		 val_m, SG_MEASUREMENT_ARCMINUTE_STRING);
}




void SlavGPS::convert_lat_dec_to_dmm(QString & lat_string, double lat)
{
	char c_str[DMM_LAT_STRING_REPRESENTATION_LENGTH + 1] = { 0 };

	convert_dec_to_dmm(c_str, sizeof (c_str), lat, 'N', 'S');
	lat_string = QString(c_str);
}




void SlavGPS::convert_lon_dec_to_dmm(QString & lon_string, double lon)
{
	char c_str[DMM_LON_STRING_REPRESENTATION_LENGTH] = { 0 };

	convert_dec_to_dmm(c_str, sizeof (c_str), lon, 'E', 'W');
	lon_string = QString(c_str);
}




/**
 * @param pos_c char for positive value
 * @param neg_c char for negative value
 */
static void convert_dec_to_dms(char * str, size_t size, double dec, char pos_c, char neg_c)
{
	char sign_c = ' ';

	if (dec > 0) {
		sign_c = pos_c;
	} else if (dec < 0) {
		sign_c = neg_c;
	} else { /* Nul value */
		sign_c = ' ';
	}

	/* Degree. */
	double tmp = std::fabs(dec);
	int val_d = (int) tmp;

	/* Minutes. */
	tmp = (tmp - val_d) * 60;
	int val_m = (int)tmp;

	/* Seconds. */
	double val_s = (tmp - val_m) * 60;

	/* Format. */
	snprintf(str, size, "%c%d%s"  "%d%s"  "%.4f%s",
		 sign_c, val_d, SG_MEASUREMENT_DEGREE_STRING,
		 val_m, SG_MEASUREMENT_ARCMINUTE_STRING,
		 val_s, SG_MEASUREMENT_ARCSECOND_STRING);
}




void SlavGPS::convert_lat_dec_to_dms(QString & lat_string, double lat)
{
	char c_str[DMM_LAT_STRING_REPRESENTATION_LENGTH + 1] = { 0 };

	convert_dec_to_dms(c_str, sizeof (c_str), lat, 'N', 'S');
	lat_string = QString(c_str);
}




void SlavGPS::convert_lon_dec_to_dms(QString & lon_string, double lon)
{
	char c_str[DMM_LON_STRING_REPRESENTATION_LENGTH + 1] = { 0 };

	convert_dec_to_dms(c_str, sizeof (c_str), lon, 'E', 'W');
	lon_string = QString(c_str);
}




double SlavGPS::convert_dms_to_dec(char const * dms)
{
	double d = 0.0; /* Degree. */
	double m = 0.0; /* Minutes. */
	double s = 0.0; /* Seconds. */
	int neg = false;

	if (dms != NULL) {
		int nbFloat = 0;
		const char * ptr = NULL;
		const char * endptr = NULL;

		/* Compute the sign.
		   It is negative if:
		   - the '-' sign occurs
		   - it is a west longitude or south latitude. */
		if (strpbrk(dms, "-wWsS") != NULL) {
			neg = true;
		}

		/* Peek the different components. */
		endptr = dms;
		do {
			double value;
			ptr = strpbrk(endptr, "0123456789,.");
			if (ptr != NULL) {
				const char * tmpptr = endptr;
				value = strtod(ptr, (char **) &endptr); /* TODO_LATER: remove casting. */
				/* Detect when endptr hasn't changed (which may occur if no conversion took place),
				   particularly if the last character is a ',' or there are multiple '.'s like '5.5.'. */
				if (endptr == tmpptr) {
					break;
				}
				nbFloat++;
				switch (nbFloat) {
				case 1:
					d = value;
					break;
				case 2:
					m = value;
					break;
				case 3:
					s = value;
					break;
				default: break;
				}
			}
		} while (ptr != NULL && endptr != NULL);
	}

	double result = d + m / 60 + s / 3600;
	if (neg) {
		result = -result;
	}

	return result;
}




Latitude::Latitude(const char * str)
{
	if (str) {
		this->set_value(strtod(str, NULL));
	}
}




Latitude::Latitude(const QString & str)
{
	this->set_value(str.toDouble());
}




QString Latitude::to_string(void) const
{
	QString lat_string;
	const DegreeFormat format = Preferences::get_degree_format();

	switch (format) {
	case DegreeFormat::DDD:
		convert_lat_dec_to_ddd(lat_string, this->m_value);
		break;
	case DegreeFormat::DMM:
		convert_lat_dec_to_dmm(lat_string, this->m_value);
		break;
	case DegreeFormat::DMS:
		convert_lat_dec_to_dms(lat_string, this->m_value);
		break;
	case DegreeFormat::Raw:
		LatLon::lat_to_string_raw(lat_string, *this);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unknown degree format %d" << (int) format;
		break;
	}

	return lat_string;
}




const QString Latitude::value_to_string_for_file(void) const
{
	static QLocale c_locale = QLocale::c();
	return c_locale.toString(this->m_value, 'f', SG_LATITUDE_PRECISION);
}




bool Latitude::set_value(double new_value)
{
	this->m_value = new_value;
	this->validate();
	return this->m_valid;
}




double Latitude::value(void) const
{
	return this->m_value;
}




bool Latitude::is_valid(void) const
{
	return this->m_valid;
}




void Latitude::invalidate(void)
{
	this->m_value = NAN;
	this->m_valid = false;
}




bool Latitude::validate(void)
{
	if (std::isnan(this->m_value)) {
		this->m_valid = false;
	} else if (this->m_value > SG_LATITUDE_MAX) {
		this->m_valid = false;
	} else if (this->m_value < SG_LATITUDE_MIN) {
		this->m_valid = false;
	} else {
		this->m_valid = true;
	}
	return this->m_valid;
}




bool Latitude::operator==(const Latitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return false;
	}
	return this->m_value == rhs.m_value;
}




bool Latitude::operator!=(const Latitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return true;
	}
	return this->m_value != rhs.m_value;
}




Latitude & Latitude::operator=(const Latitude & rhs)
{
	this->m_valid = rhs.m_valid;
	this->m_value = rhs.m_value;

	return *this;
}




bool Latitude::operator<(const Latitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return false;
	}
	return this->m_value < rhs.m_value;
}




bool Latitude::operator>(const Latitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return false;
	}
	return this->m_value > rhs.m_value;
}





bool Latitude::operator<=(const Latitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return false;
	}
	return this->m_value <= rhs.m_value;
}





bool Latitude::operator>=(const Latitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return false;
	}
	return this->m_value >= rhs.m_value;
}




Latitude & Latitude::operator+=(double rhs)
{
	if (!this->m_valid) {
		qDebug() << SG_PREFIX_W << "Invalid 'this' operand";
		return *this;
	}
	if (std::isnan(rhs)) {
		qDebug() << SG_PREFIX_W << "Invalid 'rhs' operand";
		return *this;
	}

	this->m_value += rhs;
	this->validate();
	return *this;
}




Latitude & Latitude::operator-=(double rhs)
{
	if (!this->m_valid) {
		qDebug() << SG_PREFIX_W << "Invalid 'this' operand";
		return *this;
	}
	if (std::isnan(rhs)) {
		qDebug() << SG_PREFIX_W << "Invalid 'rhs' operand";
		return *this;
	}

	this->m_value -= rhs;
	this->validate();
	return *this;
}




QDebug SlavGPS::operator<<(QDebug debug, const Latitude & lat)
{
	debug << (lat.is_valid() ? "valid" : "invalid") << ", " << lat.value();
	return debug;
}




Longitude::Longitude(const QString & str)
{
	this->set_value(str.toDouble());
}




Longitude::Longitude(const char * str)
{
	if (str) {
		this->set_value(strtod(str, NULL));
	}
}




QString Longitude::to_string(void) const
{
	QString lon_string;
	const DegreeFormat format = Preferences::get_degree_format();

	switch (format) {
	case DegreeFormat::DDD:
		convert_lon_dec_to_ddd(lon_string, this->m_unbound_value);
		break;
	case DegreeFormat::DMM:
		convert_lon_dec_to_dmm(lon_string, this->m_unbound_value);
		break;
	case DegreeFormat::DMS:
		convert_lon_dec_to_dms(lon_string, this->m_unbound_value);
		break;
	case DegreeFormat::Raw:
		LatLon::lon_to_string_raw(lon_string, *this);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unknown degree format %d" << (int) format;
		break;
	}

	return lon_string;
}




const QString Longitude::value_to_string_for_file(void) const
{
	static QLocale c_locale = QLocale::c();
	return c_locale.toString(this->m_unbound_value, 'f', SG_LONGITUDE_PRECISION);
}




bool Longitude::set_value(double new_value)
{
	this->m_unbound_value = new_value;
	this->validate();

	return this->m_valid;
}




double Longitude::unbound_value(void) const
{
	return this->m_unbound_value;
}




double Longitude::bound_value(void) const
{
	/* TODO_LATER: verify this calculation. */

	double tmp = this->m_unbound_value;
	if (tmp > 0) {
		while (tmp > SG_LONGITUDE_BOUND_MAX) {
			tmp -= 360.0;
		}
	} else {
		while (tmp < SG_LONGITUDE_BOUND_MIN) {
			tmp += 360.0;
		}
	}
	return tmp;
}




bool Longitude::is_valid(void) const
{
	return this->m_valid;
}




void Longitude::invalidate(void)
{
	this->m_unbound_value = NAN;
	this->m_valid = false;
}




bool Longitude::validate(void)
{
	if (std::isnan(this->m_unbound_value)) {
		this->m_valid = false;
	} else if (this->m_unbound_value > SG_LONGITUDE_UNBOUND_MAX) {
		this->m_valid = false;
	} else if (this->m_unbound_value < SG_LONGITUDE_UNBOUND_MIN) {
		this->m_valid = false;
	} else {
		this->m_valid = true;
	}
	return this->m_valid;
}




bool Longitude::operator==(const Longitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return false;
	}
	return this->m_unbound_value == rhs.m_unbound_value;
}




bool Longitude::operator!=(const Longitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return true;
	}
	return this->m_unbound_value != rhs.m_unbound_value;
}




Longitude & Longitude::operator=(const Longitude & rhs)
{
	this->m_valid = rhs.m_valid;
	this->m_unbound_value = rhs.m_unbound_value;

	return *this;
}




bool Longitude::operator<(const Longitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return false;
	}
	return this->m_unbound_value < rhs.m_unbound_value;
}




bool Longitude::operator>(const Longitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return false;
	}
	return this->m_unbound_value > rhs.m_unbound_value;
}





bool Longitude::operator<=(const Longitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return false;
	}
	return this->m_unbound_value <= rhs.m_unbound_value;
}





bool Longitude::operator>=(const Longitude & rhs) const
{
	if (!this->m_valid || !rhs.m_valid) {
		return false;
	}
	return this->m_unbound_value >= rhs.m_unbound_value;
}




Longitude & Longitude::operator+=(double rhs)
{
	if (!this->m_valid) {
		qDebug() << SG_PREFIX_W << "Invalid 'this' operand";
		return *this;
	}
	if (std::isnan(rhs)) {
		qDebug() << SG_PREFIX_W << "Invalid 'rhs' operand";
		return *this;
	}

	this->m_unbound_value += rhs;
	this->validate();
	return *this;
}




Longitude & Longitude::operator-=(double rhs)
{
	if (!this->m_valid) {
		qDebug() << SG_PREFIX_W << "Invalid 'this' operand";
		return *this;
	}
	if (std::isnan(rhs)) {
		qDebug() << SG_PREFIX_W << "Invalid 'rhs' operand";
		return *this;
	}

	this->m_unbound_value -= rhs;
	this->validate();
	return *this;
}




QDebug SlavGPS::operator<<(QDebug debug, const Longitude & lon)
{
	debug << (lon.is_valid() ? "valid" : "invalid") << ", " << lon.unbound_value() << "/" << lon.bound_value();
	return debug;
}
