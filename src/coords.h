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




#include "lat_lon.h"
#include "globals.h"




namespace SlavGPS {




#define UTM_ZONES_COUNT                        60 /* Number of UTM zones */
#define UTM_NORTHING_AT_EQUATOR        10000000.0 /* [m] */
#define UTM_CENTRAL_MERIDIAN_EASTING     500000.0 /* [m] */




	class UTM;
	class LatLonBBox;




	class Coords {
	public:
		static bool unit_tests(void);
	};




	class LatLon {
	public:
		LatLon(double new_lat = NAN, double new_lon = NAN) : lat(new_lat), lon(new_lon) {};
		LatLon(const Latitude & lat, const Longitude & lon);

		double lat = NAN;
		double lon = NAN;

		bool is_valid(void) const { return !std::isnan(this->lat) && !std::isnan(this->lon); };
		void invalidate(void) { this->lat = NAN; this->lon = NAN; };

		/* Convert value to string with DegreeFormat::Raw format. */
		static void lat_to_string_raw(QString & lat_string, const LatLon & lat_lon);
		static void lon_to_string_raw(QString & lon_string, const LatLon & lat_lon);
		static void lat_to_string_raw(QString & lat_string, const Latitude & lat);
		static void lon_to_string_raw(QString & lon_string, const Longitude & lon);

		/* Generate "lat,lon" string with DegreeFormat::Raw. o*/
		QString to_string_raw(const QString & separator = ",") const;

		/* Convert value to pair of strings using preferred representation. */
		QString to_string(void) const;

		/* Convert value to strings with DegreeFormat::Raw format. */
		void to_strings_raw(QString & lat, QString & lon) const;

		static LatLon get_average(const LatLon & max, const LatLon & min) { return LatLon((max.lat + min.lat) / 2, (max.lon + min.lon) / 2); };
		static LatLon get_interpolated(const LatLon & lat_lon_1, const LatLon & lat_lon_2, double scale);

		static UTM to_utm(const LatLon & lat_lon);

		static double get_distance(const LatLon & lat_lon_1, const LatLon & lat_lon_2);
	};
	QDebug operator<<(QDebug debug, const LatLon & lat_lon);




	enum class UTMLetter : char {
		A = 'A',
		B = 'B',
		C = 'C',
		D = 'D',
		E = 'E',
		F = 'F',
		G = 'G',
		H = 'H',
		J = 'J',
		K = 'K',
		L = 'L',
		M = 'M',
		N = 'N',
		P = 'P',
		Q = 'Q',
		R = 'R',
		S = 'S',
		T = 'T',
		U = 'U',
		V = 'V',
		W = 'W',
		X = 'X',
		Y = 'Y',
		Z = 'Z',

		None
	};




	class UTM {
	public:
		UTM(double new_northing = NAN, double new_easting = NAN, int new_zone = 0, UTMLetter new_band_letter = UTMLetter::None) {};

		static bool is_equal(const UTM & utm1, const UTM & utm2);
		static LatLon to_lat_lon(const UTM & utm);

		QString to_string(void) const;



		bool has_band_letter(void) const;

		static bool is_the_same_zone(const UTM & utm1, const UTM & utm2);
		static bool is_northern_hemisphere(const UTM & utm);
		static QStringList get_band_symbols(void);
		static bool is_band_letter(char character); /* Is given character a band letter? */
		static bool is_band_letter(UTMLetter letter); /* Is given character a band letter? */
		static bool is_band_symbol(char character); /* Is given character a band letter or "none band" indicator? */

		static double get_distance(const UTM & utm1, const UTM & utm2);

		sg_ret set_northing(double value);
		sg_ret set_easting(double value);
		sg_ret set_zone(int value);
		sg_ret set_band_letter(char character);
		sg_ret set_band_letter(UTMLetter letter);

		sg_ret shift_northing_by(double delta_meters);
		sg_ret shift_easting_by(double delta_meters);

		sg_ret shift_zone_by(int shift);

		double get_northing(void) const { return this->northing; }
		double get_easting(void) const { return this->easting; }
		int get_zone(void) const { return this->zone; }
		UTMLetter get_band_letter(void) const;
		char get_band_as_letter(void) const;

		/* TODO_HARD: revisit data types (double or int?) for northing/easting. */
		double northing = 0;
		double easting = 0;

		static bool close_enough(const UTM & utm1, const UTM & utm2);

	private:
		int zone = 0;
		UTMLetter band_letter = UTMLetter::None;
	};
	QDebug operator<<(QDebug debug, const UTM & utm);





} /* namespace SlavGPS */




#endif /* #ifndef _VIKING_COORDS_H */
