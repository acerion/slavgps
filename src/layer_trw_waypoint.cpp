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
#include "layer_trw.h"
#include "layer_trw_menu.h"
#include "layer_trw_painter.h"
#include "layer_trw_geotag.h"
#include "layer_trw_waypoint.h"
#include "layer_trw_waypoint_properties.h"
#include "garmin_symbols.h"
#include "dem_cache.h"
#include "util.h"
#include "window.h"
#include "tree_view_internal.h"
#include "layers_panel.h"
#include "dialog.h"
#include "ui_util.h"
#include "geotag_exif.h"




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

	this->has_properties_dialog = true;
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
	this->set_image_full_path(wp.image_full_path);
	this->set_symbol(wp.symbol_name);

	/* kamilTODO: what about image_width / image_height? */
}




Waypoint::~Waypoint()
{
}




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




void Waypoint::set_image_full_path(const QString & new_image_full_path)
{
	this->image_full_path = new_image_full_path;
	/* NOTE - ATM the image (thumbnail) size is calculated on demand when needed to be first drawn. */
}




/* Sets both symbol name and symbol pixmap. The pixmap is fetched from GarminSymbols. */
void Waypoint::set_symbol(const QString & new_symbol_name)
{
	/* this->symbol_pixmap is just a reference, so no need to free it. */

	if (new_symbol_name.isEmpty()) {
		this->symbol_name = "";
		this->symbol_pixmap = NULL;
	} else {
		const QString normalized = GarminSymbols::get_normalized_symbol_name(new_symbol_name);
		if (normalized.isEmpty()) {
			this->symbol_name = new_symbol_name;
		} else {
			this->symbol_name = normalized;
		}

		this->symbol_pixmap = GarminSymbols::get_wp_symbol(this->symbol_name);
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
void Waypoint::marshall(Pickle & pickle)
{
	/* This creates space for fixed sized members like ints and whatnot
	   and copies that amount of data from the waypoint to byte array. */
	pickle.put_object((char *) this, sizeof (Waypoint));

	pickle.put_string(this->name);
	pickle.put_string(this->comment);
	pickle.put_string(this->description);
	pickle.put_string(this->source);
	pickle.put_string(this->type);
	pickle.put_string(this->url);
	pickle.put_string(this->image_full_path);
	pickle.put_string(this->symbol_name);
}




/*
 * Take a byte array and convert it into a Waypoint.
 */
Waypoint * Waypoint::unmarshall(Pickle & pickle)
{
	const pickle_size_t data_size = pickle.take_size();
	const QString type_id = pickle.take_string();

	Waypoint * wp = new Waypoint();

	/* This copies the fixed sized elements (i.e. visibility, altitude, image_width, etc...). */
	pickle.take_object(wp);

	wp->name = pickle.take_string();
	wp->comment = pickle.take_string();
	wp->description = pickle.take_string();
	wp->source = pickle.take_string();
	wp->type = pickle.take_string();
	wp->url = pickle.take_string();
	wp->image_full_path = pickle.take_string();
	wp->symbol_name = pickle.take_string();

	return wp;
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



void Waypoint::sublayer_menu_waypoint_misc(LayerTRW * parent_layer_, QMenu & menu, bool tree_view_context_menu)
{
	QAction * qa = NULL;


	if (tree_view_context_menu) {
		/* Add this menu item only if context menu is displayed for item in tree view.
		   There is little sense in command "show this waypoint in main viewport"
		   if context menu is already displayed in main viewport. */
		qa = menu.addAction(QIcon::fromTheme("go-jump"), tr("&Show this Waypoint in main Viewport"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_in_viewport_cb()));
	}

	if (!this->name.isEmpty()) {
		if (is_valid_geocache_name(this->name.toUtf8().constData())) {
			qa = menu.addAction(QIcon::fromTheme("go-jump"), tr("&Visit Geocache Webpage"));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_geocache_webpage_cb()));
		}
#ifdef VIK_CONFIG_GEOTAG
		qa = menu.addAction(QIcon::fromTheme("go-jump"), tr("Geotag &Images..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_waypoint_cb()));
		qa->setToolTip(tr("Geotag multiple images against this waypoint"));
#endif
	}


	if (!this->image_full_path.isEmpty()) {
		qa = menu.addAction(QIcon(":/icons/layer_tool/trw_show_picture_18.png"), tr("&Show Picture..."));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (show_wp_picture_cb()));

#ifdef VIK_CONFIG_GEOTAG
		{
			QMenu * geotag_submenu = menu.addMenu(QIcon::fromTheme("view-refresh"), tr("Update Geotag on &Image"));

			qa = geotag_submenu->addAction(tr("&Update"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_waypoint_mtime_update_cb()));

			qa = geotag_submenu->addAction(tr("Update and &Keep File Timestamp"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_waypoint_mtime_keep_cb()));
		}
#endif
	}

	if (this->has_any_url()) {
		qa = menu.addAction(QIcon::fromTheme("applications-internet"), tr("Visit &Webpage associated with this Waypoint"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_waypoint_webpage_cb()));
	}
}




bool Waypoint::add_context_menu_items(QMenu & menu, bool tree_view_context_menu)
{
	QAction * qa = NULL;
	bool context_menu_in_items_tree = false;

	qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Properties"));
	connect(qa, SIGNAL (triggered(bool)), (LayerTRW *) this, SLOT (properties_dialog_cb()));


	/* Common "Edit" items. */
	{
		qa = menu.addAction(QIcon::fromTheme("edit-cut"), QObject::tr("Cut"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (cut_sublayer_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-copy"), QObject::tr("Copy"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_sublayer_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-delete"), QObject::tr("Delete"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_sublayer_cb()));
	}


	menu.addSeparator();


	this->sublayer_menu_waypoint_misc((LayerTRW *) this->owning_layer, menu, tree_view_context_menu);


	if (g_tree->tree_get_items_tree()) {
		context_menu_in_items_tree = true;
		qa = menu.addAction(QIcon::fromTheme("document-new"), tr("&New Waypoint..."));
		connect(qa, SIGNAL (triggered(bool)), (LayerTRW *) this->owning_layer, SLOT (new_waypoint_cb()));
	}


	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));

	/* These are only made available if a suitable program is installed. */
	if (g_have_astro_program || g_have_diary_program) {
		if (g_have_diary_program) {
			qa = external_submenu->addAction(QIcon::fromTheme("SPELL_CHECK"), QObject::tr("&Diary"));
			QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_diary_cb()));
			qa->setToolTip(QObject::tr("Open diary program at this date"));
		}

		if (g_have_astro_program) {
			qa = external_submenu->addAction(QObject::tr("&Astronomy"));
			QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_astro_cb()));
			qa->setToolTip(QObject::tr("Open astronomy program at this date and location"));
		}
	}


	layer_trw_sublayer_menu_all_add_external_tools((LayerTRW *) this->owning_layer, external_submenu);


	QMenu * transform_submenu = menu.addMenu(QIcon::fromTheme("CONVERT"), QObject::tr("&Transform"));
	{
		QMenu * dem_submenu = transform_submenu->addMenu(QIcon::fromTheme("vik-icon-DEM Download"), QObject::tr("&Apply DEM Data"));

		qa = dem_submenu->addAction(QObject::tr("&Overwrite"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_all_cb()));
		qa->setToolTip(QObject::tr("Overwrite any existing elevation values with DEM values"));

		qa = dem_submenu->addAction(QObject::tr("&Keep Existing"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_only_missing_cb()));
		qa->setToolTip(QObject::tr("Keep existing elevation values, only attempt for missing values"));
	}


	return context_menu_in_items_tree;
}




