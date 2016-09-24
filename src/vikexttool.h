/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
 *
 */
#ifndef _SG_EXTERNAL_H
#define _SG_EXTERNAL_H


#include "window.h"
#include "coord.h"





namespace SlavGPS {





	class External {
	public:
		External();
		~External();

		char * get_label();
		void set_label(char const * new_label);

		void set_id(int new_id);
		int get_id();

		virtual void open(Window * window) = 0;
		virtual void open_at_position(Window * window, VikCoord * vc) = 0;

	protected:
		int id;
		char * label;
	};





} /* namespace SlavGPS */





#endif /* #ifndef _SG_EXTERNAL_H */
