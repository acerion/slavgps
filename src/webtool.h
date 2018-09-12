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
 */




#ifndef _SG_WEBTOOL_H_
#define _SG_WEBTOOL_H_




#include <QString>




#include "external_tool.h"
#include "map_source.h"




namespace SlavGPS {




	class Window;
	class Viewport;




	class WebTool : public ExternalTool {
		Q_OBJECT
	public:
		WebTool(const QString & new_tool_name) : ExternalTool(new_tool_name) {};
		~WebTool();

		void run_at_current_position(Window * a_window);
		void run_at_position(Window * a_window, const Coord * a_coord);

		void set_url_format(const QString & new_url_format);

		virtual QString get_url_at_current_position(Viewport * viewport) = 0;
		virtual QString get_url_at_position(Viewport * viewport, const Coord * coord) = 0;

		virtual MapSourceZoomLevel mpp_to_zoom_level(double mpp);



	protected:
		QString url_format;

	}; /* class WebTool */




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WEBTOOL_H_ */
