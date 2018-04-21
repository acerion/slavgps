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
 *
 */

/*
 * This uses EXIF information from images to create waypoints at those positions
 *
 * The initial implementation uses libexif, which keeps Viking a pure C program.
 * Now libgexiv2 is available (in C as a wrapper around the more powerful libexiv2 [C++]) so this is the preferred build.
 *  The attentative reader will notice the use of gexiv2 is a lot simpler as well.
 * For the time being the libexif code + build is still made available.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QDebug>

#include <cstring>
#include <cstdlib>
#include <ctype.h>
#include <cmath>



#include "geotag_exif.h"
#include "config.h"
#include "file.h"
#include "layer_trw_waypoint.h"



#include <sys/stat.h>
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif



#ifdef K_INCLUDES
#include <glib/gstdio.h>
#ifdef HAVE_LIBGEXIV2
#include <gexiv2/gexiv2.h>
#endif
#ifdef HAVE_LIBEXIF
#include <libexif/exif-data.h>
#include "libjpeg/jpeg-data.h"
#endif
#endif




using namespace SlavGPS;



#ifdef K_TODO


#ifdef HAVE_LIBGEXIV2
/**
 * Attempt to get a single comment from the various exif fields.
 */
static QString geotag_get_exif_comment(GExiv2Metadata * gemd)
{
	QString result;

	// Try various options to create a comment.
	if (gexiv2_metadata_has_tag(gemd, "Exif.Image.ImageDescription")) {
		result = QString(gexiv2_metadata_get_tag_interpreted_string(gemd, "Exif.Image.ImageDescription"));
	} else if (gexiv2_metadata_has_tag(gemd, "Exif.Image.XPComment")) {
		result = QString(gexiv2_metadata_get_tag_interpreted_string(gemd, "Exif.Image.XPComment"));
	} else if (gexiv2_metadata_has_tag(gemd, "Exif.Image.XPSubject")) {
		result = QString(gexiv2_metadata_get_tag_interpreted_string(gemd, "Exif.Image.XPSubject"));
	} else if (gexiv2_metadata_has_tag(gemd, "Exif.Image.DateTimeOriginal")) {
		result = QString(gexiv2_metadata_get_tag_interpreted_string(gemd, "Exif.Image.DateTimeOriginal"));
	} else {
		; /* Otherwise nothing found. */
	}

	return result;
}
#endif




#ifdef HAVE_LIBEXIF
/**
 * Attempt to get a single comment from the various exif fields.
 */
static QString geotag_get_exif_comment(ExifData *ed)
{
	char str[128] = { 0 };
	QString result;
	ExifEntry *ee;

	/* Try various options to create a comment. */
	if (NULL != (ee = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_IMAGE_DESCRIPTION))) {
		exif_entry_get_value(ee, str, sizeof (128));
		result = str;

	} else if (NULL != (ee = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_XP_COMMENT))) {
		exif_entry_get_value(ee, str, sizeof (128));
		result = str;

	} else if (NULL != (ee = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_XP_SUBJECT))) {
		exif_entry_get_value(ee, str, sizeof (128));
		result = str;

	} else if (NULL != (ee = exif_content_get_entry(ed->ifd[EXIF_IFD_EXIF], EXIF_TAG_DATE_TIME_ORIGINAL))) {
		exif_entry_get_value(ee, str, sizeof (128));
		result = str;

	} else {
		; /* Otherwise nothing found. */
	}

	/* Consider using these for existing GPS info?? */
	//#define EXIF_TAG_GPS_TIME_STAMP        0x0007
	//#define EXIF_TAG_GPS_DATE_STAMP         0x001d

	return result;
}




/**
 * Handles 3 part location Rationals.
 * Handles 1 part rational (must specify 0 for the offset).
 */
static double Rational2Double(unsigned char *data, int offset, ExifByteOrder order)
{
	/*
	Explaination from GPS Correlate 'exif-gps.cpp' v 1.6.1
	What we are trying to do here is convert the three rationals:
	   dd/v mm/v ss/v
	To a decimal
	   dd.dddddd...
	dd/v is easy: result = dd/v.
	mm/v is harder:
	   mm
	   -- / 60 = result.
	    v
	ss/v is sorta easy.
	    ss
	    -- / 3600 = result
	     v
	Each part is added to the final number.
	*/
	double ans;
	ExifRational er;
	er = exif_get_rational(data, order);
	ans = (double) er.numerator / (double) er.denominator;
	if (offset <= 0) {
		return ans;
	}

	er = exif_get_rational(data + (1 * offset), order);
	ans = ans + (((double) er.numerator / (double) er.denominator) / 60.0);
	er = exif_get_rational (data+(2*offset), order);
	ans = ans + (((double) er.numerator / (double) er.denominator) / 3600.0);

	return ans;
}




