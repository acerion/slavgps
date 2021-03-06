/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2006-2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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




#include "globals.h"




#define SG_LATITUDE_MIN             -90.0
#define SG_LATITUDE_MAX              90.0

/* Longitudes bound to basic range. */
#define SG_LONGITUDE_BOUND_MIN     -180.0
#define SG_LONGITUDE_BOUND_MAX      180.0

/* In theory we could scroll viewport left and right to infinity and
   have totally unbound longitudes, but for practical reasons I'm
   limiting the range. Sooner or later (rather later than sooner) we
   would hit the limit of double data type anyway :) */
#define SG_LONGITUDE_UNBOUND_MIN    (10 * SG_LONGITUDE_BOUND_MIN)
#define SG_LONGITUDE_UNBOUND_MAX    (10 * SG_LONGITUDE_BOUND_MAX)

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

		/* Returns result of validating of new value after it
		   has been assigned. */
		bool set_value(double value);

		double value(void) const;

		bool is_valid(void) const;
		void invalidate(void);

		bool operator==(const Latitude & rhs) const;
		bool operator!=(const Latitude & rhs) const;
		Latitude & operator=(const Latitude & rhs);

		bool operator<(const Latitude & rhs) const;
		bool operator>(const Latitude & rhs) const;
		bool operator<=(const Latitude & rhs) const;
		bool operator>=(const Latitude & rhs) const;

		Latitude & operator+=(double rhs);
		Latitude & operator-=(double rhs);

	private:
		/**
		   Check validity of current value of latitude. Set
		   ::m_valid accordingly. Return value of ::m_valid.
		*/
		bool validate(void);

		double m_value = NAN;
		bool m_valid = false;
	};
	QDebug operator<<(QDebug debug, const Latitude & lat);




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

		/**
		   Returns result of validating of new value after it
		   has been assigned.
		*/
		bool set_value(double value);

		/* Get value of longitude, bound to <-180.0;180.0>
		   range. */
		double bound_value(void) const;
		/* Get value of longitude that is not bound to
		   <-180.0;180.0> range. */
		double unbound_value(void) const;

		bool is_valid(void) const;
		void invalidate(void);

		bool operator==(const Longitude & rhs) const;
		bool operator!=(const Longitude & rhs) const;
		Longitude & operator=(const Longitude & rhs);

		bool operator<(const Longitude & rhs) const;
		bool operator>(const Longitude & rhs) const;
		bool operator<=(const Longitude & rhs) const;
		bool operator>=(const Longitude & rhs) const;

		Longitude & operator+=(double rhs);
		Longitude & operator-=(double rhs);

	private:
		/**
		   Check validity of current value of longitude. Set
		   ::m_valid accordingly. Return value of ::m_valid.
		*/
		bool validate(void);

		/* Value of longitude that is not bound to
		   <-180.0;180.0> range. */
		double m_unbound_value = NAN;
		bool m_valid = false;
	};
	QDebug operator<<(QDebug debug, const Longitude & lon);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAT_LON_H_ */
