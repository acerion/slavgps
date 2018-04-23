/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




#if 1 /* Only for testing of compilation. */
#define VIK_CONFIG_GEOCACHES
#endif

#ifdef VIK_CONFIG_GEOCACHES



#include <cstring>
#include <cstdlib>

#include <QDebug>
#include <QStandardPaths>

#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "preferences.h"
#include "datasources.h"
#include "datasource_gc.h"
#include "viewport_internal.h"
#include "util.h"




using namespace SlavGPS;




/* Could have an array of programs instead... */
#define GC_PROGRAM1 "geo-nearest"
#define GC_PROGRAM2 "geo-html2gpx"

#define METERSPERMILE 1609.344

/* Params will be geocaching.username, geocaching.password
   We have to make sure these don't collide. */
#define PREFERENCES_NAMESPACE_GC "geocaching"




#ifdef VIK_CONFIG_GEOCACHES
// DataSourceInterface datasource_gc_interface;
#endif




DataSourceGeoCache::DataSourceGeoCache()
{
	this->window_title = QObject::tr("Download Geocaches");
	this->layer_title = QObject::tr("Geocaching.com Caches");
	this->mode = DataSourceMode::AutoLayerManagement;
	this->input_type = DataSourceInputType::None;
	this->autoview = true;         /* true = automatically update the display - otherwise we won't see the geocache waypoints! */
	this->keep_dialog_open = true; /* true = keep dialog open after success. */
	this->is_thread = true;
}



static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GC, "username", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("geocaching.com username:"), WidgetType::ENTRY, NULL, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_GC, "password", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("geocaching.com password:"), WidgetType::ENTRY, NULL, NULL, NULL, NULL },
};




void DataSourceGeoCache::init(void)
{
	int i = 0;
	Preferences::register_group(PREFERENCES_NAMESPACE_GC, QObject::tr("Geocaching"));

	Preferences::register_parameter(prefs[i], SGVariant("username", prefs[i].type_id));
	i++;
	Preferences::register_parameter(prefs[i], SGVariant("password", prefs[i].type_id));
	i++;
}




bool DataSourceGeoCache::have_programs(void)
{
	const QString location1 = QStandardPaths::findExecutable(GC_PROGRAM1);
	const bool OK1 = !location1.isEmpty();

	const QString location2 = QStandardPaths::findExecutable(GC_PROGRAM2);
	const bool OK2 = !location2.isEmpty();

	if (OK1 && OK2) {
		return true;
	}

	const QString error_msg = QObject::tr("Can't find %1 or %2 in standard location! Check that you have installed it correctly.").arg(GC_PROGRAM1).arg(GC_PROGRAM2);
	Dialog::error(error_msg, NULL);

	return false;
}




void DataSourceGCDialog::draw_circle_cb(void)
{
	double lat, lon;
	if (this->circle_onscreen) {
		this->viewport->draw_arc(this->circle_pen,
					 this->circle_x - this->circle_width/2,
					 this->circle_y - this->circle_width/2,
					 this->circle_width, this->circle_width, 0, 360,
					 false);
	}

	/* Calculate widgets circle_x and circle_y. */
	/* Split up lat,lon into lat and lon. */
	if (2 == sscanf(this->center_entry.text().toUtf8().constData(), "%lf,%lf", &lat, &lon)) {

		const Coord coord(LatLon(lat, lon), this->viewport->get_coord_mode());
		const ScreenPos circle_pos = this->viewport->coord_to_screen_pos(coord);

		/* TODO: real calculation. */
		if (circle_pos.x > -1000
		    && circle_pos.y > -1000
		    && circle_pos.x < (this->viewport->get_width() + 1000)
		    && circle_pos.y < (this->viewport->get_width() + 1000)) {

			this->circle_x = circle_pos.x;
			this->circle_y = circle_pos.y;

			/* Determine miles per pixel. */
			const Coord coord1 = this->viewport->screen_pos_to_coord(0, this->viewport->get_height()/2);
			const Coord coord2 = this->viewport->screen_pos_to_coord(this->viewport->get_width(), this->viewport->get_height()/2);
			const double pixels_per_meter = ((double)this->viewport->get_width()) / Coord::distance(coord1, coord2);

			/* This is approximate. */
			this->circle_width = this->miles_radius_spin.value()
				* METERSPERMILE * pixels_per_meter * 2;

			this->viewport->draw_arc(this->circle_pen,
						 this->circle_x - this->circle_width/2,
						 this->circle_y - this->circle_width/2,
						 this->circle_width, this->circle_width, 0, 360,
						 false);

			this->circle_onscreen = true;
		} else {
			this->circle_onscreen = false;
		}
	}

	/* See if onscreen. */
	/* Okay. */
	this->viewport->sync();
}




