/*
coords.c
borrowed from:
http://acme.com/software/coords/
I (Evan Battaglia <viking@greentorch.org>) have only made some small changes such as
renaming functions and defining LatLon and UTM structs.
2004-02-10 -- I also added a function of my own -- UTM::get_distance() (a_coords_utm_diff()) -- that I felt belonged in coords.c
2004-02-21 -- I also added UTM::is_equal().

*/
/* coords.h - include file for coords routines
**
** Copyright © 2001 by Jef Poskanzer <jef@acme.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
*/




#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>




#include <QLocale>
#include <QDebug>




#include "measurements.h"
#include "coords.h"
#include "preferences.h"
#include "lat_lon.h"
#include "bbox.h"




using namespace SlavGPS;




#define SG_MODULE "Coords"

/* Number of UTM zones */
#define UTM_ZONES 60




static const char * utm_letters = "ABCDEFGH" "JKLM"  "N" "PQRSTUVWXYZ";
/* UTM "none band" symbol shall be a single letter, just like symbols for valid UTM bands. */
static const char utm_none_band_symbol = '-';



bool UTM::is_band_letter(char letter)
{
	const int c = toupper(letter);
	for (size_t i = 0; i < strlen(utm_letters); i++) {
		if (c == utm_letters[i]) {
			return true;
		}
	}

	return false;
}




bool UTM::is_band_letter(UTMLetter letter)
{
	return UTM::is_band_letter((char) letter);
}




LatLon::LatLon(const Latitude & latitude, const Longitude & longitude)
{
	this->lat = latitude.get_value();
	this->lon = longitude.get_value();
}




void LatLon::lat_to_string_raw(QString & lat_string, const LatLon & lat_lon)
{
	static QLocale c_locale = QLocale::c();
	lat_string = c_locale.toString(lat_lon.lat, 'f', SG_LATITUDE_PRECISION);
}




void LatLon::lon_to_string_raw(QString & lon_string, const LatLon & lat_lon)
{
	static QLocale c_locale = QLocale::c();
	lon_string = c_locale.toString(lat_lon.lon, 'f', SG_LONGITUDE_PRECISION);
}




QString LatLon::to_string(void) const
{
	static QLocale c_locale = QLocale::c();
	return QString("%1,%2")
		.arg(c_locale.toString(this->lat, 'f', SG_LATITUDE_PRECISION))
		.arg(c_locale.toString(this->lon, 'f', SG_LONGITUDE_PRECISION));
}




/*
  \brief Convert values from LatLon class to a pair of strings in C locale

  Strings will have a non-localized, regular dot as a separator
  between integer part and fractional part.
*/
void LatLon::to_strings_raw(QString & lat_string, QString & lon_string) const
{
	static QLocale c_locale = QLocale::c();

	lat_string = c_locale.toString(this->lat, 'f', SG_LATITUDE_PRECISION);
	lon_string = c_locale.toString(this->lon, 'f', SG_LONGITUDE_PRECISION);

	return;
}




QDebug SlavGPS::operator<<(QDebug debug, const LatLon & lat_lon)
{
	debug << lat_lon.to_string();
	return debug;
}




#define PIOVER180 0.01745329252

#define K0 0.9996

/* WGS-84 */
#define EquatorialRadius 6378137
#define EccentricitySquared 0.00669438

static UTMLetter coords_utm_band_letter(double latitude);




sg_ret UTM::set_northing(double value)
{
	this->northing = value;
	return sg_ret::ok;
}




sg_ret UTM::set_easting(double value)
{
	this->easting = value;
	return sg_ret::ok;
}




sg_ret UTM::set_zone(int value)
{
	if (value <= 0 || value > UTM_ZONES) {
		qDebug() << SG_PREFIX_E << "Invalid UTM zone" << value;
		return sg_ret::err;
	}

	this->zone = value;
	return sg_ret::ok;
}




