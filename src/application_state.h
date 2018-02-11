/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_APPLICATION_STATE_H_
#define _SG_APPLICATION_STATE_H_




#include <vector>




#include <QString>




namespace SlavGPS {




	class ApplicationState {
	public:
		static void init();
		static void uninit();

		static bool get_boolean(const char * name, bool * val);
		static void set_boolean(const char * name, bool val);

		static bool get_string(const char * name, QString & val);
		static void set_string(const char * name, const QString & val);

		static bool get_integer(const char * name, int * val);
		static void set_integer(const char * name, int val);

		static bool get_double(const char * name, double * val);
		static void set_double(const char * name, double val);

		static bool get_integer_list_contains(const char * name, int val);
		static void set_integer_list_containing(const char * name, int val);

	private:
		static bool get_integer_list(const char * name, std::vector<int> & integers);
		static void set_integer_list(const char * name, std::vector<int> & integers);
	}; /* class ApplicationState */




} /* namespace SlavGPS */




#endif /* #ifndef _SG_APPLICATION_STATE_H_ */
