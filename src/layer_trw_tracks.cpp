/*
 * SlavGPS -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Project started in 2016 by forking viking project.
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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




#include <unordered_map>
#include <cstring>
#include <cassert>




#include <QDebug>




#include "layer_trw.h"
#include "layer_trw_painter.h"
#include "layer_trw_menu.h"
#include "layer_trw_track_internal.h"
#include "viewport_internal.h"
#include "tree_view_internal.h"
#include "clipboard.h"
#include "statusbar.h"
#include "window.h"
#include "layers_panel.h"




using namespace SlavGPS;




#define SG_MODULE "Layer TRW Tracks"




extern SelectedTreeItems g_selected;




/* This is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5

#define LAYER_TRW_TRACK_COLORS_MAX 10




LayerTRWTracks::LayerTRWTracks(bool is_routes)
{
	this->editable = false;
	this->m_menu_operation_ids.push_back(StandardMenuOperation::Paste);

	if (is_routes) {
		this->m_type_id = LayerTRWRoutes::type_id();
		this->accepted_child_type_ids.push_back(Route::type_id());
		this->set_name(tr("Routes"));
	} else {
		this->m_type_id = LayerTRWTracks::type_id();
		this->accepted_child_type_ids.push_back(Track::type_id());
		this->set_name(tr("Tracks"));
	}
}




LayerTRWTracks::LayerTRWTracks(bool is_routes, TreeView * ref_tree_view) : LayerTRWTracks(is_routes)
{
	this->tree_view = ref_tree_view;
}




LayerTRWTracks::~LayerTRWTracks()
{
	qDebug() << SG_PREFIX_I << "Destructor of" << this->get_name() << "called";
	this->clear();
}




SGObjectTypeID LayerTRWTracks::get_type_id(void) const
{
	return this->m_type_id;
}
SGObjectTypeID LayerTRWTracks::type_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.trw.tracks");
	return id;
}
#if 0
SGObjectTypeID LayerTRWRoutes::get_type_id(void) const
{
	return this->m_type_id;
}
#endif
SGObjectTypeID LayerTRWRoutes::type_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.trw.routes");
	return id;
}




QString LayerTRWTracks::get_tooltip(void) const
{
	QString tooltip;

	int rows = this->child_rows_count();
	if (rows < 0) {
		rows = 0;
	}

	if (this->get_type_id() == LayerTRWRoutes::type_id()) {
		/* Very simple tooltip - may expand detail in the future. */
		tooltip = tr("Routes: %1").arg(rows);
	} else {
		/* Very simple tooltip - may expand detail in the future. */
		tooltip = tr("Tracks: %1").arg(rows);
	}

	return tooltip;
}




std::list<TreeItem *> LayerTRWTracks::find_children_by_date(const QDate & search_date) const
{
	const QString search_date_string = search_date.toString("yyyy-MM-dd");
	qDebug() << SG_PREFIX_I << "Search date =" << search_date << search_date_string;

	std::list<TreeItem *> result;

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}
		Track * trk = (Track *) tree_item;

		if (trk->empty()) {
			continue;
		}
		if (!(*trk->trackpoints.begin())->timestamp.is_valid()) {
			continue;
		}

		/* Might be an easier way to compare dates rather than converting the strings all the time... */
		const QString trk_date_string = (*trk->trackpoints.begin())->timestamp.strftime_utc("%Y-%m-%d");
		if (search_date_string == trk_date_string) {
			result.push_back(trk);
		}
	}
	return result;
}




void LayerTRWTracks::recalculate_bbox(void)
{
	this->bbox.invalidate();

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}
		Track * trk = (Track *) tree_item;

		trk->recalculate_bbox();
		const LatLonBBox trk_bbox = trk->get_bbox();
		this->bbox.expand_with_bbox(trk_bbox);
	}

	this->bbox.validate();
}