UTMLetter UTM::get_band_letter(void) const
{
	return this->band_letter;
}




char UTM::get_band_as_letter(void) const
{
	return (char) this->band_letter;
}




sg_ret UTM::set_band_letter(UTMLetter letter)
{
	if (!is_band_letter((char) letter)) {
		qDebug() << SG_PREFIX_E << "Invalid utm band letter/decimal" << (char) letter;
		return sg_ret::err;
	}
	this->band_letter = letter;
	return sg_ret::ok;
}




sg_ret UTM::set_band_letter(char letter)
{
	if (!is_band_letter(letter)) {
		qDebug() << SG_PREFIX_E << "Invalid utm band letter/decimal" << letter;
		return sg_ret::err;
	}
	this->band_letter = (UTMLetter) letter;
	return sg_ret::ok;
}




QStringList UTM::get_band_symbols(void)
{
	static QStringList symbols;
	if (symbols.size() == 0) {
		int i = 0;
		while ('\0' != utm_letters[i]) {
			symbols << QString(utm_letters[i]);
			i++;
		}
		symbols << QString(utm_none_band_symbol);
	}

	return symbols;
}




bool UTM::is_equal(const UTM & utm1, const UTM & utm2)
{
	return (utm1.easting == utm2.easting && utm1.northing == utm2.northing && utm1.zone == utm2.zone);
}




QString UTM::to_string(void) const
{
	const QString result = QString("N = %1, E = %2, Zone = %3, Band Letter = %4")
		.arg(this->northing, 0, 'f', 4)
		.arg(this->easting, 0, 'f', 4)
		.arg(this->zone)
		.arg((char) this->band_letter);

	return result;
}




bool UTM::is_northern_hemisphere(const UTM & utm)
{
	return utm.band_letter >= UTMLetter::N;
}




QDebug SlavGPS::operator<<(QDebug debug, const UTM & utm)
{
	debug << utm.to_string();
	return debug;
}




double UTM::get_distance(const UTM & utm1, const UTM & utm2)
{
	if (utm1.zone == utm2.zone) {
		return sqrt(pow(utm1.easting - utm2.easting, 2) + pow(utm1.northing - utm2.northing, 2));
	} else {
		const LatLon tmp1 = UTM::to_latlon(utm1);
		const LatLon tmp2 = UTM::to_latlon(utm2);
		return LatLon::get_distance(tmp1, tmp2);
	}
}




double LatLon::get_distance(const LatLon & lat_lon_1, const LatLon & lat_lon_2)
{
	const double lat1 = lat_lon_1.lat * PIOVER180;
	const double lon1 = lat_lon_1.lon * PIOVER180;
	const double lat2 = lat_lon_2.lat * PIOVER180;
	const double lon2 = lat_lon_2.lon * PIOVER180;

	const double tmp = EquatorialRadius * acos(sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(lon1 - lon2));

	/* For very small differences we can sometimes get NaN returned. */
	return std::isnan(tmp) ? 0 : tmp;
}




