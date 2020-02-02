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




#ifndef _SG_ONLINE_SERVICE_H_
#define _SG_ONLINE_SERVICE_H_




#include <QString>




#include "external_tool.h"
#include "layer_map_source.h"




namespace SlavGPS {




	class GisViewport;




	class OnlineService : public ExternalTool {
		Q_OBJECT
	public:
		OnlineService(const QString & tool_label) : ExternalTool(tool_label) {};
		~OnlineService();

		void run_at_current_position(GisViewport * gisview);
		void run_at_position(GisViewport * gisview, const Coord & coord);

		void set_url_format(const QString & new_url_format);

		virtual QString get_url_for_viewport(GisViewport * gisview) = 0;
		virtual QString get_url_at_position(GisViewport * gisview, const Coord * coord) = 0;


	protected:
		QString url_format;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ONLINE_SERVICE_H_ */