std::list<Track *> LayerTRWTracks::find_tracks_with_timestamp_type(bool with_timestamps, Track * exclude)
{
	std::list<Track *> result;

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}
		Track * trk = (Track *) tree_item;

		if (trk == exclude) {
			continue;
		}

		if (!trk->empty()) {
			Trackpoint * p1 = trk->get_tp_first();
			Trackpoint * p2 = trk->get_tp_last();

			if (with_timestamps) {
				if (!p1->timestamp.is_valid() || !p2->timestamp.is_valid()) {
					continue;
				}
			} else {
				/* Don't add tracks with timestamps when getting non timestamp tracks. */
				if (p1->timestamp.is_valid() || p2->timestamp.is_valid()) {
					continue;
				}
			}
		}

		result.push_front(trk);
	}

	return result; /* I hope that Return Value Optimization works. */
}




/**
 * Called for each track in tracks container.
 * If the main track is close enough (threshold)
 * to given track, then the given track is added to returned list.
 */
std::list<Track *> LayerTRWTracks::find_nearby_tracks_by_time(Track * main_trk, const Duration & threshold)
{
	std::list<Track *> result;

	if (!main_trk || main_trk->empty()) {
		return result;
	}

	Time main_ts_begin;
	Time main_ts_end;
	if (sg_ret::ok  != main_trk->get_timestamps(main_ts_begin, main_ts_end)) {
		qDebug() << SG_PREFIX_W << "Main track has no timestamps, not searching from nearby tracks";
		return result;
	}


	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}
		Track * trk = (Track *) tree_item;

		/* Skip self. */
		if (TreeItem::the_same_object(trk, main_trk)) {
			continue;
		}

		Time ts_begin;
		Time ts_end;
		if (sg_ret::ok != trk->get_timestamps(ts_begin, ts_end)) {
			continue;
		}


		/* FIXME: this code is apparently fine with negative
		   values of time diffs, but Time class doesn't handle
		   them well make sure that negative values of Time
		   are considered as valid. */

		const Time diff1 = main_ts_begin - ts_end;
		const Time diff2 = ts_begin - main_ts_end;

		if (!(std::labs(diff1.ll_value()) < threshold.ll_value()          /* ts_begin ts_end                 main_ts_begin main_ts_end */
		      || std::labs(diff2.ll_value()) < threshold.ll_value())) {   /* main_ts_begin main_ts_end       ts_begin ts_end */
			continue;
		}

		result.push_front(trk);
	}

	return result; /* I hope that Return Value Optimization works. */
}




std::list<Track *> LayerTRWTracks::children_list(const Track * exclude) const
{
	std::list<Track *> result;
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}
		Track * trk = (Track *) tree_item;

		/* Skip given track. */
		if (nullptr != exclude && TreeItem::the_same_object(trk, exclude)) {
			continue;
		}
		result.push_back(trk);
	}

	return result;
}




std::list<Track *> LayerTRWTracks::children_list_sorted_by_name(const Track * exclude) const
{
	std::list<Track *> result = this->children_list(exclude);
	result.sort(TreeItem::compare_name_ascending);
	return result;
}




/**
   @brief Find out if any tracks have the same name in this sublayer

   @return a track, which name is duplicated (i.e. some other track has the same name)
*/
Track * LayerTRWTracks::find_track_with_duplicate_name(void) const
{
	/* Build list of names. Sort list alphabetically. Find any two adjacent duplicates on the list. */

	const int rows = this->child_rows_count();
	if (rows <= 1) {
		return nullptr;
	}

	const std::list<Track *> tracks = this->children_list_sorted_by_name();

	for (auto iter = std::next(tracks.begin()); iter != tracks.end(); iter++) {
		const QString this_one = (*iter)->get_name();
		const QString previous = (*(std::prev(iter)))->get_name();

		if (this_one == previous) {
			return *iter;
		}
	}

	return NULL;
}




