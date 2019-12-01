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




#include <cstring>
#include <cstdlib>
#include <cassert>




#include <QDebug>
#include <QStandardPaths>




#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "preferences.h"
#include "datasources.h"
#include "datasource_geocache.h"
#include "viewport_internal.h"
#include "util.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource GeoCache"

/* Could have an array of programs instead... */
#define GC_PROGRAM1 "geo-nearest"
#define GC_PROGRAM2 "geo-html2gpx"

#define METERSPERMILE 1609.344

/* Params will be geocaching.username, geocaching.password
   We have to make sure these don't collide. */
#define PREFERENCES_NAMESPACE_GC "geocaching."




DataSourceGeoCache::DataSourceGeoCache(GisViewport * new_gisview)
{
	this->gisview = new_gisview;

	this->window_title = QObject::tr("Download Geocaches");
	this->layer_title = QObject::tr("Geocaching.com Caches");
	this->mode = DataSourceMode::AutoLayerManagement;
	this->input_type = DataSourceInputType::None;
	this->autoview = true;         /* true = automatically update the display - otherwise we won't see the geocache waypoints! */
	this->keep_dialog_open = true; /* true = keep dialog open after success. */
}




SGObjectTypeID DataSourceGeoCache::get_source_id(void) const
{
	return DataSourceGeoCache::source_id();
}
SGObjectTypeID DataSourceGeoCache::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.geocache");
	return id;
}




static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GC "username", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("geocaching.com username:"), WidgetType::Entry, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_GC "password", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("geocaching.com password:"), WidgetType::Entry, NULL, NULL, NULL },
};




