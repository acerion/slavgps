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
#include <cassert>
#include <mutex>




#include "coords.h"
#include "coord.h"
#include "layer_trw.h"
#include "layer_trw_menu.h"
#include "layer_trw_painter.h"
#include "layer_trw_geotag.h"
#include "layer_trw_waypoint.h"
#include "layer_trw_waypoint_properties.h"
#include "layer_trw_tools.h"
#include "garmin_symbols.h"
#include "layer_dem_dem_cache.h"
#include "util.h"
#include "window.h"
#include "toolbox.h"
#include "tree_item_list.h"
#include "tree_view_internal.h"
#include "layers_panel.h"
#include "dialog.h"
#include "ui_util.h"
#include "geotag_exif.h"
#include "preferences.h"
#include "application_state.h"
#include "astro.h"




using namespace SlavGPS;




#define SG_MODULE "Waypoint"




extern SelectedTreeItems g_selected;

extern bool g_have_astro_program;
extern bool g_have_diary_program;




Waypoint::Waypoint()
{
	this->set_name(tr("Waypoint"));

	this->m_type_id = Waypoint::type_id();

	this->has_properties_dialog = true;

	this->m_menu_operation_ids.push_back(StandardMenuOperation::Properties);
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Cut);
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Copy);
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Delete);
}




/* Copy constructor. */
Waypoint::Waypoint(const Waypoint & wp) : Waypoint()
{
	this->m_coord = wp.m_coord;
	this->m_visible = wp.m_visible;
	this->set_timestamp(wp.timestamp);
	this->altitude = wp.altitude;

	this->set_name(wp.get_name());
	this->set_comment(wp.comment);
	this->set_description(wp.description);
	this->set_source(wp.source);
	this->set_type(wp.type);
	this->set_url(wp.url);
	this->set_image_full_path(wp.image_full_path);
	this->set_symbol_name(wp.symbol_name);

	this->drawn_image_rect = wp.drawn_image_rect;
}




Waypoint::Waypoint(const Coord & coord) : Waypoint()
{
	this->m_coord = coord;
}




Waypoint::~Waypoint()
{
}




SGObjectTypeID Waypoint::get_type_id(void) const
{
	return Waypoint::type_id();
}
SGObjectTypeID Waypoint::type_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.trw.waypoint");
	return id;
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
void Waypoint::set_symbol_name(const QString & new_symbol_name)
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




sg_ret Waypoint::set_coord(const Coord & new_coord, bool do_recalculate_bbox, bool only_set_value)
{
	this->m_coord = new_coord;
	if (do_recalculate_bbox) {
		LayerTRW * trw = this->owner_trw_layer();
		trw->waypoints_node().recalculate_bbox();
	}

	if (only_set_value) {
		return sg_ret::ok;
	} else {

		/* Update properties dialog with the most recent coordinates
		   of released waypoint. */
		/* TODO_MAYBE: optimize by changing only coordinates in
		   the dialog. This is the only parameter that will change
		   when a point is moved in x/y plane. We may consider also
		   updating an alternative altitude indicator, if the altitude
		   is retrieved from DEM info. */
		this->properties_dialog_set();

		this->emit_tree_item_changed("Waypoint's coordinate has been set");

		return sg_ret::ok;
	}
}