/* Returns invalid value on errors. */
static LatLon get_latlon(ExifData * ed)
{
	LatLon lat_lon;
	char str[128];
	ExifEntry *ee;

	/* Lat & Long is necessary to form a waypoint. */
	ee = exif_content_get_entry(ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LATITUDE);
	if (!(ee && ee->components == 3 && ee->format == EXIF_FORMAT_RATIONAL)) {
		lat_lon.invalidate();
		return lat_lon;
	}

	lat_lon.lat = Rational2Double(ee->data,
				      exif_format_get_size(ee->format),
				      exif_data_get_byte_order(ed));

	ee = exif_content_get_entry(ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LATITUDE_REF);
	if (ee) {
		exif_entry_get_value(ee, str, 128);
		if (str[0] == 'S') {
			lat_lon.lat = -lat_lon.lat;
		}
	}

	ee = exif_content_get_entry(ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LONGITUDE);
	if (!(ee && ee->components == 3 && ee->format == EXIF_FORMAT_RATIONAL)) {
		lat_lon.invalidate();
		return lat_lon;
	}

	lat_lon.lon = Rational2Double(ee->data,
				      exif_format_get_size(ee->format),
				      exif_data_get_byte_order(ed));

	ee = exif_content_get_entry(ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LONGITUDE_REF);
	if (ee) {
		exif_entry_get_value(ee, str, 128);
		if (str[0] == 'W') {
			lat_lon.lon = -lat_lon.lon;
		}
	}

	return lat_lon;
}
#endif
#endif



/**
 * @filename: The (JPG) file with EXIF information in it
 *
 * Returns: The position in LatLon format.
 On errors the returned value will be invalid (LatLon::is_valid() will return false).
 */
LatLon SlavGPS::a_geotag_get_position(const char *filename)
{
	LatLon lat_lon;

#ifdef K_TODO
#ifdef HAVE_LIBGEXIV2
	GExiv2Metadata *gemd = gexiv2_metadata_new();
	if (gexiv2_metadata_open_path(gemd, filename, NULL)) {
		double lat;
		double lon;
		double alt;
		if (gexiv2_metadata_get_gps_info(gemd, &lon, &lat, &alt)) {
			lat_lon.lat = lat;
			lat_lon.lon = lon;
		}
	}
	gexiv2_metadata_free(gemd);
#else
#ifdef HAVE_LIBEXIF
	/* Open image with libexif. */
	ExifData *ed = exif_data_new_from_file(filename);

	/* Detect EXIF load failure. */
	if (!ed) {
		lat_lon.invalidate();
		return lat_lon;
	}

	ExifEntry *ee = exif_content_get_entry(ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_VERSION_ID);
	/* Confirm this has a GPS Id - normally "2.0.0.0" or "2.2.0.0". */
	if (!(ee && ee->components == 4)) {
		lat_lon.invalidate();
		goto MyReturn0;
	}

	lat_lon = get_latlon(ed); /* This function may return invalid lat_lon on error. */

MyReturn0:
	/* Finished with EXIF. */
	exif_data_free(ed);
#endif
#endif

#endif /* #ifdef K_TODO */
	return lat_lon;
}




/**
 * @filename: The image file to process
 * @vcmode:   The current location mode to use in the positioning of Waypoint
 * @name:     Returns a name for the Waypoint (can be empty)
 *
 * Returns: An allocated Waypoint or NULL if Waypoint could not be generated (e.g. no EXIF info).
 */
