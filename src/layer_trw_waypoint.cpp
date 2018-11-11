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
 */




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
#include "tree_item_list.h"
#include "tree_view_internal.h"
#include "layers_panel.h"
#include "dialog.h"
#include "ui_util.h"
#include "geotag_exif.h"
#include "preferences.h"




using namespace SlavGPS;




#define SG_MODULE "Waypoint"
#define PREFIX ": Waypoint:" << __FUNCTION__ << __LINE__ << ">"




extern SelectedTreeItems g_selected;

extern bool g_have_astro_program;
extern bool g_have_diary_program;




Waypoint::Waypoint()
{
	this->tree_item_type = TreeItemType::Sublayer;

	this->name = tr("Waypoint");

	this->type_id = "sg.trw.waypoint";

	this->has_properties_dialog = true;
}




/* Copy constructor. */
Waypoint::Waypoint(const Waypoint & wp) : Waypoint()
{
	this->tree_item_type = TreeItemType::Sublayer;

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

	/* TODO_LATER: what about image_width / image_height? */
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
	if (this->altitude.is_valid() && skip_existing) {
		return false;
	}

	const Altitude elev = DEMCache::get_elev_by_coord(this->coord, DemInterpolation::Best);
	if (!elev.is_valid()) {
		return true;
	}

	this->altitude = elev;

	return true;
}



/*
 * Take a Waypoint and convert it into a byte array.
 */
void Waypoint::marshall(Pickle & pickle)
{
	/* This creates space for fixed sized members like ints and whatnot
	   and copies that amount of data from the waypoint to byte array. */
	pickle.put_raw_object((char *) this, sizeof (Waypoint));

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
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Properties"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (properties_dialog_cb()));


	/* Common "Edit" items. */
	{
		qa = menu.addAction(QIcon::fromTheme("edit-cut"), QObject::tr("Cut"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (cut_sublayer_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-copy"), QObject::tr("Copy"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_sublayer_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-delete"), QObject::tr("Delete"));
		qa->setData((unsigned int) this->get_uid());
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_waypoint_cb()));
	}


	menu.addSeparator();


	this->sublayer_menu_waypoint_misc((LayerTRW *) this->owning_layer, menu, tree_view_context_menu);


	if (ThisApp::get_layers_panel()) {
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

	const std::tuple<bool, bool> result = waypoint_properties_dialog(this, this->name, parent_layer_->coord_mode, ThisApp::get_main_window());

	if (std::get<SG_WP_DIALOG_OK>(result)) {
		/* "OK" pressed in dialog, waypoint's parameters entered in the dialog are valid. */

		if (std::get<SG_WP_DIALOG_NAME>(result)) {
			/* Waypoint's name has been changed. */
			parent_layer_->waypoints.propagate_new_waypoint_name(this);
		}

		parent_layer_->get_waypoints_node().set_new_waypoint_icon(this);

		if (parent_layer_->visible) {
			parent_layer_->emit_layer_changed("TRW - Waypoint - properties");
		}
	}
}




QString Waypoint::get_tooltip(void) const
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
	LayersPanel * panel = ThisApp::get_layers_panel();
	if (!panel->has_any_layer_of_type(LayerType::DEM)) {
		Dialog::error(tr("No DEM layers available, thus no DEM values can be applied."), ThisApp::get_main_window());
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
		Dialog::info(tr("This waypoint has no date information."), ThisApp::get_main_window());
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
		snprintf(alt_buf, sizeof(alt_buf), "%d", (int) round(this->altitude.get_value()));
		parent_layer->astro_open(date_buf, time_buf, lat_str, lon_str, alt_buf);
		free(lat_str);
		free(lon_str);
	} else {
		Dialog::info(tr("This waypoint has no date information."), ThisApp::get_main_window());
	}
}




void Waypoint::show_in_viewport_cb(void)
{
	((LayerTRW *) this->owning_layer)->goto_coord(ThisApp::get_main_viewport(), this->coord);
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

	if (parent_layer->waypoints.find_waypoint_by_name(new_name)) {
		/* An existing waypoint has been found with the requested name. */
		if (!Dialog::yes_or_no(tr("A waypoint with the name \"%1\" already exists. Really rename to the same name?").arg(new_name), ThisApp::get_main_window())) {
			return empty_string;
		}
	}

	/* Update WP name and refresh the tree view. */
	this->set_name(new_name);

	parent_layer->tree_view->apply_tree_item_name(this);
	parent_layer->tree_view->sort_children(&parent_layer->waypoints, parent_layer->wp_sort_order);

	ThisApp::get_layers_panel()->emit_items_tree_updated_cb("Redrawing items after renaming waypoint");

	return new_name;
}




bool Waypoint::handle_selection_in_tree(void)
{
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	parent_layer->set_statusbar_msg_info_wpt(this);

	parent_layer->reset_internal_selections(); /* No other tree item (that is a sublayer of this layer) is selected... */
	parent_layer->set_edited_wp(this); /* But this tree item is selected. */

	qDebug() << SG_PREFIX_I << "Tree item" << this->name << "becomes selected tree item";
	g_selected.add_to_set(this);

	this->display_debug_info("At selection in tree view");

	return true;
}




/**
 * Only handles a single waypoint
 * It assumes the waypoint belongs to the TRW Layer (it doesn't check this is the case)
 */
