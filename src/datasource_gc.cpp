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

/* Params will be geocaching.username, geocaching.password
   We have to make sure these don't collide. */
#define PREFERENCES_NAMESPACE_GC "geocaching"




static DataSourceDialog * datasource_gc_create_setup_dialog(Viewport * viewport, void * user_data);
static bool datasource_gc_check_existence(QString error_msg);




#define METERSPERMILE 1609.344




DataSourceInterface datasource_gc_interface = {
	N_("Download Geocaches"),
	N_("Geocaching.com Caches"),
	DataSourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,  /* Yes automatically update the display - otherwise we won't see the geocache waypoints! */
	true,  /* true = keep dialog open after success. */
	true,  /* true = run as thread. */

	(DataSourceInitFunc)		      NULL,
	(DataSourceCheckExistenceFunc)        datasource_gc_check_existence,
	(DataSourceCreateSetupDialogFunc)     datasource_gc_create_setup_dialog,
	(DataSourceGetProcessOptionsFunc)     NULL,
	(DataSourceProcessFunc)               a_babel_convert_from,
	(DataSourceProgressFunc)              NULL,
	(DataSourceCreateProgressDialogFunc)  NULL,
	(DataSourceCleanupFunc)               NULL,
	(DataSourceTurnOffFunc)               NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};




static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GC, "username", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, QObject::tr("geocaching.com username:"), WidgetType::ENTRY, NULL, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_GC, "password", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, QObject::tr("geocaching.com password:"), WidgetType::ENTRY, NULL, NULL, NULL, NULL },
};




void a_datasource_gc_init()
{
	Preferences::register_group(PREFERENCES_NAMESPACE_GC, QObject::tr("Geocaching"));

	Preferences::register_parameter(prefs + 0, SGVariant("username"));
	Preferences::register_parameter(prefs + 1, SGVariant("password"));
}




static bool datasource_gc_check_existence(QString error_msg)
{
	bool OK1 = false;
	bool OK2 = false;

	char * location1 = g_find_program_in_path(GC_PROGRAM1);
	if (location1) {
		free(location1);
		OK1 = true;
	}

	char * location2 = g_find_program_in_path(GC_PROGRAM2);
	if (location2) {
		free(location2);
		OK2 = true;
	}

	if (OK1 && OK2) {
		return true;
	}

	error_msg = QObject::tr("Can't find %1 or %2 in path! Check that you have installed it correctly.").arg(GC_PROGRAM1).arg(GC_PROGRAM2);

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

		int x, y;
		struct LatLon lat_lon = { .lat = lat, .lon = lon };

		const Coord coord(lat_lon, this->viewport->get_coord_mode());
		this->viewport->coord_to_screen(&coord, &x, &y);
		/* TODO: real calculation. */
		if (x > -1000 && y > -1000 && x < (this->viewport->get_width() + 1000) &&
		    y < (this->viewport->get_width() + 1000)) {

			this->circle_x = x;
			this->circle_y = y;

			/* Determine miles per pixel. */
			const Coord coord1 = this->viewport->screen_to_coord(0, this->viewport->get_height()/2);
			const Coord coord2 = this->viewport->screen_to_coord(this->viewport->get_width(), this->viewport->get_height()/2);
			double pixels_per_meter = ((double)this->viewport->get_width()) / Coord::distance(coord1, coord2);

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




static DataSourceDialog * datasource_gc_create_setup_dialog(Viewport * viewport, void * user_data)
{
	return new DataSourceGCDialog();
}




DataSourceGCDialog::DataSourceGCDialog()
{
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

	struct LatLon lat_lon = viewport->get_center()->get_latlon();
	this->center_entry.setText(QString("%1,%2").arg(lat_lon.lat).arg(lat_lon.lon));


	this->viewport = viewport;
	this->circle_pen.setColor(QColor("#000000"));
	this->circle_pen.setWidth(3);
#ifdef K
	gdk_gc_set_function (this->circle_pen, GDK_INVERT);
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




ProcessOptions * DataSourceGCDialog::get_process_options(DownloadOptions & dl_options)
{
	ProcessOptions * po = new ProcessOptions();

	//char *safe_string = g_shell_quote (this->center_entry.text());
	char *safe_user = g_shell_quote(Preferences::get_param_value(PREFERENCES_NAMESPACE_GC ".username")->val_string.toUtf8().constData());
	char *safe_pass = g_shell_quote(Preferences::get_param_value(PREFERENCES_NAMESPACE_GC ".password")->val_string.toUtf8().constData());

	double lat, lon;
	if (2 != sscanf(this->center_entry.text().toUtf8().constData(), "%lf,%lf", &lat, &lon)) {
		qDebug() << "WW: Datasource GC: broken input - using some defaults";
		lat = Preferences::get_default_lat();
		lon = Preferences::get_default_lon();
	}

	/* Convert double as string in C locale. */
	char * slat = a_coords_dtostr(lat);
	char * slon = a_coords_dtostr(lon);

	/* Unix specific shell commands
	   1. Remove geocache webpages (maybe be from different location).
	   2, Gets upto n geocaches as webpages for the specified user in radius r Miles.
	   3. Converts webpages into a single waypoint file, ignoring zero location waypoints '-z'.
	      Probably as they are premium member only geocaches and user is only a basic member.
	   Final output is piped into GPSbabel - hence removal of *html is done at beginning of the command sequence. */
	po->shell_command = g_strdup_printf("rm -f ~/.geo/caches/*.html ; %s -H ~/.geo/caches -P -n%d -r%.1fM -u %s -p %s %s %s ; %s -z ~/.geo/caches/*.html ",
					    GC_PROGRAM1,
					    this->num_spin.value(),
					    this->miles_radius_spin.value(),
					    safe_user,
					    safe_pass,
					    slat, slon,
					    GC_PROGRAM2);
	//free(safe_string);
	free(safe_user);
	free(safe_pass);
	free(slat);
	free(slon);

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
