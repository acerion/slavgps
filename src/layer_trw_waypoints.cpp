/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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




#include <QDebug>




#include "layer_trw.h"
#include "layer_trw_waypoint.h"
#include "layer_trw_waypoints.h"
#include "layer_trw_menu.h"
#include "layer_trw_painter.h"
#include "viewport_internal.h"
#include "tree_view_internal.h"
#include "window.h"
#include "layers_panel.h"
#include "clipboard.h"
#include "thumbnails.h"
#include "garminsymbols.h"




using namespace SlavGPS;




#define PREFIX ": Layer TRW Waypoints:" << __FUNCTION__ << __LINE__ << ">"




extern Tree * g_tree;




/* This is how it knows when you click if you are clicking close to a waypoint. */
#define WAYPOINT_SIZE_APPROX 5




LayerTRWWaypoints::LayerTRWWaypoints()
{
	this->tree_item_type = TreeItemType::SUBLAYER;
	this->type_id = "sg.trw.waypoints";
	this->accepted_child_type_ids << "sg.trw.waypoint";
	this->editable = false;
}




LayerTRWWaypoints::LayerTRWWaypoints(TreeView * ref_tree_view) : LayerTRWWaypoints()
{
	this->tree_view = ref_tree_view;
}




LayerTRWWaypoints::~LayerTRWWaypoints()
{
	/* kamilTODO: call destructors of Waypoint objects? */
	this->items.clear();
}




QString LayerTRWWaypoints::get_tooltip(void)
{
	/* Very simple tooltip - may expand detail in the future. */
	return tr("Waypoints: %1").arg(this->items.size());
}




Waypoint * LayerTRWWaypoints::find_waypoint_by_name(const QString & wp_name)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		if (i->second && !i->second->name.isEmpty()) {
			if (i->second->name == wp_name) {
				return i->second;
			}
		}
	}
	return NULL;
}




Waypoint * LayerTRWWaypoints::find_waypoint_by_date(char const * date)
{
	char date_buf[20];
	Waypoint * wp = NULL;

	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		date_buf[0] = '\0';
		wp = i->second;

		/* Might be an easier way to compare dates rather than converting the strings all the time... */
		if (wp->has_timestamp) {
			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&wp->timestamp));

			if (!g_strcmp0(date, date_buf)) {
				return wp;
			}
		}
	}
	return NULL;
}




void LayerTRWWaypoints::find_maxmin(LatLonMinMax & min_max)
{
	if (this->items.size() > 1) { /* kamil TODO this condition may have to be improved. */
		min_max = LatLonMinMax(this->bbox);
	}
}




void LayerTRWWaypoints::list_wp_uids(std::list<sg_uid_t> & list)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		list.push_back(i->first);
	}
}




std::list<QString> LayerTRWWaypoints::get_sorted_wp_name_list(void)
{
	std::list<QString> result;
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		result.push_back(i->second->name);
	}
	result.sort();

	return result;
}




/**
 * Find out if any waypoints have the same name in this layer.
 */
QString LayerTRWWaypoints::has_duplicate_waypoint_names(void)
{
	/* Build list of names. Sort list alphabetically. Find any two adjacent duplicates on the list. */

	if (this->items.size() <= 1) {
		return QString("");
	}

	std::list<QString> waypoint_names = this->get_sorted_wp_name_list();

	for (auto iter = std::next(waypoint_names.begin()); iter != waypoint_names.end(); iter++) {
		QString const this_one = *iter;
		QString const previous = *(std::prev(iter));

		if (this_one == previous) {
			return this_one;
		}
	}

	return QString("");
}




void LayerTRWWaypoints::set_items_visibility(bool on_off)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		i->second->visible = on_off;
		this->tree_view->set_tree_item_visibility(i->second->index, on_off);
	}
}




void LayerTRWWaypoints::toggle_items_visibility(void)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		i->second->visible = !i->second->visible;
		this->tree_view->toggle_tree_item_visibility(i->second->index);
	}
}




