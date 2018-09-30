/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011-2014, Rob Norris <rw_norris@hotmail.com>
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

/*
  This uses EXIF information from images to create waypoints at those
  positions.
*/




#include <cstring>
#include <cstdlib>
#include <ctype.h>
#include <cmath>
#include <cassert>




#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif




#include <exiv2/exiv2.hpp>




#include <QDebug>




#include "geotag_exif.h"
#include "file.h"
#include "layer_trw_waypoint.h"




using namespace SlavGPS;




#define PREFIX ": Geotag Exif:" << __FUNCTION__ << __LINE__ << ">"




static bool geotag_exif_get_float(Exiv2::ExifData & exif_data, float & val, const char * key)
{
	Exiv2::ExifData::iterator iter = exif_data.findKey(Exiv2::ExifKey(key));
	if (iter == exif_data.end()) {
		return false;
	}
	val = iter->getValue()->toFloat();
	return true;
}




static bool geotag_exif_set_float(Exiv2::ExifData & exif_data, float val, const char * key)
{
	exif_data[key] = Exiv2::floatToRationalCast(val);

	return true;
}




static bool geotag_exif_get_string(Exiv2::ExifData & exif_data, QString & val, const char * key)
{
	Exiv2::ExifData::iterator iter = exif_data.findKey(Exiv2::ExifKey(key));
	if (iter == exif_data.end()) {
		return false;
	}
	val = QString::fromStdString(iter->getValue()->toString());
	return true;
}




static bool geotag_exif_get_gps_info(Exiv2::ExifData & exif_data, double & lat, double & lon, double & alt)
{
	float tmp_lat;
	float tmp_lon;
	float tmp_alt;


	if (!geotag_exif_get_float(exif_data, tmp_lat, "Exif.GPSInfo.GPSLatitude")) {
		qDebug() << "WW" PREFIX << "Can't find GPS Latitude";
		return false;
	}

	if (!geotag_exif_get_float(exif_data, tmp_lon, "Exif.GPSInfo.GPSLongitude")) {
		qDebug() << "WW" PREFIX << "Can't find GPS Longitude";
		return false;
	}

	if (!geotag_exif_get_float(exif_data, tmp_alt, "Exif.GPSInfo.GPSAltitude")) {
		qDebug() << "WW" PREFIX << "Can't find GPS Altitude";
		return false;
	}


	lat = tmp_lat;
	lon = tmp_lon;
	alt = tmp_alt;


	qDebug() << "II" PREFIX << "lat =" << lat << "lon =" << lon << "alt =" << alt;

	return true;
}




static bool geotag_exif_set_gps_info(Exiv2::ExifData & exif_data, double lat, double lon, double alt)
{
	if (!geotag_exif_set_float(exif_data, lat, "Exif.GPSInfo.GPSLatitude")) {
		qDebug() << "WW" PREFIX << "Can't set GPS Latitude";
		return false;
	}

	if (!geotag_exif_set_float(exif_data, lon, "Exif.GPSInfo.GPSLongitude")) {
		qDebug() << "WW" PREFIX << "Can't set GPS Longitude";
		return false;
	}

	if (!geotag_exif_set_float(exif_data, alt, "Exif.GPSInfo.GPSAltitude")) {
		qDebug() << "WW" PREFIX << "Can't set GPS Altitude";
		return false;
	}

	return true;
}




static QString geotag_get_exif_name(Exiv2::ExifData & exif_data)
{
	QString result;

	Exiv2::ExifData::iterator iter = exif_data.findKey(Exiv2::ExifKey("Exif.Image.XPTitle"));
	if (iter != exif_data.end()) {
		Exiv2::Value::AutoPtr value = iter->getValue(); /* "Get a pointer to a copy of the value." */
		result = QString::fromStdString(value->toString());
	}
	return result;
}




/**
   Attempt to get a single comment from the various exif fields.
*/
static QString geotag_get_exif_comment(Exiv2::ExifData & exif_data)
{
	QString result;

	const char * keys[] = {
		"Exif.Image.ImageDescription",
		"Exif.Image.XPComment",
		"Exif.Image.XPSubject",
		"Exif.Image.DateTimeOriginal",
		NULL
	};

	int i = 0;
	while (keys[i]) {
		Exiv2::ExifData::iterator iter = exif_data.findKey(Exiv2::ExifKey(keys[i]));
		i++;

		if (iter == exif_data.end()) {
			continue;
		}

		Exiv2::Value::AutoPtr value = iter->getValue(); /* "Get a pointer to a copy of the value." */
		result = QString::fromStdString(value->toString());
		break;
	}

	return result;
}




/**
   @file_full_path: The (JPG) file with EXIF information in it

   Returns: The position in LatLon format.
   On errors the returned value will be invalid (LatLon::is_valid() will return false).
*/
LatLon GeotagExif::get_position(const QString & file_full_path)
{
	LatLon lat_lon;

	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	assert(image.get() != 0);
	image->readMetadata();
	Exiv2::ExifData &exif_data = image->exifData();

	if (!exif_data.empty()) {
		double lat;
		double lon;
		double alt;
		if (geotag_exif_get_gps_info(exif_data, lat, lon, alt)) {
			lat_lon = LatLon(lat, lon);
		}
	}

	return lat_lon;
}




