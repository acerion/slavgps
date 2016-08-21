/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SG_OSM_TRACES_H_
#define _SG_OSM_TRACES_H_




#include <glib.h>
#include <gtk/gtk.h>

#include "viktrwlayer.h"




namespace SlavGPS {



	void osm_traces_init();
	void osm_traces_uninit();
	void osm_traces_upload_viktrwlayer(LayerTRW * trw, Track * trk);

	void osm_set_login(const char * user, const char * password);
	char * osm_get_login();
	void osm_login_widgets(GtkWidget * user_entry, GtkWidget * password_entry);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_OSM_TRACES_H_ */
