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
#include "layer_trw.h"
#include "layer_trw_menu.h"
//#include "garminsymbols.h"
#include "dem_cache.h"
#include "util.h"
#include "window.h"
#include "tree_view_internal.h"
#include "waypoint_properties.h"




using namespace SlavGPS;




extern Tree * g_tree;

extern bool g_have_astro_program;
extern bool g_have_diary_program;




/* Simple UID implementation using an integer. */
static sg_uid_t global_wp_uid = SG_UID_INITIAL;
static std::mutex wp_uid_mutex;




Waypoint::Waypoint()
{
	this->tree_item_type = TreeItemType::SUBLAYER;

	this->name = tr("Waypoint");
	wp_uid_mutex.lock();
	this->uid = ++global_wp_uid;
	wp_uid_mutex.unlock();

	this->type_id = "sg.trw.waypoint";

	/* kamilTODO: what about image_width / image_height? */
}




/* Copy constructor. */
Waypoint::Waypoint(const Waypoint & wp) : Waypoint()
{
	this->tree_item_type = TreeItemType::SUBLAYER;

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
	this->set_symbol_name(wp.symbol_name);

	/* kamilTODO: what about image_width / image_height? */
}




Waypoint::~Waypoint()
{
	/* kamilFIXME: in C code I had to add free()ing of Waypoint::name. */
}




/* Hmmm tempted to put in new constructor. */
void Waypoint::set_name(const QString & new_name)
{
	this->name = new_name;
}




void Waypoint::set_comment(const QString & new_comment)
{
	this->comment = new_comment;
}




void Waypoint::set_description(const QString & new_description)
{
	this->description = new_description;
}




void Waypoint::set_source(const QString & new_source)
{
	this->source = new_source;
}




void Waypoint::set_type(const QString & new_type)
{
	this->type = new_type;
}




void Waypoint::set_url(const QString & new_url)
{
	this->url = new_url;
}




void Waypoint::set_image(const QString & new_image)
{
	this->image = new_image;
	/* NOTE - ATM the image (thumbnail) size is calculated on demand when needed to be first drawn. */
}




