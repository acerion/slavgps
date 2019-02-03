/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007-2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 * Based on:
 * Copyright (C) 2003-2007, Leandro A. F. Pereira <leandro@linuxmag.com.br>
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

#ifndef _SG_UTIL_H_
#define _SG_UTIL_H_




#include <QString>
#include <QFile>




namespace SlavGPS {




	/* Hash function for std::map<QString, T>, with case insensitivity. */
	struct MyQStringKeyHashInsensitive {
		std::size_t operator()(const QString & s) const {
			using std::hash;
			using std::string;

			return hash<string>()(s.toLower().toUtf8().constData());
		}
	};


	/* Equality function for std::map<QString, T>, with case insensitivity. */
	struct MyQStringKeyEqualInsensitive
	{
		bool operator()(const QString & s1, const QString & s2) const {
			return s1.toLower() == s2.toLower();
		}
	};




	class Util {
	public:
		static int get_number_of_threads(void);

		static QString uri_escape(const QString & buffer);

		static bool split_string_from_file_on_equals(const QString & line, QString & key, QString & val);

		static void add_to_deletion_list(const QString & file_full_path);
		static void remove_all_in_deletion_list(void);

		static bool remove(const QString & file_full_path);
		static bool remove(QFile & file);

		static QString write_tmp_file_from_bytes(const void * buffer, size_t count);

		static QString shell_quote(const QString & string);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_UTIL_H_ */
