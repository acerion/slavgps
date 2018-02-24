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

#ifndef _SG_DIR_H_
#define _SG_DIR_H_




#include <QString>
#include <QDir>




namespace SlavGPS {




	/**
	   \brief Get path to user's Viking directory

	   The path is without trailing separator. The path is with native (platform-dependent) separators.
	*/
	QString get_viking_dir(void);

	QString get_viking_dir_no_create();
	char ** get_viking_data_path();

	/**
	   \brief Get XDG compliant user's data directory

	   \return String with path (the string can be empty)
	*/
	QString get_viking_data_home();




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DIR_H_ */