bool Waypoint::properties_dialog()
{
	this->properties_dialog_cb();
	return true;
}




void Waypoint::properties_dialog_cb(void)
{
	if (this->name.isEmpty()) {
		return;
	}

	LayerTRW * parent_layer_ = (LayerTRW *) this->owning_layer;

	const std::tuple<bool, bool> result = waypoint_properties_dialog(this, this->name, parent_layer_->coord_mode, g_tree->tree_get_main_window());

	if (std::get<SG_WP_DIALOG_OK>(result)) {
		/* "OK" pressed in dialog, waypoint's parameters entered in the dialog are valid. */

		if (std::get<SG_WP_DIALOG_NAME>(result)) {
			/* Waypoint's name has been changed. */
			parent_layer_->waypoints->propagate_new_waypoint_name(this);
		}

		parent_layer_->get_waypoints_node().set_new_waypoint_icon(this);

		if (parent_layer_->visible) {
			parent_layer_->emit_layer_changed();
		}
	}
}




QString Waypoint::get_tooltip(void)
{
	if (!this->comment.isEmpty()) {
		return this->comment;
	} else {
		return this->description;
	}
}




void Waypoint::apply_dem_data_all_cb(void)
{
	this->apply_dem_data_common(false);
}




void Waypoint::apply_dem_data_only_missing_cb(void)
{
	this->apply_dem_data_common(true);
}




