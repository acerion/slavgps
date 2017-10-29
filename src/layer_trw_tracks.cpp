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




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unordered_map>

#ifdef HAVE_STRING_H
#include <cstring>
#endif

#include <QDebug>

#include "layer_trw.h"
#include "layer_trw_painter.h"
#include "layer_trw_menu.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_track_list_dialog.h"
#include "viewport_internal.h"
#include "tree_view_internal.h"
//#include "thumbnails.h"




using namespace SlavGPS;




extern Tree * g_tree;




/* This is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5



LayerTRWTracks::LayerTRWTracks()
{
	this->tree_item_type = TreeItemType::SUBLAYER;

	/* Default values, may be changed by caller's code. */
	this->type_id = "sg.trw.routes";
	this->accepted_child_type_ids << "sg.trw.route";
	this->editable = false;
}




LayerTRWTracks::LayerTRWTracks(bool is_routes) : LayerTRWTracks()
{
	if (is_routes) {
		this->type_id = "sg.trw.routes";
		this->accepted_child_type_ids << "sg.trw.route";
	} else {
		this->type_id = "sg.trw.tracks";
		this->accepted_child_type_ids << "sg.trw.tracks";
	}
}




LayerTRWTracks::LayerTRWTracks(bool is_routes, TreeView * ref_tree_view) : LayerTRWTracks(is_routes)
{
	this->tree_view = ref_tree_view;
}




LayerTRWTracks::~LayerTRWTracks()
{
	/* kamilTODO: call destructors of Track objects? */
	this->items.clear();
}




QString LayerTRWTracks::get_tooltip(void)
{
	if (this->type_id == "sg.trw.routes") {
		/* Very simple tooltip - may expand detail in the future. */
		return QString("Routes: %1").arg(this->items.size());
	} else {
		/* Very simple tooltip - may expand detail in the future. */
		return QString("Tracks: %1").arg(this->items.size());
	}
}




Track * LayerTRWTracks::find_track_by_date(char const * date)
{
	char date_buf[20];
	Track * trk = NULL;

	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		date_buf[0] = '\0';
		trk = i->second;

		/* Might be an easier way to compare dates rather than converting the strings all the time... */
		if (!trk->empty()
		    && (*trk->trackpoints.begin())->has_timestamp) {

			strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", gmtime(&(*trk->trackpoints.begin())->timestamp));

			if (!g_strcmp0(date, date_buf)) {
				return trk;
			}
		}
	}
	return NULL;
}




/*
 * ATM use a case sensitive find.
 * Finds the first one.
 */
Track * LayerTRWTracks::find_track_by_name(const QString & trk_name)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		Track * trk = i->second;
		if (trk && !trk->name.isEmpty()) {
			if (trk->name == trk_name) {
				return trk;
			}
		}
	}
	return NULL;
}




void LayerTRWTracks::find_maxmin(struct LatLon maxmin[2])
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		i->second->find_maxmin(maxmin);
	}
}




void LayerTRWTracks::list_trk_uids(GList ** l)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		*l = g_list_append(*l, (void *) ((long) i->first)); /* kamilTODO: i->first or i->second? */
	}
}




std::list<sg_uid_t> * LayerTRWTracks::find_tracks_with_timestamp_type(bool with_timestamps, Track * exclude)
{
	std::list<sg_uid_t> * result = new std::list<sg_uid_t>;

	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		Trackpoint * p1, * p2;
		Track * trk = i->second;
		if (trk == exclude) {
			continue;
		}

		if (!trk->empty()) {
			p1 = trk->get_tp_first();
			p2 = trk->get_tp_last();

			if (with_timestamps) {
				if (!p1->has_timestamp || !p2->has_timestamp) {
					continue;
				}
			} else {
				/* Don't add tracks with timestamps when getting non timestamp tracks. */
				if (p1->has_timestamp || p2->has_timestamp) {
					continue;
				}
			}
		}

		result->push_front(i->first);
	}

	return result;
}




/**
 * Called for each track in track hash table.
 * If the original track orig_trk is close enough (threshold)
 * to given track, then the given track is added to returned list.
 */
