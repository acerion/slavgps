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




#include <cassert>




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




#define SG_MODULE "Layer TRW Waypoints"




extern SelectedTreeItems g_selected;




/* This is how it knows when you click if you are clicking close to a waypoint. */
#define WAYPOINT_SIZE_APPROX 5




LayerTRWWaypoints::LayerTRWWaypoints()
{
	this->m_type_id = LayerTRWWaypoints::type_id();
	this->accepted_child_type_ids.push_back(Waypoint::type_id());
	this->editable = false;
	this->name_generator.set_parent_sublayer(this);
	this->set_name(tr("Waypoints"));
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Paste);
}




LayerTRWWaypoints::LayerTRWWaypoints(TreeView * ref_tree_view) : LayerTRWWaypoints()
{
	this->tree_view = ref_tree_view;
}




LayerTRWWaypoints::~LayerTRWWaypoints()
{
	this->clear();
}




SGObjectTypeID LayerTRWWaypoints::get_type_id(void) const
{
	return LayerTRWWaypoints::type_id();
}
SGObjectTypeID LayerTRWWaypoints::type_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.trw.waypoints");
	return id;
}




QString LayerTRWWaypoints::get_tooltip(void) const
{
	/* Very simple tooltip - may expand detail in the future. */
	return tr("Waypoints: %1").arg(this->children_list.size());
}




Waypoint * LayerTRWWaypoints::find_waypoint_by_name(const QString & wp_name)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		if ((*iter)->get_name().isEmpty()) {
			if ((*iter)->get_name() == wp_name) {
				return *iter;
			}
		}
	}
	return NULL;
}




Waypoint * LayerTRWWaypoints::find_child_by_uid(sg_uid_t child_uid) const
{
	auto iter = this->children_map.find(child_uid);
	if (iter == this->children_map.end()) {
		qDebug() << SG_PREFIX_W << "Can't find waypoint with specified UID" << child_uid;
		return NULL;
	} else {
		return iter->second;
	}
}




std::list<TreeItem *> LayerTRWWaypoints::get_waypoints_by_date(const QDate & search_date) const
{
	const QString search_date_str = search_date.toString("yyyy-MM-dd");
	qDebug() << SG_PREFIX_I << "Search date =" << search_date << search_date_str;

	std::list<TreeItem *> result;

	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		Waypoint * wp = *iter;

		if (!wp->get_timestamp().is_valid()) {
			continue;
		}

		/* Might be an easier way to compare dates rather than converting the strings all the time... */
		const QString wp_date_str = wp->get_timestamp().strftime_utc("%Y-%m-%d");
		if (search_date_str == wp_date_str) {
			result.push_back(wp);
		}
	}
	return result;
}




void LayerTRWWaypoints::list_wp_uids(std::list<sg_uid_t> & list)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		list.push_back((*iter)->get_uid());
	}
}




std::list<Waypoint *> LayerTRWWaypoints::get_sorted_by_name(void) const
{
	std::list<Waypoint *> result;
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		result.push_back(*iter);
	}
	result.sort(TreeItem::compare_name_ascending);

	return result;
}




/**
   @brief Find out if any waypoints have the same name in this sublayer

   @return a waypoint, which name is duplicated (i.e. some other waypoint has the same name)
*/
Waypoint * LayerTRWWaypoints::find_waypoint_with_duplicate_name(void) const
{
	/* Build list of names. Sort list alphabetically. Find any two adjacent duplicates on the list. */

	if (this->children_list.size() <= 1) {
		return NULL;
	}

	std::list<Waypoint *> waypoints = this->get_sorted_by_name();

	for (auto iter = std::next(waypoints.begin()); iter != waypoints.end(); iter++) {
		QString const this_one = (*iter)->get_name();
		QString const previous = (*(std::prev(iter)))->get_name();

		if (this_one == previous) {
			return *iter;
		}
	}

	return NULL;
}




void LayerTRWWaypoints::set_items_visibility(bool on_off)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		(*iter)->set_visible(on_off);
		this->tree_view->apply_tree_item_visibility(*iter);
	}
}




void LayerTRWWaypoints::toggle_items_visibility(void)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		(*iter)->toggle_visible();
		this->tree_view->apply_tree_item_visibility(*iter);
	}
}




