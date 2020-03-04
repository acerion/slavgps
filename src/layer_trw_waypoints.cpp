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




#include "clipboard.h"
#include "garmin_symbols.h"
#include "layer_trw.h"
#include "layer_trw_menu.h"
#include "layer_trw_painter.h"
#include "layer_trw_tools.h"
#include "layer_trw_waypoint.h"
#include "layer_trw_waypoint_properties.h"
#include "layer_trw_waypoints.h"
#include "layers_panel.h"
#include "thumbnails.h"
#include "toolbox.h"
#include "tree_view_internal.h"
#include "viewport_internal.h"
#include "window.h"




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
	qDebug() << SG_PREFIX_I << "Destructor of" << this->get_name() << "called";
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
	int rows = this->child_rows_count();
	if (rows < 0) {
		rows = 0;
	}
	return tr("Waypoints: %1").arg(rows);
}




/**
   @reviewed-on 2020-02-25

   Simple wrapper that does cast to Waypoint* internally before returning a pointer.
*/
Waypoint * LayerTRWWaypoints::find_waypoint_by_name(const QString & wp_name)
{
	return (Waypoint *) this->find_child_by_name(wp_name);
}




std::list<TreeItem *> LayerTRWWaypoints::find_children_by_date(const QDate & search_date) const
{
	const QString search_date_str = search_date.toString("yyyy-MM-dd");
	qDebug() << SG_PREFIX_I << "Search date =" << search_date << search_date_str;

	std::list<TreeItem *> result;

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		if (!tree_item->get_timestamp().is_valid()) {
			continue;
		}

		/* Might be an easier way to compare dates rather than converting the strings all the time... */
		const QString tree_item_date_str = tree_item->get_timestamp().strftime_utc("%Y-%m-%d");
		if (search_date_str == tree_item_date_str) {
			result.push_back(tree_item);
		}
	}
	return result;
}




std::list<Waypoint *> LayerTRWWaypoints::children_list(const Waypoint * exclude) const
{
	std::list<Waypoint *> result;
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		/* Skip given waypoint. */
		if (nullptr != exclude && TreeItem::the_same_object(tree_item, exclude)) {
			continue;
		}

		result.push_back((Waypoint *) tree_item);
	}
	result.sort(TreeItem::compare_name_ascending);

	return result;
}




std::list<Waypoint *> LayerTRWWaypoints::children_list_sorted_by_name(const Waypoint * exclude) const
{
	std::list<Waypoint *> result = this->children_list(exclude);
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

	if (this->child_rows_count() <= 1) {
		return nullptr;
	}

	std::list<Waypoint *> waypoints = this->children_list_sorted_by_name();

	for (auto iter = std::next(waypoints.begin()); iter != waypoints.end(); iter++) {
		QString const this_one = (*iter)->get_name();
		QString const previous = (*(std::prev(iter)))->get_name();

		if (this_one == previous) {
			return *iter;
		}
	}

	return NULL;
}




void LayerTRWWaypoints::search_closest_wp(WaypointSearch & search)
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		Waypoint * wp = (Waypoint *) tree_item;
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

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {

		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		Waypoint * wp = (Waypoint *) tree_item;
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

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {

		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		const Waypoint * wp = (Waypoint *) tree_item;
		if (!wp->image_full_path.isEmpty() && !Thumbnails::thumbnail_exists(wp->image_full_path)) {
			paths.push_back(wp->image_full_path);
		}
	}

	return paths;
}




void LayerTRWWaypoints::change_coord_mode(CoordMode new_mode)
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {

		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		((Waypoint *) tree_item)->convert(new_mode);
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

	TreeItem * existing_wp = nullptr;
	do {
		existing_wp = this->find_waypoint_by_name(new_name);
		/* If found a name already in use try adding 1 to it and we try again. */
		if (existing_wp) {
			new_name = QString("%1#%2").arg(existing_name).arg(i);
			i++;
		}
	} while (existing_wp != nullptr);

	return new_name;
}