UTM LatLon::to_utm(const LatLon & lat_lon)
{
	double latitude = lat_lon.lat;
	double longitude = lat_lon.lon;

	/* We want the longitude within SG_LONGITUDE_MIN..SG_LONGITUDE_MAX. */
	if (longitude < SG_LONGITUDE_MIN) {
		longitude += 360.0;
	}
	if (longitude > SG_LONGITUDE_MAX) {
		longitude -= 360.0;
	}

	/* Now convert. */
	const double lat_rad = DEG2RAD(latitude);
	const double long_rad = DEG2RAD(longitude);
	int zone = (int) ((longitude + 180) / 6) + 1;
	qDebug() << "---- lon -> zone" << longitude << zone;
	if (latitude >= 56.0 && latitude < 64.0 && longitude >= 3.0 && longitude < 12.0) {
		zone = 32;
	}

	/* Special zones for Svalbard. */
	if (latitude >= 72.0 && latitude < 84.0) {
		if      (longitude >= 0.0  && longitude <  9.0) zone = 31;
		else if (longitude >= 9.0  && longitude < 21.0) zone = 33;
		else if (longitude >= 21.0 && longitude < 33.0) zone = 35;
		else if (longitude >= 33.0 && longitude < 42.0) zone = 37;
	}

	const double long_origin = (zone - 1) * 6 - 180 + 3; /* +3 puts origin in middle of zone */
	const double long_origin_rad = DEG2RAD(long_origin);
	const double eccPrimeSquared = EccentricitySquared / (1.0 - EccentricitySquared);
	const double N = EquatorialRadius / sqrt(1.0 - EccentricitySquared * sin(lat_rad) * sin(lat_rad));
	const double T = tan(lat_rad ) * tan( lat_rad);
	const double C = eccPrimeSquared * cos(lat_rad) * cos(lat_rad);
	const double A = cos(lat_rad) * (long_rad - long_origin_rad);
	const double M = EquatorialRadius * ((1.0 - EccentricitySquared / 4 - 3 * EccentricitySquared * EccentricitySquared / 64 - 5 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 256) * lat_rad - (3 * EccentricitySquared / 8 + 3 * EccentricitySquared * EccentricitySquared / 32 + 45 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 1024) * sin(2 * lat_rad) + (15 * EccentricitySquared * EccentricitySquared / 256 + 45 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 1024) * sin(4 * lat_rad) - (35 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 3072) * sin(6 * lat_rad));
	double easting = K0 * N * (A + (1 - T + C) * A * A * A / 6 + (5 - 18 * T + T * T + 72 * C - 58 * eccPrimeSquared) * A * A * A * A * A / 120) + 500000.0;
	double northing = K0 * (M + N * tan(lat_rad) * (A * A / 2 + (5 - T + 9 * C + 4 * C * C) * A * A * A * A / 24 + (61 - 58 * T + T * T + 600 * C - 330 * eccPrimeSquared) * A * A * A * A * A * A / 720));
	if (latitude < 0.0) {
		northing += 10000000.0;  /* 1e7 meter offset for southern hemisphere */
	}

	/* All done. */

	UTM utm;
	utm.northing = northing;
	utm.easting = easting;
	utm.set_zone(zone);
	utm.set_band_letter(coords_utm_band_letter(latitude)); /* We don't check result of set_band_letter() here, we assume that the letter has been calculated by coords_utm_band_letter() correctly. */
	return utm;
}




static UTMLetter coords_utm_band_letter(double latitude)
{
	/*
	  This routine determines the correct UTM band letter
	  designator for the given latitude.  It returns 'Z' if the
	  latitude is outside the UTM limits of 84N to 80S.
	*/
	if (latitude <= 84.0      && latitude >=  72.0) return UTMLetter::X;
	else if (latitude <  72.0 && latitude >=  64.0) return UTMLetter::W;
	else if (latitude <  64.0 && latitude >=  56.0) return UTMLetter::V;
	else if (latitude <  56.0 && latitude >=  48.0) return UTMLetter::U;
	else if (latitude <  48.0 && latitude >=  40.0) return UTMLetter::T;
	else if (latitude <  40.0 && latitude >=  32.0) return UTMLetter::S;
	else if (latitude <  32.0 && latitude >=  24.0) return UTMLetter::R;
	else if (latitude <  24.0 && latitude >=  16.0) return UTMLetter::Q;
	else if (latitude <  16.0 && latitude >=   8.0) return UTMLetter::P;
	else if (latitude <   8.0 && latitude >=   0.0) return UTMLetter::N;
	else if (latitude <   0.0 && latitude >=  -8.0) return UTMLetter::M;
	else if (latitude <  -8.0 && latitude >= -16.0) return UTMLetter::L;
	else if (latitude < -16.0 && latitude >= -24.0) return UTMLetter::K;
	else if (latitude < -24.0 && latitude >= -32.0) return UTMLetter::J;
	else if (latitude < -32.0 && latitude >= -40.0) return UTMLetter::H;
	else if (latitude < -40.0 && latitude >= -48.0) return UTMLetter::G;
	else if (latitude < -48.0 && latitude >= -56.0) return UTMLetter::F;
	else if (latitude < -56.0 && latitude >= -64.0) return UTMLetter::E;
	else if (latitude < -64.0 && latitude >= -72.0) return UTMLetter::D;
	else if (latitude < -72.0 && latitude >= -80.0) return UTMLetter::C;
	else return UTMLetter::Z;
}