DataSourceDialog * DataSourceGeoCache::create_setup_dialog(Viewport * viewport, void * user_data)
{
	return new DataSourceGCDialog(viewport);
}




DataSourceGCDialog::DataSourceGCDialog(Viewport * new_viewport)
{
	this->viewport = new_viewport;


	QLabel * num_label = new QLabel(QObject::tr("Number geocaches:"), this);
	this->num_spin.setMinimum(1);
	this->num_spin.setMaximum(1000);
	this->num_spin.setSingleStep(10);
	this->num_spin.setValue(20);


	QLabel * center_label = new QLabel(QObject::tr("Centered around:"), this);


	QLabel * miles_radius_label = new QLabel(QObject::tr("Miles Radius:"), this);
	this->miles_radius_spin.setMinimum(1);
	this->miles_radius_spin.setMaximum(1000);
	this->miles_radius_spin.setSingleStep(1);
	this->miles_radius_spin.setValue(5);

	const LatLon lat_lon = this->viewport->get_center()->get_latlon();
	this->center_entry.setText(lat_lon.to_string()); /* "lat,lon" string. */

	this->circle_pen.setColor(QColor("#000000"));
	this->circle_pen.setWidth(3);
#ifdef K_FIXME_RESTORE
	gdk_gc_set_function(this->circle_pen, GDK_INVERT);
#endif
	this->circle_onscreen = true;
	this->draw_circle_cb();

	QObject::connect(&this->center_entry, SIGNAL (editingFinished(void)), this, SLOT (draw_circle_cb()));
	QObject::connect(&this->miles_radius_spin, SIGNAL (valueChanged(double)), this, SLOT (draw_circle_cb()));

	/* Packing all dialog's widgets */
	this->grid->addWidget(num_label, 0, 0);
	this->grid->addWidget(&this->num_spin, 0, 1);

	this->grid->addWidget(center_label, 1, 0);
	this->grid->addWidget(&this->center_entry, 1, 1);

	this->grid->addWidget(miles_radius_label, 2, 0);
	this->grid->addWidget(&this->miles_radius_spin, 2, 1);
}




ProcessOptions * DataSourceGCDialog::get_process_options(void)
{
	ProcessOptions * po = new ProcessOptions();

	//char *safe_string = g_shell_quote (this->center_entry.text());
	char *safe_user = g_shell_quote(Preferences::get_param_value(PREFERENCES_NAMESPACE_GC ".username").val_string.toUtf8().constData());
	char *safe_pass = g_shell_quote(Preferences::get_param_value(PREFERENCES_NAMESPACE_GC ".password").val_string.toUtf8().constData());

	double lat, lon;
	if (2 != sscanf(this->center_entry.text().toUtf8().constData(), "%lf,%lf", &lat, &lon)) {
		qDebug() << "WW: Datasource GC: broken input - using some defaults";
		lat = Preferences::get_default_lat();
		lon = Preferences::get_default_lon();
	}

	/* Unix specific shell commands
	   1. Remove geocache webpages (maybe be from different location).
	   2, Gets upto n geocaches as webpages for the specified user in radius r Miles.
	   3. Converts webpages into a single waypoint file, ignoring zero location waypoints '-z'.
	      Probably as they are premium member only geocaches and user is only a basic member.
	   Final output is piped into GPSbabel - hence removal of *html is done at beginning of the command sequence. */
	const QString command1 = "rm -f ~/.geo/caches/*.html; ";
	const QString command2 = QString("%1 -H ~/.geo/caches -P -n%2 -r%3M -u %4 -p %5 %6 %7; ")
		.arg(GC_PROGRAM1)
		.arg(this->num_spin.value())
		.arg(this->miles_radius_spin.value(), 0, 'f', 1)
		.arg(safe_user)
		.arg(safe_pass)
		.arg(SGUtils::double_to_c(lat).toUtf8().constData())
		.arg(SGUtils::double_to_c(lon).toUtf8().constData());
	const QString command3 = QString("%1 -z ~/.geo/caches/*.html").arg(GC_PROGRAM2);

	po->shell_command = command1 + command2 + command3;

	//free(safe_string);
	free(safe_user);
	free(safe_pass);

	return po;
}




DataSourceGCDialog::~DataSourceGCDialog()
{
	if (this->circle_onscreen) {
		this->viewport->draw_arc(this->circle_pen,
					 this->circle_x - this->circle_width/2,
					 this->circle_y - this->circle_width/2,
					 this->circle_width, this->circle_width, 0, 360,
					 false);
		this->viewport->sync();
	}
}




#endif /* #ifdef VIK_CONFIG_GEOCACHES */