/**
   (Re)Calculate the bounds of the waypoints in this layer.
   This should be called whenever waypoints are changed (added/removed/moved).
*/
void LayerTRWWaypoints::recalculate_bbox(void)
{
	this->bbox.invalidate();

	if (this->child_rows_count() <= 0) {
		/* E.g. after all waypoints have been removed from TRW layer. */
		return;
	}


	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {

		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		const LatLon lat_lon = ((Waypoint *) tree_item)->get_coord().get_lat_lon();
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

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {

		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		if (!tree_item->get_timestamp().is_valid()) {
			continue;
		}

		/* When timestamp not set yet - use the first value encountered. */
		if (!result.is_valid()) {
			result = tree_item->get_timestamp();
		} else if (result > tree_item->get_timestamp()) {
			result = tree_item->get_timestamp();
		} else {
			; /* NOOP */
		}
	}

	return result;
}




sg_ret LayerTRWWaypoints::post_read_2(void)
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {

		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		if (tree_item->is_in_tree()) {
			continue;
		}

		qDebug() << SG_PREFIX_I << "Attaching item" << tree_item->get_name() << "to tree under" << this->get_name();
		this->attach_child_to_tree(tree_item);
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
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-remove"), tr("&Hide All Waypoints"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_off_cb()));

		qa = vis_submenu->addAction(tr("&Toggle Visibility of All Waypoints"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_toggle_cb()));
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
sg_ret LayerTRWWaypoints::menu_add_type_specific_operations(QMenu & menu, __attribute__((unused)) bool in_tree_view)
{
	QAction * qa = NULL;
	LayerTRW * parent_trw = this->owner_trw_layer();


	if (ThisApp::layers_panel()) {
		qa = menu.addAction(QIcon::fromTheme("document-new"), tr("&New Waypoint..."));
		connect(qa, SIGNAL (triggered(bool)), parent_trw, SLOT (new_waypoint_cb()));
	}


	this->sublayer_menu_waypoints_misc(parent_trw, menu);


	this->sublayer_menu_sort(menu);


	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));
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




/**
   \brief Re-adjust main viewport to show all items in this node
*/
void LayerTRWWaypoints::move_viewport_to_show_all_cb(void) /* Slot. */
{
	GisViewport * gisview = ThisApp::main_gisview();
	const int n_items = this->child_rows_count();

	if (1 == n_items) {
		/* Only 1 waypoint - jump straight to it. Notice that we don't care about waypoint's visibility.  */
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok == this->child_from_row(0, &tree_item)) {
			gisview->set_center_coord(((Waypoint *) tree_item)->get_coord());
		} else {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item";
		}

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




void LayerTRWWaypoints::children_visibility_on_cb(void) /* Slot. */
{
	const int changed = this->set_direct_children_only_visibility_flag(true);
	if (changed) {
		/* Redraw. */
		this->emit_tree_item_changed("Requesting redrawing of TRW waypoints after visibility was turned on");
	}
}




void LayerTRWWaypoints::children_visibility_off_cb(void) /* Slot. */
{
	const int changed = this->set_direct_children_only_visibility_flag(false);
	if (changed) {
		/* Redraw. */
		this->emit_tree_item_changed("Requesting redrawing of TRW waypoints after visibility was turned off");
	}
}




void LayerTRWWaypoints::children_visibility_toggle_cb(void) /* Slot. */
{
	const int changed = this->toggle_direct_children_only_visibility_flag();
	if (changed) {
		/* Redraw. */
		this->emit_tree_item_changed("Requesting redrawing of TRW waypoints after visibility was toggled");
	}
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
	LayersPanel * panel = ThisApp::layers_panel();
	if (!panel->has_any_layer_of_kind(LayerKind::DEM)) {
		Dialog::error(tr("No DEM layers available, thus no DEM values can be applied."), ThisApp::main_window());
		return;
	}

	int changed_ = 0;
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {

		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		changed_ = changed_ + (int) ((Waypoint *) tree_item)->apply_dem_data(skip_existing_elevations);
	}

	this->owner_trw_layer()->wp_changed_message(changed_);
}




bool LayerTRWWaypoints::handle_selection_in_tree(void)
{
	LayerTRW * parent_trw = this->owner_trw_layer();

	//parent_trw->set_statusbar_msg_info_trk(this);
	parent_trw->reset_internal_selections(); /* No other tree item (that is a sublayer of this layer) is selected... */

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
		qDebug() << SG_PREFIX_I << "Not drawing Waypoints - no waypoints";
		return;
	}

	if (!this->is_in_tree()) {
		/* This subnode hasn't been added to tree yet. */
		qDebug() << SG_PREFIX_I << "Not drawing Waypoints - node not in tree";
		return;
	}

	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this)) {
		qDebug() << SG_PREFIX_I << "Not drawing Waypoints - not visible";
		return;
	}

	SelectedTreeItems::print_draw_mode(*this, parent_is_selected);

	const bool item_is_selected = parent_is_selected || g_selected.is_in_set(this);
	const LatLonBBox viewport_bbox = gisview->get_bbox();

	if (!this->bbox.intersects_with(viewport_bbox)) {
		qDebug() << SG_PREFIX_I << "Not drawing Waypoints - not intersecting with viewport";
		return;
	}

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {

		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		qDebug() << SG_PREFIX_I << "Will now draw tree item" << tree_item->m_type_id << tree_item->get_name();
		tree_item->draw_tree_item(gisview, highlight_selected, item_is_selected);
	}
}