const Coord & Waypoint::get_coord(void) const
{
	return this->m_coord;
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

	const Altitude elev = DEMCache::get_elev_by_coord(this->m_coord, DEMInterpolation::Best);
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

	pickle.put_string(this->get_name());
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

	/* This copies the fixed sized elements (i.e. visibility, altitude, image_rect, etc...). */
	pickle.take_object(wp);

	wp->set_name(pickle.take_string());
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
	this->m_coord.recalculate_to_mode(dest_mode);
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



void Waypoint::sublayer_menu_waypoint_misc(LayerTRW * parent_layer_, QMenu & menu, bool in_tree_view)
{
	QAction * qa = NULL;


	if (in_tree_view) {
		/* Add this menu item only if context menu is displayed for item in tree view.
		   There is little sense in command "show this waypoint in main viewport"
		   if context menu is already displayed in main viewport. */
		qa = menu.addAction(QIcon::fromTheme("go-jump"), tr("&Show this Waypoint in main GisViewport"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_in_viewport_cb()));
	}

	if (!this->get_name().isEmpty()) {
		if (is_valid_geocache_name(this->get_name().toUtf8().constData())) {
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




/**
   @reviewed-on 2019-12-30
*/
sg_ret Waypoint::menu_add_standard_operations(QMenu & menu, const StandardMenuOperations & ops, __attribute__((unused)) bool in_tree_view)
{
	if (ops.is_member(StandardMenuOperation::Properties)) {
		QAction * qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Properties"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_properties_dialog_cb()));
	}

	if (ops.is_member(StandardMenuOperation::Cut)) {
		QAction * qa = menu.addAction(QIcon::fromTheme("edit-cut"), QObject::tr("Cut"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (cut_tree_item_cb()));
	}

	if (ops.is_member(StandardMenuOperation::Copy)) {
		QAction * qa = menu.addAction(QIcon::fromTheme("edit-copy"), QObject::tr("Copy"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_tree_item_cb()));
	}

	if (ops.is_member(StandardMenuOperation::Delete)) {
		QAction * qa = menu.addAction(QIcon::fromTheme("edit-delete"), QObject::tr("Delete"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_tree_item_cb()));
	}

	return sg_ret::ok;
}




sg_ret Waypoint::menu_add_type_specific_operations(QMenu & menu, bool in_tree_view)
{
	QAction * qa = NULL;

	LayerTRW * parent_trw = this->owner_trw_layer();

	this->sublayer_menu_waypoint_misc(parent_trw, menu, in_tree_view);


	if (ThisApp::layers_panel()) {
		qa = menu.addAction(QIcon::fromTheme("document-new"), tr("&New Waypoint..."));
		connect(qa, SIGNAL (triggered(bool)), parent_trw, SLOT (new_waypoint_cb()));
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


	layer_trw_sublayer_menu_all_add_external_tools(parent_trw, external_submenu);


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


	return sg_ret::ok;
}




bool Waypoint::show_properties_dialog(void)
{
	return this->show_properties_dialog_cb();
}




bool Waypoint::show_properties_dialog_cb(void)
{
	LayerTRW * parent_trw = this->owner_trw_layer();
	Window * window = ThisApp::main_window();
	LayerToolTRWEditWaypoint * tool = (LayerToolTRWEditWaypoint *) window->toolbox()->get_tool(LayerToolTRWEditWaypoint::tool_id());


	/* Signals. */
	{
		/* Disconnect all old connections that may have been made from
		   this global dialog to other TRW layer. */
		/* TODO_LATER: also disconnect these signals in dialog code when the dialog is closed? */
		tool->point_properties_dialog->disconnect();

		/* Make new connections to current TRW layer. */
		connect(tool->point_properties_dialog, SIGNAL (point_coordinates_changed()), parent_trw, SLOT (on_wp_properties_dialog_wp_coordinates_changed_cb()));
	}


	/* Show properties dialog. */
	{
		const CoordMode coord_mode = parent_trw->get_coord_mode();
		tool->point_properties_dialog->set_coord_mode(coord_mode);
		window->tools_dock()->setWidget(tool->point_properties_dialog);
		window->set_tools_dock_visibility_cb(true);
	}


	/* Fill properties dialog with current point. */
	{
		const Waypoint * wp = parent_trw->selected_wp_get();
		if (nullptr == wp) {
			qDebug() << SG_PREFIX_W << "Parent layer doesn't have any 'edited' waypoint set";
			tool->point_properties_dialog->dialog_data_reset();
			return false;
		} else if (wp->get_uid() != this->get_uid()) {
			qDebug() << SG_PREFIX_W << "Tree item uid mismatch";
			tool->point_properties_dialog->dialog_data_reset();
			return false;
		} else {
			qDebug() << SG_PREFIX_I << "Will fill properties dialog with waypoint" << this->get_name();
			tool->point_properties_dialog->dialog_data_set(this);
			return true;
		}
	}
}




QString Waypoint::get_tooltip(void) const
{
	QString result;
	if (!GarminSymbols::is_none_symbol_name(this->symbol_name)) {
		/* TODO_LATER: test that symbol name is visible in tooltip. */
		result += this->symbol_name + "\n";
	}

	if (!this->comment.isEmpty()) {
		result += this->comment;
	} else {
		result += this->description;
	}

	return result;
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
	LayersPanel * panel = ThisApp::layers_panel();
	if (!panel->has_any_layer_of_kind(LayerKind::DEM)) {
		Dialog::error(tr("No DEM layers available, thus no DEM values can be applied."), ThisApp::main_window());
		return;
	}

	LayerTRW * parent_trw = this->owner_trw_layer();
	int changed = (int) this->apply_dem_data(skip_existing_elevations);

	parent_trw->wp_changed_message(changed);
}




/**
   \brief Open a diary at the date of the waypoint
*/
void Waypoint::open_diary_cb(void)
{
	if (this->timestamp.is_valid()) {
		const QString date_buf = this->timestamp.strftime_utc("%Y-%m-%d");
		this->owner_trw_layer()->diary_open(date_buf);
	} else {
		Dialog::info(tr("This waypoint has no date information."), ThisApp::main_window());
	}
}




/**
   \brief Open an astronomy program at the date & position of the waypoint
*/
void Waypoint::open_astro_cb(void)
{
	Window * main_window = ThisApp::main_window();

	if (this->timestamp.is_valid()) {

		const QString date_buf = this->timestamp.strftime_utc("%Y%m%d");
		const QString time_buf = this->timestamp.strftime_utc("%H:%M:%S");

		const LatLon lat_lon = this->m_coord.get_lat_lon();
		const QString lat_str = Astro::convert_to_dms(lat_lon.lat.value());
		const QString lon_str = Astro::convert_to_dms(lat_lon.lon.bound_value());
		const QString alt_str = QString("%1").arg((int) round(this->altitude.ll_value()));
		Astro::open(date_buf, time_buf, lat_str, lon_str, alt_str, main_window);
	} else {
		Dialog::info(tr("This waypoint has no date information."), main_window);
	}
}




void Waypoint::show_in_viewport_cb(void)
{
	if (sg_ret::ok != ThisApp::main_gisview()->set_center_coord(this->m_coord)) {
		return;
	}
	ThisApp::main_gisview()->request_redraw("Waypoint's 'show in viewport' callback");
}




void Waypoint::open_geocache_webpage_cb(void)
{
	const QString webpage = QString("http://www.geocaching.com/seek/cache_details.aspx?wp=%1").arg(this->get_name());
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

	LayerTRW * parent_trw = this->owner_trw_layer();

	/* No actual change to the name supplied. */
	if (!this->get_name().isEmpty()) {
		if (new_name == this->get_name()) {
			return empty_string;
		}
	}

	if (parent_trw->waypoints_node().find_child_by_name(new_name)) {
		/* An existing waypoint has been found with the requested name. */
		if (!Dialog::yes_or_no(tr("A waypoint with the name \"%1\" already exists. Really rename to the same name?").arg(new_name), ThisApp::main_window())) {
			return empty_string;
		}
	}

	/* Update WP name and refresh the tree view. */
	this->set_name(new_name);

	parent_trw->tree_view->apply_tree_item_name(this);
	parent_trw->tree_view->sort_children(&parent_trw->waypoints_node(), parent_trw->wp_sort_order);

	ThisApp::layers_panel()->emit_items_tree_updated_cb("Redrawing items after renaming waypoint");

	return new_name;
}




bool Waypoint::handle_selection_in_tree(void)
{
	LayerTRW * parent_trw = this->owner_trw_layer();

	parent_trw->set_statusbar_msg_info_wpt(this);

	parent_trw->reset_internal_selections(); /* No other tree item (that is a sublayer of this layer) is selected... */
	parent_trw->selected_wp_set(this); /* But this tree item is selected. */

	qDebug() << SG_PREFIX_I << "Tree item" << this->get_name() << "becomes selected tree item";
	g_selected.add_to_set(this);

	this->display_debug_info("At selection in tree view");

	return true;
}




/**
 * Only handles a single waypoint
 * It assumes the waypoint belongs to the TRW Layer (it doesn't check this is the case)
 */
void Waypoint::draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected)
{
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this)) {
		return;
	}

	SelectedTreeItems::print_draw_mode(*this, parent_is_selected);

	const bool item_is_selected = parent_is_selected || g_selected.is_in_set(this);
	LayerTRW * parent_trw = this->owner_trw_layer();
	parent_trw->painter->draw_waypoint(this, gisview, item_is_selected && highlight_selected);
}




#ifdef VIK_CONFIG_GEOTAG
void Waypoint::geotagging_waypoint_mtime_keep_cb(void)
{
	/* Update directly - not changing the mtime. */
	GeotagExif::write_exif_gps(this->image_full_path, this->m_coord, this->altitude, true);
}




void Waypoint::geotagging_waypoint_mtime_update_cb(void)
{
	/* Update directly. */
	GeotagExif::write_exif_gps(this->image_full_path, this->m_coord, this->altitude, false);
}




void Waypoint::geotagging_waypoint_cb(void)
{
	trw_layer_geotag_dialog(ThisApp::main_window(), this->owner_trw_layer(), this, NULL);
}
#endif




sg_ret Waypoint::cut_tree_item_cb(void)
{
	LayerTRW * parent_trw = this->owner_trw_layer();
	return parent_trw->cut_child_item(this);
}




sg_ret Waypoint::copy_tree_item_cb(void)
{
	LayerTRW * parent_trw = this->owner_trw_layer();
	return parent_trw->copy_child_item(this);
}




/**
   @reviewed-on 2019-12-30
*/
sg_ret Waypoint::delete_tree_item_cb(void)
{
	LayerTRW * parent_trw = this->owner_trw_layer();
	/* false: don't require confirmation in callbacks. */
	return parent_trw->delete_child_item(this, false);
}




/**
   @reviewed-on 2020-03-06
*/
LayerTRW * Waypoint::owner_trw_layer(void) const
{
	return (LayerTRW *) this->parent_layer();
}




/**
   @reviewed-on 2020-03-06
*/
Layer * Waypoint::parent_layer(void) const
{
	/* Waypoint's immediate parent is LayerTRWWaypoints. Above the
	   Waypoints container is a parent layer. */

	const TreeItem * parent = this->parent_member();
	if (nullptr == parent) {
		return nullptr;
	}
	const TreeItem * grandparent = parent->parent_member();
	if (nullptr == grandparent) {
		return nullptr;
	}

	Layer * layer = (Layer *) grandparent;
	assert (layer->m_kind == LayerKind::TRW);
	return layer;
}




QList<QStandardItem *> Waypoint::get_list_representation(const TreeItemViewFormat & view_format)
{
	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QVariant variant;
	const QString tooltip(this->description);
	QString date_time_string;

	LayerTRW * trw = this->owner_trw_layer();

	bool a_visible = trw->is_visible() && this->m_visible;
	a_visible = a_visible && trw->get_waypoints_visibility();

	Qt::DateFormat date_time_format = Qt::ISODate;
	ApplicationState::get_integer(VIK_SETTINGS_SORTABLE_DATE_TIME_FORMAT, (int *) &date_time_format);


	for (const TreeItemViewColumn & col : view_format.columns) {
		switch (col.id) {

		case TreeItemPropertyID::ParentLayer:
			item = new QStandardItem(trw->get_name());
			item->setToolTip(tooltip);
			item->setEditable(false); /* Item's properties widget is not a good place to edit its parent name. */
			items << item;
			break;

		case TreeItemPropertyID::TheItem:
			item = new QStandardItem(this->get_name());
			item->setToolTip(tooltip);
			variant = QVariant::fromValue(this);
			item->setData(variant, RoleLayerData);
			items << item;
			break;

		case TreeItemPropertyID::Timestamp:
			if (this->timestamp.is_valid()) {
				date_time_string = this->timestamp.get_time_string(date_time_format);
			}
			item = new QStandardItem(date_time_string);
			item->setToolTip(tooltip);
			items << item;
			break;

		case TreeItemPropertyID::Icon:
			item = new QStandardItem();
			item->setToolTip(tooltip);
			item->setIcon(get_wp_icon_small(this->symbol_name));
			item->setEditable(false);
			items << item;
			break;

		case TreeItemPropertyID::Visibility:
			/* Visibility */
			item = new QStandardItem();
			item->setToolTip(tooltip);
			item->setCheckable(true);
			item->setCheckState(a_visible ? Qt::Checked : Qt::Unchecked);
			items << item;
			break;

		case TreeItemPropertyID::Editable:
			item = new QStandardItem();
			variant = QVariant::fromValue(this->editable);
			item->setData(variant, RoleLayerData);
			items << item;
			break;


		case TreeItemPropertyID::Comment:
			item = new QStandardItem(this->comment);
			item->setToolTip(tooltip);
			items << item;
			break;

		case TreeItemPropertyID::Elevation:
			{
				const AltitudeType::Unit height_unit = Preferences::get_unit_height();
				const Altitude display_alt = this->altitude.convert_to_unit(height_unit);
				item = new QStandardItem();
				item->setToolTip(tooltip);
				variant = QVariant::fromValue(display_alt.value_to_string());
				item->setData(variant, RoleLayerData);
				items << item;
			}
			break;

		case TreeItemPropertyID::Coordinate:
		default:
			qDebug() << SG_PREFIX_E << "Unexpected TreeItem Column ID" << (int) col.id;
			break;
		}
	}

	return items;
}




void Waypoint::display_debug_info(const QString & reference) const
{
	LayerTRW * parent_trw = this->owner_trw_layer();

	qDebug() << SG_PREFIX_D << "@" << reference;
	qDebug() << SG_PREFIX_D << "               Type ID =" << this->m_type_id;

	qDebug() << SG_PREFIX_D << "               Pointer =" << (quintptr) this;
	qDebug() << SG_PREFIX_D << "                  Name =" << this->get_name();
	qDebug() << SG_PREFIX_D << "                   UID =" << this->uid;

	qDebug() << SG_PREFIX_D << "  Parent layer pointer =" << (quintptr) parent_trw;
	qDebug() << SG_PREFIX_D << "     Parent layer name =" << (parent_trw ? parent_trw->get_name() : "<no parent layer>");
	//qDebug() << SG_PREFIX_D << "      Parent layer UID =" << (parent_trw ? parent_trw->uid : "<no parent layer>");

	qDebug() << SG_PREFIX_D << "            Is in tree =" << this->is_in_tree();
	qDebug() << SG_PREFIX_D << "      Tree index valid =" << this->index().isValid();
	qDebug() << SG_PREFIX_D << "          Debug string =" << this->debug_string;

	return;
}




/**
   Update how track is displayed in tree view - primarily update track's icon
*/
sg_ret Waypoint::update_tree_item_properties(void)
{
	if (!this->index().isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid index of tree item";
		return sg_ret::err;
	}

	this->propagate_new_waypoint_name();
	this->set_new_waypoint_icon();

	return sg_ret::ok;
}




void Waypoint::self_assign_icon(void)
{
	this->icon = get_wp_icon_small(this->symbol_name);
}





/**
   Use waypoint's ::symbol_name to set waypoint's icon. Make sure that
   new icon of a waypoint (or lack of the icon) is shown wherever it
   needs to be shown.
*/
sg_ret Waypoint::set_new_waypoint_icon(void)
{
	/* Update the tree view. */
	if (!this->index().isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid index of a waypoint";
		return sg_ret::err;
	}

	this->self_assign_icon();
	this->tree_view->apply_tree_item_icon(this);

	return sg_ret::ok;
}




/**
   Make sure that new name of waypoint is update in all relevant places
*/
sg_ret Waypoint::propagate_new_waypoint_name(void)
{
	/* Update the tree view. */
	if (!this->index().isValid()) {
		qDebug() << SG_PREFIX_E << "Invalid index of a waypoint";
		return sg_ret::err;
	}


	LayerTRW * parent_trw = this->owner_trw_layer();

	this->tree_view->apply_tree_item_name(this);
	this->tree_view->sort_children(&parent_trw->waypoints_node(), parent_trw->wp_sort_order);

	return sg_ret::ok;
}




/**
   @reviewed-on 2019-12-02
*/
void Waypoint::list_dialog(QString const & title, Layer * parent_layer)
{
	assert (parent_layer->m_kind == LayerKind::TRW || parent_layer->m_kind == LayerKind::Aggregate);

	const std::list<SGObjectTypeID> wanted_types = { Waypoint::type_id() };
	TreeItemListDialogWrapper<Waypoint> list_dialog(parent_layer, parent_layer->m_kind == LayerKind::Aggregate);
	if (!list_dialog.find_tree_items(wanted_types)) {
		Dialog::info(QObject::tr("No Waypoints found"), parent_layer->get_window());
		return;
	}
	list_dialog.show_tree_items(title);
}




sg_ret Waypoint::properties_dialog_set(void)
{
	Window * window = ThisApp::main_window();
	LayerToolTRWEditWaypoint * wp_tool = (LayerToolTRWEditWaypoint *) window->toolbox()->get_tool(LayerToolTRWEditWaypoint::tool_id());
	if (!wp_tool->is_activated()) {
		/* Someone is asking to fill dialog data with waypoint
		   when WP edit tool is not active. This is ok, maybe
		   generic select tool is active and has been used to
		   select a waypoint? */

		LayerToolSelect * select_tool = (LayerToolSelect *) window->toolbox()->get_tool(LayerToolSelect::tool_id());
		if (!select_tool->is_activated()) {
			qDebug() << SG_PREFIX_E << "Trying to fill 'wp properties' dialog when neither 'wp edit' tool nor 'generic select' tool are active";
			return sg_ret::err;
		}
	}

	wp_tool->point_properties_dialog->dialog_data_set(this);
	return sg_ret::ok;
}




sg_ret Waypoint::properties_dialog_reset(void)
{
	Window * window = ThisApp::main_window();
	LayerToolTRWEditWaypoint * tool = (LayerToolTRWEditWaypoint *) window->toolbox()->get_tool(LayerToolTRWEditWaypoint::tool_id());
	if (!tool->is_activated()) {
		return sg_ret::ok;
	}
	tool->point_properties_dialog->dialog_data_reset();
	return sg_ret::ok;
}




TreeItemViewFormat Waypoint::get_view_format_header(bool include_parent_layer)
{
	const AltitudeType::Unit height_unit = Preferences::get_unit_height();

	TreeItemViewFormat view_format;
	if (include_parent_layer) {
		view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::ParentLayer, true, QObject::tr("ParentLayer"))); // this->view->horizontalHeader()->setSectionResizeMode(WaypointListModel::LayerName, QHeaderView::Interactive);
	}
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::TheItem,    true, QObject::tr("Name"))); // this->view->horizontalHeader()->setSectionResizeMode(WaypointListModel::Waypoint, QHeaderView::Interactive);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Timestamp,  true, QObject::tr("Timestamp"))); // this->view->horizontalHeader()->setSectionResizeMode(WaypointListModel::Date, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Visibility, true, QObject::tr("Visibility"))); // this->view->horizontalHeader()->setSectionResizeMode(WaypointListModel::Visibility, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Comment,    true, QObject::tr("Comment"))); // this->view->horizontalHeader()->setSectionResizeMode(WaypointListModel::Comment, QHeaderView::Stretch);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Elevation,  true, QObject::tr("Height\n(%1)").arg(Altitude::unit_full_string(height_unit)))); // this->view->horizontalHeader()->setSectionResizeMode(WaypointListModel::Elevation, QHeaderView::ResizeToContents);
	view_format.columns.push_back(TreeItemViewColumn(TreeItemPropertyID::Icon,       true, QObject::tr("Symbol"))); // this->view->horizontalHeader()->setSectionResizeMode(WaypointListModel::Icon, QHeaderView::ResizeToContents);


	return view_format;
}




uint64_t Waypoint::apply_new_preferences(void)
{
	if (this->symbol_name.isEmpty()) {
		return 0;
	}

	/* This function is called to re-get waypoint's symbol from
	   GarminSymbols, with current "waypoint's symbol size"
	   setting from Preferences -> General.  The symbol is shown
	   in viewport */
	const QString tmp_symbol_name = this->symbol_name;
	this->set_symbol_name(tmp_symbol_name);

	return 1;
}