void DataSourceGeoCache::init(void)
{
#ifdef VIK_CONFIG_GEOCACHES
	int i = 0;
	Preferences::register_parameter_group(PREFERENCES_NAMESPACE_GC, QObject::tr("Geocaching"));

	Preferences::register_parameter_instance(prefs[i], SGVariant("username", prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(prefs[i], SGVariant("password", prefs[i].type_id));
	i++;
#endif
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




void DataSourceGeoCacheDialog::draw_circle_cb(void)
{
	if (this->circle_onscreen) {
		this->gisview->draw_ellipse(this->circle_pen,
					    this->circle_pos,
					    this->circle_radius, this->circle_radius);
	}

	/* Calculate widgets circle_pos. */
	const LatLon lat_lon = this->center_entry->get_value();
	if (!lat_lon.is_valid()) {
		qDebug() << SG_PREFIX_W << "Invalid lat/lon from center entry" << lat_lon;
		return;
	}

	const Coord circle_center_coord(lat_lon, this->gisview->get_coord_mode());
	ScreenPos circle_center;
	sg_ret ret = this->gisview->coord_to_screen_pos(circle_center_coord, circle_center);
	if (sg_ret::ok == ret && this->circle_is_onscreen(circle_center)) {

		this->circle_pos = circle_center;

		const int width = this->gisview->central_get_width();
		const fpixel y_center_pixel  = this->gisview->central_get_y_center_pixel();
		const int leftmost_pixel  = this->gisview->central_get_leftmost_pixel();
		const int rightmost_pixel = this->gisview->central_get_rightmost_pixel();

		/* Determine miles per pixel. */
		const Coord coord1 = this->gisview->screen_pos_to_coord(leftmost_pixel, y_center_pixel);
		const Coord coord2 = this->gisview->screen_pos_to_coord(rightmost_pixel, y_center_pixel);
		const double pixels_per_meter = ((double) width) / Coord::distance(coord1, coord2);

		/* This is approximate. */
		this->circle_radius = this->miles_radius_spin->value() * METERSPERMILE * pixels_per_meter;

		this->gisview->draw_ellipse(this->circle_pen,
					    this->circle_pos,
					    this->circle_radius, this->circle_radius);

		this->circle_onscreen = true;
	} else {
		this->circle_onscreen = false;
	}
}




int DataSourceGeoCache::run_config_dialog(AcquireContext * acquire_context)
{
	DataSourceGeoCacheDialog config_dialog(this->window_title, this->gisview);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




DataSourceGeoCacheDialog::DataSourceGeoCacheDialog(const QString & window_title, GisViewport * new_gisview) : DataSourceDialog(window_title)
{
	this->gisview = new_gisview;


	QLabel * num_label = new QLabel(QObject::tr("Number geocaches:"), this);
	this->num_spin = new QSpinBox();
	this->num_spin->setMinimum(1);
	this->num_spin->setMaximum(1000);
	this->num_spin->setSingleStep(10);
	this->num_spin->setValue(20);


	QLabel * center_label = new QLabel(QObject::tr("Centered around:"), this);
	const LatLon lat_lon = this->gisview->get_center_coord().get_lat_lon();
	this->center_entry = new LatLonEntryWidget();
	this->center_entry->set_value(lat_lon);


	QLabel * miles_radius_label = new QLabel(QObject::tr("Miles Radius:"), this);
	this->miles_radius_spin = new QDoubleSpinBox();
	this->miles_radius_spin->setMinimum(1);
	this->miles_radius_spin->setMaximum(1000);
	this->miles_radius_spin->setSingleStep(1);
	this->miles_radius_spin->setValue(5);


	this->circle_pen.setColor(QColor("#000000"));
	this->circle_pen.setWidth(3);
#ifdef K_FIXME_RESTORE
	gdk_gc_set_function(this->circle_pen, GDK_INVERT);
#endif

	this->draw_circle_cb();

	QObject::connect(this->center_entry, SIGNAL (value_changed(void)), this, SLOT (draw_circle_cb()));
	QObject::connect(this->miles_radius_spin, SIGNAL (valueChanged(double)), this, SLOT (draw_circle_cb()));

	/* Packing all dialog's widgets */
	this->grid->addWidget(num_label, 0, 0);
	this->grid->addWidget(this->num_spin, 0, 1);

	this->grid->addWidget(center_label, 1, 0);
	this->grid->addWidget(this->center_entry, 1, 1);

	this->grid->addWidget(miles_radius_label, 2, 0);
	this->grid->addWidget(this->miles_radius_spin, 2, 1);
}




AcquireOptions * DataSourceGeoCacheDialog::create_acquire_options(AcquireContext * acquire_context)
{
	const QString safe_user = Util::shell_quote(Preferences::get_param_value(PREFERENCES_NAMESPACE_GC "username").val_string);
	const QString safe_pass = Util::shell_quote(Preferences::get_param_value(PREFERENCES_NAMESPACE_GC "password").val_string);

	LatLon lat_lon = this->center_entry->get_value();
	if (!lat_lon.is_valid()) {
		qDebug() << SG_PREFIX_E << "Invalid lat/lon from center entry, using default values" << lat_lon;

		/* LatLon from entry is invalid, but we still have a chance to get a valid value: */

		lat_lon = LatLon(Preferences::get_default_lat(), Preferences::get_default_lon());
		if (!lat_lon.is_valid()) {
			qDebug() << SG_PREFIX_E << "Invalid lat/lon from defaults" << lat_lon;
			/* TODO_2_LATER: now what? How to handle invalid lat/lon? */
		}
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
		.arg(this->num_spin->value())
		.arg(this->miles_radius_spin->value(), 0, 'f', 1)
		.arg(safe_user)
		.arg(safe_pass)
		.arg(SGUtils::double_to_c(lat_lon.lat))
		.arg(SGUtils::double_to_c(lat_lon.lon));
	const QString command3 = QString("%1 -z ~/.geo/caches/*.html").arg(GC_PROGRAM2);

	AcquireOptions * babel_options = new AcquireOptions(AcquireOptions::Mode::FromShellCommand);
	babel_options->shell_command = command1 + command2 + command3;

	return babel_options;
}




DataSourceGeoCacheDialog::~DataSourceGeoCacheDialog()
{
	if (this->circle_onscreen) {
		this->gisview->draw_ellipse(this->circle_pen,
					    this->circle_pos,
					    this->circle_radius, this->circle_radius);
	}
}




bool DataSourceGeoCacheDialog::circle_is_onscreen(const ScreenPos & circle_center)
{
	/* TODO_2_LATER: real calculation. */
	return circle_center.x() > -1000
		&& circle_center.y() > -1000
		&& circle_center.x() < (this->gisview->central_get_width() + 1000)
		&& circle_center.y() < (this->gisview->central_get_width() + 1000);
}