sg_ret LayerTRWWaypoints::paste_child_tree_item_cb(void)
{
	/* Slightly cheating method, routing via the panels capability. */
	bool dummy = false;
	return Clipboard::paste(ThisApp::layers_panel(), dummy);
}





void LayerTRWWaypoints::sort_order_a2z_cb(void)
{
	this->owner_trw_layer()->wp_sort_order = TreeViewSortOrder::AlphabeticalAscending;
	this->tree_view->sort_children(this, TreeViewSortOrder::AlphabeticalAscending);
}




void LayerTRWWaypoints::sort_order_z2a_cb(void)
{
	this->owner_trw_layer()->wp_sort_order = TreeViewSortOrder::AlphabeticalDescending;
	this->tree_view->sort_children(this, TreeViewSortOrder::AlphabeticalDescending);
}




void LayerTRWWaypoints::sort_order_timestamp_ascend_cb(void)
{
	this->owner_trw_layer()->wp_sort_order = TreeViewSortOrder::DateAscending;
	this->tree_view->sort_children(this, TreeViewSortOrder::DateAscending);
}




void LayerTRWWaypoints::sort_order_timestamp_descend_cb(void)
{
	this->owner_trw_layer()->wp_sort_order = TreeViewSortOrder::DateDescending;
	this->tree_view->sort_children(this, TreeViewSortOrder::DateDescending);
}




void LayerTRWWaypoints::clear(void)
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {

		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}

		this->tree_view->detach_tree_item(tree_item);
		delete tree_item;
	}
}




int LayerTRWWaypoints::size(void) const
{
	int rows = this->child_rows_count();
	if (rows < 0) {
		rows = 0;
	}
	return 0;
}




bool LayerTRWWaypoints::empty(void) const
{
	const int rows = this->child_rows_count();
	if (rows < 0) {
		qDebug() << SG_PREFIX_E << "Failed to find count of child items of" << this->get_name();
	}
	return rows <= 0;
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
		while (this->highest_item_number > 0 && !this->parent->find_child_by_name(name)) {
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




sg_ret LayerTRWWaypoints::accept_dropped_child(TreeItem * tree_item, int row, __attribute__((unused)) int col)
{
	/* Better to calculate 'previous_trw' at the beginning of the
	   function, before the parent will be changed as a result of
	   drop. */
	LayerTRW * previous_trw = ((Waypoint *) tree_item)->owner_trw_layer();
	LayerTRW * current_trw = this->owner_trw_layer();
	const bool the_same_trw = TreeItem::the_same_object(previous_trw, current_trw);

	/* Handle item in old location. */
	{
		tree_item->disconnect(); /* Disconnect all old signals. */

		/* Stuff specific to TRW layer. */
		{
			Waypoint * wp = (Waypoint *) tree_item;
			if (nullptr == wp) {
				qDebug() << SG_PREFIX_E << "NULL pointer to waypoint";
				return sg_ret::err;
			}
			TreeItem * parent_tree_item = wp->parent_tree_item();

			if (!the_same_trw) {
				if (wp == previous_trw->selected_wp_get()) {
					previous_trw->selected_wp_reset();
					previous_trw->moving_wp = false;
				}
#ifdef K_TODO_LATER
				this->name_generator.remove_name(wp->get_name());
#endif
				parent_tree_item->update_tree_item_tooltip(); /* Previous LayerTRWWaypoints. */
			}
		}
	}


	/* Handle item in new location. */
	{
		qDebug() << SG_PREFIX_I << "Attaching item" << tree_item->get_name() << "to tree under" << this->get_name();
		this->tree_view->attach_to_tree(this, tree_item, row);

		if (!the_same_trw) {
			this->recalculate_bbox();

			/* Update our own tooltip in tree view. */
			this->update_tree_item_tooltip();
		}
	}


	return sg_ret::ok;
}




/*
  @reviewed-on 2020-01-21
*/
sg_ret LayerTRWWaypoints::move_selection_to_next_child(void)
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {

		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}


		if (!g_selected.is_in_set(tree_item)) {
			/* This item is not selected, skip it. */
			continue;
		}

		if (row + 1 == rows) {
			/* Can't move selection to next item. */
			qDebug() << SG_PREFIX_E << "Can't move selection to next item - given item is already last";
			return sg_ret::err;

		} else {
			/* Deselect old and and select new item. */

			g_selected.remove_from_set(tree_item);

			TreeItem * next_tree_item = nullptr;
			if (sg_ret::ok == this->child_from_row(row + 1, &next_tree_item)) {
				Waypoint * next_waypoint = (Waypoint *) next_tree_item;
				g_selected.add_to_set(next_waypoint);
				this->owner_trw_layer()->selected_wp_set(next_waypoint);
				this->tree_view->select_and_expose_tree_item(next_waypoint);

				LayerToolTRWEditWaypoint * tool = (LayerToolTRWEditWaypoint *) ThisApp::main_window()->toolbox()->get_tool(LayerToolTRWEditWaypoint::tool_id());
				tool->point_properties_dialog->dialog_data_set(next_waypoint);
				return sg_ret::ok;
			} else {
				qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
				return sg_ret::err;
			}
		}
	}

	return sg_ret::err; /* No item was selected, so selection was not moved. */
}




