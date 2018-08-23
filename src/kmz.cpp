/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "kmz.h"

#ifdef HAVE_ZIP_H
#include <zip.h>
#endif

#include <glib/gstdio.h>
#ifdef HAVE_EXPAT_H
#include <expat.h>
#endif




using namespace SlavGPS;




#ifdef HAVE_ZIP_H
/**
 * Simple KML 'file' with a Ground Overlay.
 *
 * See https://developers.google.com/kml/documentation/kmlreference
 *
 * AFAIK the projection is always in Web Mercator.
 * Probably for normal use case of not too large an area coverage (on a Garmin device) the projection is near enough...
 */
/* Hopefully image_filename will not break the XML file tag structure. */
static char * doc_kml_str(const QString & file_name, const char * image_filename, double north, double south, double east, double west)
{
	char *doc_kml = g_strdup_printf(
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<kml xmlns=\"http://www.opengis.net/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\" xmlns:kml=\"http://www.opengis.net/kml/2.2\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n"
		"<GroundOverlay>\n"
		"  <name>%s</name>\n"
		"  <Icon>\n"
		"    <href>%s</href>\n"
		"  </Icon>\n"
		"  <LatLonBox>\n"
		"    <north>%s</north>\n"
		"    <south>%s</south>\n"
		"    <east>%s</east>\n"
		"    <west>%s</west>\n"
		"    <rotation>0</rotation>\n" // Rotation always zero
		"  </LatLonBox>\n"
		"</GroundOverlay>\n"
		"</kml>\n",
		file_name.toUtf8().constData(), image_filename,
		SGUtils::double_to_c(north).toUtf8().constData(),
		SGUtils::double_to_c(south).toUtf8().constData(),
		SGUtils::double_to_c(east).toUtf8().constData(),
		SGUtils::double_to_c(west).toUtf8().constData());

	return doc_kml;
}
#endif




/**
 * @pixmap:   The image to save
 * @file_full_path: Save the KMZ as this file path
 * @north:    Top latitude in degrees
 * @east:     Right most longitude in degrees
 * @south:    Bottom latitude in degrees
 * @west:     Left most longitude in degrees
 *
 * Returns:
 *  -1 if KMZ not supported (this shouldn't happen)
 *   0 on success
 *   >0 some kind of error
 *
 * Mostly intended for use with as a Custom Map on a Garmin.
 *
 * See http://garminbasecamp.wikispaces.com/Custom+Maps
 *
 * The KMZ is a zipped file containing a KML file with the associated image.
 */
int SlavGPS::kmz_save_file(const QPixmap & pixmap, const QString & file_full_path, double north, double east, double south, double west)
{
#ifdef HAVE_ZIP_H
/* Older libzip compatibility: */
#ifndef zip_source_t
typedef struct zip_source zip_source_t;
#endif

	int ans = ZIP_ER_OK;
	char *image_filename = "image.jpg";

	/* Generate KMZ file (a zip file). */
	struct zip* archive = zip_open(file_full_path, ZIP_CREATE | ZIP_TRUNCATE, &ans);
	if (!archive) {
		fprintf(stderr, "WARNING: Unable to create archive: '%s' Error code %d\n", file_full_path, ans);
		return ans;
	}

	/* Generate KML file. */
	char *dk = doc_kml_str(FileUtils::get_base_name(file_full_path), image_filename, north, south, east, west);
	int dkl = strlen(dk);

	/* KML must be named doc.kml in the kmz file. */
	zip_source_t *src_kml = zip_source_buffer(archive, dk, dkl, 0);
	zip_file_add(archive, "doc.kml", src_kml, ZIP_FL_OVERWRITE);

	GError *error = NULL;
	char *buffer;
	size_t blen;
	gdk_pixbuf_save_to_buffer(pixmap, &buffer, &blen, "jpeg", &error, "x-dpi", "72", "y-dpi", "72", NULL);
	if (error) {
		fprintf(stderr, "WARNING: Save to buffer error: %s\n", error->message);
		g_error_free(error);
		zip_discard(archive);
		ans = 130;

		free(dk);
		return ans;
	}

	zip_source_t *src_img = zip_source_buffer(archive, buffer, (int)blen, 0);
	zip_file_add(archive, image_filename, src_img, ZIP_FL_OVERWRITE);
	/* NB Only store as limited use trying to (further) compress a JPG. */
	zip_set_file_compression(archive, 1, ZIP_CM_STORE, 0);

	ans = zip_close(archive);

	free(buffer);

 kml_cleanup:
	free(dk);
 finish:
	return ans;
#else
	return -1;
#endif
}