GList * LayerTRWTracks::find_nearby_tracks_by_time(Track * orig_trk, unsigned int threshold)
{
	GList * nearby_tracks = NULL;

	if (!orig_trk || orig_trk->empty()) {
		return NULL;
	}

	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		Track * trk = i->second;

		/* Outline:
		 * Detect reasons for not merging, and return.
		 * If no reason is found not to merge, then do it.
		 */

		/* Exclude the original track from the compiled list. */
		if (trk == orig_trk) { /* kamilFIXME: originally it was "if (trk == udata->exclude)" */
			continue;
		}

		time_t t1 = orig_trk->get_tp_first()->timestamp;
		time_t t2 = orig_trk->get_tp_last()->timestamp;

		if (!trk->empty()) {

			Trackpoint * p1 = trk->get_tp_first();
			Trackpoint * p2 = trk->get_tp_last();

			if (!p1->has_timestamp || !p2->has_timestamp) {
				//fprintf(stdout, "no timestamp\n");
				continue;
			}

			//fprintf(stdout, "Got track named %s, times %d, %d\n", trk->name, p1->timestamp, p2->timestamp);
			if (! (labs(t1 - p2->timestamp) < threshold
			       /*  p1 p2      t1 t2 */
			       || labs(p1->timestamp - t2) < threshold)
			    /*  t1 t2      p1 p2 */
			    ) {
				continue;
			}
		}

		nearby_tracks = g_list_prepend(nearby_tracks, i->second);
	}

	return nearby_tracks;
}




/* c.f. get_sorted_track_name_list()
   but don't add the specified track to the list (normally current track). */
std::list<QString> LayerTRWTracks::get_sorted_track_name_list_exclude_self(Track const * self)
{
	std::list<QString> result;
	for (auto i = this->items.begin(); i != this->items.end(); i++) {

		/* Skip self. */
		if (i->second == self) {
			continue;
		}
		result.push_back(i->second->name);
	}

	result.sort();

	return result;
}




/* Currently unused
static void trw_layer_sorted_name_list(void * key, void * value, void * udata)
{
	GList **list = (GList**)udata;
	// *list = g_list_prepend(*all, key); //unsorted method
	// Sort named list alphabetically
	*list = g_list_insert_sorted_with_data (*list, key, sort_alphabetically, NULL);
}
*/




std::list<QString> LayerTRWTracks::get_sorted_track_name_list(void)
{
	std::list<QString> result;
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		result.push_back(i->second->name);
	}

	/* Sort list of names alphabetically. */
	result.sort();

	return result;
}




/**
 * Find out if any tracks have the same name in this hash table.
 */
QString LayerTRWTracks::has_duplicate_track_names(void)
{
	/* Build list of names. Sort list alphabetically. Find any two adjacent duplicates on the list. */

	if (this->items.size() <= 1) {
		return QString("");
	}

	std::list<QString> track_names = this->get_sorted_track_name_list();

	bool found = false;
	for (auto iter = std::next(track_names.begin()); iter != track_names.end(); iter++) {
		QString const this_one = *iter;
		QString const previous = *(std::prev(iter));

		if (this_one == previous) {
			return this_one;
		}
	}

	return QString("");
}




void LayerTRWTracks::set_items_visibility(bool on_off)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		i->second->visible = on_off;
		this->tree_view->set_tree_item_visibility(i->second->index, on_off);
	}
}




void LayerTRWTracks::toggle_items_visibility(void)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		i->second->visible = !i->second->visible;
		this->tree_view->toggle_tree_item_visibility(i->second->index);
	}
}




std::list<Track *> * LayerTRWTracks::get_track_values(std::list<Track *> * target)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		target->push_back(i->second);
	}

	return target;
}