void LayerTRWWaypoints::search_closest_wp(WaypointSearch * search)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		Waypoint * wp = i->second;
		if (!wp->visible) {
			continue;
		}

		const ScreenPos wp_pos = search->viewport->coord_to_screen_pos(wp->coord);

		/* If waypoint has an image then use the image size to select. */
		if (search->draw_images && !wp->image_full_path.isEmpty()) {

			int slackx = wp->image_width / 2;
			int slacky = wp->image_height / 2;

			if (wp_pos.x <= search->x + slackx && wp_pos.x >= search->x - slackx
			    && wp_pos.y <= search->y + slacky && wp_pos.y >= search->y - slacky) {

				search->closest_wp = wp;
				search->closest_x = wp_pos.x;
				search->closest_y = wp_pos.y;
			}
		} else {
			const int dist_x = abs(wp_pos.x - search->x);
			const int dist_y = abs(wp_pos.y - search->y);

			if (dist_x <= WAYPOINT_SIZE_APPROX && dist_y <= WAYPOINT_SIZE_APPROX
			    && ((!search->closest_wp)
				/* Was the old waypoint we already found closer than this one? */
				|| dist_x + dist_y < abs(wp_pos.x - search->closest_x) + abs(wp_pos.y - search->closest_y))) {

				search->closest_wp = wp;
				search->closest_x = wp_pos.x;
				search->closest_y = wp_pos.y;
			}
		}

	}
}




QString LayerTRWWaypoints::tool_show_picture_wp(int event_x, int event_y, Viewport * viewport)
{
	QString found;

	for (auto i = this->items.begin(); i != this->items.end(); i++) {

		Waypoint * wp = i->second;
		if (!wp->image_full_path.isEmpty() && wp->visible) {
			const ScreenPos wp_pos = viewport->coord_to_screen_pos(wp->coord);
			int slackx = wp->image_width / 2;
			int slacky = wp->image_height / 2;
			if (wp_pos.x <= event_x + slackx && wp_pos.x >= event_x - slackx
			    && wp_pos.y <= event_y + slacky && wp_pos.y >= event_y - slacky) {

				found = wp->image_full_path;
				/* We've found a match. However continue searching
				   since we want to find the last match -- that
				   is, the match that was drawn last. */
			}
		}

	}

	return found;
}




QStringList LayerTRWWaypoints::get_list_of_missing_thumbnails(void) const
{
	QStringList paths;

	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		const Waypoint * wp = i->second;
		if (!wp->image_full_path.isEmpty() && !Thumbnails::thumbnail_exists(wp->image_full_path)) {
			paths.push_back(wp->image_full_path);
		}
	}

	return paths;
}




void LayerTRWWaypoints::change_coord_mode(CoordMode new_mode)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		i->second->convert(new_mode);
	}
}




/**
   \brief Force unqiue waypoint names for Waypoints sublayer
*/
void LayerTRWWaypoints::uniquify(sort_order_t sort_order)
{
	if (this->items.empty()) {
		qDebug() << "EE: Layer TRW: ::uniquify() called for empty waypoints set";
		return;
	}

	/*
	  - Search waypoints set for an instance of repeated name
	  - get waypoint with this name
	  - create new name
	  - rename waypoint
	  - repeat until there are no waypoints with duplicate names
	*/

	/* TODO: make the ::has_duplicate_waypoint_names() return the waypoint itself (or NULL). */
	QString duplicate_name = this->has_duplicate_waypoint_names();
	while (duplicate_name != "") {

		/* Get that waypoint that has duplicate name. */
		Waypoint * wp = this->find_waypoint_by_name(duplicate_name);
		if (!wp) {
			/* Broken :( */
			qDebug() << "EE: Layer TRW: can't retrieve waypoint with duplicate name" << duplicate_name;
			g_tree->tree_get_main_window()->get_statusbar()->set_message(StatusBarField::INFO, tr("Internal Error during making waypoints unique"));
			return;
		}

		/* Rename it. */
		const QString uniq_name = this->new_unique_element_name(wp->name);
		this->rename_waypoint(wp, uniq_name);
		/* kamilFIXME: in C application did we free this unique name anywhere? */

		/* Try to find duplicate names again in the updated set of waypoints. */
		duplicate_name = this->has_duplicate_waypoint_names();
	}

	return;
}




/**
   \brief Get a a unique new name for element of type \param item_type_id
*/
QString LayerTRWWaypoints::new_unique_element_name(const QString & old_name)
{
	int i = 2;
	QString new_name = old_name;

	Waypoint * existing = NULL;
	do {
		existing = this->find_waypoint_by_name(new_name);
		/* If found a name already in use try adding 1 to it and we try again. */
		if (existing) {
			new_name = QString("%1#%2").arg(old_name).arg(i);
			i++;
		}
	} while (existing != NULL);

	return new_name;
}




