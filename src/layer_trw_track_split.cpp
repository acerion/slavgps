/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2012, Rob Norris <rw_norris@hotmail.com>
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




#include "layer_trw_track_internal.h"
#include "layer_trw.h"
#include "window.h"
#include "measurements.h"




using namespace SlavGPS;




#define SG_MODULE "Layer TRW Track Split"




/**
   \brief Split a track at given trackpoint

   Return a track that has been created as a result of spilt. The new
   track contains trackpoints from tp+1 till the last tp of original
   track.

   @return sg_ret status
*/
sg_ret Track::split_at_trackpoint(const TrackpointReference & tp_ref)
{
	if (this->empty()) {
		qDebug() << SG_PREFIX_N << "Can't split: track is empty";
		return sg_ret::err_cond;
	}

	if (!tp_ref.m_iter_valid) {
		qDebug() << SG_PREFIX_N << "Can't split: split trackpoint is invalid";
		return sg_ret::err_cond;
	}


	bool is_first = true;
	bool is_last = false;
	/* This call will also validate if TP is member of the track. */
	if (sg_ret::ok != this->get_item_position(tp_ref, is_first, is_last)) {
		qDebug() << SG_PREFIX_E << "Can't get trackpoint's position";
		return sg_ret::err;
	}
	if (is_first) {
		/* First TP in track. Don't split. This function shouldn't be called at all. */
		qDebug() << SG_PREFIX_N << "Can't split: split trackpoint is first trackpoint";
		return sg_ret::err_cond;
	}
	if (is_last) {
		/* Last TP in track. Don't split. This function shouldn't be called at all. */
		qDebug() << SG_PREFIX_N << "Can't split: split trackpoint is last trackpoint";
		return sg_ret::err_cond;
	}


	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;


	/* Configuration dialog. */
	{
		/* None. */
	}



	/* Process of determining ranges of trackpoints for new tracks. */
	std::list<TrackPoints::iterator> split_iters;
	{
		int n = 0;

		auto iter = this->trackpoints.begin();
		split_iters.push_back(TrackPoints::iterator(iter)); /* First iterator on the list is always trackpoints.begin(); */
		n++;
		qDebug() << SG_PREFIX_I << "Pushed trackpoints::begin() iter" << n << "=" << (*iter)->timestamp;


		iter = tp_ref.m_iter;
		split_iters.push_back(TrackPoints::iterator(iter));
		n++;
		qDebug() << SG_PREFIX_I << "Pushed trackpoints iter" << n << "=" << (*iter)->timestamp;


		iter = this->trackpoints.end();
		split_iters.push_back(TrackPoints::iterator(iter)); /* Last iterator on the list is always trackpoints.end(). */
		n++;
		qDebug() << SG_PREFIX_I << "Pushed trackpoints::end() iter" << n;
	}


	/* Creation of new tracks. */
	return this->split_at_iterators(split_iters, parent_layer);
}