sg_ret LayerTRWWaypoints::get_tree_items(std::list<TreeItem *> & list) const
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		list.push_back(*iter);
	}

	return sg_ret::ok;
}




void LayerTRWWaypoints::search_closest_wp(WaypointSearch & search)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		Waypoint * wp = *iter;
		if (!wp->is_visible()) {
			continue;
		}

		if (NULL != search.skip_wp && wp == search.skip_wp) {
			continue;
		}

		ScreenPos wp_pos;
		search.gisview->coord_to_screen_pos(wp->get_coord(), wp_pos);

		bool found = false;

		/* If waypoint has non-empty image then use the image size to select. */
		if (!wp->drawn_image_rect.isNull()) {

			int slackx = wp->drawn_image_rect.width() / 2;
			int slacky = wp->drawn_image_rect.height() / 2;

			if (wp_pos.x() <= search.x + slackx
			    && wp_pos.x() >= search.x - slackx
			    && wp_pos.y() <= search.y + slacky
			    && wp_pos.y() >= search.y - slacky) {

				found = true;
			}
		} else {
			const int dist_x = std::fabs(wp_pos.x() - search.x);
			const int dist_y = std::fabs(wp_pos.y() - search.y);

			if (dist_x <= WAYPOINT_SIZE_APPROX && dist_y <= WAYPOINT_SIZE_APPROX
			    && ((!search.closest_wp)
				/* Was the old waypoint we already found closer than this one? */
				|| dist_x + dist_y < std::fabs(wp_pos.x() - search.closest_pos.x()) + std::fabs(wp_pos.y() - search.closest_pos.y()))) {

				found = true;
			}
		}

		if (found) {
			search.closest_wp = wp;
			search.closest_pos = wp_pos;
		}
	}
}




QString LayerTRWWaypoints::tool_show_picture_wp(int event_x, int event_y, GisViewport * gisview)
{
	QString found;

	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {

		Waypoint * wp = *iter;
		if (wp->drawn_image_rect.isNull()) {
			/* Waypoint with empty 'drawn image rectangle'
			   is not shown in viewport so it couldn't
			   have been clicked. */
			continue;
		}

		ScreenPos wp_pos;
		gisview->coord_to_screen_pos(wp->get_coord(), wp_pos);

		int slackx = wp->drawn_image_rect.width() / 2;
		int slacky = wp->drawn_image_rect.height() / 2;
		if (wp_pos.x() <= event_x + slackx && wp_pos.x() >= event_x - slackx
		    && wp_pos.y() <= event_y + slacky && wp_pos.y() >= event_y - slacky) {

			found = wp->image_full_path;
			/* We've found a match. However continue searching
			   since we want to find the last match -- that
			   is, the match that was drawn last. */
		}

	}

	return found;
}




QStringList LayerTRWWaypoints::get_list_of_missing_thumbnails(void) const
{
	QStringList paths;

	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		const Waypoint * wp = *iter;
		if (!wp->image_full_path.isEmpty() && !Thumbnails::thumbnail_exists(wp->image_full_path)) {
			paths.push_back(wp->image_full_path);
		}
	}

	return paths;
}




void LayerTRWWaypoints::change_coord_mode(CoordMode new_mode)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		(*iter)->convert(new_mode);
	}
}




/**
   \brief Force unqiue waypoint names for Waypoints sublayer
*/
void LayerTRWWaypoints::uniquify(TreeViewSortOrder sort_order)
{
	if (this->empty()) {
		qDebug() << SG_PREFIX_E << "Called for empty waypoints set";
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
		const QString uniqe_name = this->new_unique_element_name(wp->get_name());
		wp->set_name(uniqe_name);
		this->tree_view->apply_tree_item_name(wp);

		/* Try to find duplicate names again in the updated set of waypoints. */
		wp = this->find_waypoint_with_duplicate_name();
	}

	/* Sort waypoints only after completing uniquify-ing operation. */
	this->tree_view->sort_children(this, sort_order);

	return;
}