Waypoint * SlavGPS::a_geotag_create_waypoint_from_file(const QString & filename, CoordMode vcmode, QString & name)
{
	/* Default return values (for failures). */
	name = "";
	Waypoint * wp = NULL;
#ifdef K_TODO

#ifdef HAVE_LIBGEXIV2
	GExiv2Metadata *gemd = gexiv2_metadata_new();
	if (gexiv2_metadata_open_path(gemd, filename, NULL)) {
		double lat;
		double lon;
		double alt;
		if (gexiv2_metadata_get_gps_info(gemd, &lon, &lat, &alt)) {

			/* Now create Waypoint with acquired information. */
			wp = new Waypoint();
			wp->visible = true;
			/* Set info from exif values. */
			/* Location. */
			wp->coord = Coord(LatLon(lat, lon), vcmode);
			/* Altitude. */
			wp->altitude = alt;

			if (gexiv2_metadata_has_tag(gemd, "Exif.Image.XPTitle")) {
				name = Qstring(gexiv2_metadata_get_tag_interpreted_string(gemd, "Exif.Image.XPTitle"));
			}
			wp->comment = geotag_get_exif_comment(gemd);

			wp->set_image_full_path(filename);
		}
	}
	gexiv2_metadata_free(gemd);
#else
#ifdef HAVE_LIBEXIF
	/* TODO use log? */
	//ExifLog *log = NULL;

	/* Open image with libexif. */
	ExifData *ed = exif_data_new_from_file(filename);

	/* Detect EXIF load failure. */
	if (!ed) {
		/* Return with no Waypoint. */
		return wp;
	}


	char str[128];
	ExifEntry *ee;

	ee = exif_content_get_entry(ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_VERSION_ID);
	/* Confirm this has a GPS Id - normally "2.0.0.0" or "2.2.0.0". */
	if (!(ee && ee->components == 4)) {
		goto MyReturn;
	}
	/* Could test for these versions explicitly but may have byte order issues... */
	//if (!(ee->data[0] == 2 && ee->data[2] == 0 && ee->data[3] == 0)) {
	//	goto MyReturn;
	//}

	const LatLon lat_lon = get_latlon(ed);
	if (!lat_lon.is_valid()) {
		goto MyReturn;
	}

	/* Not worried if none of the other fields exist, as can default the values to something. */
	double alt = VIK_DEFAULT_ALTITUDE;
	ee = exif_content_get_entry(ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_ALTITUDE);
	if (ee && ee->components == 1 && ee->format == EXIF_FORMAT_RATIONAL) {
		alt = Rational2Double(ee->data,
				      0,
				      exif_data_get_byte_order(ed));

		ee = exif_content_get_entry(ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_ALTITUDE_REF);
		if (ee && ee->components == 1 && ee->format == EXIF_FORMAT_BYTE && ee->data[0] == 1) {
			alt = -alt;
		}
	}

	/* Name. */
	ee = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_XP_TITLE);
	if (ee) {
		exif_entry_get_value(ee, str, 128);
		name = QString(str);
	}

	/* Now create Waypoint with acquired information. */
	wp = vik_waypoint_new();
	wp->visible = true;
	/* Set info from exif values. */
	/* Location. */
	wp->coord = Coord(lat_lon, vcmode);
	/* Altitude. */
	wp->altitude = alt;

	wp->comment = geotag_get_exif_comment(ed);

	vik_waypoint_set_image(wp, filename);

MyReturn:
	/* Finished with EXIF. */
	exif_data_free(ed);
#endif
#endif

#endif /* #ifdef K_TODO */
	return wp;
}




/**
 * @filename: The image file to process
 * @coord:    The location for positioning the Waypoint
 * @name:     Returns a name for the Waypoint (can be NULL)
 * @waypoint: An existing waypoint to update (can be NULL to generate a new waypoint)
 *
 * Returns: An allocated waypoint if the input waypoint is NULL,
 * otherwise the passed in waypoint is updated.
 *
 * Here EXIF processing is used to get non position related information (i.e. just the comment).
 */
Waypoint * SlavGPS::a_geotag_waypoint_positioned(const char *filename, const Coord & coord, double alt, QString & name, Waypoint *wp)
{
	name = "";
	if (wp == NULL) {
		/* Need to create waypoint. */
		wp = new Waypoint();
		wp->visible = true;
	}
	wp->coord = coord;
	wp->altitude = alt;
#ifdef K_TODO

#ifdef HAVE_LIBGEXIV2
	GExiv2Metadata *gemd = gexiv2_metadata_new();
	if (gexiv2_metadata_open_path(gemd, filename, NULL)) {
			wp->comment = geotag_get_exif_comment (gemd);
			if (gexiv2_metadata_has_tag(gemd, "Exif.Image.XPTitle")) {
				name = QString(gexiv2_metadata_get_tag_interpreted_string(gemd, "Exif.Image.XPTitle"));
			}
	}
	gexiv2_metadata_free(gemd);
#else
#ifdef HAVE_LIBEXIF
	ExifData *ed = exif_data_new_from_file(filename);

	/* Set info from exif values. */
	if (ed) {
		wp->comment = geotag_get_exif_comment(ed);

		char str[128];
		ExifEntry *ee;
		/* Name. */
		ee = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_XP_TITLE);
		if (ee) {
			exif_entry_get_value(ee, str, 128);
			name = QString(str);
		}

		/* Finished with EXIF. */
		exif_data_free(ed);
	}
