/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_TRWLAYER_GEOTAG_H_
#define _SG_TRWLAYER_GEOTAG_H_




#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>




#include "dialog.h"
#include "widget_file_list.h"




namespace SlavGPS {




	class Window;
	class LayerTRW;
	class Track;
	class Waypoint;




	/* To be only called from within LayerTRW. */
	void trw_layer_geotag_dialog(Window * parent, LayerTRW * layer, Waypoint * wp, Track * trk);




	class GeoTagDialog : public BasicDialog {
		Q_OBJECT
	public:
		GeoTagDialog(QWidget * parent) : BasicDialog(parent) {};
		~GeoTagDialog();

	public slots:
		void on_accept_cb(void);
		void write_exif_cb_cb(void);
		void create_waypoints_cb_cb(void);

	public:

		LayerTRW * trw = NULL;      /* To pass on. */
		Waypoint * wp = NULL;       /* Use specified waypoint or otherwise the track(s) if NULL. */
		Track * trk = NULL;         /* Use specified track or all tracks if NULL. */

		FileList * files_selection = NULL;

		QLabel * create_waypoints_l = NULL;
		QCheckBox * create_waypoints_cb = NULL;

		QLabel * overwrite_waypoints_l = NULL;    /* Referenced so the sensitivity can be changed. */
		QCheckBox * overwrite_waypoints_cb = NULL;

		QCheckBox * write_exif_cb = NULL;

		QLabel * overwrite_gps_exif_l = NULL;   /* Referenced so the sensitivity can be changed. */
		QCheckBox * overwrite_gps_exif_cb = NULL;

		QLabel * no_change_mtime_l = NULL;    /* Referenced so the sensitivity can be changed. */
		QCheckBox * no_change_mtime_cb = NULL;

		QCheckBox * interpolate_segments_cb = NULL;
		QLineEdit * time_zone_entry = NULL;    /* TODO consider a more user friendly tz widget eg libtimezonemap or similar. */
		QLineEdit * time_offset_entry = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRWLAYER_GEOTAG_H_ */
