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

#ifndef _SG_LAT_LON_H_
#define _SG_LAT_LON_H_




#include <QString>



#define SG_LATITUDE_MIN         -90.0
#define SG_LATITUDE_MAX          90.0
#define SG_LONGITUDE_MIN       -180.0
#define SG_LONGITUDE_MAX        180.0
#define SG_LATITUDE_PRECISION     6
#define SG_LONGITUDE_PRECISION    6




namespace SlavGPS {




	void convert_lat_dec_to_ddd(QString & lat_string, double lat);
	void convert_lon_dec_to_ddd(QString & lon_string, double lon);

	void convert_lat_dec_to_dmm(QString & lat_string, double lat);
	void convert_lon_dec_to_dmm(QString & lon_string, double lon);

	void convert_lat_dec_to_dms(QString & lat_string, double lat);
	void convert_lon_dec_to_dms(QString & lon_string, double lon);

	double convert_dms_to_dec(const char * dms);




	class Latitude {
	public:
		Latitude(double val = NAN) { this->set_value(val); };
		Latitude(const char * str);
		Latitude(const QString & str);

		static double hardcoded_default(void) { return 53.4325; }

		QString to_string(void) const;

		/* Generate string containing only value, without unit
		   and without magnitude-dependent conversions of value.

		   Locale of the value in string is suitable for
		   saving the value in gpx or vik file. */
		const QString value_to_string_for_file(void) const;

		bool set_value(double value);
		double get_value(void) const;

		bool is_valid(void) const;
		void invalidate(void);

	private:
		double value = NAN;
		bool valid = false;
	};




	class Longitude {
	public:
		Longitude(double val = NAN) { this->set_value(val); };
		Longitude(const char * str);
		Longitude(const QString & str);

		static double hardcoded_default(void) { return 14.548056; }

		QString to_string(void) const;

		/* Generate string containing only value, without unit
		   and without magnitude-dependent conversions of value.

		   Locale of the value in string is suitable for
		   saving the value in gpx or vik file. */
		const QString value_to_string_for_file(void) const;

		bool set_value(double value);
		double get_value(void) const;

		bool is_valid(void) const;
		void invalidate(void);

	private:
		double value = NAN;
		bool valid = false;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAT_LON_H_ */