void Waypoint::apply_dem_data_common(bool skip_existing_elevations)
{
	LayersPanel * panel = g_tree->tree_get_items_tree();
	if (!panel->has_any_layer_of_type(LayerType::DEM)) {
		return;
	}

	LayerTRW * trw = (LayerTRW *) this->owning_layer;
	int changed = (int) this->apply_dem_data(skip_existing_elevations);

	trw->wp_changed_message(changed);
}




/**
   \brief Open a diary at the date of the waypoint
*/
void Waypoint::open_diary_cb(void)
{
	char date_buf[20];
	date_buf[0] = '\0';
	if (this->has_timestamp) {
		strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&this->timestamp));
		((LayerTRW *) this->owning_layer)->diary_open(date_buf);
	} else {
		Dialog::info(tr("This waypoint has no date information."), g_tree->tree_get_main_window());
	}
}




/**
   \brief Open an astronomy program at the date & position of the waypoint
*/
void Waypoint::open_astro_cb(void)
{
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	if (this->has_timestamp) {
		char date_buf[20];
		strftime(date_buf, sizeof(date_buf), "%Y%m%d", gmtime(&this->timestamp));
		char time_buf[20];
		strftime(time_buf, sizeof(time_buf), "%H:%M:%S", gmtime(&this->timestamp));
		const LatLon ll = this->coord.get_latlon();
		char *lat_str = convert_to_dms(ll.lat);
		char *lon_str = convert_to_dms(ll.lon);
		char alt_buf[20];
		snprintf(alt_buf, sizeof(alt_buf), "%d", (int) round(this->altitude));
		parent_layer->astro_open(date_buf, time_buf, lat_str, lon_str, alt_buf);
		free(lat_str);
		free(lon_str);
	} else {
		Dialog::info(tr("This waypoint has no date information."), g_tree->tree_get_main_window());
	}
}




void Waypoint::show_in_viewport_cb(void)
{
	((LayerTRW *) this->owning_layer)->goto_coord(g_tree->tree_get_main_viewport(), this->coord);
}




void Waypoint::open_geocache_webpage_cb(void)
{
	const QString webpage = QString("http://www.geocaching.com/seek/cache_details.aspx?wp=%1").arg(this->name);
	open_url(webpage);
}




void Waypoint::open_waypoint_webpage_cb(void)
{
	if (!this->has_any_url()) {
		return;
	}

	open_url(this->get_any_url());
}