/**
 *  Rename waypoint and maintain corresponding name of waypoint in the tree view.
 */
void LayerTRWWaypoints::rename_waypoint(Waypoint * wp, const QString & new_name)
{
	wp->set_name(new_name);

	/* Update the tree view as well. */
	if (wp->index.isValid()) {
		this->tree_view->set_tree_item_name(wp->index, new_name);
		this->tree_view->sort_children(this->get_index(), ((LayerTRW *) this->owning_layer)->wp_sort_order);
	} else {
		qDebug() << "EE: Layer TRW: trying to rename waypoint with invalid index";
	}
}




/**
 *  Maintain icon of waypoint in the tree view.
 */
void LayerTRWWaypoints::reset_waypoint_icon(Waypoint * wp)
{
	/* Update the tree view. */
	if (wp->index.isValid()) {
		this->tree_view->set_tree_item_icon(wp->index, get_wp_icon_small(wp->symbol_name));
	} else {
		qDebug() << "EE: Layer TRW: trying to reset icon of waypoint with invalid index";
	}
}




/*
 * (Re)Calculate the bounds of the waypoints in this layer.
 * This should be called whenever waypoints are changed.
 */
void LayerTRWWaypoints::calculate_bounds()
{
	LatLon topleft;
	LatLon bottomright;


	auto i = this->items.begin();
	if (i == this->items.end()) {
		/* E.g. after all waypoints have been removed from trw layer. */
		return;
	}
	Waypoint * wp = i->second;
	/* Set bounds to first point. */
	if (wp) {
		topleft = wp->coord.get_latlon();
		bottomright = wp->coord.get_latlon();
	}

	/* Ensure there is another point... */
	if (this->items.size() > 1) {

		while (++i != this->items.end()) { /* kamilTODO: check the conditon. */

			wp = i->second;

			/* See if this point increases the bounds. */
			const LatLon ll = wp->coord.get_latlon();

			if (ll.lat > topleft.lat) {
				topleft.lat = ll.lat;
			}
			if (ll.lon < topleft.lon) {
				topleft.lon = ll.lon;
			}
			if (ll.lat < bottomright.lat) {
				bottomright.lat = ll.lat;
			}
			if (ll.lon > bottomright.lon){
				bottomright.lon = ll.lon;
			}
		}
	}

	this->bbox.north = topleft.lat;
	this->bbox.east = bottomright.lon;
	this->bbox.south = bottomright.lat;
	this->bbox.west = topleft.lon;

	return;
}




/*
  Can accept an empty symbol name, and may return null value
*/
QIcon SlavGPS::get_wp_icon_small(const QString & symbol_name)
{
	QPixmap * wp_pixmap = GarminSymbols::get_wp_symbol(symbol_name.toUtf8().constData());
	/* ATM GarminSymbols::get_wp_symbol() returns a cached icon, with the size dependent on the preferences.
	   So needing a small icon for the tree view may need some resizing: */
	if (wp_pixmap && wp_pixmap->width() != SMALL_ICON_SIZE) {
		/* TODO: is this assignment safe? */
		*wp_pixmap = wp_pixmap->scaled(SMALL_ICON_SIZE, SMALL_ICON_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}

	if (wp_pixmap) {
		return QIcon(*wp_pixmap);
	} else {
		return QIcon();
	}
}




/**
 * Get the earliest timestamp available from all waypoints.
 */
time_t LayerTRWWaypoints::get_earliest_timestamp()
{
	time_t timestamp = 0;

	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		Waypoint * wp = i->second;
		if (wp->has_timestamp) {
			/* When timestamp not set yet - use the first value encountered. */
			if (timestamp == 0) {
				timestamp = wp->timestamp;
			} else if (timestamp > wp->timestamp) {
				timestamp = wp->timestamp;
			} else {

			}
		}
	}

	return timestamp;
}




void LayerTRWWaypoints::add_children_to_tree(void)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		time_t timestamp = 0;
		Waypoint * wp = i->second;
		if (wp->has_timestamp) {
			timestamp = wp->timestamp;
		}

		/* At this point each item is expected to have ::owning_layer member set to enclosing TRW layer. */

		this->tree_view->add_tree_item(this->index, wp, wp->name);
		this->tree_view->set_tree_item_timestamp(wp->index, timestamp);
	}
}