/**
   \brief Get a a unique new name for element of type \param item_type_id
*/
QString LayerTRWWaypoints::new_unique_element_name(const QString & existing_name)
{
	int i = 2;
	QString new_name = existing_name;

	Waypoint * existing_wp = NULL;
	do {
		existing_wp = this->find_waypoint_by_name(new_name);
		/* If found a name already in use try adding 1 to it and we try again. */
		if (existing_wp) {
			new_name = QString("%1#%2").arg(existing_name).arg(i);
			i++;
		}
	} while (existing_wp != NULL);

	return new_name;
}




/**
   (Re)Calculate the bounds of the waypoints in this layer.
   This should be called whenever waypoints are changed (added/removed/moved).
*/
void LayerTRWWaypoints::recalculate_bbox(void)
{
	this->bbox.invalidate();

	if (0 == this->children_list.size()) {
		/* E.g. after all waypoints have been removed from TRW layer. */
		return;
	}


	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		const LatLon lat_lon = (*iter)->get_coord().get_lat_lon();
		this->bbox.expand_with_lat_lon(lat_lon);
	}
	this->bbox.validate();

	qDebug() << SG_PREFIX_D << "Recalculated bounds of waypoints:" << this->bbox;

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
		qDebug() << SG_PREFIX_W << "No symbol from garmin symbols";
		return result;
	}

	if (wp_symbol->width() == SMALL_ICON_SIZE) {
		/* Symbol from GarminSymbols has just the right size. */
		qDebug() << SG_PREFIX_I << "Symbol from garmin symbols has correct size";
		result = QIcon(*wp_symbol);
	} else {
		const QPixmap scaled = wp_symbol->scaled(SMALL_ICON_SIZE, SMALL_ICON_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		if (!scaled.isNull()) {
			qDebug() << SG_PREFIX_I << "Scaled symbol is non-empty";
			result = QIcon(scaled);
		} else {
			/* Too bad, we will return empty icon. */
			qDebug() << SG_PREFIX_W << "Scaled symbol is empty";
		}
	}

	return result;
}




/**
 * Get the earliest timestamp available from all waypoints.
 */
Time LayerTRWWaypoints::get_earliest_timestamp(void) const
{
	Time result;

	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		const Waypoint * wp = *iter;
		if (!wp->get_timestamp().is_valid()) {
			continue;
		}

		/* When timestamp not set yet - use the first value encountered. */
		if (!result.is_valid()) {
			result = wp->get_timestamp();
		} else if (result > wp->get_timestamp()) {
			result = wp->get_timestamp();
		} else {
			; /* NOOP */
		}
	}

	return result;
}




sg_ret LayerTRWWaypoints::attach_children_to_tree(void)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		Waypoint * wp = *iter;
		if (wp->is_in_tree()) {
			continue;
		}

		qDebug() << SG_PREFIX_I << "Attaching to tree item" << wp->get_name() << "under" << this->get_name();
		this->tree_view->attach_to_tree(this, wp);
	}
	/* Update our own tooltip in tree view. */
	this->update_tree_item_tooltip();

	return sg_ret::ok;
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
bool LayerTRWWaypoints::menu_add_type_specific_operations(QMenu & menu, bool tree_view_context_menu)
{
	QAction * qa = NULL;
	bool rv = false;


	if (ThisApp::get_layers_panel()) {
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
	GisViewport * gisview = ThisApp::get_main_gis_view();
	const unsigned int n_items = this->children_list.size();

	if (1 == n_items) {
		/* Only 1 waypoint - jump straight to it. Notice that we don't care about waypoint's visibility.  */
		const auto iter = this->children_list.begin();
		gisview->set_center_coord((*iter)->get_coord());

	} else if (1 < n_items) {
		/* If at least 2 waypoints - find center and then zoom to fit */
		gisview->set_bbox(this->bbox);
	} else {
		return; /* Zero items. */
	}

	/* We have re-aligned main viewport. Ask main application window to redraw the viewport. */
	gisview->request_redraw("Re-align viewport to show to show all TRW Waypoints");

	return;
}




void LayerTRWWaypoints::items_visibility_on_cb(void) /* Slot. */
{
	this->set_items_visibility(true);
	/* Redraw. */
	this->emit_tree_item_changed("TRW - Waypoints - Visibility On");
}




void LayerTRWWaypoints::items_visibility_off_cb(void) /* Slot. */
{
	this->set_items_visibility(false);
	/* Redraw. */
	this->emit_tree_item_changed("TRW - Waypoints - Visibility Off");
}




void LayerTRWWaypoints::items_visibility_toggle_cb(void) /* Slot. */
{
	this->toggle_items_visibility();
	/* Redraw. */
	this->emit_tree_item_changed("TRW - Waypoints - Visibility Toggle");
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
	LayersPanel * panel = ThisApp::get_layers_panel();
	if (!panel->has_any_layer_of_kind(LayerKind::DEM)) {
		Dialog::error(tr("No DEM layers available, thus no DEM values can be applied."), ThisApp::get_main_window());
		return;
	}

	int changed_ = 0;
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		changed_ = changed_ + (int) (*iter)->apply_dem_data(skip_existing_elevations);
	}

	LayerTRW * trw = (LayerTRW *) this->owning_layer;
	trw->wp_changed_message(changed_);
}




