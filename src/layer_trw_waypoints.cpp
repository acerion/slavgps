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
#include "garmin_symbols.h"




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
	this->name_generator.set_parent_sublayer(this);
}




LayerTRWWaypoints::LayerTRWWaypoints(TreeView * ref_tree_view) : LayerTRWWaypoints()
{
	this->tree_view = ref_tree_view;
}




LayerTRWWaypoints::~LayerTRWWaypoints()
{
	this->clear();
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




void LayerTRWWaypoints::list_wp_uids(std::list<sg_uid_t> & list)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		list.push_back(i->first);
	}
}




std::list<Waypoint *> LayerTRWWaypoints::get_sorted_by_name(void) const
{
	std::list<Waypoint *> result;
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		result.push_back(i->second);
	}
	result.sort(TreeItem::compare_name);

	return result;
}




/**
   @brief Find out if any waypoints have the same name in this sublayer

   @return a waypoint, which name is duplicated (i.e. some other waypoint has the same name)
*/
Waypoint * LayerTRWWaypoints::find_waypoint_with_duplicate_name(void) const
{
	/* Build list of names. Sort list alphabetically. Find any two adjacent duplicates on the list. */

	if (this->items.size() <= 1) {
		return NULL;
	}

	std::list<Waypoint *> waypoints = this->get_sorted_by_name();

	for (auto iter = std::next(waypoints.begin()); iter != waypoints.end(); iter++) {
		QString const this_one = (*iter)->name;
		QString const previous = (*(std::prev(iter)))->name;

		if (this_one == previous) {
			return *iter;
		}
	}

	return NULL;
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
		qDebug() << "EE" PREFIX << "called for empty waypoints set";
		return;
	}

	/*
	  - Search waypoints set for an instance of duplicate name
	  - get waypoint with duplicate name
	  - create new name
	  - rename waypoint
	  - repeat until there are no waypoints with duplicate names
	*/

	Waypoint * wp = this->find_waypoint_with_duplicate_name();
	while (wp) {
		/* Rename it. */
		const QString uniqe_name = this->new_unique_element_name(wp->name);
		wp->set_name(uniqe_name);
		this->propagate_new_waypoint_name(wp);
		/* kamilFIXME: in C application did we free this unique name anywhere? */

		/* Try to find duplicate names again in the updated set of waypoints. */
		wp = this->find_waypoint_with_duplicate_name();
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
void LayerTRWWaypoints::propagate_new_waypoint_name(const Waypoint * wp)
{
	/* Update the tree view. */
	if (wp->index.isValid()) {
		this->tree_view->set_tree_item_name(wp->index, wp->name);
		this->tree_view->sort_children(this->get_index(), ((LayerTRW *) this->owning_layer)->wp_sort_order);
	} else {
		qDebug() << "EE: Layer TRW: trying to rename waypoint with invalid index";
	}
}




/**
   Use waypoint's ::symbol_name to set waypoint's icon. Make sure that
   new icon of a waypoint (or lack of the icon) is shown wherever it
   needs to be shown.
*/
void LayerTRWWaypoints::set_new_waypoint_icon(Waypoint * wp)
{
	/* Update the tree view. */
	if (wp->index.isValid()) {
		this->icon = get_wp_icon_small(wp->symbol_name);
		this->tree_view->set_tree_item_icon(wp->index, this->icon);
	} else {
		qDebug() << "EE" PREFIX << "Invalid index of a waypoint";
	}
}




/**
   (Re)Calculate the bounds of the waypoints in this layer.
   This should be called whenever waypoints are changed (added/removed/moved).
*/
void LayerTRWWaypoints::recalculate_bbox(void)
{
	this->bbox.invalidate();

	if (0 == this->items.size()) {
		/* E.g. after all waypoints have been removed from TRW layer. */
		return;
	}


	for (auto iter = this->items.begin(); iter != this->items.end(); iter++) {
		const LatLon lat_lon = iter->second->coord.get_latlon();
		BBOX_EXPAND_WITH_LATLON(this->bbox, lat_lon);
	}
	this->bbox.validate();

	qDebug() << "DD" PREFIX << "Recalculated bounds of waypoints:" << this->bbox;

	return;
}




/*
  Can accept an empty symbol name, and may return null value
*/
QIcon SlavGPS::get_wp_icon_small(const QString & symbol_name)
{
	QIcon result;

	if (symbol_name.isEmpty()) {
		return result; /* Empty symbol name -> empty icon. */
	}

	QPixmap * wp_symbol = GarminSymbols::get_wp_symbol(symbol_name);
	/* ATM GarminSymbols::get_wp_symbol() returns a cached icon, with the size dependent on the preferences.
	   So needing a small icon for the tree view may need some resizing. */
	if (!wp_symbol) {
		qDebug() << PREFIX << "No symbol from garmin symbols";
		return result;
	}

	if (wp_symbol->width() == SMALL_ICON_SIZE) {
		/* Symbol from GarminSymbols has just the right size. */
		qDebug() << PREFIX << "Symbol from garmin symbols has correct size";
		result = QIcon(*wp_symbol);
	} else {
		const QPixmap scaled = wp_symbol->scaled(SMALL_ICON_SIZE, SMALL_ICON_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		if (!scaled.isNull()) {
			qDebug() << PREFIX << "Scaled symbol is non-empty";
			result = QIcon(scaled);
		} else {
			/* Too bad, we will return empty icon. */
			qDebug() << PREFIX << "Scaled symbol is empty";
		}
	}

	return result;
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
		viewport->show_bbox(this->bbox);
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
	if (!this->is_in_tree()) {
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
		    || TreeItem::the_same_object(g_tree->selected_tree_item, this)); /* This item discovers that it is selected and decides to be highlighted. */

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




void LayerTRWWaypoints::clear(void)
{
	for (auto iter = this->items.begin(); iter != this->items.end(); iter++) {
		delete (*iter).second;
	}

	this->items.clear();
}




void LayerTRWWaypoints::add_waypoint(Waypoint * wp)
{
	wp->owning_layer = this->owning_layer;
	this->items.insert({{ wp->uid, wp }});

	this->set_new_waypoint_icon(wp);

	return;
}




bool LayerTRWWaypoints::delete_waypoint(Waypoint * wp)
{
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	if (!wp) {
		qDebug() << "EE" PREFIX << "NULL pointer to waypoint";
		return false;
	}

	if (wp->name.isEmpty()) {
		qDebug() << "WW" PREFIX << "waypoint with empty name, deleting anyway";
	}

	if (wp == parent_layer->get_edited_wp()) {
		parent_layer->reset_edited_wp();
		parent_layer->moving_wp = false;
	}

	const bool was_visible = wp->visible;

	parent_layer->tree_view->detach_item(wp);

	this->name_generator.remove_name(wp->name);

	this->items.erase(wp->uid); /* Erase by key. */

	delete wp;

	return was_visible;
}




int DefaultNameGenerator::name_to_number(const QString & name) const
{
	if (name.size() == 3) {
		int n = name.toInt(); /* TODO: use locale-aware conversion? */
		if (n < 100 && name[0] != '0') {
			return -1;
		}

		if (n < 10 && name[0] != '0') {
			return -1;
		}
		return n;
	}
	return -1;
}




QString DefaultNameGenerator::number_to_name(int number) const
{
	return QString("%1").arg((int) (number + 1), 3, 10, (QChar) '0');
}




void DefaultNameGenerator::reset(void)
{
	this->highest_item_number = 0;
}




void DefaultNameGenerator::add_name(const QString & new_item_name)
{
	/* If is bigger than top, add it. */
	int number = this->name_to_number(new_item_name);
	if (number > this->highest_item_number) {
		this->highest_item_number = number;
	}
}




void DefaultNameGenerator::remove_name(const QString & item_name)
{
	/* If wasn't top, do nothing. if was top, count backwards until we find one used. */
	int number = this->name_to_number(item_name);
	if (this->highest_item_number == number) {

		this->highest_item_number--;
		QString name = this->number_to_name(this->highest_item_number);

		/* Search down until we find something that *does* exist. */
		while (this->highest_item_number > 0 && !this->parent->find_waypoint_by_name(name)) {
		       this->highest_item_number--;
		       name = this->number_to_name(this->highest_item_number);
		}
	}
}




QString DefaultNameGenerator::try_new_name(void) const
{
	QString result;

	if (this->highest_item_number < 0 /* TODO: handle overflow in different way. */
	    || this->highest_item_number >= 999) { /* TODO: that's rather limiting, isn't it? */

		return result;
	}
	result = this->number_to_name(this->highest_item_number);

	return result;
}
