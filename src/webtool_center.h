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

#ifndef _SG_ONLINE_SERVICE_CENTER_H_
#define _SG_ONLINE_SERVICE_CENTER_H_




#include "webtool.h"




namespace SlavGPS {




	class GisViewport;




	class OnlineService_center : public OnlineService {
		Q_OBJECT
	public:
		OnlineService_center(const QString & tool_label, const QString & url_format);
		~OnlineService_center();

		QString get_url_for_viewport(GisViewport * gisview) override;
		QString get_url_at_position(GisViewport * gisview, const Coord * coord) override;

		QString get_url_for_coord(const Coord & a_coord, const VikingScale & viking_scale);

	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ONLINE_SERVICE_CENTER_H_ */