bool LayerTRWWaypoints::handle_selection_in_tree(void)
{
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	//parent_layer->set_statusbar_msg_info_trk(this);
	parent_layer->reset_internal_selections(); /* No other tree item (that is a sublayer of this layer) is selected... */

	qDebug() << SG_PREFIX_I << "Tree item" << this->get_name() << "becomes selected tree item";
	g_selected.add_to_set(this);

	return true;
}




/**
 * Generally for drawing all tracks or routes or waypoints
 * tracks may be actually routes
 * It assumes they belong to the TRW Layer (it doesn't check this is the case)
 */
void LayerTRWWaypoints::draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected)
{
	if (this->empty()) {
		return;
	}

	if (!this->is_in_tree()) {
		/* This subnode hasn't been added to tree yet. */
		return;
	}

	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this)) {
		return;
	}

	if (1) {
		if (g_selected.is_in_set(this)) {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << this->get_name() << "as selected (selected directly)";
		} else if (parent_is_selected) {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << this->get_name() << "as selected (selected through parent)";
		} else {
			qDebug() << SG_PREFIX_I << "Drawing tree item" << this->get_name() << "as non-selected";
		}
	}

	const bool item_is_selected = parent_is_selected || g_selected.is_in_set(this);
	const LatLonBBox viewport_bbox = gisview->get_bbox();

	if (this->bbox.intersects_with(viewport_bbox)) {
		for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
			qDebug() << SG_PREFIX_I << "Will now draw tree item" << (*iter)->m_type_id << (*iter)->get_name();
			(*iter)->draw_tree_item(gisview, highlight_selected, item_is_selected);
		}
	}
}




sg_ret LayerTRWWaypoints::paste_child_tree_item_cb(void)
{
	/* Slightly cheating method, routing via the panels capability. */
	bool dummy = false;
	return Clipboard::paste(ThisApp::get_layers_panel(), dummy);
}





void LayerTRWWaypoints::sort_order_a2z_cb(void)
{
	((LayerTRW *) this->owning_layer)->wp_sort_order = TreeViewSortOrder::AlphabeticalAscending;
	this->tree_view->sort_children(this, TreeViewSortOrder::AlphabeticalAscending);
}




void LayerTRWWaypoints::sort_order_z2a_cb(void)
{
	((LayerTRW *) this->owning_layer)->wp_sort_order = TreeViewSortOrder::AlphabeticalDescending;
	this->tree_view->sort_children(this, TreeViewSortOrder::AlphabeticalDescending);
}




void LayerTRWWaypoints::sort_order_timestamp_ascend_cb(void)
{
	((LayerTRW *) this->owning_layer)->wp_sort_order = TreeViewSortOrder::DateAscending;
	this->tree_view->sort_children(this, TreeViewSortOrder::DateAscending);
}




void LayerTRWWaypoints::sort_order_timestamp_descend_cb(void)
{
	((LayerTRW *) this->owning_layer)->wp_sort_order = TreeViewSortOrder::DateDescending;
	this->tree_view->sort_children(this, TreeViewSortOrder::DateDescending);
}




void LayerTRWWaypoints::clear(void)
{
	this->children_map.clear();
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		delete *iter;
	}

	this->children_list.clear();
}




