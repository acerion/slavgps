/*
coords.h
borrowed from:
http://acme.com/software/coords/
I (Evan Battaglia <viking@greentorch.org> have only made some small changes such as
renaming functions and defining LatLon and UTM structs.
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

		/* Convert value to string with DegreeFormat::RAW format. */
		static QString lat_to_string_raw(const LatLon & lat_lon);
		static QString lon_to_string_raw(const LatLon & lat_lon);

		/* Convert value to "lat,lon" string with DegreeFormat::RAW format for each token in the string. */
		QString to_string(void) const;

		/* Convert value to pair of strings using preferred representation. */
		static void to_strings(const LatLon & lat_lon, QString & lat, QString & lon);

		/* Convert value to strings with DegreeFormat::RAW format. */
		void to_strings_raw(QString & lat, QString & lon) const;

		static LatLon get_average(const LatLon & max, const LatLon & min) { return LatLon((max.lat + min.lat) / 2, (max.lon + min.lon) / 2); };

		static UTM to_utm(const LatLon & lat_lon);
	};
	QDebug operator<<(QDebug debug, const LatLon & lat_lon);




	/*
	  north = min_max.max.lat
	  east = min_max.max.lon
	  south = min_max.min.lat
	  west = min_max.min.lon
	*/
	class LatLonMinMax {
	public:
		LatLonMinMax() {};
		LatLonMinMax(const LatLon & new_min, const LatLon & new_max) : min(new_min), max(new_max) {};
		LatLonMinMax(const LatLonBBox & bbox);

		static LatLon get_average(const LatLonMinMax & min_max) { return LatLon::get_average(min_max.max, min_max.min); };

		bool is_valid(void) const { return this->min.is_valid() && this->max.is_valid(); };
		void invalidate(void) { this->min.invalidate(); this->max.invalidate(); };

		LatLon min;
		LatLon max;
	};




	class UTM {
	public:
		UTM() {};
		UTM(const QString & northing_string, const QString & easting_string, int zone, char band_letter);

		static bool is_equal(const UTM & utm1, const UTM & utm2);
		static LatLon to_latlon(const UTM & utm);

		QString to_string(void) const;

		double northing = 0;
		double easting = 0;
		int zone = 0;
		char band_letter = 0;
	};
	QDebug operator<<(QDebug debug, const UTM & utm);





} /* namespace SlavGPS */


#ifdef __cplusplus
extern "C" {
#endif




double a_coords_utm_diff(const SlavGPS::UTM * utm1, const SlavGPS::UTM * utm2);
double a_coords_latlon_diff(const SlavGPS::LatLon & lat_lon_1, const SlavGPS::LatLon & lat_lon_2);


#ifdef __cplusplus
}
#endif

#endif