typedef enum {
	tt_unknown = 0,
	tt_kml,
	tt_kml_go,
	tt_kml_go_name,
	tt_kml_go_image,
	tt_kml_go_latlonbox,
	tt_kml_go_latlonbox_n,
	tt_kml_go_latlonbox_e,
	tt_kml_go_latlonbox_s,
	tt_kml_go_latlonbox_w,
} xtag_type;

typedef struct {
	xtag_type tag_type;             /* Enum from above for this tag. */
	const char *tag_name;           /* xpath-ish tag name. */
} xtag_mapping;

typedef struct {
	GString *xpath;
	GString *c_cdata;
	xtag_type current_tag;
	char *name;
	char *image; // AKA icon
	double north;
	double east;
	double south;
	double west;
} xml_data;




#ifdef HAVE_ZIP_H
#ifdef HAVE_EXPAT_H
/* NB No support for orientation ATM. */
static xtag_mapping xtag_path_map[] = {
	{ tt_kml,                "/kml" },
	{ tt_kml_go,             "/kml/GroundOverlay" },
	{ tt_kml_go_name,        "/kml/GroundOverlay/name" },
	{ tt_kml_go_image,       "/kml/GroundOverlay/Icon/href" },
	{ tt_kml_go_latlonbox,   "/kml/GroundOverlay/LatLonBox" },
	{ tt_kml_go_latlonbox_n, "/kml/GroundOverlay/LatLonBox/north" },
	{ tt_kml_go_latlonbox_e, "/kml/GroundOverlay/LatLonBox/east" },
	{ tt_kml_go_latlonbox_s, "/kml/GroundOverlay/LatLonBox/south" },
	{ tt_kml_go_latlonbox_w, "/kml/GroundOverlay/LatLonBox/west" },
};




/* NB Don't be pedantic about matching case of strings for tags. */
static xtag_type get_tag(const char *t)
{
	xtag_mapping *tm;
	for (tm = xtag_path_map; tm->tag_type != 0; tm++) {
		if (0 == g_ascii_strcasecmp(tm->tag_name, t)) {
			return tm->tag_type;
		}
	}
	return tt_unknown;
}




static void kml_start(xml_data *xd, const char *el, const char **attr)
{
	g_string_append_c(xd->xpath, '/');
	g_string_append(xd->xpath, el);

	xd->current_tag = get_tag(xd->xpath->str);
	switch (xd->current_tag) {
	case tt_kml_go_name:
	case tt_kml_go_image:
	case tt_kml_go_latlonbox_n:
	case tt_kml_go_latlonbox_s:
	case tt_kml_go_latlonbox_e:
	case tt_kml_go_latlonbox_w:
		g_string_erase(xd->c_cdata, 0, -1);
		break;
	default: break;  /* Ignore cdata from other things. */
	}
}