size_t LayerTRWWaypoints::size(void) const
{
	return this->children_list.size();
}




bool LayerTRWWaypoints::empty(void) const
{
	return this->children_list.empty();
}




sg_ret LayerTRWWaypoints::attach_to_container(Waypoint * wp)
{
	wp->set_owning_layer(this->get_owning_layer());
	this->children_map.insert({{ wp->get_uid(), wp }});
	this->children_list.push_back(wp);

	wp->set_new_waypoint_icon();

	this->name_generator.add_name(wp->get_name());

	return sg_ret::ok;
}




sg_ret LayerTRWWaypoints::detach_from_container(Waypoint * wp, bool * was_visible)
{
	if (!wp) {
		qDebug() << SG_PREFIX_E << "NULL pointer to waypoint";
		return sg_ret::err;
	}

	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	if (wp->get_name().isEmpty()) {
		qDebug() << SG_PREFIX_W << "Waypoint with empty name, deleting anyway";
	}

	if (wp == parent_layer->selected_wp_get()) {
		parent_layer->selected_wp_reset();
		parent_layer->moving_wp = false;
	}

	if (NULL != was_visible) {
		*was_visible = wp->is_visible();
	}

	this->name_generator.remove_name(wp->get_name());

	this->children_map.erase(wp->get_uid()); /* Erase by key. */


	TreeItemIdentityPredicate pred(wp);
	auto iter = std::find_if(this->children_list.begin(), this->children_list.end(), pred);
	if (iter != this->children_list.end()) {
		qDebug() << SG_PREFIX_I << "Will remove" << (*iter)->get_name() << "from list" << this->get_name();
		this->children_list.erase(iter);
	}

#if 0   /* Old code. */
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		qDebug() << SG_PREFIX_I << "Will compare waypoints" << (*iter)->get_name() << "and" << wp->get_name();
		if (TreeItem::the_same_object(*iter, wp)) {
			this->children_list.erase(iter);
			break;
		}
	}
#endif

	return sg_ret::ok;
}




int DefaultNameGenerator::name_to_number(const QString & name) const
{
	if (name.size() == 3) {
		int n = name.toInt(); /* TODO_LATER: use locale-aware conversion? */
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

	if (this->highest_item_number < 0 /* TODO_LATER: handle overflow in different way. */
	    || this->highest_item_number >= 999) { /* TODO_LATER: that's rather limiting, isn't it? */

		return result;
	}
	result = this->number_to_name(this->highest_item_number);

	return result;
}




sg_ret LayerTRWWaypoints::drag_drop_request(TreeItem * tree_item, int row, int col)
{
	/* Handle item in old location. */
	{
		LayerTRW * trw = (LayerTRW *) tree_item->get_owning_layer();
		trw->detach_from_container((Waypoint *) tree_item);
		/* Detaching of tree item from tree view will be handled by QT. */

		/* Update our own tooltip in tree view. */
		trw->update_tree_item_tooltip();
	}

	/* Handle item in new location. */
	{
		this->attach_to_container((Waypoint *) tree_item);
		qDebug() << SG_PREFIX_I << "Attaching to tree item" << tree_item->get_name() << "under" << this->get_name();
		this->tree_view->attach_to_tree(this, tree_item);

		/* Update our own tooltip in tree view. */
		this->update_tree_item_tooltip();
	}

	return sg_ret::ok;
}




bool LayerTRWWaypoints::move_child(TreeItem & child_tree_item, bool up)
{
	if (child_tree_item.get_type_id() != Waypoint::type_id()) {
		qDebug() << SG_PREFIX_E << "Attempting to move non-waypoint child" << child_tree_item.m_type_id;
		return false;
	}

	Waypoint * wp = (Waypoint *) &child_tree_item;

	qDebug() << SG_PREFIX_I << "Will now try to move child item of" << this->get_name() << (up ? "up" : "down");
	const bool result = move_tree_item_child_algo(this->children_list, wp, up);
	qDebug() << SG_PREFIX_I << "Result of attempt to move child item" << (up ? "up" : "down") << ":" << (result ? "success" : "failure");

	/* In this function we only move children in container of tree items.
	   Movement in tree widget is handled elsewhere. */

	return result;
}
