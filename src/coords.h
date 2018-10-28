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




#include <cmath>




#include <QString>




/* Number of UTM zones */
#define UTM_ZONES 60




namespace SlavGPS {




	class UTM;
	class LatLonBBox;




	class LatLon {
	public:
		LatLon(double new_lat = NAN, double new_lon = NAN) : lat(new_lat), lon(new_lon) {};

		double lat = NAN;
		double lon = NAN;

		bool is_valid(void) const { return !std::isnan(this->lat) && !std::isnan(this->lon); };
		void invalidate(void) { this->lat = NAN; this->lon = NAN; };

		/* Convert value to string with DegreeFormat::Raw format. */
		static QString lat_to_string_raw(const LatLon & lat_lon);
		static QString lon_to_string_raw(const LatLon & lat_lon);

		/* Convert value to "lat,lon" string with DegreeFormat::Raw format for each token in the string. */
		QString to_string(void) const;

		/* Convert value to pair of strings using preferred representation. */
		static void to_strings(const LatLon & lat_lon, QString & lat, QString & lon);

		/* Convert value to strings with DegreeFormat::Raw format. */
		void to_strings_raw(QString & lat, QString & lon) const;

		static LatLon get_average(const LatLon & max, const LatLon & min) { return LatLon((max.lat + min.lat) / 2, (max.lon + min.lon) / 2); };
		static LatLon get_interpolated(const LatLon & lat_lon_1, const LatLon & lat_lon_2, double scale);

		static UTM to_utm(const LatLon & lat_lon);

		static double latlon_diff(const LatLon & lat_lon_1, const LatLon & lat_lon_2);
	};
	QDebug operator<<(QDebug debug, const LatLon & lat_lon);




	class UTM {
	public:
		UTM() {};

		static bool is_equal(const UTM & utm1, const UTM & utm2);
		static LatLon to_latlon(const UTM & utm);

		QString to_string(void) const;

		char get_band_letter(void) const;
		bool set_band_letter(char character);
		bool has_band_letter(void) const;

		static bool is_northern_hemisphere(const UTM & utm);
		static QStringList get_band_symbols(void);
		static bool is_band_letter(char character); /* Is given character a band letter? */
		static bool is_band_symbol(char character); /* Is given character a band letter or "none band" indicator? */

		static double utm_diff(const UTM & utm1, const UTM & utm2);

		/* TODO_HARD: revisit data types (double or int?) for northing/easting. */
		double northing = 0;
		double easting = 0;
		int zone = 0;

	private:
		char band_letter = 0;
	};
	QDebug operator<<(QDebug debug, const UTM & utm);





} /* namespace SlavGPS */




#endif /* #ifndef _VIKING_COORDS_H */