void LayerTRWTracks::track_search_closest_tp(TrackpointSearch & search) const
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}
		Track * trk = (Track *) tree_item;

		if (!trk->is_visible()) {
			continue;
		}

		if (!trk->bbox.intersects_with(search.bbox)) {
			continue;
		}


		for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {

			ScreenPos tp_pos;
			search.m_gisview->coord_to_screen_pos((*iter)->coord, tp_pos);

			const int dist_x = std::fabs(tp_pos.x() - search.m_event_pos.x());
			const int dist_y = std::fabs(tp_pos.y() - search.m_event_pos.y());

			if (NULL != search.skip_tp && search.skip_tp == *iter) {
				continue;
			}

			if (dist_x <= TRACKPOINT_SIZE_APPROX && dist_y <= TRACKPOINT_SIZE_APPROX
			    && ((!search.closest_tp)
				/* Was the old trackpoint we already found closer than this one? */
				|| dist_x + dist_y < std::fabs(tp_pos.x() - search.closest_pos.x()) + std::fabs(tp_pos.y() - search.closest_pos.y()))) {

				search.closest_track = trk;
				search.closest_tp = *iter;
				search.closest_tp_iter = iter;
				search.closest_pos = tp_pos;
			}
		}
	}
}




void LayerTRWTracks::change_coord_mode(CoordMode dest_mode)
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}
		Track * trk = (Track *) tree_item;

		trk->change_coord_mode(dest_mode);
	}
}




/**
   \brief Force unqiue track names for Tracks/Routes sublayer
*/
void LayerTRWTracks::uniquify(TreeViewSortOrder sort_order)
{
	if (this->attached_empty()) {
		qDebug() << SG_PREFIX_E << "Called for empty tracks/routes set";
		return;
	}

	/*
	  - Search tracks set for an instance of duplicate name
	  - get track with duplicate name
	  - create new name
	  - rename track & update equiv. tree view iter
	  - repeat until there are no tracks with duplicate names
	*/

	Track * trk = this->find_track_with_duplicate_name();
	while (trk) {
		/* Rename it. */
		const QString uniq_name = this->new_unique_element_name(trk->get_name());
		trk->set_name(uniq_name);

		/* TODO_LATER: do we really need to do this? Isn't the name in tree view auto-updated? */
		if (trk->index().isValid()) {
			this->tree_view->apply_tree_item_name(trk);
			this->tree_view->sort_children(this, sort_order);
		}

		/* Try to find duplicate names again in the updated set of tracks. */
		trk = this->find_track_with_duplicate_name();
	}
}




/**
   \brief Get a a unique new name for element of type \param item_type_id
*/
QString LayerTRWTracks::new_unique_element_name(const QString & existing_name)
{
	int i = 2;
	QString new_name = existing_name;

	TreeItem * existing_trk = nullptr;
	do {
		existing_trk = this->find_child_by_name(new_name);
		/* If found a name already in use try adding 1 to it and we try again. */
		if (existing_trk) {
			new_name = QString("%1#%2").arg(existing_name).arg(i);
			i++;
		}
	} while (existing_trk != nullptr);

	return new_name;
}




static QString my_track_colors(int ii)
{
	static const QString colors[LAYER_TRW_TRACK_COLORS_MAX] = {
		"#2d870a",
		"#135D34",
		"#0a8783",
		"#0e4d87",
		"#05469f",
		"#695CBB",
		"#2d059f",
		"#4a059f",
		"#5A171A",
		"#96059f"
	};
	/* Fast and reliable way of returning a color. */
	return colors[(ii % LAYER_TRW_TRACK_COLORS_MAX)];
}