LatLon UTM::to_latlon(const UTM & utm)
{
	double x = utm.easting - 500000.0; /* remove 500000 meter offset */
	double y = utm.northing;
	assert (utm.band_letter >= UTMLetter::A && utm.band_letter <= UTMLetter::Z);
	if (utm.band_letter < UTMLetter::N) {
		/* southern hemisphere */
		y -= 10000000.0; /* remove 1e7 meter offset */
	}

	const double long_origin = (utm.zone - 1) * 6 - 180 + 3;	/* +3 puts origin in middle of zone */
	const double eccPrimeSquared = EccentricitySquared / (1.0 - EccentricitySquared);
	const double e1 = (1.0 - sqrt(1.0 - EccentricitySquared)) / (1.0 + sqrt(1.0 - EccentricitySquared));
	const double M = y / K0;
	const double mu = M / (EquatorialRadius * (1.0 - EccentricitySquared / 4 - 3 * EccentricitySquared * EccentricitySquared / 64 - 5 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 256));
	const double phi1_rad = mu + (3 * e1 / 2 - 27 * e1 * e1 * e1 / 32)* sin(2 * mu) + (21 * e1 * e1 / 16 - 55 * e1 * e1 * e1 * e1 / 32) * sin(4 * mu) + (151 * e1 * e1 * e1 / 96) * sin(6 *mu);
	const double N1 = EquatorialRadius / sqrt(1.0 - EccentricitySquared * sin(phi1_rad) * sin(phi1_rad));
	const double T1 = tan(phi1_rad) * tan(phi1_rad);
	const double C1 = eccPrimeSquared * cos(phi1_rad) * cos(phi1_rad);
	const double R1 = EquatorialRadius * (1.0 - EccentricitySquared) / pow(1.0 - EccentricitySquared * sin(phi1_rad) * sin(phi1_rad), 1.5);
	const double D = x / (N1 * K0);

	double latitude = phi1_rad - (N1 * tan(phi1_rad) / R1) * (D * D / 2 -(5 + 3 * T1 + 10 * C1 - 4 * C1 * C1 - 9 * eccPrimeSquared) * D * D * D * D / 24 + (61 + 90 * T1 + 298 * C1 + 45 * T1 * T1 - 252 * eccPrimeSquared - 3 * C1 * C1) * D * D * D * D * D * D / 720);
	latitude = RAD2DEG(latitude);
	double longitude = (D - (1 + 2 * T1 + C1) * D * D * D / 6 + (5 - 2 * C1 + 28 * T1 - 3 * C1 * C1 + 8 * eccPrimeSquared + 24 * T1 * T1) * D * D * D * D * D / 120) / cos(phi1_rad);
	longitude = long_origin + RAD2DEG(longitude);

	return LatLon(latitude, longitude);
}




