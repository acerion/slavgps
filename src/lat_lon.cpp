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
#include "measurements.h"




using namespace SlavGPS;




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
	const double val_d = fabs(dec);

	/* Format. */
	snprintf(str, size, "%c%.*f" DEGREE_SYMBOL, sign_c, precision, val_d);
}




void SlavGPS::convert_lat_dec_to_ddd(QString & lat_string, double lat)
{
	/* 1 sign character + 2 digits + 1 dot + N fractional digits + terminating NUL. */
	char c_str[1 + 2 + 1 + SG_LATITUDE_PRECISION + 1] = { 0 };

	convert_dec_to_ddd(c_str, sizeof (c_str), SG_LATITUDE_PRECISION, lat, 'N', 'S');
	lat_string = QString(c_str);
}




void SlavGPS::convert_lon_dec_to_ddd(QString & lon_string, double lon)
{
	/* 1 sign character + 3 digits + 1 dot + N fractional digits + terminating NUL. */
	char c_str[1 + 3 + 1 + SG_LONGITUDE_PRECISION + 1] = { 0 };
	convert_dec_to_ddd(c_str, sizeof (c_str), SG_LONGITUDE_PRECISION, lon, 'E', 'W');
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
	double tmp = fabs(dec);
	int val_d = (int) tmp;

	/* Minutes. */
	double val_m = (tmp - val_d) * 60;

	/* Format. */
	snprintf(str, size, "%c%d" DEGREE_SYMBOL "%f'", sign_c, val_d, val_m);
}




void SlavGPS::convert_lat_dec_to_dmm(QString & lat_string, double lat)
{
	char c_str[64] = { 0 }; /* TODO_LATER: calculate the size of buffer in some reasonable way. */

	convert_dec_to_dmm(c_str, sizeof (c_str), lat, 'N', 'S');
	lat_string = QString(c_str);
}




void SlavGPS::convert_lon_dec_to_dmm(QString & lon_string, double lon)
{
	char c_str[64] = { 0 }; /* TODO_LATER: calculate the size of buffer in some reasonable way. */

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
	double tmp = fabs(dec);
	int val_d = (int) tmp;

	/* Minutes. */
	tmp = (tmp - val_d) * 60;
	int val_m = (int)tmp;

	/* Seconds. */
	double val_s = (tmp - val_m) * 60;

	/* Format. */
	snprintf(str, size, "%c%d" DEGREE_SYMBOL "%d'%.4f\"", sign_c, val_d, val_m, val_s);
}




void SlavGPS::convert_lat_dec_to_dms(QString & lat_string, double lat)
{
	char c_str[64] = { 0 }; /* TODO_LATER: calculate the size of buffer in some reasonable way. */

	convert_dec_to_dms(c_str, sizeof (c_str), lat, 'N', 'S');
	lat_string = QString(c_str);
}




void SlavGPS::convert_lon_dec_to_dms(QString & lon_string, double lon)
{
	char c_str[64] = { 0 }; /* TODO_LATER: calculate the size of buffer in some reasonable way. */

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
				value = strtod((const char *) ptr, (char **) &endptr); /* TODO_LATER: remove casting. */
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
	static QLocale c_locale = QLocale::c();
	return c_locale.toString(this->value, 'f', SG_LATITUDE_PRECISION);
}




const QString Latitude::value_to_string_for_file(void) const
{
	static QLocale c_locale = QLocale::c();
	return c_locale.toString(this->value, 'f', SG_LATITUDE_PRECISION);
}




bool Latitude::set_value(double new_value)
{
	if (std::isnan(new_value)) {
		this->value = NAN;
	} else if (new_value < SG_LATITUDE_MIN || new_value > SG_LATITUDE_MAX) {
		this->value = NAN;
	} else {
		this->value = new_value;
	}

	this->valid = !std::isnan(new_value);

	return this->valid;
}




double Latitude::get_value(void) const
{
	return this->value;
}




bool Latitude::is_valid(void) const
{
	return this->valid;
}




void Latitude::invalidate(void)
{
	this->value = NAN;
	this->valid = false;
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
	static QLocale c_locale = QLocale::c();
	return c_locale.toString(this->value, 'f', SG_LONGITUDE_PRECISION);
}




const QString Longitude::value_to_string_for_file(void) const
{
	static QLocale c_locale = QLocale::c();
	return c_locale.toString(this->value, 'f', SG_LONGITUDE_PRECISION);
}




bool Longitude::set_value(double new_value)
{
	if (std::isnan(new_value)) {
		this->value = NAN;
	} else if (new_value < SG_LONGITUDE_MIN || new_value > SG_LONGITUDE_MAX) {
		this->value = NAN;
	} else {
		this->value = new_value;
	}

	this->valid = !std::isnan(new_value);

	return this->valid;
}




double Longitude::get_value(void) const
{
	return this->value;
}




bool Longitude::is_valid(void) const
{
	return this->valid;
}




void Longitude::invalidate(void)
{
	this->value = NAN;
	this->valid = false;
}