void Waypoint::draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected)
{
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this)) {
		return;
	}

	if (1) {
		if (g_selected.is_in_set(this)) {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << this->name << "as selected (selected directly)";
		} else if (parent_is_selected) {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << this->name << "as selected (selected through parent)";
		} else {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << this->name << "as non-selected";
		}
	}

	const bool item_is_selected = parent_is_selected || g_selected.is_in_set(this);
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;
	parent_layer->painter->draw_waypoint(this, viewport, item_is_selected && highlight_selected);
}




#ifdef VIK_CONFIG_GEOTAG
void Waypoint::geotagging_waypoint_mtime_keep_cb(void)
{
	/* Update directly - not changing the mtime. */
	GeotagExif::write_exif_gps(this->image_full_path, this->coord, this->altitude, true);
}




void Waypoint::geotagging_waypoint_mtime_update_cb(void)
{
	/* Update directly. */
	GeotagExif::write_exif_gps(this->image_full_path, this->coord, this->altitude, false);
}




void Waypoint::geotagging_waypoint_cb(void)
{
	trw_layer_geotag_dialog(ThisApp::get_main_window(), (LayerTRW *) this->owning_layer, this, NULL);
}
#endif






void Waypoint::cut_sublayer_cb(void)
{
	/* false: don't require confirmation in callbacks. */
	((LayerTRW *) this->owning_layer)->cut_sublayer_common(this, false);
}

void Waypoint::copy_sublayer_cb(void)
{
	((LayerTRW *) this->owning_layer)->copy_sublayer_common(this);
}




/**
   Simple accessor

   Created to avoid constant casting of Waypoint::owning_layer to LayerTRW* type.
*/
LayerTRW * Waypoint::get_parent_layer_trw() const
{
	return (LayerTRW *) this->owning_layer;

}




QList<QStandardItem *> Waypoint::get_list_representation(const TreeItemListFormat & list_format)
{
	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QVariant variant;
	const QString tooltip(this->description);
	QString date_time_string;


	for (const TreeItemListColumn & col : list_format.columns) {
		switch (col.id) {
		case TreeItemPropertyID::TheItem:
			item = new QStandardItem(this->name);
			item->setToolTip(tooltip);
			variant = QVariant::fromValue(this);
			item->setData(variant, RoleLayerData);
			items << item;
			break;

		case TreeItemPropertyID::Timestamp:
			if (this->has_timestamp) {
				QDateTime date_time;
				date_time.setTime_t(this->timestamp);
#ifdef K_TODO
				date_time_string = date_start.toString(this->date_time_format);
#else
				date_time_string = date_time.toString(Qt::ISODate);
#endif
			}
			item = new QStandardItem(date_time_string);
			item->setToolTip(tooltip);
			items << item;
			break;

		default:
			qDebug() << SG_PREFIX_E << "Unexpected TreeItem Column ID" << (int) col.id;
			break;
		}
	}


	return items;

#if 0

	list_format.column_descriptions.push_back(TreeItemListColumn(TreeItemPropertyID::Name));
	list_format.column_descriptions.push_back(TreeItemListColumn(TreeItemPropertyID::Timestamp));

	const DistanceUnit distance_unit = Preferences::get_unit_distance();
	const SpeedUnit speed_units = Preferences::get_unit_speed();
	const HeightUnit height_unit = Preferences::get_unit_height();




	LayerTRW * trw = this->get_parent_layer_trw();

	/* This parameter doesn't include aggegrate visibility. */
	bool a_visible = trw->visible && this->visible;
#ifdef K_TODO
	a_visible = a_visible && trw->get_tree_items_visibility();
#endif



	/* LayerName */
	item = new QStandardItem(trw->name);
	item->setToolTip(tooltip);
	item->setEditable(false); /* This dialog is not a good place to edit layer name. */
	items << item;



	/* Date */

	/* Visibility */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	item->setCheckable(true);
	item->setCheckState(a_visible ? Qt::Checked : Qt::Unchecked);
	items << item;

	/* Comment */
	item = new QStandardItem(this->comment);
	item->setToolTip(tooltip);
	items << item;

	/* Elevation */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	variant = QVariant::fromValue(this->altitude.convert_to_unit(height_unit).to_string());
	item->setData(variant, RoleLayerData);
	items << item;

	/* Icon */
	item = new QStandardItem();
	item->setToolTip(tooltip);
	item->setIcon(get_wp_icon_small(this->symbol_name));
	item->setEditable(false);
	items << item;
#endif
}




void Waypoint::display_debug_info(const QString & reference) const
{
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	qDebug() << SG_PREFIX_D << "@" << reference;
	qDebug() << SG_PREFIX_D << "               Type ID =" << this->type_id;

	qDebug() << SG_PREFIX_D << "               Pointer =" << (quintptr) this;
	qDebug() << SG_PREFIX_D << "                  Name =" << this->name;
	qDebug() << SG_PREFIX_D << "                   UID =" << this->uid;

	qDebug() << SG_PREFIX_D << "  Parent layer pointer =" << (quintptr) parent_layer;
	qDebug() << SG_PREFIX_D << "     Parent layer name =" << (parent_layer ? parent_layer->name : "<no parent layer>");
	//qDebug() << SG_PREFIX_D << "      Parent layer UID =" << (parent_layer ? parent_layer->uid : "<no parent layer>");

	qDebug() << SG_PREFIX_D << "            Is in tree =" << this->is_in_tree();
	qDebug() << SG_PREFIX_D << "      Tree index valid =" << this->index.isValid();
	qDebug() << SG_PREFIX_D << "          Debug string =" << this->debug_string;

	return;
}
