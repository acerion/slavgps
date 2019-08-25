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




#define SG_MODULE "Geotag Exif"




bool SGExif::get_float(Exiv2::ExifData & exif_data, float & val, const char * key)
{
	Exiv2::ExifData::iterator iter = exif_data.findKey(Exiv2::ExifKey(key));
	if (iter == exif_data.end()) {
		return false;
	}
	val = iter->getValue()->toFloat();
	return true;
}




bool SGExif::set_float(Exiv2::ExifData & exif_data, float val, const char * key)
{
	exif_data[key] = Exiv2::floatToRationalCast(val);

	return true;
}




bool SGExif::get_string(Exiv2::ExifData & exif_data, QString & val, const char * key)
{
	Exiv2::ExifData::iterator iter = exif_data.findKey(Exiv2::ExifKey(key));
	if (iter == exif_data.end()) {
		return false;
	}
	val = QString::fromStdString(iter->getValue()->toString());
	return true;
}




bool SGExif::get_uint16(Exiv2::ExifData & exif_data, uint16_t & val, const char * key)
{
	Exiv2::ExifData::iterator iter = exif_data.findKey(Exiv2::ExifKey(key));
	if (iter == exif_data.end()) {
		return false;
	}

	val = (uint16_t) iter->getValue()->toLong();
	qDebug() << SG_PREFIX_I << "Read value" << val;
	return true;
}




static bool geotag_exif_get_gps_info(Exiv2::ExifData & exif_data, LatLon & lat_lon, Altitude & alti)
{
	float tmp_lat;
	float tmp_lon;
	float tmp_alti;

	if (false == SGExif::get_float(exif_data, tmp_lat, "Exif.GPSInfo.GPSLatitude")) {
		qDebug() << SG_PREFIX_W << "Can't find GPS Latitude";
		return false;
	}

	if (false == SGExif::get_float(exif_data, tmp_lon, "Exif.GPSInfo.GPSLongitude")) {
		qDebug() << SG_PREFIX_W << "Can't find GPS Longitude";
		return false;
	}

	if (false == SGExif::get_float(exif_data, tmp_alti, "Exif.GPSInfo.GPSAltitude")) {
		qDebug() << SG_PREFIX_W << "Can't find GPS Altitude";
		return false;
	}


	lat_lon = LatLon(tmp_lat, tmp_lon);
	alti = Altitude(tmp_alti, HeightUnit::Metres); /* GPS info, hence metres. */


	return true;
}




