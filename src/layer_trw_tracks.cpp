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
 * Copyright (c) 2016 Kamil Ignacak <acerion@wp.pl>
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




void LayerTRWTracks::init_item(void)
{
	this->editable = false;
	this->menu_operation_ids = MenuOperationPaste;
}




LayerTRWTracks::LayerTRWTracks(bool is_routes)
{
	this->init_item();

	if (is_routes) {
		this->type_id = "sg.trw.routes";
		this->accepted_child_type_ids << "sg.trw.route";
		this->name = tr("Routes");
	} else {
		this->type_id = "sg.trw.tracks";
		this->accepted_child_type_ids << "sg.trw.track";
		this->name = tr("Tracks");
	}
}




LayerTRWTracks::LayerTRWTracks(bool is_routes, TreeView * ref_tree_view) : LayerTRWTracks(is_routes)
{
	this->tree_view = ref_tree_view;
}




LayerTRWTracks::~LayerTRWTracks()
{
	this->clear();
}




QString LayerTRWTracks::get_tooltip(void) const
{
	QString tooltip;

	if (this->type_id == "sg.trw.routes") {
		/* Very simple tooltip - may expand detail in the future. */
		tooltip = tr("Routes: %1").arg(this->children_list.size());
	} else {
		/* Very simple tooltip - may expand detail in the future. */
		tooltip = tr("Tracks: %1").arg(this->children_list.size());
	}

	return tooltip;
}




std::list<TreeItem *> LayerTRWTracks::get_tracks_by_date(const QDate & search_date) const
{
	const QString search_date_string = search_date.toString("yyyy-MM-dd");
	qDebug() << SG_PREFIX_I << "Search date =" << search_date << search_date_string;

	std::list<TreeItem *> result;

	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		Track * trk = *iter;

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




/*
 * ATM use a case sensitive find.
 * Finds the first one.
 */
Track * LayerTRWTracks::find_track_by_name(const QString & trk_name)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		Track * trk = *iter;
		if (trk && !trk->name.isEmpty()) {
			if (trk->name == trk_name) {
				return trk;
			}
		}
	}
	return NULL;
}




Track * LayerTRWTracks::find_child_by_uid(sg_uid_t child_uid) const
{
	auto iter = this->children_map.find(child_uid);
	if (iter == this->children_map.end()) {
		qDebug() << SG_PREFIX_W << "Can't find track with specified UID" << child_uid;
		return NULL;
	} else {
		return iter->second;
	}
}




void LayerTRWTracks::recalculate_bbox(void)
{
	this->bbox.invalidate();

	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		(*iter)->recalculate_bbox();
		const LatLonBBox trk_bbox = (*iter)->get_bbox();
		this->bbox.expand_with_bbox(trk_bbox);
	}

	this->bbox.validate();
}




void LayerTRWTracks::list_trk_uids(std::list<sg_uid_t> & list)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		list.push_back((*iter)->get_uid());
	}
}




std::list<Track *> LayerTRWTracks::find_tracks_with_timestamp_type(bool with_timestamps, Track * exclude)
{
	std::list<Track *> result;

	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		Track * trk = *iter;

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
std::list<Track *> LayerTRWTracks::find_nearby_tracks_by_time(Track * main_trk, unsigned int threshold)
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


	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		Track * trk = *iter;

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

		if (!(std::labs(diff1.get_value()) < threshold          /* ts_begin ts_end                 main_ts_begin main_ts_end */
		      || std::labs(diff2.get_value()) < threshold)) {   /* main_ts_begin main_ts_end       ts_begin ts_end */
			continue;
		}

		result.push_front(*iter);
	}

	return result; /* I hope that Return Value Optimization works. */
}




std::list<Track *> LayerTRWTracks::get_sorted_by_name(const Track * exclude) const
{
	std::list<Track *> result;
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		/* Skip given track. */
		if (exclude && *iter == exclude) {
			continue;
		}
		result.push_back(*iter);
	}

	/* Sort list of names alphabetically. */
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

	if (this->children_list.size() <= 1) {
		return NULL;
	}

	const std::list<Track *> tracks = this->get_sorted_by_name();

	bool found = false;
	for (auto iter = std::next(tracks.begin()); iter != tracks.end(); iter++) {
		const QString this_one = (*iter)->name;
		const QString previous = (*(std::prev(iter)))->name;

		if (this_one == previous) {
			return *iter;
		}
	}

	return NULL;
}




void LayerTRWTracks::set_items_visibility(bool on_off)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		(*iter)->set_visible(on_off);
		this->tree_view->apply_tree_item_visibility(*iter);
	}
}