/*
  @reviewed-on 2020-01-21
*/
sg_ret LayerTRWWaypoints::move_selection_to_previous_child(void)
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {

		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
			continue;
		}


		if (!g_selected.is_in_set(tree_item)) {
			/* This item is not selected, skip it. */
			continue;
		}

		if (row == 0) {
			/* Can't move selection to previous item. */
			qDebug() << SG_PREFIX_E << "Can't move selection to previous item - given item is already first";
			return sg_ret::err;

		} else {
			/* Deselect old and and select new item. */

			g_selected.remove_from_set(tree_item);

			TreeItem * prev_tree_item = nullptr;
			if (sg_ret::ok == this->child_from_row(row - 1, &prev_tree_item)) {

				Waypoint * prev_wp = (Waypoint *) prev_tree_item;

				g_selected.add_to_set(prev_tree_item);
				this->owner_trw_layer()->selected_wp_set(prev_wp);
				this->tree_view->select_and_expose_tree_item(prev_tree_item);

				LayerToolTRWEditWaypoint * tool = (LayerToolTRWEditWaypoint *) ThisApp::main_window()->toolbox()->get_tool(LayerToolTRWEditWaypoint::tool_id());
				tool->point_properties_dialog->dialog_data_set(prev_wp);
				return sg_ret::ok;
			} else {
				qDebug() << SG_PREFIX_E << "Failed to find valid tree item in row" << row << "/" << rows;
				return sg_ret::err;
			}
		}
	}

	return sg_ret::err; /* No item was selected, so selection was not moved. */
}




/**
   Method created to avoid constant casting of LayerTRWWaypoints::owning_layer
   to LayerTRW* type.

   @reviewed-on 2020-01-20
*/
LayerTRW * LayerTRWWaypoints::owner_trw_layer(void) const
{
	return (LayerTRW *) this->owner_tree_item();

}




sg_ret LayerTRWWaypoints::add_child(Waypoint * child)
{
	if (this->is_in_tree()) {
		/* This container is attached to Qt Model, so it can
		   attach the new child to the Model too, directly
		   under itself. */
		qDebug() << SG_PREFIX_I << "Attaching item" << child->get_name() << "to tree under" << this->get_name();
		if (sg_ret::ok != this->attach_as_tree_item_child(child, -1)) {
			qDebug() << SG_PREFIX_E << "Failed to attach" << child->get_name() << "as tree item child of" << this->get_name();
			return sg_ret::err;
		}

		/* Update our own tooltip in tree view. */
		this->update_tree_item_tooltip();
		return sg_ret::ok;
	} else {
		/* This container is not attached to Qt Model yet,
		   most probably because the TRW layer is being read
		   from file and won't be attached to Qt Model until
		   whole file is read.

		   So the container has to put the child on list of
		   un-attached items, to be attached later, in
		   post_read() function. */
		qDebug() << SG_PREFIX_I << this->get_name() << "container is not attached to Model yet, adding" << child->get_name() << "to list of unattached children of" << this->get_name();
		this->unattached_children.push_back(child);
		return sg_ret::ok;
	}
}




sg_ret LayerTRWWaypoints::attach_as_tree_item_child(TreeItem * child, int row)
{
	if (sg_ret::ok != this->tree_view->attach_to_tree(this, child, row)) {
		return sg_ret::err;
	}

	LayerTRW * trw = this->owner_trw_layer();

	QObject::connect(child, SIGNAL (tree_item_changed(const QString &)), trw, SLOT (child_tree_item_changed_cb(const QString &)));
	return sg_ret::ok;
}