/**
   @file_full_path: The image file to process
   @coord_mode:     The current coordinate mode to use in the positioning of Waypoint

   Name of created waypoint may be empty if there was no way to generate the name.

   Returns: An allocated Waypoint or NULL if Waypoint could not be generated (e.g. no EXIF info).
*/
Waypoint * GeotagExif::create_waypoint_from_file(const QString & file_full_path, CoordMode coord_mode)
{
	/* Default return values (for failures). */
	Waypoint * wp = NULL;

	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	assert(image.get() != 0);
	image->readMetadata();
	Exiv2::ExifData &exif_data = image->exifData();

	if (!exif_data.empty()) {
		double lat;
		double lon;
		double alt;

		if (geotag_exif_get_gps_info(exif_data, lat, lon, alt)) {

			/* Now create Waypoint with acquired information. */
			wp = new Waypoint();
			wp->visible = true;
			/* Set info from exif values. */
			/* Location. */
			wp->coord = Coord(LatLon(lat, lon), coord_mode);
			/* Altitude. */
			wp->altitude = Altitude(alt, HeightUnit::Metres); /* GPS info -> metres. */

			wp->name = geotag_get_exif_name(exif_data);

			wp->comment = geotag_get_exif_comment(exif_data);

			wp->set_image_full_path(file_full_path);
		}
	}

	return wp;
}




/**
   @file_full_path: The image file to process
   @wp:             An existing waypoint to update

   Returns: string with waypoint's name

   Here EXIF processing is used to get non position related information (i.e. just the comment and name).
*/
QString GeotagExif::waypoint_set_comment_get_name(const QString & file_full_path, Waypoint * wp)
{
	QString name = "";

	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	assert(image.get() != 0);
	image->readMetadata();
	Exiv2::ExifData &exif_data = image->exifData();

	if (!exif_data.empty()) {
		name = geotag_get_exif_name(exif_data);
		wp->comment = geotag_get_exif_comment(exif_data);
	}

	return name;
}




/**
   @file_full_path: The image file to process
   @has_GPS_info: Returns whether the file has existing GPS information

   Returns: A string with the date and time in EXIF_DATE_FORMAT, otherwise empty string on some kind of failure.

   Here EXIF processing is used to get time information.
*/
QString GeotagExif::get_exif_date_from_file(const QString & file_full_path, bool * has_GPS_info)
{
	QString date_time;
	*has_GPS_info = false;


	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	assert(image.get() != 0);
	image->readMetadata();
	Exiv2::ExifData &exif_data = image->exifData();

	if (!exif_data.empty()) {
		float lat;
		float lon;
		*has_GPS_info = (geotag_exif_get_float(exif_data, lat, "Exif.GPSInfo.GPSLatitude") && geotag_exif_get_float(exif_data, lon, "Exif.GPSInfo.GPSLongitude"));

		/* Prefer 'Photo' version over 'Image'. */
		if (!geotag_exif_get_string(exif_data, date_time, "Exif.Photo.DateTimeOriginal")) {
			qDebug() << "WW" PREFIX << "Failed to get Photo Date Time Original";
			if (!geotag_exif_get_string(exif_data, date_time, "Exif.Image.DateTimeOriginal")) {
				qDebug() << "WW" PREFIX << "Failed to get Image Date Time Original";
				/* We can't do anything more. */
			}
		}
	}

	return date_time;
}




static int write_exif_gps_data(const QString & file_full_path, const Coord & coord, const Altitude & alt)
{
	int result = 0;

	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	if (NULL == image.get()) {
		qDebug() << "EE" PREFIX << "get() failed on pointer to" << file_full_path << "after open()";
		result = 4;
		return result;
	}

	image->readMetadata();
	Exiv2::ExifData &exif_data = image->exifData();
	if (exif_data.empty()) {
		result = 0;
		return result;
	}

	const LatLon lat_lon = coord.get_latlon();

	if (!geotag_exif_set_gps_info(exif_data, lat_lon.lat, lat_lon.lon, alt.get_value())) {
		result = 1; /* Failed. */
		return result;
	}

	image->setExifData(exif_data);
	image->writeMetadata();

	return result;
}




/**
   @file_full_path: The image file to save information in
   @coord:          The location
   @alt:            The elevation

   Returns: A value indicating success: 0, or some other value for failure.
*/
int GeotagExif::write_exif_gps(const QString & file_full_path, const Coord & coord, const Altitude & alt, bool no_change_mtime)
{
	/* Save mtime for later use. */
	struct stat stat_save;
	if (no_change_mtime) {
		if (stat(file_full_path.toUtf8().constData(), &stat_save) != 0) {
			qDebug() << "WW: Geotag Exif" << __FUNCTION__ << "couldn't read file" << file_full_path;
		}
	}


	const int result = write_exif_gps_data(file_full_path, coord, alt);


	if (no_change_mtime) {
		/* Restore mtime, using the saved value. */
		struct stat stat_tmp;
		struct utimbuf utb;
		(void) stat(file_full_path.toUtf8().constData(), &stat_tmp);
		utb.actime = stat_tmp.st_atime;
		utb.modtime = stat_save.st_mtime;
		/* Not security critical, thus potential Time of Check Time of Use race condition is not bad. */
		/* coverity[toctou] */
		if (0 != utime(file_full_path.toUtf8().constData(), &utb)) {
			fprintf(stderr, "WARNING: %s couldn't set time on: %s\n", __FUNCTION__, file_full_path.toUtf8().constData());
		}
	}

	return result;
}