void LayerTRWTracks::track_search_closest_tp(TrackpointSearch * search)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {

		Track * trk = i->second;

		if (!trk->visible) {
			continue;
		}

		if (!BBOX_INTERSECT (trk->bbox, search->bbox)) {
			continue;
		}


		for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {

			int x, y;
			search->viewport->coord_to_screen(&(*iter)->coord, &x, &y);

			const int dist_x = abs(x - search->x);
			const int dist_y = abs(y - search->y);

			if (dist_x <= TRACKPOINT_SIZE_APPROX && dist_y <= TRACKPOINT_SIZE_APPROX
			    && ((!search->closest_tp)
				/* Was the old trackpoint we already found closer than this one? */
				|| dist_x + dist_y < abs(x - search->closest_x) + abs(y - search->closest_y))) {

				search->closest_track = i->second;
				search->closest_tp = *iter;
				search->closest_tp_iter = iter;
				search->closest_x = x;
				search->closest_y = y;
			}

		}
	}
}




void LayerTRWTracks::change_coord_mode(CoordMode dest_mode)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		i->second->convert(dest_mode);
	}
}




void LayerTRWTracks::calculate_bounds(void)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		i->second->calculate_bounds();
	}
}




/**
   \brief Force unqiue track names for Tracks/Routes sublayer
*/
void LayerTRWTracks::uniquify(sort_order_t sort_order)
{
	if (this->items.empty()) {
		qDebug() << "EE: Layer TRW: ::uniquify() called for empty tracks/routes set";
		return;
	}

	/*
	  - Search tracks set for an instance of repeated name
	  - get track with this name
	  - create new name
	  - rename track & update equiv. tree view iter
	  - repeat until all different
	*/

	/* TODO: make the ::has_duplicate_track_names() return the track/route itself (or NULL). */
	QString duplicate_name = this->has_duplicate_track_names();
	while (duplicate_name != "") {

		/* Get the track/route with duplicate name. */
		Track * trk = this->find_track_by_name(duplicate_name);
		if (!trk) {
			/* Broken :( */
			qDebug() << "EE: Layer TRW: can't retrieve track/route with duplicate name" << duplicate_name;
#ifdef K
			this->get_window()->get_statusbar()->set_message(StatusBarField::INFO, tr("Internal Error during making tracks/routes unique"));
#endif
			return;
		}

		/* Rename it. */
		const QString uniq_name = this->new_unique_element_name(trk->name);
		trk->set_name(uniq_name);

		/* TODO: do we really need to do this? Isn't the name in tree view auto-updated? */
		if (trk->index.isValid()) {
			this->tree_view->set_tree_item_name(trk->index, uniq_name);
			this->tree_view->sort_children(this->get_index(), sort_order);
		}

		/* Try to find duplicate names again in the updated set of tracks. */
		QString duplicate_name_ = this->has_duplicate_track_names(); /* kamilTODO: there is a variable in this class with this name. */
	}
}




/**
   \brief Get a a unique new name for element of type \param item_type_id
*/
QString LayerTRWTracks::new_unique_element_name(const QString & old_name)
{
	int i = 2;
	QString new_name = old_name;

	Track * existing = NULL;
	do {
		existing = this->find_track_by_name(new_name);
		/* If found a name already in use try adding 1 to it and we try again. */
		if (existing) {
			new_name = QString("%1#%2").arg(old_name).arg(i);
			i++;
		}
	} while (existing != NULL);

	return new_name;
}





