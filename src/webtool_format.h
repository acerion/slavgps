/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Format Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Format Public License for more details.
 *
 * You should have received a copy of the GNU Format Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _SG_ONLINE_SERVICE_FORMAT_H_
#define _SG_ONLINE_SERVICE_FORMAT_H_




#include "webtool.h"




namespace SlavGPS {




	class Window;




	class OnlineService_format : public OnlineService {
		Q_OBJECT
	public:
		OnlineService_format(const QString & tool_label, const QString & url_format, const QString & url_format_code);
		~OnlineService_format();

		QString get_url_for_viewport(Viewport * viewport) override;
		QString get_url_at_position(Viewport * viewport, const Coord * a_coord) override;

	private:
		QString url_format_code;  /* Template URL format code, "AOZ" -  default value Lat, Long, Zoom */

	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ONLINE_SERVICE_FORMAT_H_ */