void LayerTRWTracks::assign_colors(LayerTRWTrackDrawingMode track_drawing_mode, const QColor & track_color_common)
{
	if (this->get_type_id() == LayerTRWTracks::type_id()) {

		int color_i = 0;
		const int rows = this->child_rows_count();
		for (int row = 0; row < rows; row++) {
			TreeItem * tree_item = nullptr;
			if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
				qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
				continue;
			}
			Track * trk = (Track *) tree_item;

			/* Tracks get a random spread of colors if not already assigned. */
			if (!trk->has_color) {
				if (track_drawing_mode == LayerTRWTrackDrawingMode::AllSameColor) {
					trk->color = track_color_common;
				} else {
					trk->color.setNamedColor(my_track_colors(color_i));
				}
				trk->has_color = true;
			}

			trk->update_tree_item_properties();

			color_i++;
			if (color_i > LAYER_TRW_TRACK_COLORS_MAX) {
				color_i = 0;
			}
		}

	} else { /* Routes. */

		bool use_dark = false;
		const int rows = this->child_rows_count();
		for (int row = 0; row < rows; row++) {

			TreeItem * tree_item = nullptr;
			if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
				qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
				continue;
			}
			Track * route = (Track *) tree_item;

			/* Routes get an intermix of reds. */
			if (!route->has_color) {
				if (use_dark) {
					route->color.setNamedColor("#B40916"); /* Dark Red. */
				} else {
					route->color.setNamedColor("#FF0000"); /* Red. */
				}
				route->has_color = true;
			}

			route->update_tree_item_properties();

			use_dark = !use_dark;
		}
	}
}




/**
 * Get the earliest timestamp available from all tracks.
 */
Time LayerTRWTracks::get_earliest_timestamp(void) const
{
	Time result;
	std::list<TreeItem *> tree_items;
	this->list_tree_items(tree_items);
	if (tree_items.empty()) {
		return result;
	}


	tree_items.sort(Track::compare_timestamp);


	/* Some tracks may have no timestamps. Find first track that
	   does have timestamps. Since the tracks are sorted by their
	   timestamps, first track with timestamps should have the
	   earliest timestamps. */
	for (auto iter = tree_items.begin(); iter != tree_items.end(); iter++) {

		const Track * trk = (const Track *) *tree_items.begin();

		/* Assume trackpoints in track are sorted by time. */
		const Trackpoint * tp = trk->get_tp_first();
		if (nullptr == tp) {
			continue;
		}
		if (!tp->timestamp.is_valid()) {
			continue;
		}

		result = tp->timestamp;
		break;
	}


	return result;
}




sg_ret LayerTRWTracks::attach_unattached_children(void)
{
	if (this->unattached_children.empty()) {
		return sg_ret::ok;
	}

	for (auto iter = this->unattached_children.begin(); iter != this->unattached_children.end(); iter++) {
		TreeItem * tree_item = *iter;
		((Track *) tree_item)->self_assign_icon();
		((Track *) tree_item)->self_assign_timestamp();

		qDebug() << SG_PREFIX_I << "Attaching item" << tree_item->get_name() << "to tree under" << this->get_name();
		this->attach_child_to_tree(tree_item);
	}

	this->unattached_children.clear();

	return sg_ret::ok;
}