static const char * my_track_colors(int ii)
{
	static const char * colors[TRW_LAYER_TRACK_COLORS_MAX] = {
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
	return colors[(ii % TRW_LAYER_TRACK_COLORS_MAX)];
}




void LayerTRWTracks::assign_colors(int track_drawing_mode, const QColor & track_color_common)
{
	if (this->type_id == "sg.trw.tracks") {

		int ii = 0;
		for (auto i = this->items.begin(); i != this->items.end(); i++) {

			Track * trk = i->second;

			/* Tracks get a random spread of colors if not already assigned. */
			if (!trk->has_color) {
				if (track_drawing_mode == DRAWMODE_ALL_SAME_COLOR) {
					trk->color = track_color_common;
				} else {
					trk->color.setNamedColor(QString(my_track_colors(ii)));
				}
				trk->has_color = true;
			}

			this->update_tree_view(trk);

			ii++;
			if (ii > TRW_LAYER_TRACK_COLORS_MAX) {
				ii = 0;
			}
		}

	} else { /* Routes. */

		int ii = 0;
		for (auto i = this->items.begin(); i != this->items.end(); i++) {

			Track * trk = i->second;

			/* Routes get an intermix of reds. */
			if (!trk->has_color) {
				if (ii) {
					trk->color.setNamedColor("#FF0000"); /* Red. */
				} else {
					trk->color.setNamedColor("#B40916"); /* Dark Red. */
				}
				trk->has_color = true;
			}

			this->update_tree_view(trk);

			ii = !ii;
		}
	}
}




/**
 * Get the earliest timestamp available from all tracks.
 */
time_t LayerTRWTracks::get_earliest_timestamp()
{
	time_t timestamp = 0;
	std::list<Track *> * tracks_ = new std::list<Track *>;
	tracks_ = this->get_track_values(tracks_);

	if (!tracks_->empty()) {
		tracks_->sort(Track::compare_timestamp);

		/* Only need to check the first track as they have been sorted by time. */
		Track * trk = *(tracks_->begin());
		/* Assume trackpoints already sorted by time. */
		Trackpoint * tpt = trk->get_tp_first();
		if (tpt && tpt->has_timestamp) {
			timestamp = tpt->timestamp;
		}
	}
	delete tracks_;
	return timestamp;
}




/*
 * Update the tree view of the track id - primarily to update the icon.
 */
void LayerTRWTracks::update_tree_view(Track * trk)
{
	if (trk->index.isValid()) {
		QPixmap pixmap(SMALL_ICON_SIZE, SMALL_ICON_SIZE);
		pixmap.fill(trk->color);
		QIcon icon(pixmap);
		this->tree_view->set_tree_item_icon(trk->index, &icon);
	}
}




void LayerTRWTracks::add_children_to_tree(void)
{
	for (auto i = this->items.begin(); i != this->items.end(); i++) {
		Track * trk = i->second;

		QIcon * icon = NULL;
		if (trk->has_color) {
			QPixmap pixmap(SMALL_ICON_SIZE, SMALL_ICON_SIZE);
			pixmap.fill(trk->color);
			icon = new QIcon(pixmap);
		}

		time_t timestamp = 0;
		Trackpoint * tpt = trk->get_tp_first();
		if (tpt && tpt->has_timestamp) {
			timestamp = tpt->timestamp;
		}

		/* At this point each item is expected to have ::owning_layer member set to enclosing TRW layer. */

		this->tree_view->add_tree_item(this->index, trk, trk->name);
		this->tree_view->set_tree_item_icon(trk->index, icon);
		this->tree_view->set_tree_item_timestamp(trk->index, timestamp);

		delete icon;
	}
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
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (rezoom_to_show_all_items_cb()));

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
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (rezoom_to_show_all_items_cb()));

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


	qa = menu.addAction(QIcon::fromTheme("edit-paste"), tr("Paste"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (paste_sublayer_cb()));
#ifdef K
	/* TODO: only enable if suitable item is in clipboard - want to determine *which* sublayer type. */
	qa->setEnabled(a_clipboard_type() == VIK_CLIPBOARD_DATA_SUBLAYER);
#endif


	menu.addSeparator();


	if (this->type_id == "sg.trw.tracks") {
		this->sublayer_menu_tracks_misc((LayerTRW *) this->owning_layer, menu);
	}


	if (this->type_id == "sg.trw.routes") {
		this->sublayer_menu_routes_misc((LayerTRW *) this->owning_layer, menu);
	}


	this->sublayer_menu_sort(menu);


	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));
	layer_trw_sublayer_menu_all_add_external_tools((LayerTRW *) this->owning_layer, menu, external_submenu);


	return true;
}





/**
   \brief Re-adjust main viewport to show all items in this node
*/
void LayerTRWTracks::rezoom_to_show_all_items_cb(void) /* Slot. */
{
	const unsigned int n_items = this->items.size();

	if (0 < n_items) {
		struct LatLon maxmin[2] = { {0,0}, {0,0} };
		this->find_maxmin(maxmin);
		((LayerTRW *) this->owning_layer)->zoom_to_show_latlons(g_tree->tree_get_main_viewport(), maxmin);

		qDebug() << "SIGNAL: Layer TRW Tracks: re-zooming to show all items (" << n_items << "items)";
		g_tree->emit_update_window();
	}
}




