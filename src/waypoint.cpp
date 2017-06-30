/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#include <glib.h>
#include <cstring>
#include <cstdlib>
#include <mutex>

#include "coords.h"
#include "coord.h"
#include "waypoint.h"
#include "globals.h"
//#include "garminsymbols.h"
#include "dem_cache.h"
#include "util.h"




using namespace SlavGPS;




/* Simple UID implementation using an integer. */
static sg_uid_t global_wp_uid = SG_UID_INITIAL;
static std::mutex wp_uid_mutex;




Waypoint::Waypoint()
{
	this->name = strdup(_("Waypoint"));
	wp_uid_mutex.lock();
	this->uid = ++global_wp_uid;
	wp_uid_mutex.unlock();

	this->sublayer_type = SublayerType::WAYPOINT;

	/* kamilTODO: what about image_width / image_height? */
}




/* Copy constructor. */
Waypoint::Waypoint(const Waypoint & wp) : Waypoint()
{
	this->coord = wp.coord;
	this->visible = wp.visible;
	this->has_timestamp = wp.has_timestamp;
	this->timestamp = wp.timestamp;
	this->altitude = wp.altitude;

	this->set_name(wp.name);
	this->set_comment(wp.comment);
	this->set_description(wp.description);
	this->set_source(wp.source);
	this->set_type(wp.type);
	this->set_url(wp.url);
	this->set_image(wp.image);
	this->set_symbol(wp.symbol);

	/* kamilTODO: what about image_width / image_height? */
}




Waypoint::~Waypoint()
{
	free_string(&name);  /* kamilFIXME: I had to add free()ing of name. */
	free_string(&comment);
	free_string(&description);
	free_string(&source);
	free_string(&type);
	free_string(&url);
	free_string(&image);
	free_string(&symbol);
}




/* Hmmm tempted to put in new constructor. */
void Waypoint::set_name(char const * name_)
{
	free_string(&name);

	if (name_) {
		name = strdup(name_);
	}

	return;
}




void Waypoint::set_comment_no_copy(char * comment_)
{
	free_string(&comment); /* kamilTODO: should we free() string in _no_copy? */

	if (comment_) {
		comment = comment_;
	}
}




void Waypoint::set_comment(char const * comment_)
{
	free_string(&comment);

	if (comment_ && comment_[0] != '\0') {
		comment = strdup(comment_);
	}

	return;
}




void Waypoint::set_description(char const * description_)
{
	free_string(&description);

	if (description_ && description_[0] != '\0') {
		description = strdup(description_);
	}

}




void Waypoint::set_source(char const * source_)
{
	free_string(&source);

	if (source_ && source_[0] != '\0') {
		source = strdup(source_);
	}

}




void Waypoint::set_type(char const * type_)
{
	free_string(&type);

	if (type_ && type_[0] != '\0') {
		type = strdup(type_);
	}

}




void Waypoint::set_url(char const * url_)
{
	free_string(&url);

	if (url_ && url_[0] != '\0') {
		url = strdup(url_);
	}

}




void Waypoint::set_image(char const * image_)
{
	free_string(&image);

	if (image_ && image_[0] != '\0') {
		image = strdup(image_);
	}

	/* NOTE - ATM the image (thumbnail) size is calculated on demand when needed to be first drawn. */
}




void Waypoint::set_symbol(char const * symname_)
{
	free_string(&symbol);

	/* NB symbol_pixmap is just a reference, so no need to free it */

	if (symname_ && symname_[0] != '\0') {
#ifdef K
		char const * hashed_symname = a_get_hashed_sym(symname_);
		if (hashed_symname) {
			symname_ = hashed_symname;
		}
		symbol = strdup(symname_);
		this->symbol_pixmap = a_get_wp_sym(symbol);
#endif
	} else {
		symbol = NULL;
		this->symbol_pixmap = NULL;
	}
}




/**
 * @skip_existing: When true, don't change the elevation if the waypoint already has a value
 *
 * Set elevation data for a waypoint using available DEM information.
 *
 * Returns: true if the waypoint was updated
 */
bool Waypoint::apply_dem_data(bool skip_existing)
{
	bool updated = false;
	if (!(skip_existing && altitude != VIK_DEFAULT_ALTITUDE)) { /* kamilTODO: check this condition. */
		int16_t elev = DEMCache::get_elev_by_coord(&coord, DemInterpolation::BEST);
		if (elev != DEM_INVALID_ELEVATION) {
			altitude = (double) elev;
			updated = true;
		}
	}
	return updated;
}




/*
 * Take a Waypoint and convert it into a byte array.
 */
void Waypoint::marshall(uint8_t **data, size_t * datalen)
{
	GByteArray *b = g_byte_array_new();
	size_t len;

	/* This creates space for fixed sized members like ints and whatnot
	   and copies that amount of data from the waypoint to byte array. */
	g_byte_array_append(b, (uint8_t *) this, sizeof(Waypoint));

	/* This allocates space for variant sized strings
	   and copies that amount of data from the waypoint to byte array. */
#define vwm_append(s)						\
	len = (s) ? strlen(s) + 1 : 0;				\
	g_byte_array_append(b, (uint8_t *) &len, sizeof(len));	\
	if (s) g_byte_array_append(b, (uint8_t *) s, len);

	vwm_append(name);
	vwm_append(comment);
	vwm_append(description);
	vwm_append(source);
	vwm_append(type);
	vwm_append(url);
	vwm_append(image);
	vwm_append(symbol);

	*data = b->data;
	*datalen = b->len;
	g_byte_array_free(b, false);
#undef vwm_append
}




/*
 * Take a byte array and convert it into a Waypoint.
 */
Waypoint *Waypoint::unmarshall(uint8_t * data, size_t datalen)
{
	size_t len;
	Waypoint *wp = new Waypoint();

	/* This copies the fixed sized elements (i.e. visibility, altitude, image_width, etc...). */
	memcpy(wp, data, sizeof(Waypoint));
	data += sizeof (Waypoint);

	/* Now the variant sized strings... */
#define vwu_get(s)				\
	len = *(size_t *)data;			\
	data += sizeof (len);			\
	if (len) {				\
		(s) = g_strdup((char *) data);	\
	} else {				\
		(s) = NULL;			\
	}					\
	data += len;

	vwu_get(wp->name);
	fprintf(stderr, "---- name = '%s'\n", wp->name);
	vwu_get(wp->comment);
	fprintf(stderr, "---- comment = '%s'\n", wp->comment);
	vwu_get(wp->description);
	fprintf(stderr, "---- description = '%s'\n", wp->description);
	vwu_get(wp->source);
	fprintf(stderr, "---- source = '%s'\n", wp->source);
	vwu_get(wp->type);
	fprintf(stderr, "---- type = '%s'\n", wp->type);
	vwu_get(wp->url);
	fprintf(stderr, "---- url = '%s'\n", wp->url);
	vwu_get(wp->image);
	fprintf(stderr, "---- image = '%s'\n", wp->image);
	vwu_get(wp->symbol);
	fprintf(stderr, "---- symbol = '%s'\n", wp->symbol);

	return wp;
#undef vwu_get
}




void Waypoint::delete_waypoint(Waypoint * wp)
{
	delete wp;
	return;
}




void Waypoint::convert(CoordMode dest_mode)
{
	vik_coord_convert(&this->coord, dest_mode);
}