static bool geotag_exif_set_gps_info(Exiv2::ExifData & exif_data, double lat, double lon, double alt)
{
	if (false == SGExif::set_float(exif_data, lat, "Exif.GPSInfo.GPSLatitude")) {
		qDebug() << SG_PREFIX_W << "Can't set GPS Latitude";
		return false;
	}

	if (false == SGExif::set_float(exif_data, lon, "Exif.GPSInfo.GPSLongitude")) {
		qDebug() << SG_PREFIX_W << "Can't set GPS Longitude";
		return false;
	}

	if (false == SGExif::set_float(exif_data, alt, "Exif.GPSInfo.GPSAltitude")) {
		qDebug() << SG_PREFIX_W << "Can't set GPS Altitude";
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
LatLon GeotagExif::get_object_lat_lon(const QString & file_full_path)
{
	LatLon lat_lon;


	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	assert(image.get() != 0);
	image->readMetadata();
	Exiv2::ExifData &exif_data = image->exifData();

	if (!exif_data.empty()) {
		LatLon tmp_lat_lon;
		Altitude alti;
		if (geotag_exif_get_gps_info(exif_data, tmp_lat_lon, alti)) {
			lat_lon = tmp_lat_lon;
			qDebug() << SG_PREFIX_I << "Lat/Lon =" << lat_lon << "altitude =" << alti.to_string();
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

		LatLon tmp_lat_lon;
		Altitude alti;

		if (geotag_exif_get_gps_info(exif_data, tmp_lat_lon, alti)) {
			/* Now create Waypoint with acquired information. */
			wp = new Waypoint();
			wp->coord = Coord(tmp_lat_lon, coord_mode);
			wp->altitude = alti;
			wp->name = geotag_get_exif_name(exif_data);
			wp->comment = geotag_get_exif_comment(exif_data);
			wp->set_image_full_path(file_full_path);
		}
	}

	return wp;
}




/**
   @brief Extract a name from exif data of given image

   @file_full_path: The image file to process

   Returns: string with name

   Here EXIF processing is used to get non position related information.
*/
QString GeotagExif::get_object_name(const QString & file_full_path)
{
	QString result;

	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	assert(image.get() != 0);
	image->readMetadata();
	Exiv2::ExifData &exif_data = image->exifData();

	if (!exif_data.empty()) {
		result = geotag_get_exif_name(exif_data);
	}

	return result;
}




/**
   @brief Extract a comment from exif data of given image

   @file_full_path: The image file to process

   Returns: string with comment

   Here EXIF processing is used to get non position related information.
*/
QString GeotagExif::get_object_comment(const QString & file_full_path)
{
	QString result;

	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	assert(image.get() != 0);
	image->readMetadata();
	Exiv2::ExifData &exif_data = image->exifData();

	if (!exif_data.empty()) {
		result = geotag_get_exif_comment(exif_data);
	}

	return result;
}




/**
   @file_full_path: The image file to process

   Returns: A string with the date and time in EXIF_DATE_FORMAT, otherwise empty string on some kind of failure.

   Here EXIF processing is used to get time information.
*/
QString GeotagExif::get_object_datetime(const QString & file_full_path)
{
	QString date_time;

	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	assert(image.get() != 0);
	image->readMetadata();
	Exiv2::ExifData &exif_data = image->exifData();

	if (!exif_data.empty()) {
		/* Prefer 'Photo' version over 'Image'. */
		if (false == SGExif::get_string(exif_data, date_time, "Exif.Photo.DateTimeOriginal")) {
			qDebug() << SG_PREFIX_W << "Failed to get Photo Date Time Original";
			if (false == SGExif::get_string(exif_data, date_time, "Exif.Image.DateTimeOriginal")) {
				qDebug() << SG_PREFIX_W << "Failed to get Image Date Time Original";
				/* We can't do anything more. */
			}
		}
	}

	return date_time;
}




/**
   @file_full_path: The image file to process
   @has_GPS_info: Returns whether the file has existing GPS information

   Returns: true if file has GPS latitude/longitude, false otherwise
*/
bool GeotagExif::object_has_gps_info(const QString & file_full_path)
{
	bool result = false;

	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	assert(image.get() != 0);
	image->readMetadata();
	Exiv2::ExifData &exif_data = image->exifData();

	if (!exif_data.empty()) {
		float lat;
		float lon;
		result = (SGExif::get_float(exif_data, lat, "Exif.GPSInfo.GPSLatitude") && SGExif::get_float(exif_data, lon, "Exif.GPSInfo.GPSLongitude"));
	}

	return result;
}




static sg_ret write_exif_gps_data(const QString & file_full_path, const Coord & coord, const Altitude & alt)
{
	sg_ret retv = sg_ret::ok;

	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	if (NULL == image.get()) {
		qDebug() << SG_PREFIX_E << "get() failed on pointer to" << file_full_path << "after open()";
		return sg_ret::err;
	}

	image->readMetadata();
	Exiv2::ExifData &exif_data = image->exifData();
	if (exif_data.empty()) {
		return sg_ret::err;
	}

	const LatLon lat_lon = coord.get_lat_lon();

	if (!geotag_exif_set_gps_info(exif_data, lat_lon.lat, lat_lon.lon, alt.get_ll_value())) {
		return sg_ret::err;
	}

	image->setExifData(exif_data);
	image->writeMetadata();

	return sg_ret::ok;
}




/**
   @file_full_path: The image file to save information in
   @coord:          The location
   @alt:            The elevation
*/
sg_ret GeotagExif::write_exif_gps(const QString & file_full_path, const Coord & coord, const Altitude & alt, bool no_change_mtime)
{
	/* Save mtime for later use. */
	struct stat stat_save;
	if (no_change_mtime) {
		if (stat(file_full_path.toUtf8().constData(), &stat_save) != 0) {
			qDebug() << SG_PREFIX_E << "Couldn't read file" << file_full_path;
			return sg_ret::err;
		}
	}


	const sg_ret result = write_exif_gps_data(file_full_path, coord, alt);


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
			qDebug() << SG_PREFIX_E << "Couldn't set time on file" << file_full_path;
			return sg_ret::err; /* TODO: some kind of rollback? */
		}
	}

	return result;
}




Exiv2::ExifData SGExif::get_exif_data(const QString & file_full_path)
{
	Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(file_full_path.toUtf8().constData());
	assert(image.get() != 0);
	image->readMetadata();
	return image->exifData();
}




bool SGExif::get_image_orientation(sg_exif_image_orientation & result, const QString & file_full_path)
{
        uint16_t orientation;

	Exiv2::ExifData exif_data = SGExif::get_exif_data(file_full_path);
	if (false == SGExif::get_uint16(exif_data, orientation, "Exif.Image.Orientation")) {
		/* Not an error. Maybe there is no orientation info in the file. */
		return false;
	}

	result = orientation;
	qDebug() << SG_PREFIX_I << "Orientation of image" << file_full_path << "is" << result;

	return true;
}