sg_ret Track::split_at_iterators(std::list<TrackPoints::iterator> & split_iters, LayerTRW * parent_layer)
{
	/* Only bother updating if the split results in new tracks. */
	if (split_iters.size() == 2) {
		/* Only two iterators: begin() and end() iterator to
		   track's trackpoints. Not an error */
		qDebug() << SG_PREFIX_I << "Not enough trackpoint ranges to split track";
		return sg_ret::err_cond;
	}


	auto iter = split_iters.begin();
	/* Skip first range of trackpoints. These trackpoints will be
	   kept in original track. The rest of trackpoints (those from
	   second, third etc. range) will go to newly created
	   tracks. */
	iter++;

	for (; iter != std::prev(split_iters.end()); iter++) {

		TrackPoints::iterator tp_iter_begin = *iter;
		TrackPoints::iterator tp_iter_end = *std::next(iter);

		if (1) { /* Debug. */
			if (tp_iter_end != this->trackpoints.end()) {
				Trackpoint * tp1 = *tp_iter_begin;
				Trackpoint * tp2 = *tp_iter_end;
				qDebug() << SG_PREFIX_I << "Trackpoint" << tp1->timestamp << "(range from" << tp1->timestamp << "to" << tp2->timestamp << ")";
			} else {
				Trackpoint * tp1 = *tp_iter_begin;
				qDebug() << SG_PREFIX_I << "Trackpoint" << tp1->timestamp << "(range from" << tp1->timestamp << "to end)";
			}
		}

		Track * new_trk = new Track(*this); /* Just copy track properties. */
		__attribute__((unused)) const sg_ret mv = new_trk->move_trackpoints_from(*this, tp_iter_begin, tp_iter_end); /* Now move a range of trackpoints. */

		const QString new_trk_name = parent_layer->new_unique_element_name(this->m_type_id, this->get_name());
		new_trk->set_name(new_trk_name);

		parent_layer->add_track(new_trk);
	}


	/* Original track is not removed. It keeps those trackpoints
	   that were described by first pair of iterators in
	   @split_iters list. Rest of trackpoints from the original
	   track have been transferred to new tracks. */


	this->emit_tree_item_changed("A TRW Track has been split into several tracks");

	/* Track has been changed. Parent layer has to know about this. */
	parent_layer->deselect_current_trackpoint(this);

	return sg_ret::ok;
}




void Track::split_by_timestamp_cb(void)
{
	if (this->empty()) {
		qDebug() << SG_PREFIX_I << "Can't split: track is empty";
		return;
	}

	Duration threshold(60, DurationType::Unit::E::Seconds);
	QWidget * dialog_parent = ThisApp::main_window();
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;


	/* Configuration dialog. */
	{
		if (false == Dialog::duration(tr("Split Threshold..."),
					      tr("Split when time between trackpoints exceeds:"),
					      threshold,
					      dialog_parent)) {
			return;
		}

		if (threshold.is_zero()) {
			return;
		}
	}


	/* Process of determining ranges of trackpoints for new tracks. */
	std::list<TrackPoints::iterator> split_iters;
	{
		int n = 0;
		Time prev_timestamp = (*this->trackpoints.begin())->timestamp;

		auto iter = this->trackpoints.begin();
		split_iters.push_back(TrackPoints::iterator(iter)); /* First iterator on the list is always trackpoints.begin(); */
		n++;
		qDebug() << SG_PREFIX_I << "Pushed trackpoints::begin() iter" << n << "=" << (*iter)->timestamp;


		for (; iter != this->trackpoints.end(); iter++) {
			const Time this_timestamp = (*iter)->timestamp;
			const Duration timestamp_delta = Duration::get_abs_duration(this_timestamp, prev_timestamp);

			/* Check for unordered time points - this is quite a rare occurence - unless one has reversed a track. */

			if (timestamp_delta.is_negative()) {
				if (Dialog::yes_or_no(tr("Can not split track due to trackpoints not ordered in time - such as at %1.\n\nGoto this trackpoint?").arg(this_timestamp.strftime_local("%c"))), dialog_parent) {
					parent_layer->request_new_viewport_center(ThisApp::main_gisview(), (*iter)->coord);
				}
				return;
			}

			if (timestamp_delta > threshold) {
				prev_timestamp = this_timestamp;
				split_iters.push_back(TrackPoints::iterator(iter));
				n++;
				qDebug() << SG_PREFIX_I << "Pushed trackpoints iter" << n << "=" << (*iter)->timestamp;
			}
		}


		iter = this->trackpoints.end();
		split_iters.push_back(TrackPoints::iterator(iter)); /* Last iterator on the list is always trackpoints.end(). */
		n++;
		qDebug() << SG_PREFIX_I << "Pushed trackpoints::end() iter" << n;
	}


	/* Creation of new tracks. */
	this->split_at_iterators(split_iters, parent_layer);


	return;
}