void LayerTRWTracks::items_visibility_on_cb(void) /* Slot. */
{
	this->set_items_visibility(true);
	/* Redraw. */
	((LayerTRW *) this->owning_layer)->emit_layer_changed();
}




void LayerTRWTracks::items_visibility_off_cb(void) /* Slot. */
{
	this->set_items_visibility(false);
	/* Redraw. */
	((LayerTRW *) this->owning_layer)->emit_layer_changed();
}




void LayerTRWTracks::items_visibility_toggle_cb(void) /* Slot. */
{
	this->toggle_items_visibility();
	/* Redraw. */
	((LayerTRW *) this->owning_layer)->emit_layer_changed();
}




void LayerTRWTracks::track_list_dialog_cb(void) /* Slot. */
{
	QString title;
	if (this->type_id == "sg.trw.tracks") {
		title = tr("%1: Track List").arg(this->owning_layer->name);
	} else {
		title = tr("%1: Route List").arg(this->owning_layer->name);
	}
	track_list_dialog(title, this->owning_layer, this->type_id);
}




bool LayerTRWTracks::handle_selection_in_tree(void)
{
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	//parent_layer->set_statusbar_msg_info_trk(this);
	parent_layer->reset_internal_selections();

	g_tree->selected_tree_item = this;

	return true;
}




/**
 * Generally for drawing all tracks or routes or waypoints
 * tracks may be actually routes
 * It assumes they belong to the TRW Layer (it doesn't check this is the case)
 */
void LayerTRWTracks::draw_tree_item(Viewport * viewport, bool hl_is_allowed, bool hl_is_required)
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
		    || (g_tree->selected_tree_item && g_tree->selected_tree_item == this)); /* This item discovers that it is selected and decides to be highlighted. */ /* TODO: use UID to compare items */


#ifdef K
	LatLonBBox viewport_bbox;
	viewport->get_bbox(&viewport_bbox);

	if (BBOX_INTERSECT (this->bbox, viewport_bbox)) {
#endif
		for (auto i = this->items.begin(); i != this->items.end(); i++) {
			i->second->draw_tree_item(viewport, allowed, required);
		}
#ifdef K
	}
#endif
}




void LayerTRWTracks::paste_sublayer_cb(void)
{
	/* Slightly cheating method, routing via the panels capability. */
#ifdef K
	a_clipboard_paste(g_tree->tree_get_items_tree());
#endif
}




void LayerTRWTracks::sort_order_a2z_cb(void)
{
	((LayerTRW *) this->owning_layer)->track_sort_order = VL_SO_ALPHABETICAL_ASCENDING;
	this->tree_view->sort_children(this->index, VL_SO_ALPHABETICAL_ASCENDING);
}




void LayerTRWTracks::sort_order_z2a_cb(void)
{
	((LayerTRW *) this->owning_layer)->track_sort_order = VL_SO_ALPHABETICAL_DESCENDING;
	this->tree_view->sort_children(this->index, VL_SO_ALPHABETICAL_DESCENDING);
}




void LayerTRWTracks::sort_order_timestamp_ascend_cb(void)
{
	((LayerTRW *) this->owning_layer)->track_sort_order = VL_SO_DATE_ASCENDING;
	this->tree_view->sort_children(this->index, VL_SO_DATE_ASCENDING);
}




void LayerTRWTracks::sort_order_timestamp_descend_cb(void)
{
	((LayerTRW *) this->owning_layer)->track_sort_order = VL_SO_DATE_DESCENDING;
	this->tree_view->sort_children(this->index, VL_SO_DATE_DESCENDING);
}




TrackpointSearch::TrackpointSearch(int ev_x, int ev_y, Viewport * viewport_)
{
	this->x = ev_x;
	this->y = ev_y;
	this->viewport = viewport_;
	this->viewport->get_bbox(&this->bbox);
}