static void kml_end(xml_data *xd, const char *el)
{
	g_string_truncate(xd->xpath, xd->xpath->len - strlen(el) - 1);

	switch (xd->current_tag) {
	case tt_kml_go_name:
		xd->name = g_strdup(xd->c_cdata->str);
		g_string_erase(xd->c_cdata, 0, -1);
		break;
	case tt_kml_go_image:
		xd->image = g_strdup(xd->c_cdata->str);
		g_string_erase(xd->c_cdata, 0, -1);
		break;
	case tt_kml_go_latlonbox_n:
		xd->north = SGUtils::c_to_double(xd->c_cdata->str);
		g_string_erase(xd->c_cdata, 0, -1);
		break;
	case tt_kml_go_latlonbox_s:
		xd->south = SGUtils::c_to_double(xd->c_cdata->str);
		g_string_erase(xd->c_cdata, 0, -1);
		break;
	case tt_kml_go_latlonbox_e:
		xd->east = SGUtils::c_to_double(xd->c_cdata->str);
		g_string_erase(xd->c_cdata, 0, -1);
		break;
	case tt_kml_go_latlonbox_w:
		xd->west = SGUtils::c_to_double(xd->c_cdata->str);
		g_string_erase(xd->c_cdata, 0, -1);
		break;
	default:
		break;
	}

	xd->current_tag = get_tag(xd->xpath->str);
}




static void kml_cdata(xml_data *xd, const XML_Char *s, int len)
{
	switch (xd->current_tag) {
	case tt_kml_go_name:
	case tt_kml_go_image:
	case tt_kml_go_latlonbox_n:
	case tt_kml_go_latlonbox_s:
	case tt_kml_go_latlonbox_e:
	case tt_kml_go_latlonbox_w:
		g_string_append_len(xd->c_cdata, s, len);
		break;
	default: break;  /* ignore cdata from other things. */
	}
}
#endif




static bool parse_kml(const char * buffer, int len, char ** name, char ** image, double * north, double * south, double * east, double * west)
{
#ifdef HAVE_EXPAT_H
	XML_Parser parser = XML_ParserCreate(NULL);
	enum XML_Status status = XML_STATUS_ERROR;

	xml_data * xd = (xml_data *) malloc(sizeof (xml_data));
	/* Set default (invalid) values. */
	xd->xpath = g_string_new("");
	xd->c_cdata = g_string_new("");
	xd->current_tag = tt_unknown;
	xd->north = NAN;
	xd->south = NAN;
	xd->east = NAN;
	xd->west = NAN;
	xd->name = NULL;
	xd->image = NULL;

	XML_SetElementHandler(parser, (XML_StartElementHandler) kml_start, (XML_EndElementHandler) kml_end);
	XML_SetUserData(parser, xd);
	XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler) kml_cdata);

	status = XML_Parse(parser, buffer, len, true);

	XML_ParserFree(parser);

	*north = xd->north;
	*south = xd->south;
	*east = xd->east;
	*west = xd->west;
	*name = xd->name; /* NB don't free xd->name. */
	*image = xd->image; /* NB don't free xd->image. */

	g_string_free(xd->xpath, true);
	g_string_free(xd->c_cdata, true);
	free(xd);

	return status != XML_STATUS_ERROR;
#else
	return false;
#endif
}
#endif