#endif
#endif

#endif /* #ifdef K_TODO */

	wp->set_image_full_path(filename);

	return wp;
}




/**
 * @filename: The image file to process
 * @has_GPS_info: Returns whether the file has existing GPS information
 *
 * Returns: An allocated string with the date and time in EXIF_DATE_FORMAT, otherwise NULL if some kind of failure.
 *
 * Here EXIF processing is used to get time information.
 */
QString SlavGPS::a_geotag_get_exif_date_from_file(const QString & filename, bool * has_GPS_info)
{
	QString datetime;
	*has_GPS_info = false;

#ifdef K_TODO

#ifdef HAVE_LIBGEXIV2
	GExiv2Metadata *gemd = gexiv2_metadata_new();
	if (gexiv2_metadata_open_path(gemd, filename.toUtf8().constData(), NULL)) {
		double lat, lon;
		*has_GPS_info = (gexiv2_metadata_get_gps_longitude(gemd,&lon) && gexiv2_metadata_get_gps_latitude(gemd,&lat));

		/* Prefer 'Photo' version over 'Image'. */
		if (gexiv2_metadata_has_tag(gemd, "Exif.Photo.DateTimeOriginal")) {
			datetime = QString(gexiv2_metadata_get_tag_interpreted_string(gemd, "Exif.Photo.DateTimeOriginal"));
		} else {
			datetime = QString(gexiv2_metadata_get_tag_interpreted_string(gemd, "Exif.Image.DateTimeOriginal"));
		}
	}
	gexiv2_metadata_free(gemd);
#else
#ifdef HAVE_LIBEXIF
	ExifData *ed = exif_data_new_from_file(filename.toUtf8().constData());

	/* Detect EXIF load failure. */
	if (!ed) {
		return datetime;
	}

	char str[128];
	ExifEntry *ee;

	ee = exif_content_get_entry(ed->ifd[EXIF_IFD_EXIF], EXIF_TAG_DATE_TIME_ORIGINAL);
	if (ee) {
		exif_entry_get_value(ee, str, sizeof (str));
		datetime = str;
	}

	/* Check GPS Info. */

	ee = exif_content_get_entry(ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_VERSION_ID);
	/* Confirm this has a GPS Id - normally "2.0.0.0" or "2.2.0.0". */
	if (ee && ee->components == 4) {
		*has_GPS_info = true;
	}

	/* Check other basic GPS fields exist too.
	   I have encountered some images which have just the EXIF_TAG_GPS_VERSION_ID but nothing else.
	   So to confirm check more EXIF GPS TAGS: */
	ee = exif_content_get_entry(ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LATITUDE);
	if (!ee) {
		*has_GPS_info = false;
	}
	ee = exif_content_get_entry(ed->ifd[EXIF_IFD_GPS], EXIF_TAG_GPS_LONGITUDE);
	if (!ee) {
		*has_GPS_info = false;
	}

	exif_data_free(ed);
#endif
#endif

#endif /* #ifdef K_TODO */

	return datetime;
}