void Waypoint::set_symbol_name(const QString & new_symbol_name)
{
	/* this->symbol_pixmap is just a reference, so no need to free it. */

	if (new_symbol_name.isEmpty()) {
		this->symbol_name = "";
		this->symbol_pixmap = NULL;
	} else {
#ifdef K
		char const * hashed_symname = a_get_hashed_sym(new_symbol_name);
		if (hashed_symname) {
			new_symbol_name = hashed_symname;
		}
		this->symbol_name = new_symbol_name;
		this->symbol_pixmap = a_get_wp_sym(symbol);
#endif
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

	vwm_append(name.toUtf8().constData());
	vwm_append(comment.toUtf8().constData());
	vwm_append(description.toUtf8().constData());
	vwm_append(source.toUtf8().constData());
	vwm_append(type.toUtf8().constData());
	vwm_append(url.toUtf8().constData());
	vwm_append(image.toUtf8().constData());
	vwm_append(symbol_name.toUtf8().constData());

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
#ifdef K
	vwu_get(wp->name);
	vwu_get(wp->comment);
	vwu_get(wp->description);
	vwu_get(wp->source);
	vwu_get(wp->type);
	vwu_get(wp->url);
	vwu_get(wp->image);
	vwu_get(wp->symbol_name);
#endif

	return wp;
#undef vwu_get
}




void Waypoint::convert(CoordMode dest_mode)
{
	this->coord.change_mode(dest_mode);
}




bool Waypoint::has_any_url(void) const
{
	return !this->url.isEmpty()
		|| (!this->comment.isEmpty() && this->comment.left(4) == "http")
		|| (!this->description.isEmpty() && this->description.left(4) == "http");
}




QString Waypoint::get_any_url(void) const
{
	if (!this->url.isEmpty()) {
		return this->url;
	} else if (this->comment.left(4) == "http") {
		return this->comment;
	} else if (this->description.left(4) == "http") {
		return this->description;
	} else {
		return QString("");
	}
}



void Waypoint::sublayer_menu_waypoint_misc(LayerTRW * parent_layer_, QMenu & menu)
{
	QAction * qa = NULL;


	/* Could be a right-click using the tool. */
	if (g_tree->tree_get_layers_panel() != NULL) {
		qa = menu.addAction(QIcon::fromTheme("go-jump"), tr("&Go to this Waypoint"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (go_to_selected_waypoint_cb()));
	}

	if (!this->name.isEmpty()) {
		if (is_valid_geocache_name(this->name.toUtf8().constData())) {
			qa = menu.addAction(QIcon::fromTheme("go-jump"), tr("&Visit Geocache Webpage"));
				connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (waypoint_geocache_webpage_cb()));
		}
#ifdef VIK_CONFIG_GEOTAG
		qa = menu.addAction(QIcon::fromTheme("go-jump"), tr("Geotag &Images..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (geotagging_waypoint_cb()));
		qa->setToolTip(tr("Geotag multiple images against this waypoint"));
#endif
	}


	if (!this->image.isEmpty()) {
		/* Set up image parameter. */
		parent_layer_->menu_data->string = this->image;

		qa = menu.addAction(QIcon::fromTheme("vik-icon-Show Picture"), tr("&Show Picture...")); /* TODO: icon. */
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (show_picture_cb()));

#ifdef VIK_CONFIG_GEOTAG
		{
			QMenu * geotag_submenu = menu.addMenu(QIcon::fromTheme("view-refresh"), tr("Update Geotag on &Image"));

			qa = geotag_submenu->addAction(tr("&Update"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (geotagging_waypoint_mtime_update_cb()));

			qa = geotag_submenu->addAction(tr("Update and &Keep File Timestamp"));
			connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (geotagging_waypoint_mtime_keep_cb()));
		}
#endif
	}

	if (this->has_any_url()) {
		qa = menu.addAction(QIcon::fromTheme("applications-internet"), tr("Visit &Webpage"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (waypoint_webpage_cb()));
	}
}




bool Waypoint::add_context_menu_items(QMenu & menu)
{
	QAction * qa = NULL;
	bool rv = false;

	rv = true;

	qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Properties"));
	connect(qa, SIGNAL (triggered(bool)), (LayerTRW *) this, SLOT (properties_dialog_cb()));


	layer_trw_sublayer_menu_waypoint_track_route_edit((LayerTRW *) this->owning_layer, menu);


	menu.addSeparator();


	this->sublayer_menu_waypoint_misc((LayerTRW *) this->owning_layer, menu);


	if (g_tree->tree_get_layers_panel()) {
		rv = true;
		qa = menu.addAction(QIcon::fromTheme("document-new"), tr("&New Waypoint..."));
		connect(qa, SIGNAL (triggered(bool)), (LayerTRW *) this->owning_layer, SLOT (new_waypoint_cb()));
	}


	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));

	/* These are only made available if a suitable program is installed. */
	if (g_have_astro_program || g_have_diary_program) {
		layer_trw_sublayer_menu_track_waypoint_diary_astro((LayerTRW *) this->owning_layer, menu, external_submenu);
	}


	layer_trw_sublayer_menu_all_add_external_tools((LayerTRW *) this->owning_layer, menu, external_submenu);


	layer_trw_sublayer_menu_waypoints_waypoint_transform((LayerTRW *) this->owning_layer, menu);


	return rv;
}




void Waypoint::properties_dialog_cb(void)
{
	if (this->name.isEmpty()) {
		return;
	}

	bool updated = false;
	LayerTRW * parent_layer_ = (LayerTRW *) this->owning_layer;

	const QString new_name = waypoint_properties_dialog(g_tree->tree_get_main_window(), this->name, this, parent_layer_->coord_mode, false, &updated);
	if (new_name.size()) {
		parent_layer_->waypoints->rename_waypoint(this, new_name);
	}

	if (updated && this->index.isValid()) {
		this->tree_view->set_icon(this->index, get_wp_sym_small(this->symbol_name));
	}

	if (updated && parent_layer_->visible) {
		parent_layer_->emit_layer_changed();
	}
}