void LayerTRWTracks::toggle_items_visibility(void)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		(*iter)->toggle_visible();
		this->tree_view->apply_tree_item_visibility(*iter);
	}
}




void LayerTRWTracks::get_tracks_list(std::list<Track *> & list) const
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		list.push_back(*iter);
	}
}




void LayerTRWTracks::track_search_closest_tp(TrackpointSearch & search)
{
	for (auto track_iter = this->children_list.begin(); track_iter != this->children_list.end(); track_iter++) {

		Track * trk = *track_iter;

		if (!trk->is_visible()) {
			continue;
		}

		if (!BBOX_INTERSECT (trk->bbox, search.bbox)) {
			continue;
		}


		for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {

			ScreenPos tp_pos;
			search.gisview->coord_to_screen_pos((*iter)->coord, tp_pos);

			const int dist_x = std::fabs(tp_pos.x() - search.x);
			const int dist_y = std::fabs(tp_pos.y() - search.y);

			if (dist_x <= TRACKPOINT_SIZE_APPROX && dist_y <= TRACKPOINT_SIZE_APPROX
			    && ((!search.closest_tp)
				/* Was the old trackpoint we already found closer than this one? */
				|| dist_x + dist_y < std::fabs(tp_pos.x() - search.closest_pos.x()) + std::fabs(tp_pos.y() - search.closest_pos.y()))) {

				search.closest_track = *track_iter;
				search.closest_tp = *iter;
				search.closest_tp_iter = iter;
				search.closest_pos = tp_pos;
			}

		}
	}
}




void LayerTRWTracks::change_coord_mode(CoordMode dest_mode)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		(*iter)->convert(dest_mode);
	}
}