void LayerTRWWaypoints::sublayer_menu_waypoints_misc(LayerTRW * parent_layer_, QMenu & menu)
{
	QAction * qa = NULL;

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&View All Waypoints"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (move_viewport_to_show_all_cb()));

	qa = menu.addAction(QIcon::fromTheme("edit-find"), tr("Find &Waypoint..."));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (find_waypoint_dialog_cb()));

	qa = menu.addAction(QIcon::fromTheme("list-remove"), tr("Delete &All Waypoints"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (delete_all_waypoints_cb()));

	qa = menu.addAction(tr("&Delete Waypoints From Selection..."));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (delete_selected_waypoints_cb()));

	{
		QMenu * vis_submenu = menu.addMenu(tr("&Visibility"));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), tr("&Show All Waypoints"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (items_visibility_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-remove"), tr("&Hide All Waypoints"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (items_visibility_off_cb()));

		qa = vis_submenu->addAction(tr("&Toggle Visibility of All Waypoints"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (items_visibility_toggle_cb()));
	}

	qa = menu.addAction(tr("&Waypoints List..."));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (waypoint_list_dialog_cb()));
}




void LayerTRWWaypoints::sublayer_menu_sort(QMenu & menu)
{
	QAction * qa = NULL;

	QMenu * sort_submenu = menu.addMenu(QIcon::fromTheme("view-refresh"), QObject::tr("&Sort"));

	qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), QObject::tr("Name &Ascending"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_a2z_cb()));

	qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), QObject::tr("Name &Descending"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_z2a_cb()));

	qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), QObject::tr("Date Ascending"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_timestamp_ascend_cb()));

	qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), QObject::tr("Date Descending"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_timestamp_descend_cb()));
}




/* Panel can be NULL if necessary - i.e. right-click from a tool. */
/* Viewpoint is now available instead. */
bool LayerTRWWaypoints::add_context_menu_items(QMenu & menu, bool tree_view_context_menu)
{
	QAction * qa = NULL;
	bool rv = false;


	qa = menu.addAction(QIcon::fromTheme("edit-paste"), tr("Paste"));
	/* TODO: only enable if suitable item is in clipboard - want to determine *which* sublayer type. */
	qa->setEnabled(Clipboard::get_current_type() == ClipboardDataType::SUBLAYER);
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (paste_sublayer_cb()));


	menu.addSeparator();


	if (g_tree->tree_get_items_tree()) {
		rv = true;
		qa = menu.addAction(QIcon::fromTheme("document-new"), tr("&New Waypoint..."));
		connect(qa, SIGNAL (triggered(bool)), (LayerTRW *) this->owning_layer, SLOT (new_waypoint_cb()));
	}


	this->sublayer_menu_waypoints_misc((LayerTRW *) this->owning_layer, menu);


	this->sublayer_menu_sort(menu);


	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));
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


	return rv;
}




/**
   \brief Re-adjust main viewport to show all items in this node
*/
void LayerTRWWaypoints::move_viewport_to_show_all_cb(void) /* Slot. */
{
	Viewport * viewport = g_tree->tree_get_main_viewport();
	const unsigned int n_items = this->items.size();

	if (1 == n_items) {
		/* Only 1 waypoint - jump straight to it. Notice that we don't care about waypoint's visibility.  */
		const auto item = this->items.begin();
		viewport->set_center_from_coord(item->second->coord, true);

	} else if (1 < n_items) {
		/* If at least 2 waypoints - find center and then zoom to fit */
		viewport->show_latlons(LatLonMinMax(this->bbox));
	} else {
		return; /* Zero items. */
	}

	/* We have re-zoomed main viewport. Ask main application window to redraw the viewport. */
	qDebug() << "SIGNAL: Layer TRW Waypoints: re-zoom to show all items (" << n_items << "item(s))";
	g_tree->emit_items_tree_updated();

	return;
}




void LayerTRWWaypoints::items_visibility_on_cb(void) /* Slot. */
{
	this->set_items_visibility(true);
	/* Redraw. */
	this->owning_layer->emit_layer_changed();
}