#ifdef K_TODO
#ifdef HAVE_LIBEXIF
/**
   If the entry doesn't exist, create it.
   Based on exif command line action_create_value function in exif 0.6.20.
*/
static ExifEntry* my_exif_create_value(ExifData *ed, ExifTag tag, ExifIfd ifd)
{
	ExifEntry *e = exif_content_get_entry(ed->ifd[ifd], tag);
	if (!e) {
	    e = exif_entry_new();
	    exif_content_add_entry(ed->ifd[ifd], e);

		exif_entry_initialize(e, tag);

		/* exif_entry_initialize doesn't seem to do much, especially for the GPS tags
		   so have to setup fields ourselves: */
		e->tag = tag;

		if (tag == EXIF_TAG_GPS_VERSION_ID) {
			e->format = EXIF_FORMAT_BYTE;
			e->components = 4;
			e->size = sizeof (char) * e->components;
			if (e->data) {
				free(e->data);
			}
			e->data = malloc(e->size);
		}
		if (tag == EXIF_TAG_GPS_MAP_DATUM
		    || tag == EXIF_TAG_GPS_LATITUDE_REF
		    || tag == EXIF_TAG_GPS_LONGITUDE_REF
		    || tag == EXIF_TAG_GPS_PROCESSING_METHOD) {

			e->format = EXIF_FORMAT_ASCII;
			/* NB Allocation is handled later on when the actual string used is known. */
		}
		if (tag == EXIF_TAG_GPS_LATITUDE || tag == EXIF_TAG_GPS_LONGITUDE) {
			e->format = EXIF_FORMAT_RATIONAL;
			e->components = 3;
			e->size = sizeof (ExifRational) * e->components;
			if (e->data) {
				free(e->data);
			}
			e->data = malloc(e->size);
		}
		if (tag == EXIF_TAG_GPS_ALTITUDE) {
			e->format = EXIF_FORMAT_RATIONAL;
			e->components = 1;
			e->size = sizeof (ExifRational) * e->components;
			if (e->data) {
				free(e->data);
			}
			e->data = malloc(e->size);
		}
		if (tag == EXIF_TAG_GPS_ALTITUDE_REF) {
			e->components = 1;
			e->size = sizeof (char) * e->components;
			if (e->data) {
				free(e->data);
			}
			e->data = malloc(e->size);
		}
		/* The entry has been added to the IFD, so we can unref it. */
		//exif_entry_unref(e);
		/* Crashes later on, when saving to jpeg if the above unref is enabled!! */
		/* Some other malloc problem somewhere? */
	}
	return e;
}




/**
 *  Heavily based on convert_arg_to_entry from exif command line tool.
 *  But without ExifLog, exitting, use of g_* io functions
 *  and can take a double value instead of a string.
 */