QString Waypoint::sublayer_rename_request(const QString & new_name)
{
	static const QString empty_string("");

	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	/* No actual change to the name supplied. */
	if (!this->name.isEmpty()) {
		if (new_name == this->name) {
			return empty_string;
		}
	}

	if (parent_layer->waypoints->find_waypoint_by_name(new_name)) {
		/* An existing waypoint has been found with the requested name. */
		if (!Dialog::yes_or_no(tr("A waypoint with the name \"%1\" already exists. Really rename to the same name?").arg(new_name), g_tree->tree_get_main_window())) {
			return empty_string;
		}
	}

	/* Update WP name and refresh the tree view. */
	this->set_name(new_name);

	parent_layer->tree_view->set_tree_item_name(this->index, new_name);
	parent_layer->tree_view->sort_children(parent_layer->waypoints->get_index(), parent_layer->wp_sort_order);

	g_tree->emit_items_tree_updated();

	return new_name;
}




bool Waypoint::handle_selection_in_tree(void)
{
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	parent_layer->set_statusbar_msg_info_wpt(this);

	parent_layer->reset_internal_selections(); /* No other tree item (that is a sublayer of this layer) is selected... */
	parent_layer->set_edited_wp(this); /* But this tree item is selected. */

	g_tree->selected_tree_item = this;

	return true;
}




/**
 * Only handles a single waypoint
 * It assumes the waypoint belongs to the TRW Layer (it doesn't check this is the case)
 */
void Waypoint::draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected)
{
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this->index)) {
		return;
	}

	const bool item_is_selected = parent_is_selected || TreeItem::the_same_object(g_tree->selected_tree_item, this);

	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;
	parent_layer->painter->draw_waypoint(this, viewport, item_is_selected && highlight_selected);
}




#ifdef VIK_CONFIG_GEOTAG
void Waypoint::geotagging_waypoint_mtime_keep_cb(void)
{
	/* Update directly - not changing the mtime. */
	a_geotag_write_exif_gps(this->image_full_path, this->coord, this->altitude, true);
}




void Waypoint::geotagging_waypoint_mtime_update_cb(void)
{
	/* Update directly. */
	a_geotag_write_exif_gps(this->image_full_path, this->coord, this->altitude, false);
}




void Waypoint::geotagging_waypoint_cb(void)
{
	trw_layer_geotag_dialog(g_tree->tree_get_main_window(), (LayerTRW *) this->owning_layer, this, NULL);
}
#endif






void Waypoint::delete_sublayer(bool confirm)
{
	if (this->name.isEmpty()) {
		return;
	}

	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;
	Window * main_window = g_tree->tree_get_main_window();

	if (confirm) {
		/* Get confirmation from the user. */
		/* Maybe this Waypoint Delete should be optional as is it could get annoying... */
		if (!Dialog::yes_or_no(tr("Are you sure you want to delete the waypoint \"%1\"?").arg(this->name)), main_window) {
			return;
		}
	}

	bool was_visible = parent_layer->delete_waypoint(this);
	parent_layer->waypoints->recalculate_bbox();

	/* Reset layer timestamp in case it has now changed. */
	parent_layer->tree_view->set_tree_item_timestamp(parent_layer->index, parent_layer->get_timestamp());

	if (was_visible) {
		parent_layer->emit_layer_changed();
	}
}





void Waypoint::cut_sublayer_cb(void)
{
	/* false: don't require confirmation in callbacks. */
	((LayerTRW *) this->owning_layer)->cut_sublayer_common(this, false);
}

void Waypoint::copy_sublayer_cb(void)
{
	((LayerTRW *) this->owning_layer)->copy_sublayer_common(this);
}

void Waypoint::delete_sublayer_cb(void)
{
	/* false: don't require confirmation in callbacks. */
	this->delete_sublayer(false);
}




/**
   Simple accessor

   Created to avoid constant casting of Waypoint::owning_layer to LayerTRW* type.
*/
LayerTRW * Waypoint::get_parent_layer_trw() const
{
	return (LayerTRW *) this->owning_layer;

}