void LayerTRWWaypoints::items_visibility_off_cb(void) /* Slot. */
{
	this->set_items_visibility(false);
	/* Redraw. */
	this->owning_layer->emit_layer_changed();
}




void LayerTRWWaypoints::items_visibility_toggle_cb(void) /* Slot. */
{
	this->toggle_items_visibility();
	/* Redraw. */
	this->owning_layer->emit_layer_changed();
}




void LayerTRWWaypoints::apply_dem_data_all_cb(void)
{
	this->apply_dem_data_common(false);
}




void LayerTRWWaypoints::apply_dem_data_only_missing_cb(void)
{
	this->apply_dem_data_common(true);
}




void LayerTRWWaypoints::apply_dem_data_common(bool skip_existing_elevations)
{
	LayersPanel * panel = g_tree->tree_get_items_tree();
	if (!panel->has_any_layer_of_type(LayerType::DEM)) {
		return;
	}

	int changed_ = 0;
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		changed_ = changed_ + (int) i->second->apply_dem_data(skip_existing_elevations);
	}

	LayerTRW * trw = (LayerTRW *) this->owning_layer;
	trw->wp_changed_message(changed_);
}




bool LayerTRWWaypoints::handle_selection_in_tree(void)
{
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	//parent_layer->set_statusbar_msg_info_trk(this);
	parent_layer->reset_internal_selections(); /* No other tree item (that is a sublayer of this layer) is selected... */

	g_tree->selected_tree_item = this;

	return true;
}




/**
 * Generally for drawing all tracks or routes or waypoints
 * tracks may be actually routes
 * It assumes they belong to the TRW Layer (it doesn't check this is the case)
 */
void LayerTRWWaypoints::draw_tree_item(Viewport * viewport, bool hl_is_allowed, bool hl_is_required)
{
	if (!this->tree_view) {
		/* This subnode hasn't been added to tree yet. */
		return;
	}


	/* kamilFIXME: enabling this code and then compiling it with -O0 results in crash when selecting trackpoint in viewport. */
#if 1
	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this->index)) {
		return;
	}
#endif

	if (this->items.empty()) {
		return;
	}

	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;
	const bool allowed = hl_is_allowed;
	const bool required = allowed
		&& (hl_is_required /* Parent code requires us to do highlight. */
		    || (g_tree->selected_tree_item && g_tree->selected_tree_item == this)); /* This item discovers that it is selected and decides to be highlighted. */ /* TODO: use UID to compare tree items. */

	const LatLonBBox viewport_bbox = viewport->get_bbox();

	if (BBOX_INTERSECT (this->bbox, viewport_bbox)) {
		for (auto i = this->items.begin(); i != this->items.end(); i++) {
			qDebug() << "II: Layer TRW Waypoints: draw tree item" << i->second->type_id << i->second->name;
			i->second->draw_tree_item(viewport, allowed, required);
		}
	}
}




void LayerTRWWaypoints::paste_sublayer_cb(void)
{
	/* Slightly cheating method, routing via the panels capability. */
	Clipboard::paste(g_tree->tree_get_items_tree());
}





void LayerTRWWaypoints::sort_order_a2z_cb(void)
{
	((LayerTRW *) this->owning_layer)->wp_sort_order = VL_SO_ALPHABETICAL_ASCENDING;
	this->tree_view->sort_children(this->index, VL_SO_ALPHABETICAL_ASCENDING);
}




void LayerTRWWaypoints::sort_order_z2a_cb(void)
{
	((LayerTRW *) this->owning_layer)->wp_sort_order = VL_SO_ALPHABETICAL_DESCENDING;
	this->tree_view->sort_children(this->index, VL_SO_ALPHABETICAL_DESCENDING);
}




void LayerTRWWaypoints::sort_order_timestamp_ascend_cb(void)
{
	((LayerTRW *) this->owning_layer)->wp_sort_order = VL_SO_DATE_ASCENDING;
	this->tree_view->sort_children(this->index, VL_SO_DATE_ASCENDING);
}




void LayerTRWWaypoints::sort_order_timestamp_descend_cb(void)
{
	((LayerTRW *) this->owning_layer)->wp_sort_order = VL_SO_DATE_DESCENDING;
	this->tree_view->sort_children(this->index, VL_SO_DATE_DESCENDING);
}
