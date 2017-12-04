/*
coords.h
borrowed from:
http://acme.com/software/coords/
I (Evan Battaglia <viking@greentorch.org> have only made some small changes such as
renaming functions and defining LatLon and UTM structs.
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

#ifndef _VIKING_COORDS_H
#define _VIKING_COORDS_H




#include <QString>




namespace SlavGPS {




	class LatLon {
	public:
		LatLon(double new_lat = 0, double new_lon = 0) : lat(new_lat), lon(new_lon) {};

		double lat;
		double lon;

		/* Convert to string with DegreeFormat::RAW format. */
		static QString lat_to_string_raw(const LatLon & lat_lon);
		static QString lon_to_string_raw(const LatLon & lat_lon);

		/**
		   Convert a LatLon to strings using preferred representation
		*/
		static void to_strings(const LatLon & lat_lon, QString & lat, QString & lon);
	};




} /* namespace SlavGPS */


#ifdef __cplusplus
extern "C" {
#endif


struct UTM {
  double northing;
  double easting;
  char zone;
  char letter;
};


int a_coords_utm_equal( const struct UTM *utm1, const struct UTM *utm2 );

void a_coords_latlon_to_utm(struct UTM * utm, const SlavGPS::LatLon & lat_lon);
void a_coords_utm_to_latlon(SlavGPS::LatLon * lat_lon, const struct UTM * utm);

double a_coords_utm_diff( const struct UTM *utm1, const struct UTM *utm2 );
double a_coords_latlon_diff(const SlavGPS::LatLon & lat_lon_1, const SlavGPS::LatLon & lat_lon_2);

/**
 * Convert a double to a string WITHOUT LOCALE.
 *
 * Following GPX specifications, decimal values are xsd:decimal
 * So, they must use the period separator, not the localized one.
 *
 * The returned value must be freed by g_free.
 */
char *a_coords_dtostr ( double d );

#ifdef __cplusplus
}
#endif

#endif