void LayerTRWTracks::sublayer_menu_tracks_misc(LayerTRW * parent_layer_, QMenu & menu)
{
	QAction * qa = NULL;

	bool creation_in_progress = parent_layer_->get_track_creation_in_progress() || parent_layer_->get_route_creation_in_progress();

	if (parent_layer_->get_track_creation_in_progress()) {
		qa = menu.addAction(tr("&Finish Track"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (finish_track_cb()));

		menu.addSeparator();
	}

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&View All Tracks"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (move_viewport_to_show_all_cb()));

	qa = menu.addAction(QIcon::fromTheme("document-new"), tr("&New Track"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (new_track_cb()));
	/* Make it available only when a new track/route is *not* already in progress. */
	qa->setEnabled(!creation_in_progress);

	qa = menu.addAction(QIcon::fromTheme("list-remove"), tr("Delete &All Tracks"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (delete_all_tracks_cb()));

	qa = menu.addAction(tr("&Delete Tracks From Selection..."));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (delete_selected_tracks_cb()));

	{
		QMenu * vis_submenu = menu.addMenu(tr("&Visibility"));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), tr("&Show All Tracks"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-remove"), tr("&Hide All Tracks"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_off_cb()));

		qa = vis_submenu->addAction(tr("&Toggle Visibility of All Tracks"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_toggle_cb()));
	}

	qa = menu.addAction(tr("&Tracks List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_or_route_list_dialog_cb()));

	qa = menu.addAction(tr("&Statistics"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (tracks_stats_cb()));
}








void LayerTRWTracks::sublayer_menu_routes_misc(LayerTRW * parent_layer_, QMenu & menu)
{
	QAction * qa = NULL;

	bool creation_in_progress = parent_layer_->get_track_creation_in_progress() || parent_layer_->get_route_creation_in_progress();

	if (parent_layer_->get_route_creation_in_progress()) {
		qa = menu.addAction(tr("&Finish Route"));
		connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (finish_route_cb()));

		menu.addSeparator();
	}

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&View All Routes"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (move_viewport_to_show_all_cb()));

	qa = menu.addAction(QIcon::fromTheme("document-new"), tr("&New Route"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (new_route_cb()));
	/* Make it available only when a new track/route is *not* already in progress. */
	qa->setEnabled(!creation_in_progress);

	qa = menu.addAction(QIcon::fromTheme("list-delete"), tr("Delete &All Routes"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (delete_all_routes_cb()));

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Delete Routes From Selection..."));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (delete_selected_routes_cb()));

	{
		QMenu * vis_submenu = menu.addMenu(tr("&Visibility"));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), tr("&Show All Routes"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-delete"), tr("&Hide All Routes"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_off_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("view-refresh"), tr("&Toggle Visibility of All Routes"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (children_visibility_toggle_cb()));
	}

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Routes List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_or_route_list_dialog_cb()));


	qa = menu.addAction(tr("&Statistics"));
	connect(qa, SIGNAL (triggered(bool)), parent_layer_, SLOT (routes_stats_cb()));
}




void LayerTRWTracks::sublayer_menu_sort(QMenu & menu)
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




sg_ret LayerTRWTracks::menu_add_type_specific_operations(QMenu & menu, __attribute__((unused)) bool in_tree_view)
{
	if (this->get_type_id() == LayerTRWTracks::type_id()) {
		this->sublayer_menu_tracks_misc(this->owner_trw_layer(), menu);
	} else if (this->get_type_id() == LayerTRWRoutes::type_id()) {
		this->sublayer_menu_routes_misc(this->owner_trw_layer(), menu);
	}


	this->sublayer_menu_sort(menu);


	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));
	layer_trw_sublayer_menu_all_add_external_tools(this->owner_trw_layer(), external_submenu);


	return sg_ret::ok;
}





/**
   \brief Re-adjust main viewport to show all items in this node
*/
void LayerTRWTracks::move_viewport_to_show_all_cb(void) /* Slot. */
{
	this->recalculate_bbox();

	if (!this->attached_empty()) {
		ThisApp::main_gisview()->set_bbox(this->get_bbox());
		ThisApp::main_gisview()->request_redraw("Re-align viewport to show all tracks or routes");
	}
}




void LayerTRWTracks::children_visibility_on_cb(void) /* Slot. */
{
	const int changed = this->set_direct_children_only_visibility_flag(true);
	if (changed) {
		/* Redraw. */
		this->emit_tree_item_changed("Requesting redrawing of TRW tracks after visibility was turned on");
	}
}




void LayerTRWTracks::children_visibility_off_cb(void) /* Slot. */
{
	const int changed = this->set_direct_children_only_visibility_flag(false);
	if (changed) {
		/* Redraw. */
		this->emit_tree_item_changed("Requesting redrawing of TRW tracks after visibility was turned off");
	}
}




void LayerTRWTracks::children_visibility_toggle_cb(void) /* Slot. */
{
	const int changed = this->toggle_direct_children_only_visibility_flag();
	if (changed) {
		/* Redraw. */
		this->emit_tree_item_changed("Requesting redrawing of TRW tracks after visibility was toggled");
	}
}




/**
   @reviewed-on 2019-12-01
*/
void LayerTRWTracks::track_or_route_list_dialog_cb(void) /* Slot. */
{
	std::list<SGObjectTypeID> wanted_types;
	QString title;

	if (this->get_type_id() == LayerTRWTracks::type_id()) {
		/* Show each track in this tracks container. */
		wanted_types.push_back(Track::type_id());
		title = tr("%1: Tracks List").arg(this->owner_trw_layer()->get_name());
	} else {
		 /* Show each route in this routes container. */
		wanted_types.push_back(Route::type_id());
		title = tr("%1: Routes List").arg(this->owner_trw_layer()->get_name());
	}

	Track::list_dialog(title, this->owner_trw_layer(), wanted_types);
}




bool LayerTRWTracks::handle_selection_in_tree(void)
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
 * It assumes they belong to the TRW Layer (it doesn't check this is the case)
 */
void LayerTRWTracks::draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected)
{
	if (!this->is_in_tree()) {
		/* This subnode hasn't been added to tree yet. */
		return;
	}

	/* Check the layer for visibility (including all the parents visibilities). */
	if (!this->tree_view->get_tree_item_visibility_with_parents(this)) {
		return;
	}

	if (this->attached_empty()) {
		return;
	}

	SelectedTreeItems::print_draw_mode(*this, parent_is_selected);

	const bool item_is_selected = parent_is_selected || g_selected.is_in_set(this);

#ifdef TODO_MAYBE
	if (this->bbox.intersects_with(viewport->get_bbox())) {
#endif
		const int rows = this->child_rows_count();
		for (int row = 0; row < rows; row++) {
			TreeItem * tree_item = nullptr;
			if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
				qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
				continue;
			}

			tree_item->draw_tree_item(gisview, highlight_selected, item_is_selected);
		}
#ifdef TODO_MAYBE
	}
#endif
}




sg_ret LayerTRWTracks::paste_child_tree_item_cb(void)
{
	/* Slightly cheating method, routing via the panels capability. */
	bool dummy = false;
	return Clipboard::paste(ThisApp::layers_panel(), dummy);
}




void LayerTRWTracks::sort_order_a2z_cb(void)
{
	this->owner_trw_layer()->track_sort_order = TreeViewSortOrder::AlphabeticalDescending;
	this->tree_view->sort_children(this, TreeViewSortOrder::AlphabeticalAscending);
}




void LayerTRWTracks::sort_order_z2a_cb(void)
{
	this->owner_trw_layer()->track_sort_order = TreeViewSortOrder::AlphabeticalDescending;
	this->tree_view->sort_children(this, TreeViewSortOrder::AlphabeticalDescending);
}




void LayerTRWTracks::sort_order_timestamp_ascend_cb(void)
{
	this->owner_trw_layer()->track_sort_order = TreeViewSortOrder::DateAscending;
	this->tree_view->sort_children(this, TreeViewSortOrder::DateAscending);
}




void LayerTRWTracks::sort_order_timestamp_descend_cb(void)
{
	this->owner_trw_layer()->track_sort_order = TreeViewSortOrder::DateDescending;
	this->tree_view->sort_children(this, TreeViewSortOrder::DateDescending);
}




TrackpointSearch::TrackpointSearch(const ScreenPos & event_pos, GisViewport * gisview)
{
	this->m_event_pos = event_pos;
	this->m_gisview = gisview;
	this->bbox = this->m_gisview->get_bbox();
}




sg_ret LayerTRWTracks::accept_dropped_child(TreeItem * tree_item, __attribute__((unused)) int row)
{
	/* Better to calculate 'previous_trw' at the beginning of the
	   function, before the parent will be changed as a result of
	   drop. */
	LayerTRW * previous_trw = ((Track *) tree_item)->owner_trw_layer();
	LayerTRW * current_trw = this->owner_trw_layer();
	const bool the_same_trw = TreeItem::the_same_object(previous_trw, current_trw);

	/* Handle item in old location. */
	{
		tree_item->disconnect(); /* Disconnect all old signals. */

		/* Stuff specific to TRW Layer. */
		{
			Track * trk = (Track *) tree_item;
			if (nullptr == trk) {
				qDebug() << SG_PREFIX_E << "NULL pointer to track";
				return sg_ret::err;
			}

			if (trk->is_selected()) {
				previous_trw->selected_track_reset();
				previous_trw->moving_tp = false;
				previous_trw->route_finder_started = false;
			}

			if (trk == previous_trw->route_finder_added_track) {
				previous_trw->route_finder_added_track = NULL;
			}

			previous_trw->deselect_current_trackpoint(trk);

			if (!the_same_trw) {
#ifdef K_TODO_LATER
				this->name_generator.remove_name(trk->get_name());
#endif
			}
		}
	}


	/* Handle item in new location. */
	{
		qDebug() << SG_PREFIX_I << "Attaching item" << tree_item->get_name() << "to tree under" << this->get_name();
		this->tree_view->attach_to_tree(this, tree_item, row);
	}

	return sg_ret::ok;
}




/**
   @reviewed-on 2020-03-06
*/
LayerTRW * LayerTRWTracks::owner_trw_layer(void) const
{
	return (LayerTRW *) this->parent_layer();
}




/**
   @reviewed-on 2020-03-06
*/
Layer * LayerTRWTracks::parent_layer(void) const
{
	const TreeItem * parent = this->parent_member();
	if (nullptr == parent) {
		return nullptr;
	}

	Layer * layer = (Layer *) parent;
	assert (layer->m_kind == LayerKind::TRW);
	return layer;
}




Distance LayerTRWTracks::total_distance(void) const
{
	Distance result;

	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}
		const Track * trk = (const Track *) tree_item;

		result += trk->get_length();
	}

	return result;
}




