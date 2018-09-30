/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SG_LOCATIONS_H_
#define _SG_LOCATIONS_H_




#include <QString>




namespace SlavGPS {




	class SlavGPSLocations {
	public:

		/**
		   \brief Get path to user's SlavGPS directory

		   The path is without trailing separator. The path is with native (platform-dependent) separators.
		*/
		static QString get_config_dir(void);

		/**
		   \brief Get full path to user's home dir
		*/
		static QString get_home_dir(void);

		/**
		   \brief Get path to specific file in SlavGPS's config directory
		*/
		static QString get_file_full_path(const QString & file_name);

		/**
		   \brief See whether SlavGPS's config directory exists
		*/
		static bool config_dir_exists(void);

		/**
		   Get list of directories to scan for application data.
		*/
		static QStringList get_data_dirs(void);


		/**
		   \brief Get XDG compliant user's data directory

		   \return String with path (the string can be empty)
		*/
		static QString get_data_home(void);

	private:
		static QString get_config_dir_no_create(void);
		static QString build_final_name(const QString & base_dir);
		static QString config_dir;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LOCATIONS_H_ */
