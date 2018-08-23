/*
coords.c
borrowed from:
http://acme.com/software/coords/
I (Evan Battaglia <viking@greentorch.org>) have only made some small changes such as
renaming functions and defining LatLon and UTM structs.
2004-02-10 -- I also added a function of my own -- a_coords_utm_diff() -- that I felt belonged in coords.c
2004-02-21 -- I also added UTM::is_equal().

*/
/* coords.h - include file for coords routines
**
** Copyright � 2001 by Jef Poskanzer <jef@acme.com>.
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>

#include <QLocale>
#include <QDebug>

#include <glib.h>

#include "measurements.h"
#include "coords.h"
#ifdef HAVE_VIKING
#include "preferences.h"
#else
#define DEG2RAD(x) ((x)*(M_PI/180))
#define RAD2DEG(x) ((x)*(180/M_PI))
#endif
#include "degrees_converters.h"
#include "bbox.h"




using namespace SlavGPS;




#define PREFIX " Coords"




QString LatLon::lat_to_string_raw(const LatLon & lat_lon)
{
	static QLocale c_locale = QLocale::c();
	return c_locale.toString(lat_lon.lat, 'f', SG_PRECISION_LATITUDE);
}




QString LatLon::lon_to_string_raw(const LatLon & lat_lon)
{
	static QLocale c_locale = QLocale::c();
	return c_locale.toString(lat_lon.lon, 'f', SG_PRECISION_LONGITUDE);
}




QString LatLon::to_string(void) const
{
	static QLocale c_locale = QLocale::c();
	return QString("%1,%2")
		.arg(c_locale.toString(this->lat, 'f', SG_PRECISION_LATITUDE))
		.arg(c_locale.toString(this->lon, 'f', SG_PRECISION_LONGITUDE));
}




/*
  \brief Convert values from LatLon class to a pair of strings in C locale

  Strings will have a non-localized, regular dot as a separator
  between integer part and fractional part.
*/
void LatLon::to_strings_raw(QString & lat_string, QString & lon_string) const
{
	static QLocale c_locale = QLocale::c();

	lat_string = c_locale.toString(this->lat, 'f', SG_PRECISION_LATITUDE);
	lon_string = c_locale.toString(this->lon, 'f', SG_PRECISION_LONGITUDE);

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

static char coords_utm_band_letter(double latitude);




UTM::UTM(const QString & northing_string, const QString & easting_string, int zone_value, char new_band_letter)
{
	/* TODO_REALLY: revisit data types (double or int?) for northing/easting. */
	/* TODO_REALLY: add error checking. */
	this->northing = northing_string.toDouble();
	this->easting  = easting_string.toDouble();
	this->zone = zone_value;
	this->band_letter = new_band_letter; // _string.at(0).toUpper().toLatin1();

	qDebug() << "II:" PREFIX << "UTM northing conversion"      << northing_string << "->" << this->northing;
	qDebug() << "II:" PREFIX << "UTM easting conversion"       << easting_string  << "->" << this->easting;
	qDebug() << "II:" PREFIX << "UTM zone conversion"          << zone_value      << "->" << this->zone;
	qDebug() << "II:" PREFIX << "UTM band letter conversion"   << new_band_letter << "->" << this->band_letter;
}




bool UTM::is_equal(const UTM & utm1, const UTM & utm2)
{
	return (utm1.easting == utm2.easting && utm1.northing == utm2.northing && utm1.zone == utm2.zone);
}




QString UTM::to_string(void) const
{
	const QString result = QString("N = %1, E = %1, Z = %3, L = %4")
		.arg(this->northing)
		.arg(this->easting)
		.arg(this->zone)
		.arg(this->band_letter);

	return result;
}




QDebug SlavGPS::operator<<(QDebug debug, const UTM & utm)
{
	debug << utm.to_string();
	return debug;
}




double a_coords_utm_diff(const UTM * utm1, const UTM * utm2)
{
  static LatLon tmp1, tmp2;
  if (utm1->zone == utm2->zone) {
    return sqrt ( pow ( utm1->easting - utm2->easting, 2 ) + pow ( utm1->northing - utm2->northing, 2 ) );
  } else {
	  tmp1 = UTM::to_latlon(*utm1);
	  tmp2 = UTM::to_latlon(*utm2);
    return a_coords_latlon_diff(tmp1, tmp2);
  }
}

double a_coords_latlon_diff(const LatLon & lat_lon_1, const LatLon & lat_lon_2)
{
  static LatLon tmp1, tmp2;
  double tmp3;
  tmp1.lat = lat_lon_1.lat * PIOVER180;
  tmp1.lon = lat_lon_1.lon * PIOVER180;
  tmp2.lat = lat_lon_2.lat * PIOVER180;
  tmp2.lon = lat_lon_2.lon * PIOVER180;
  tmp3 = EquatorialRadius * acos(sin(tmp1.lat)*sin(tmp2.lat)+cos(tmp1.lat)*cos(tmp2.lat)*cos(tmp1.lon-tmp2.lon));
  // For very small differences we can sometimes get NaN returned
  return std::isnan(tmp3)?0:tmp3;
}

UTM LatLon::to_utm(const LatLon & lat_lon)
{
    double latitude;
    double longitude;
    double lat_rad, long_rad;
    double long_origin, long_origin_rad;
    double eccPrimeSquared;
    double N, T, C, A, M;
    int zone;
    double northing, easting;

    latitude = lat_lon.lat;
    longitude = lat_lon.lon;

    /* We want the longitude within -180..180. */
    if ( longitude < -180.0 )
	longitude += 360.0;
    if ( longitude > 180.0 )
	longitude -= 360.0;

    /* Now convert. */
    lat_rad = DEG2RAD(latitude);
    long_rad = DEG2RAD(longitude);
    zone = (int) ( ( longitude + 180 ) / 6 ) + 1;
    if ( latitude >= 56.0 && latitude < 64.0 &&
	 longitude >= 3.0 && longitude < 12.0 )
	zone = 32;
    /* Special zones for Svalbard. */
    if ( latitude >= 72.0 && latitude < 84.0 )
	{
	if      ( longitude >= 0.0  && longitude <  9.0 ) zone = 31;
	else if ( longitude >= 9.0  && longitude < 21.0 ) zone = 33;
	else if ( longitude >= 21.0 && longitude < 33.0 ) zone = 35;
	else if ( longitude >= 33.0 && longitude < 42.0 ) zone = 37;
	}
    long_origin = ( zone - 1 ) * 6 - 180 + 3;	/* +3 puts origin in middle of zone */
    long_origin_rad = DEG2RAD(long_origin);
    eccPrimeSquared = EccentricitySquared / ( 1.0 - EccentricitySquared );
    N = EquatorialRadius / sqrt( 1.0 - EccentricitySquared * sin( lat_rad ) * sin( lat_rad ) );
    T = tan( lat_rad ) * tan( lat_rad );
    C = eccPrimeSquared * cos( lat_rad ) * cos( lat_rad );
    A = cos( lat_rad ) * ( long_rad - long_origin_rad );
    M = EquatorialRadius * ( ( 1.0 - EccentricitySquared / 4 - 3 * EccentricitySquared * EccentricitySquared / 64 - 5 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 256 ) * lat_rad - ( 3 * EccentricitySquared / 8 + 3 * EccentricitySquared * EccentricitySquared / 32 + 45 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 1024 ) * sin( 2 * lat_rad ) + ( 15 * EccentricitySquared * EccentricitySquared / 256 + 45 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 1024 ) * sin( 4 * lat_rad ) - ( 35 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 3072 ) * sin( 6 * lat_rad ) );
    easting =
	K0 * N * ( A + ( 1 - T + C ) * A * A * A / 6 + ( 5 - 18 * T + T * T + 72 * C - 58 * eccPrimeSquared ) * A * A * A * A * A / 120 ) + 500000.0;
    northing =
	K0 * ( M + N * tan( lat_rad ) * ( A * A / 2 + ( 5 - T + 9 * C + 4 * C * C ) * A * A * A * A / 24 + ( 61 - 58 * T + T * T + 600 * C - 330 * eccPrimeSquared ) * A * A * A * A * A * A / 720 ) );
    if ( latitude < 0.0 )
	northing += 10000000.0;  /* 1e7 meter offset for southern hemisphere */

    /* All done. */

    UTM utm;
    utm.northing = northing;
    utm.easting = easting;
    utm.zone = zone;
    utm.band_letter = coords_utm_band_letter(latitude);
    return utm;
}


static char coords_utm_band_letter(double latitude)
    {
    /* This routine determines the correct UTM band letter designator for the
    ** given latitude.  It returns 'Z' if the latitude is outside the UTM
    ** limits of 84N to 80S.
    */
    if ( latitude <= 84.0 && latitude >= 72.0 ) return 'X';
    else if ( latitude < 72.0 && latitude >= 64.0 ) return 'W';
    else if ( latitude < 64.0 && latitude >= 56.0 ) return 'V';
    else if ( latitude < 56.0 && latitude >= 48.0 ) return 'U';
    else if ( latitude < 48.0 && latitude >= 40.0 ) return 'T';
    else if ( latitude < 40.0 && latitude >= 32.0 ) return 'S';
    else if ( latitude < 32.0 && latitude >= 24.0 ) return 'R';
    else if ( latitude < 24.0 && latitude >= 16.0 ) return 'Q';
    else if ( latitude < 16.0 && latitude >= 8.0 ) return 'P';
    else if ( latitude <  8.0 && latitude >= 0.0 ) return 'N';
    else if ( latitude <  0.0 && latitude >= -8.0 ) return 'M';
    else if ( latitude < -8.0 && latitude >= -16.0 ) return 'L';
    else if ( latitude < -16.0 && latitude >= -24.0 ) return 'K';
    else if ( latitude < -24.0 && latitude >= -32.0 ) return 'J';
    else if ( latitude < -32.0 && latitude >= -40.0 ) return 'H';
    else if ( latitude < -40.0 && latitude >= -48.0 ) return 'G';
    else if ( latitude < -48.0 && latitude >= -56.0 ) return 'F';
    else if ( latitude < -56.0 && latitude >= -64.0 ) return 'E';
    else if ( latitude < -64.0 && latitude >= -72.0 ) return 'D';
    else if ( latitude < -72.0 && latitude >= -80.0 ) return 'C';
    else return 'Z';
    }



LatLon UTM::to_latlon(const UTM & utm)
{
    double eccPrimeSquared;
    double e1;
    double N1, T1, C1, R1, D, M;
    double long_origin;
    double mu, phi1_rad;
    double latitude, longitude;

    /* Now convert. */
    double x = utm.easting - 500000.0; /* remove 500000 meter offset */
    double y = utm.northing;
    assert (utm.band_letter >= 'A' && utm.band_letter <= 'Z');
    if (utm.band_letter < 'N') {
      /* southern hemisphere */
      y -= 10000000.0;	/* remove 1e7 meter offset */
    }

    long_origin = ( utm.zone - 1 ) * 6 - 180 + 3;	/* +3 puts origin in middle of zone */
    eccPrimeSquared = EccentricitySquared / ( 1.0 - EccentricitySquared );
    e1 = ( 1.0 - sqrt( 1.0 - EccentricitySquared ) ) / ( 1.0 + sqrt( 1.0 - EccentricitySquared ) );
    M = y / K0;
    mu = M / ( EquatorialRadius * ( 1.0 - EccentricitySquared / 4 - 3 * EccentricitySquared * EccentricitySquared / 64 - 5 * EccentricitySquared * EccentricitySquared * EccentricitySquared / 256 ) );
    phi1_rad = mu + ( 3 * e1 / 2 - 27 * e1 * e1 * e1 / 32 )* sin( 2 * mu ) + ( 21 * e1 * e1 / 16 - 55 * e1 * e1 * e1 * e1 / 32 ) * sin( 4 * mu ) + ( 151 * e1 * e1 * e1 / 96 ) * sin( 6 *mu );
    N1 = EquatorialRadius / sqrt( 1.0 - EccentricitySquared * sin( phi1_rad ) * sin( phi1_rad ) );
    T1 = tan( phi1_rad ) * tan( phi1_rad );
    C1 = eccPrimeSquared * cos( phi1_rad ) * cos( phi1_rad );
    R1 = EquatorialRadius * ( 1.0 - EccentricitySquared ) / pow( 1.0 - EccentricitySquared * sin( phi1_rad ) * sin( phi1_rad ), 1.5 );
    D = x / ( N1 * K0 );
    latitude = phi1_rad - ( N1 * tan( phi1_rad ) / R1 ) * ( D * D / 2 -( 5 + 3 * T1 + 10 * C1 - 4 * C1 * C1 - 9 * eccPrimeSquared ) * D * D * D * D / 24 + ( 61 + 90 * T1 + 298 * C1 + 45 * T1 * T1 - 252 * eccPrimeSquared - 3 * C1 * C1 ) * D * D * D * D * D * D / 720 );
    latitude = RAD2DEG(latitude);
    longitude = ( D - ( 1 + 2 * T1 + C1 ) * D * D * D / 6 + ( 5 - 2 * C1 + 28 * T1 - 3 * C1 * C1 + 8 * eccPrimeSquared + 24 * T1 * T1 ) * D * D * D * D * D / 120 ) / cos( phi1_rad );
    longitude = long_origin + RAD2DEG(longitude);

    /* Show results. */

    return LatLon(latitude, longitude);
}




void LatLon::to_strings(const LatLon & lat_lon, QString & lat, QString & lon)
{
#ifdef HAVE_VIKING
	DegreeFormat format = Preferences::get_degree_format();

	switch (format) {
	case DegreeFormat::DDD:
		lat = convert_lat_dec_to_ddd(lat_lon.lat);
		lon = convert_lon_dec_to_ddd(lat_lon.lon);
		break;
	case DegreeFormat::DMM:
		lat = convert_lat_dec_to_dmm(lat_lon.lat);
		lon = convert_lon_dec_to_dmm(lat_lon.lon);
		break;
	case DegreeFormat::DMS:
		lat = convert_lat_dec_to_dms(lat_lon.lat);
		lon = convert_lon_dec_to_dms(lat_lon.lon);
		break;
	case DegreeFormat::RAW:
		lat = LatLon::lat_to_string_raw(lat_lon);
		lon = LatLon::lon_to_string_raw(lat_lon);
		break;
	default:
		qDebug() << "EE:" PREFIX << __FUNCTION__ << "unknown degree format %d" << (int) format;
		break;
	}
#else
	lat = convert_lat_dec_to_ddd(lat_lon.lat);
	lon = convert_lon_dec_to_ddd(lat_lon.lon);
#endif
}



LatLonMinMax::LatLonMinMax(const LatLonBBox & bbox)
{
	this->max.lat = bbox.north;
	this->max.lon = bbox.east;

	this->min.lat = bbox.south;
	this->min.lon = bbox.west;
}