void LayerTRWTracks::total_time_information(Duration & duration, Time & start_time, Time & end_time) const
{
	const int rows = this->child_rows_count();
	for (int row = 0; row < rows; row++) {
		TreeItem * tree_item = nullptr;
		if (sg_ret::ok != this->child_from_row(row, &tree_item)) {
			qDebug() << SG_PREFIX_E << "Failed to get child from row" << row << "/" << rows;
			continue;
		}
		const Track * trk = (const Track *) tree_item;

		Time ts_first;
		Time ts_last;
		if (sg_ret::ok == trk->get_timestamps(ts_first, ts_last)) {

			/* Update the earliest / the latest timestamps
			   (initialize if necessary). */
			if ((!start_time.is_valid()) || ts_first < start_time) {
				start_time = ts_first;
			}

			if ((!end_time.is_valid()) || ts_last > end_time) {
				end_time = ts_last;
			}

			/* Keep track of total time.
			   There maybe gaps within a track (eg segments)
			   but this should be generally good enough for a simple indicator. */
			duration += Duration::get_abs_duration(ts_last, ts_first);
		}
	}

	return;
}




sg_ret LayerTRWTracks::update_properties(void)
{
	this->recalculate_bbox();
	TreeItem::update_properties();

	qDebug() << SG_PREFIX_SIGNAL << "Emitting signal 'properties changed'";
	emit this->properties_changed(this->get_name() + " container"); /* Tell parent TRW layer that count of tracks or routes in the layer may have changed. */

	return sg_ret::ok;
}