/**
   @file_full_path
   @viewport
   @panel - the panel that the converted KMZ will be stored in

   @return tuple <KMZOpenStatus::Success, ...> on success,
   @return tuple <KMZOpenStatus::ZipError, zip-err-code> on zip errors,
   @return tuple <KMZOpenStatus::some-value, ...> on KMZ errors.
*/
std::tuple<KMZOpenStatus, int> SlavGPS::kmz_open_file(const QString & file_full_path, Viewport * viewport, LayersPanel * panel)
{
	std::tuple<KMZOpenStatus, int> ret;

#ifdef HAVE_ZIP_H
	/* Older libzip compatibility: */
#ifndef zip_t
	typedef struct zip zip_t;
	typedef struct zip_file zip_file_t;
#endif
#ifndef ZIP_RDONLY
#define ZIP_RDONLY 0
#endif

	/* Unzip. */
	int zip_status = ZIP_ER_OK;
	zip_t * archive = zip_open(file_full_path, ZIP_RDONLY, &zip_status);
	std::get<SG_KMZ_OPEN_ZIP>(ret) = zip_status;
	if (!archive) {
		qDebug() << SG_PREFIX_W << "Unable to open archive" << file_full_path << "Error code =" << zip_status;
		std::get<SG_KMZ_OPEN_KML>(ret) = KMZOpenStatus::ZipError;
		return ret;
	}

	zip_int64_t zindex = zip_name_locate(archive, "doc.kml", ZIP_FL_NOCASE | ZIP_FL_ENC_GUESS);
	if (zindex == -1) {
		fprintf(stderr, "WARNING: Unable to find doc.kml\n");
		std::get<SG_KMZ_OPEN_KML>(ret) = KMZOpenStatus::KMZOpenStatus::NoDoc; /* TODO_LATER: in original viking code we didn't return a "no doc.kml" status here, even though there was a status value defined for this event. */
		goto kmz_cleanup;
	}

	struct zip_stat zs;
	if (zip_stat_index(archive, zindex, 0, &zs) == 0) {
		zip_file_t *zf = zip_fopen_index(archive, zindex, 0);
		char *buffer = (char *) malloc(zs.size);
		int len = zip_fread(zf, buffer, zs.size);
		if (len != zs.size) {
			std::get<SG_KMZ_OPEN_KML>(ret) = KMZOpenStatus::KMZOpenStatus::NoDoc;
			free(buffer);
			fprintf(stderr, "WARNING: Unable to read doc.kml from zip file\n");
			goto kmz_cleanup;
		}

		double north, south, east, west;
		char *name = NULL;
		char *image = NULL;
		bool parsed = parse_kml(buffer, len, &name, &image, &north, &south, &east, &west);
		free(buffer);

		QPixmap * pixmap = NULL;

		if (parsed) {
			/* Read zip for image... */
			if (image) {
				if (zip_stat(archive, image, ZIP_FL_NOCASE | ZIP_FL_ENC_GUESS, &zs) == 0) {
					zip_file_t *zfi = zip_fopen_index(archive, zs.index, 0);
					/* Don't know a way to create a pixmap using streams.
					   Thus write out to file.
					   Could read in chunks rather than one big buffer, but don't expect images to be that big. */
					char *ibuffer = (char *) malloc(zs.size);
					int ilen = zip_fread(zfi, ibuffer, zs.size);
					if (ilen != zs.size) {
						std::get<SG_KMZ_OPEN_KML>(ret) = KMZOpenStatus::NoImage;
						fprintf(stderr, "WARNING: Unable to read %s from zip file\n", image);
					} else {
						const QString image_file = Util::write_tmp_file_from_bytes(ibuffer, ilen);
						pixmap = new QPixmap();
						if (!pixmap->load(image_file)) {
							delete pixmap;
							pixmap = NULL;
							qDebug() << "WW: KMZ: failed to load pixmap from" << image_file;
							std::get<SG_KMZ_OPEN_KML>(ret) = KMZOpenStatus::ImageFileProblem;
						} else {
							std::get<SG_KMZ_OPEN_KML>(ret) = KMZOpenStatus::Success;
							Util::remove(image_file);
						}
					}
					free(ibuffer);
				}
				free(image);
			} else {
				std::get<SG_KMZ_OPEN_KML>(ret) = KMZOpenStatus::CantGetImage;
			}
		} else {
			std::get<SG_KMZ_OPEN_KML>(ret) = KMZOpenStatus::CantUnderstandDoc;
		}

		if (pixmap) {
			/* Some simple detection of broken position values ?? */
			//if (xd->north > 90.0 || xd->north < -90.0 || xd->south > 90.0 || xd->south < -90.0)

			const Coord coord_tl(LatLon(north, west), viewport->get_coord_mode());
			const Coord coord_br(LatLon(south, east), viewport->get_coord_mode());

			Layer * grl = georef_layer_create(viewport, QString(name), pixmap, coord_tl, coord_br);
			if (grl) {
				LayerAggregate * top = panel->get_top_layer();
				top->add_layer(grl, false);
			}
		}
	}

kmz_cleanup:
	zip_discard(archive); /* Close and ensure unchanged. */
 cleanup:
	return ret;
#else
	std::get<SG_KMZ_OPEN_KML>(ret) = KMZOpenStatus::KMZNotSupported;
	return ret;
#endif
}