void LatLon::to_strings(const LatLon & lat_lon, QString & lat_string, QString & lon_string)
{
	DegreeFormat format = Preferences::get_degree_format();

	switch (format) {
	case DegreeFormat::DDD:
		convert_lat_dec_to_ddd(lat_string, lat_lon.lat);
		convert_lon_dec_to_ddd(lon_string, lat_lon.lon);
		break;
	case DegreeFormat::DMM:
		convert_lat_dec_to_dmm(lat_string, lat_lon.lat);
		convert_lon_dec_to_dmm(lon_string, lat_lon.lon);
		break;
	case DegreeFormat::DMS:
		convert_lat_dec_to_dms(lat_string, lat_lon.lat);
		convert_lon_dec_to_dms(lon_string, lat_lon.lon);
		break;
	case DegreeFormat::Raw:
		LatLon::lat_to_string_raw(lat_string, lat_lon);
		LatLon::lon_to_string_raw(lon_string, lat_lon);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unknown degree format %d" << (int) format;
		break;
	}
}




LatLon LatLon::get_interpolated(const LatLon & lat_lon_1, const LatLon & lat_lon_2, double scale)
{
	LatLon result;

	result.lat = lat_lon_1.lat + ((lat_lon_2.lat - lat_lon_1.lat) * scale);

	/* FIXME: This won't cope with going over the 180 degrees longitude boundary. */
	result.lon = lat_lon_1.lon + ((lat_lon_2.lon - lat_lon_1.lon) * scale);

	return result;
}




static bool lat_lon_close_enough(const LatLon & lat_lon1, const LatLon & lat_lon2)
{
	const double epsilon = 0.0000001;

	if (std::fabs(lat_lon1.lat - lat_lon2.lat) > epsilon) {
		return false;
	}

	if (std::fabs(lat_lon1.lon - lat_lon2.lon) > epsilon) {
		return false;
	}

	return true;
}




bool UTM::close_enough(const UTM & utm1, const UTM & utm2)
{
	const double epsilon = 0.1;

	if (std::fabs(utm1.northing - utm2.northing) > epsilon) {
		qDebug() << SG_PREFIX_E << "Northing error:" << utm1.northing << utm2.northing;
		return false;
	}

	if (std::fabs(utm1.easting - utm2.easting) > epsilon) {
		qDebug() << SG_PREFIX_E << "Easting error:" << utm1.easting << utm2.easting;
		return false;
	}

	if (utm1.zone != utm2.zone) {
		qDebug() << SG_PREFIX_E << "Zone error:" << utm1.zone << utm2.zone;
		return false;
	}

	if (utm1.get_band_letter() != utm2.get_band_letter()) {
		qDebug() << SG_PREFIX_E << "Band letter error:" << utm1.get_band_as_letter() << utm2.get_band_as_letter();
		return false;
	}

	return true;
}




bool UTM::is_the_same_zone(const UTM & utm1, const UTM & utm2)
{
	return utm1.zone == utm2.zone;
}




sg_ret UTM::shift_zone_by(int shift)
{
	this->zone += shift; /* TODO: wrap result to allowable range. */
	return sg_ret::ok;
}




bool Coords::unit_tests(void)
{
	/* LatLon -> UTM -> LatLon */
	{
		const LatLon lat_lon_in(34.123456, 12.654321);
		const UTM utm = LatLon::to_utm(lat_lon_in);
		const LatLon lat_lon_out = UTM::to_latlon(utm);

		qDebug() << SG_PREFIX_D << lat_lon_in << "->" << utm << "->" << lat_lon_out << "->" << lat_lon_close_enough(lat_lon_in, lat_lon_out);
		assert (lat_lon_close_enough(lat_lon_in, lat_lon_out));
	}



	/* UTM -> LatLon -> UTM */
	{
		UTM utm_in;
		utm_in.northing = 3778331;
		utm_in.easting = 283673;
		utm_in.set_zone(33);
		utm_in.set_band_letter(UTMLetter::S);
		const LatLon lat_lon = UTM::to_latlon(utm_in);
		const UTM utm_out = LatLon::to_utm(lat_lon);

		qDebug() << SG_PREFIX_D << utm_in << "->" << lat_lon << "->" << utm_out << "->" << UTM::close_enough(utm_in, utm_out);
		assert (UTM::close_enough(utm_in, utm_out));

		//N = , Z = 33, L = S"
	}


	return true;
}