/**
   \brief Force unqiue track names for Tracks/Routes sublayer
*/
void LayerTRWTracks::uniquify(TreeViewSortOrder sort_order)
{
	if (this->children_list.empty()) {
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
		const QString uniq_name = this->new_unique_element_name(trk->name);
		trk->set_name(uniq_name);

		/* TODO_LATER: do we really need to do this? Isn't the name in tree view auto-updated? */
		if (trk->index.isValid()) {
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

	Track * existing_trk = NULL;
	do {
		existing_trk = this->find_track_by_name(new_name);
		/* If found a name already in use try adding 1 to it and we try again. */
		if (existing_trk) {
			new_name = QString("%1#%2").arg(existing_name).arg(i);
			i++;
		}
	} while (existing_trk != NULL);

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
	if (this->type_id == "sg.trw.tracks") {

		int color_i = 0;
		for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {

			Track * trk = *iter;

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
		for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {

			Track * route = *iter;

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
	std::list<Track *> tracks_;
	this->get_tracks_list(tracks_);

	/* TODO: we assume here that all tracks have timestamps. */

	if (!tracks_.empty()) {
		tracks_.sort(Track::compare_timestamp);

		/* Only need to check the first track as they have been sorted by time. */
		Track * trk = *(tracks_.begin());
		/* Assume trackpoints already sorted by time. */
		Trackpoint * tpt = trk->get_tp_first();
		if (tpt && tpt->timestamp.is_valid()) {
			result = tpt->timestamp;
		}
	}
	return result;
}




sg_ret LayerTRWTracks::attach_children_to_tree(void)
{
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		Track * trk = *iter;

		if (trk->is_in_tree()) {
			continue;
		}

		trk->self_assign_icon();
		trk->self_assign_timestamp();

		qDebug() << SG_PREFIX_I << "Attaching to tree item" << trk->name << "under" << this->name;
		this->tree_view->attach_to_tree(this, trk);
	}

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
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (items_visibility_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-remove"), tr("&Hide All Tracks"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (items_visibility_off_cb()));

		qa = vis_submenu->addAction(tr("&Toggle Visibility of All Tracks"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (items_visibility_toggle_cb()));
	}

	qa = menu.addAction(tr("&Tracks List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_list_dialog_cb()));

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
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (items_visibility_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-delete"), tr("&Hide All Routes"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (items_visibility_off_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("view-refresh"), tr("&Toggle Visibility of All Routes"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (items_visibility_toggle_cb()));
	}

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&List Routes..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_list_dialog_cb()));


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




bool LayerTRWTracks::add_context_menu_items(QMenu & menu, bool tree_view_context_menu)
{
	QAction * qa = NULL;


	assert (this->menu_operation_ids == MenuOperationPaste);
	qa = menu.addAction(QIcon::fromTheme("edit-paste"), tr("Paste"));
	/* TODO_2_LATER: only enable if suitable item is in clipboard - want to determine *which* sublayer type. */
	qa->setEnabled(Clipboard::get_current_type() == ClipboardDataType::Sublayer);
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (paste_sublayer_cb()));


	menu.addSeparator();


	if (this->type_id == "sg.trw.tracks") {
		this->sublayer_menu_tracks_misc((LayerTRW *) this->owning_layer, menu);
	} else if (this->type_id == "sg.trw.routes") {
		this->sublayer_menu_routes_misc((LayerTRW *) this->owning_layer, menu);
	}


	this->sublayer_menu_sort(menu);


	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));
	layer_trw_sublayer_menu_all_add_external_tools((LayerTRW *) this->owning_layer, external_submenu);


	return true;
}





/**
   \brief Re-adjust main viewport to show all items in this node
*/
void LayerTRWTracks::move_viewport_to_show_all_cb(void) /* Slot. */
{
	const unsigned int n_items = this->children_list.size();
	this->recalculate_bbox();

	if (n_items > 0) {
		ThisApp::get_main_gis_view()->set_bbox(this->get_bbox());
		ThisApp::get_main_gis_view()->request_redraw("Re-align viewport to show all tracks or routes");
	}
}




void LayerTRWTracks::items_visibility_on_cb(void) /* Slot. */
{
	this->set_items_visibility(true);
	/* Redraw. */
	this->emit_tree_item_changed("TRW - Tracks - items visibility on");
}




void LayerTRWTracks::items_visibility_off_cb(void) /* Slot. */
{
	this->set_items_visibility(false);
	/* Redraw. */
	this->emit_tree_item_changed("TRW - Tracks - items visibility off");
}




void LayerTRWTracks::items_visibility_toggle_cb(void) /* Slot. */
{
	this->toggle_items_visibility();
	/* Redraw. */
	this->emit_tree_item_changed("TRW - Tracks - items visibility toggle");
}




void LayerTRWTracks::track_list_dialog_cb(void) /* Slot. */
{
	QString title;
	if (this->type_id == "sg.trw.tracks") {
		title = tr("%1: Track List").arg(this->owning_layer->name);
	} else {
		title = tr("%1: Route List").arg(this->owning_layer->name);
	}
	Track::list_dialog(title, this->owning_layer, this->type_id);
}




bool LayerTRWTracks::handle_selection_in_tree(void)
{
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	//parent_layer->set_statusbar_msg_info_trk(this);
	parent_layer->reset_internal_selections(); /* No other tree item (that is a sublayer of this layer) is selected... */

	qDebug() << SG_PREFIX_I << "Tree item" << this->name << "becomes selected tree item";
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

	if (this->children_list.empty()) {
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

#ifdef K_TODO_MAYBE
	if (BBOX_INTERSECT (this->bbox, viewport->get_bbox())) {
#endif
		for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
			(*iter)->draw_tree_item(gisview, highlight_selected, item_is_selected);
		}
#ifdef K_TODO_MAYBE
	}
#endif
}




void LayerTRWTracks::paste_sublayer_cb(void)
{
	/* Slightly cheating method, routing via the panels capability. */
	Clipboard::paste(ThisApp::get_layers_panel());
}




void LayerTRWTracks::sort_order_a2z_cb(void)
{
	this->blockSignals(true);
	this->tree_view->blockSignals(true);

	this->tree_view->detach_children(this);
	this->children_list.sort(TreeItem::compare_name_ascending);
	this->attach_children_to_tree();

	this->blockSignals(false);
	this->tree_view->blockSignals(false);
}




void LayerTRWTracks::sort_order_z2a_cb(void)
{
	((LayerTRW *) this->owning_layer)->track_sort_order = TreeViewSortOrder::AlphabeticalDescending;
	this->tree_view->sort_children(this, TreeViewSortOrder::AlphabeticalDescending);
}




void LayerTRWTracks::sort_order_timestamp_ascend_cb(void)
{
	((LayerTRW *) this->owning_layer)->track_sort_order = TreeViewSortOrder::DateAscending;
	this->tree_view->sort_children(this, TreeViewSortOrder::DateAscending);
}




void LayerTRWTracks::sort_order_timestamp_descend_cb(void)
{
	((LayerTRW *) this->owning_layer)->track_sort_order = TreeViewSortOrder::DateDescending;
	this->tree_view->sort_children(this, TreeViewSortOrder::DateDescending);
}




TrackpointSearch::TrackpointSearch(int ev_x, int ev_y, GisViewport * new_gisview)
{
	this->x = ev_x;
	this->y = ev_y;
	this->gisview = new_gisview;
	this->bbox = this->gisview->get_bbox();
}




void LayerTRWTracks::clear(void)
{
	this->children_map.clear();

	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		delete *iter;
	}

	this->children_list.clear();
}




size_t LayerTRWTracks::size(void) const
{
	return this->children_list.size();
}




bool LayerTRWTracks::empty(void) const
{
	return this->children_list.empty();
}




sg_ret LayerTRWTracks::attach_to_container(Track * trk)
{
	trk->set_owning_layer(this->get_owning_layer());
	this->children_map.insert({{ trk->get_uid(), trk }});
	this->children_list.push_back(trk);

	return sg_ret::ok;
}




sg_ret LayerTRWTracks::detach_from_container(Track * trk, bool * was_visible)
{
	if (!trk) {
		qDebug() << SG_PREFIX_E << "NULL pointer to track";
		return sg_ret::err;
	}

	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	if (trk->name.isEmpty()) {
		qDebug() << SG_PREFIX_W << "Track with empty name, deleting anyway";
	}

	if (trk->is_selected()) {
		parent_layer->reset_edited_track();
		parent_layer->moving_tp = false;
		parent_layer->route_finder_started = false;
	}

	if (NULL != was_visible) {
		*was_visible = trk->is_visible();
	}

	if (trk == parent_layer->route_finder_added_track) {
		parent_layer->route_finder_added_track = NULL;
	}

	parent_layer->deselect_current_trackpoint(trk);

	this->children_map.erase(trk->get_uid()); /* Erase by key. */


	TreeItemIdentityPredicate pred(trk);
	auto iter = std::find_if(this->children_list.begin(), this->children_list.end(), pred);
	if (iter != this->children_list.end()) {
		qDebug() << SG_PREFIX_I << "Will remove" << (*iter)->name << "from list" << this->name;
		this->children_list.erase(iter);
	}

#if 0   /* Old code. */
	for (auto iter = this->children_list.begin(); iter != this->children_list.end(); iter++) {
		qDebug() << SG_PREFIX_I << "Will compare tracks" << (*iter)->name << "and" << trk->name;
		if (TreeItem::the_same_object(*iter, trk)) {
			this->children_list.erase(iter);
			break;
		}
	}
#endif

	return sg_ret::ok;
}




sg_ret LayerTRWTracks::drag_drop_request(TreeItem * tree_item, int row, int col)
{
	/* Handle item in old location. */
	{
		LayerTRW * trw = (LayerTRW *) tree_item->get_owning_layer();
		trw->detach_from_container((Track *) tree_item);
		/* Detaching of tree item from tree view will be handled by QT. */
	}

	/* Handle item in new location. */
	{
		this->attach_to_container((Track *) tree_item);
		qDebug() << SG_PREFIX_I << "Attaching to tree item" << tree_item->name << "under" << this->name;
		this->tree_view->attach_to_tree(this, tree_item);
	}

	return sg_ret::ok;
}




sg_ret LayerTRWTracks::dropped_item_is_acceptable(TreeItem * tree_item, bool * result) const
{
	if (NULL == result) {
		return sg_ret::err;
	}

	if (TreeItemType::Sublayer != tree_item->get_tree_item_type()) {
		*result = false;
		return sg_ret::ok;
	}

	if (this->accepted_child_type_ids.contains(tree_item->type_id)) {
		qDebug() << SG_PREFIX_I << "Accepted child type ids =" << this->accepted_child_type_ids << ", dropped item type id =" << tree_item->type_id;
		*result = true;
		return sg_ret::ok;
	}

	*result = false;
	return sg_ret::ok;
}




bool LayerTRWTracks::move_child(TreeItem & child_tree_item, bool up)
{
	if (child_tree_item.type_id != "sg.trw.track" && child_tree_item.type_id != "sg.trw.route") {
		qDebug() << SG_PREFIX_E << "Attempting to move non-track/route child" << child_tree_item.type_id;
		return false;
	}

	Track * trk = (Track *) &child_tree_item;

	qDebug() << SG_PREFIX_I << "Will now try to move child item of" << this->name << (up ? "up" : "down");
	const bool result = move_tree_item_child_algo(this->children_list, trk, up);
	qDebug() << SG_PREFIX_I << "Result of attempt to move child item" << (up ? "up" : "down") << ":" << (result ? "success" : "failure");

	/* In this function we only move children in container of tree items.
	   Movement in tree widget is handled elsewhere. */

	return result;

}