static void convert_to_entry(const char *set_value, double gdvalue, ExifEntry *e, ExifByteOrder o)
{
	unsigned int i, numcomponents;
	char *value_p = NULL;
	char *buf = NULL;
	/*
	 * ASCII strings are handled separately,
	 * since they don't require any conversion.
	 */
	if (e->format == EXIF_FORMAT_ASCII ||
	    e->tag == EXIF_TAG_USER_COMMENT) {
		if (e->data) {
			free(e->data);
		}
		e->components = strlen(set_value) + 1;
		if (e->tag == EXIF_TAG_USER_COMMENT) {
			e->components += 8 - 1;
		}
		e->size = sizeof (char) * e->components;
		e->data = malloc(e->size);
		if (!e->data) {
			qDebug() << QObject::tr("WARNING: Not enough memory.");
			return;
		}
		if (e->tag == EXIF_TAG_USER_COMMENT) {
			/* Assume ASCII charset. */
			/* TODO: get this from the current locale. */
			memcpy((char *) e->data, "ASCII\0\0\0", 8);
			memcpy((char *) e->data + 8, set_value, strlen(set_value));
		} else {
			strcpy((char *) e->data, set_value);
		}
		return;
	}

	/*
	 * Make sure we can handle this entry.
	 */
	if ((e->components == 0) && *set_value) {
		qDebug() << QObject::tr("WARNING: Setting a value for this tag is unsupported!");
		return;
	}

	bool use_string = (set_value != NULL);
	if (use_string) {
		/* Copy the string so we can modify it. */
		buf = g_strdup(set_value);
		if (!buf) {
			return;
		}
		value_p = strtok(buf, " ");
	}

	numcomponents = e->components;
	for (i = 0; i < numcomponents; ++i) {
		unsigned char s;

		if (use_string) {
			if (!value_p) {
				qDebug() << QObject::tr("WARNING: Too few components specified (need %1, found %2)").arg(numcomponents).arg(i);
				return;
			}
			if (!isdigit(*value_p) && (*value_p != '+') && (*value_p != '-')) {
				qDebug() << QObject::tr("WARNING: Numeric value expected");
				return;
			}
		}

		s = exif_format_get_size(e->format);
		switch (e->format) {
		case EXIF_FORMAT_ASCII:
			qDebug() << QObject::tr("WARNING: This shouldn't happen!");
			return;
			break;
		case EXIF_FORMAT_SHORT:
			exif_set_short(e->data + (s * i), o, atoi(value_p));
			break;
		case EXIF_FORMAT_SSHORT:
			exif_set_sshort(e->data + (s * i), o, atoi(value_p));
			break;
		case EXIF_FORMAT_RATIONAL: {
			ExifRational er;

			double val = 0.0 ;
			if (use_string && value_p) {
				val = fabs(atol(value_p));
			} else {
				val = fabs(gdvalue);
			}

			if (i == 0) {
				/* One (or first) part rational. */

				/* Sneak peek into tag as location tags need rounding down to give just the degrees part. */
				if (e->tag == EXIF_TAG_GPS_LATITUDE || e->tag == EXIF_TAG_GPS_LONGITUDE) {
					er.numerator = (ExifLong) floor(val);
					er.denominator = 1.0;
				} else {
					/* I don't see any point in doing anything too complicated here,
					   such as trying to work out the 'best' denominator.
					   For the moment use KISS principle.
					   Fix a precision of 1/100 metre as that's more than enough for GPS accuracy especially altitudes! */
					er.denominator = 100.0;
					er.numerator = (ExifLong) (val * er.denominator);
				}
			}

			/* Now for Location 3 part rationals do Mins and Seconds format. */

			/* Rounded down minutes. */
			if (i == 1) {
				er.denominator = 1.0;
				er.numerator = (ExifLong) ((int) floor((val - floor(val)) * 60.0));
			}

			/* Finally seconds. */
			if (i == 2) {
				er.denominator = 100.0;

				/* Fractional minute. */
				double FracPart = ((val - floor(val)) * 60) - (double)(int) floor((val - floor(val)) * 60.0);
				er.numerator = (ExifLong) ((int)floor(FracPart * 6000)); /* Convert to seconds. */
			}
			exif_set_rational(e->data + (s * i), o, er);
			break;
		}
		case EXIF_FORMAT_LONG:
			exif_set_long(e->data + (s * i), o, atol(value_p));
			break;
		case EXIF_FORMAT_SLONG:
			exif_set_slong(e->data + (s * i), o, atol(value_p));
			break;
		case EXIF_FORMAT_BYTE:
		case EXIF_FORMAT_SBYTE:
		case EXIF_FORMAT_UNDEFINED: /* Treat as byte array. */
			e->data[s * i] = atoi(value_p);
			break;
		case EXIF_FORMAT_FLOAT:
		case EXIF_FORMAT_DOUBLE:
		case EXIF_FORMAT_SRATIONAL:
		default:
			qDebug() << QObject::tr("WARNING: Not yet implemented!");
			return;
		}

		if (use_string)
			value_p = strtok(NULL, " ");

	}

	free(buf);

	if (use_string) {
		if (value_p) {
			qDebug() << QObject::tr("WARNING: Warning; Too many components specified!");
		}
	}
}
#endif
#endif /* #ifdef K_TODO */



/**
 * @filename: The image file to save information in
 * @coord:    The location
 * @alt:      The elevation
 *
 * Returns: A value indicating success: 0, or some other value for failure.
 */