/**
   Split a track by the number of points as specified by the user
*/
void Track::split_by_n_points_cb(void)
{
	if (this->empty()) {
		qDebug() << SG_PREFIX_I << "Can't split: track is empty";
		return;
	}


	int n_points = 0;
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;
	QWidget * dialog_parent = ThisApp::main_window();


	/* Configuration dialog. */
	{
		n_points = Dialog::get_int(tr("Split Every Nth Point"),
					   tr("Split on every Nth point:"),
					   250,   /* Default value as per typical limited track capacity of various GPS devices. */
					   2,     /* Min */
					   65536, /* Max */
					   5,     /* Step */
					   NULL,  /* ok */
					   dialog_parent);

		/* Was a valid number returned? */
		if (0 == n_points) {
			return;
		}
	}


	/* Process of determining ranges of trackpoints for new tracks. */
	std::list<TrackPoints::iterator> split_iters;
	{
		int n = 0;
		int tp_counter = -1;

		auto iter = this->trackpoints.begin();
		split_iters.push_back(TrackPoints::iterator(iter)); /* First iterator on the list is always trackpoints.begin(); */
		n++;
		qDebug() << SG_PREFIX_I << "Pushed trackpoints::begin() iter" << n << "=" << (*iter)->timestamp;


		for (; iter != this->trackpoints.end(); iter++) {
			tp_counter++;
			if (tp_counter >= n_points) {
				tp_counter = 0;
				split_iters.push_back(TrackPoints::iterator(iter));
				n++;
				qDebug() << SG_PREFIX_I << "Pushed trackpoints iter" << n;
			}
		}


		iter = this->trackpoints.end();
		split_iters.push_back(TrackPoints::iterator(iter)); /* Last iterator on the list is always trackpoints.end(). */
		n++;
		qDebug() << SG_PREFIX_I << "Pushed trackpoints::end() iter" << n;
	}


	/* Creation of new tracks. */
	this->split_at_iterators(split_iters, parent_layer);


	return;
}




/**
   Split a track by its segments
   Routes do not have segments so don't call this for routes
*/
void Track::split_by_segments_cb(void)
{
	if (this->empty()) {
		qDebug() << SG_PREFIX_I << "Can't split: track is empty";
		return;
	}


	QWidget * dialog_parent = ThisApp::main_window();
	LayerTRW * parent_layer = (LayerTRW *) this->owning_layer;

	const unsigned int segs = this->get_segment_count();
	if (segs < 2) {
		Dialog::info(tr("Can not split track as it has no segments"), dialog_parent);
		return;
	}


	/* Configuration dialog. */
	{
		/* None. */
	}


	/* Process of determining ranges of trackpoints for new tracks. */
	std::list<TrackPoints::iterator> split_iters;
	{
		int n = 0;

		auto iter = this->trackpoints.begin();
		split_iters.push_back(TrackPoints::iterator(iter)); /* First iterator on the list is always trackpoints.begin(); */
		n++;
		qDebug() << SG_PREFIX_I << "Pushed trackpoints::begin() iter" << n << "=" << (*iter)->timestamp;


		/* If there are segments defined in the track (and we
		   have established this with ::get_segment_count()
		   above), then the first trackpoint in ::trackpoints
		   container should have "Trackpoint::newsegment ==
		   true", right? Let's verify this here. It's not too
		   late yet to abort splitting if this test fails. */
		if ((*iter)->newsegment != true) {
			qDebug() << SG_PREFIX_E << "Assertion about first trackpoint failed: first trackpoint's ::newsegment == false";
			return;
		}


		/* Don't let that first trackpoint with
		   "Trackpoint::newsegment == true" be pushed to
		   'split_iters' twice (above as ::begin(), and below in
		   the loop). */
		iter++;


		for (; iter != this->trackpoints.end(); iter++) {
			if ((*iter)->newsegment) {
				split_iters.push_back(TrackPoints::iterator(iter));
				n++;
				qDebug() << SG_PREFIX_I << "Pushed trackpoints iter" << n << "=" << (*iter)->timestamp;
			}
		}


		iter = this->trackpoints.end();
		split_iters.push_back(TrackPoints::iterator(iter)); /* Last iterator on the list is always trackpoints.end(). */
		n++;
		qDebug() << SG_PREFIX_I << "Pushed trackpoints::end() iter" << n;
	}


	/* Creation of new tracks. */
	this->split_at_iterators(split_iters, parent_layer);


	return;
}