int SlavGPS::a_geotag_write_exif_gps(const QString & filename, const Coord & coord, double alt, bool no_change_mtime)
{
	int result = 0; /* OK so far... */

	/* Save mtime for later use. */
	struct stat stat_save;
	if (no_change_mtime) {
		if (stat(filename.toUtf8().constData(), &stat_save) != 0) {
			qDebug() << "WW: Geotag Exif" << __FUNCTION__ << "couldn't read file" << filename;
		}
	}
#ifdef K_TODO
#ifdef HAVE_LIBGEXIV2
	GExiv2Metadata *gemd = gexiv2_metadata_new();
	if (gexiv2_metadata_open_path(gemd, filename, NULL)) {
		const LatLon lat_lon = coord.get_latlon();
		if (!gexiv2_metadata_set_gps_info(gemd, lat_lon.lon, lat_lon.lat, alt)) {
			result = 1; /* Failed. */
		} else {
			GError *error = NULL;
			if (! gexiv2_metadata_save_file(gemd, filename, &error)) {
				result = 2;
				fprintf(stderr, "WARNING: Write EXIF failure:%s\n" , error->message);
				g_error_free(error);
			}
		}
	}
	gexiv2_metadata_free(gemd);
#else
#ifdef HAVE_LIBEXIF
	/*
	  Appears libexif doesn't actually support writing EXIF data directly to files.
	  Thus embed command line exif writing method within Viking
	  (for example this is done by Enlightment - http://www.enlightenment.org/).
	  This appears to be JPEG only, but is probably 99% of our use case.
	  Alternatively consider using libexiv2 and C++...
	*/

	/* Actual EXIF settings here... */
	JPEGData *jdata;

	/* Parse the JPEG file. */
	jdata = jpeg_data_new();
	jpeg_data_load_file(jdata, filename);

	// Get current values
	ExifData *ed = exif_data_new_from_file(filename);
	if (!ed) {
		ed = exif_data_new();
	}

	/* Update ExifData with our new settings. */
	ExifEntry *ee;

	/* I don't understand it, but when saving the 'ed' nothing gets set after putting in the GPS ID tag - so it must come last
	   (unless of course there is some bug in the setting of the ID, that prevents subsequent tags). */

	ee = my_exif_create_value(ed, EXIF_TAG_GPS_ALTITUDE, EXIF_IFD_GPS);
	convert_to_entry(NULL, alt, ee, exif_data_get_byte_order(ed));

	/* Byte 0 meaning "sea level" or 1 if the value is negative. */
	ee = my_exif_create_value(ed, EXIF_TAG_GPS_ALTITUDE_REF, EXIF_IFD_GPS);
	convert_to_entry(alt < 0.0 ? "1" : "0", 0.0, ee, exif_data_get_byte_order(ed));

	ee = my_exif_create_value(ed, EXIF_TAG_GPS_PROCESSING_METHOD, EXIF_IFD_GPS);
	/* See http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/GPS.html */
	convert_to_entry("MANUAL", 0.0, ee, exif_data_get_byte_order(ed));

	ee = my_exif_create_value(ed, EXIF_TAG_GPS_MAP_DATUM, EXIF_IFD_GPS);
	convert_to_entry("WGS-84", 0.0, ee, exif_data_get_byte_order(ed));

	const LatLon lat_lon = coord.get_latlon();

	ee = my_exif_create_value(ed, EXIF_TAG_GPS_LATITUDE_REF, EXIF_IFD_GPS);
	/* N or S. */
	convert_to_entry(lat_lon.lat < 0.0 ? "S" : "N", 0.0, ee, exif_data_get_byte_order(ed));

	ee = my_exif_create_value(ed, EXIF_TAG_GPS_LATITUDE, EXIF_IFD_GPS);
	convert_to_entry(NULL, lat_lon.lat, ee, exif_data_get_byte_order(ed));

	ee = my_exif_create_value(ed, EXIF_TAG_GPS_LONGITUDE_REF, EXIF_IFD_GPS);
	/* E or W. */
	convert_to_entry(lat_lon.lon < 0.0 ? "W" : "E", 0.0, ee, exif_data_get_byte_order(ed));

	ee = my_exif_create_value(ed, EXIF_TAG_GPS_LONGITUDE, EXIF_IFD_GPS);
	convert_to_entry(NULL, lat_lon.lon, ee, exif_data_get_byte_order(ed));

	ee = my_exif_create_value(ed, EXIF_TAG_GPS_VERSION_ID, EXIF_IFD_GPS);
	//convert_to_entry("2 0 0 0", 0.0, ee, exif_data_get_byte_order(ed));
	convert_to_entry("2 2 0 0", 0.0, ee, exif_data_get_byte_order(ed));

	jpeg_data_set_exif_data(jdata, ed);

	if (jdata) {
		/* Save the modified image. */
		result = jpeg_data_save_file(jdata, filename);

		/* Convert result from 1 for success, 0 for failure into our scheme. */
		result = !result;

		jpeg_data_unref(jdata);
	} else {
		/* Epic fail - file probably not a JPEG. */
		result = 2;
	}

	exif_data_free(ed);
#endif
#endif

	if (no_change_mtime) {
		/* Restore mtime, using the saved value. */
		struct stat stat_tmp;
		struct utimbuf utb;
		(void)stat(filename, &stat_tmp);
		utb.actime = stat_tmp.st_atime;
		utb.modtime = stat_save.st_mtime;
		/* Not security critical, thus potential Time of Check Time of Use race condition is not bad. */
		/* coverity[toctou] */
		if (g_utime(filename, &utb) != 0) {
			fprintf(stderr, "WARNING: %s couldn't set time on: %s\n", __FUNCTION__, filename);
		}
	}
#endif /* #ifdef K_TODO */
	return result;
}
