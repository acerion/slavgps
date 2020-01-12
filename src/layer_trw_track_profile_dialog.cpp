/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2007, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007-2008, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2012-2014, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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




#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cassert>




#include <QDebug>
#include <QPushButton>




#include "window.h"
#include "layer_trw.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_track_profile_dialog.h"
#include "viewport_internal.h"
#include "dem_cache.h"
#include "vikutils.h"
#include "util.h"
#include "ui_util.h"
#include "dialog.h"
#include "application_state.h"
#include "preferences.h"
#include "measurements.h"
#include "graph_intervals.h"
#include "tree_view_internal.h"




using namespace SlavGPS;




#define SG_MODULE "Track Profile Dialog"

#define MY_WIDGET_PROPERTY "ProfileViewBase"


#define VIK_SETTINGS_TRACK_PROFILE_WIDTH  "track_profile_display_width"
#define VIK_SETTINGS_TRACK_PROFILE_HEIGHT "track_profile_display_height"

#define VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_DEM_ELEVATION    "track_profile_et_show_dem_elevation"
#define VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_GPS_SPEED        "track_profile_et_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_SD_SHOW_GPS_SPEED        "track_profile_sd_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_DEM_ELEVATION    "track_profile_ed_show_dem_elevation"
#define VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_GPS_SPEED        "track_profile_ed_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_GD_SHOW_GPS_SPEED        "track_profile_gd_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_ST_SHOW_GPS_SPEED        "track_profile_st_show_gps_speed"
#define VIK_SETTINGS_TRACK_PROFILE_DT_SHOW_GPS_SPEED        "track_profile_dt_show_gps_speed"




#define GRAPH_INITIAL_WIDTH 400
#define GRAPH_INITIAL_HEIGHT 300

#define GRAPH_MARGIN_LEFT 80 // 70
#define GRAPH_MARGIN_RIGHT 40 // 1
#define GRAPH_MARGIN_TOP 20
#define GRAPH_MARGIN_BOTTOM 30 // 1




enum {
	SG_TRACK_PROFILE_CANCEL,
	SG_TRACK_PROFILE_SPLIT_AT_MARKER,
	SG_TRACK_PROFILE_OK,
};




TrackProfileDialog::~TrackProfileDialog()
{
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		delete *iter;
	}
}




/**
   Change what is displayed in main GIS viewport in reaction to click
   event in one of Profile Dialog graphs.
*/
sg_ret TrackProfileDialog::set_center_at_selected_tp(const ProfileViewBase * view, QMouseEvent * ev)
{
	this->button_split_at_marker->setEnabled(false);

	const TPInfo tp_info = view->get_tp_info_under_cursor(ev);
	if (!tp_info.valid) {
		qDebug() << SG_PREFIX_E << "No valid tp info found for view" << view->get_title();
		return sg_ret::err;
	}

	if (NULL == tp_info.found_tp) {
		qDebug() << SG_PREFIX_E << "NULL 'found tp' for view" << view->get_title();
		return sg_ret::err;
	}

	if (sg_ret::ok != this->trk->select_tp(tp_info.found_tp)) {
		qDebug() << SG_PREFIX_E << "Failed to select tp for view" << view->get_title();
		return sg_ret::err;
	}

	this->main_gisview->set_center_coord(tp_info.found_tp->coord);
	this->trw->emit_tree_item_changed("Clicking on trackpoint in profile view has brought this trackpoint to center of main GIS viewport");

	/* There is a selected trackpoint, on which we can split the track. */
	this->button_split_at_marker->setEnabled(true);

	return sg_ret::ok;
}




/**
   Draw two pairs of horizontal and vertical lines intersecting at given position.

   One pair is for position of selected trackpoint.
   The other pair is for current position of cursor.

   Both "pos" arguments should indicate position in graph's coordinate system.
*/
sg_ret ProfileViewBase::draw_crosshairs(const Crosshair2D & selection_ch, const Crosshair2D & hover_ch)
{
	if (!selection_ch.valid && !hover_ch.valid) {
		/* Perhaps this should be an error, maybe we shouldn't
		   call the function when both positions are
		   invalid. */
		qDebug() << SG_PREFIX_N << "Not drawing crosshairs: both crosshairs are invalid";
		return sg_ret::ok;
	}


	/* Restore previously saved image that has no crosshairs on
	   it, just the graph, grids, borders and margins. */
	if (this->graph_2d->saved_pixmap_valid) {
		/* Debug code. */
		// qDebug() << SG_PREFIX_I << "Restoring saved image";
		this->graph_2d->set_pixmap(this->graph_2d->saved_pixmap);
	} else {
		qDebug() << SG_PREFIX_W << "NOT restoring saved image";
	}



	/*
	  Now draw crosshairs on this fresh (restored from saved) image.
	*/
	if (selection_ch.valid) {
		// qDebug() << SG_PREFIX_I << "Will now draw 'selection' crosshair in" << this->get_title();
		this->graph_2d->central_draw_simple_crosshair(selection_ch);
	}
	if (hover_ch.valid) {
		// qDebug() << SG_PREFIX_D << "Will now draw 'hover' crosshair in" << this->get_title();
		this->graph_2d->central_draw_simple_crosshair(hover_ch);
	}



	/*
	  From the test made on top of the function we know that at
	  least one crosshair has been painted. This call will call
	  GisViewport::paintEvent(), triggering final render to
	  screen.
	*/
	this->graph_2d->update();

	return sg_ret::ok;
}




ProfileViewBase * TrackProfileDialog::get_current_view(void) const
{
	const int tab_idx = this->tabs->currentIndex();
	ProfileViewBase * profile_view = (ProfileViewBase *) this->tabs->widget(tab_idx)->property(MY_WIDGET_PROPERTY).toULongLong();
	return profile_view;
}




/**
   React to mouse button release

   Find a trackpoint corresponding to cursor position when button was released.
   Draw crosshair for this trackpoint.
*/
void TrackProfileDialog::handle_mouse_button_release_cb(ViewportPixmap * vpixmap, QMouseEvent * ev)
{
	Graph2D * graph_2d = (Graph2D *) vpixmap;
	ProfileViewBase * view = this->get_current_view();
	assert (view->graph_2d == graph_2d);

	qDebug() << SG_PREFIX_I << "Crosshair. Mouse event at" << ev->x() << ev->y() << "(cbl ="
		 << ev->x() - graph_2d->central_get_leftmost_pixel()
		 << graph_2d->central_get_bottommost_pixel() - ev->y() << ")";

	/* Noninitialized == invalid. Don't draw hover crosshair on
	   clicks, it's not necessary: the two crosshairs (hover and
	   selection crosshair) would be drawn in the same
	   position. */
	const Crosshair2D hover_ch;

	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		ProfileViewBase * view_iter = *iter;
		if (!view_iter->graph_2d) {
			qDebug() << SG_PREFIX_N << "Not drawing crosshairs in" << view_iter->get_title() << ": no graph";
			continue;
		}

		if (!view_iter->track_data_is_valid()) {
			qDebug() << SG_PREFIX_N << "Not drawing crosshairs in" << view_iter->get_title() << ": track data invalid";
			/* We didn't visit that tab yet, so track data
			   hasn't been generated for current graph
			   width. */
			/* FIXME: generate the track data so that we
			   can set a crosshair over there. */
			continue;
		}

		view_iter->m_selection_ch = view_iter->get_crosshair_under_cursor(ev);
		view_iter->m_selection_ch.debug = "selection crosshair in " + view_iter->get_title();
		if (!view_iter->m_selection_ch.valid) {
			qDebug() << SG_PREFIX_N << "Not drawing crosshairs in" << view_iter->get_title() << ": failed to get selection crosshair";
			continue;
		}

		/*
		  Positions passed to draw_crosshairs() are in 2D
		  graph's coordinate system (beginning in bottom left
		  corner), not Qt's coordinate system (beginning in
		  upper left corner).
		*/
		qDebug() << SG_PREFIX_I << "Will now draw crosshairs in" << view_iter->get_title();
		view_iter->draw_crosshairs(view_iter->m_selection_ch, hover_ch);
	}

	this->button_split_at_marker->setEnabled(false);

	const TPInfo tp_info = view->get_tp_info_under_cursor(ev);
	if (!tp_info.valid) {
		qDebug() << SG_PREFIX_W << "No valid tp info found for view" << view->get_title();
		return;
	}

	if (NULL == tp_info.found_tp) {
		qDebug() << SG_PREFIX_E << "NULL 'found tp' for view" << view->get_title();
		return;
	}

	if (sg_ret::ok != this->trk->select_tp(tp_info.found_tp)) {
		qDebug() << SG_PREFIX_E << "Failed to select tp for view" << view->get_title();
		return;
	}

	this->main_gisview->set_center_coord(tp_info.found_tp->coord);
	this->trw->emit_tree_item_changed("Clicking on trackpoint in profile view has brought this trackpoint to center of main GIS viewport");

	/* There is a selected trackpoint, on which we can split the track. */
	this->button_split_at_marker->setEnabled(true);

	return;
}




Crosshair2D ProfileViewBase::tpinfo_to_crosshair(const TPInfo & tp_info) const
{
	Crosshair2D crosshair;

	const int leftmost_px = this->graph_2d->central_get_leftmost_pixel();
	const int bottommost_px = this->graph_2d->central_get_bottommost_pixel();

	crosshair.central_cbl_x_px = tp_info.found_x_px - leftmost_px;
	crosshair.central_cbl_y_px = bottommost_px - tp_info.found_y_px;

	/*
	  Use coordinates of point that is
	  a) limited to central area of 2d graph (so as if margins outside of the central area didn't exist),
	  b) is in 'beginning in bottom-left' coordinate system (cbl)
	  to calculate global, 'beginning in top-left' coordinates.
	*/
	crosshair.x_px = crosshair.central_cbl_x_px + leftmost_px;
	crosshair.y_px = bottommost_px - crosshair.central_cbl_y_px;

	crosshair.valid = true;

	return crosshair;
}





void TrackProfileDialog::handle_cursor_move_cb(ViewportPixmap * vpixmap, QMouseEvent * ev)
{
	Graph2D * graph_2d = (Graph2D *) vpixmap;
	ProfileViewBase * view = this->get_current_view();
	assert (view->graph_2d == graph_2d);

	if (!view->track_data_is_valid()) {
		qDebug() << SG_PREFIX_E << "Not handling cursor move, track data invalid";
		return;
	}

	view->on_cursor_move(trk, ev);

	return;
}




namespace SlavGPS {




template <>
sg_ret ProfileView<Distance, Altitude>::draw_dem_elevation(void)
{
	return this->draw_parameter_from_auxiliary_source();
}




template <>
sg_ret ProfileView<Time, Altitude>::draw_dem_elevation(void)
{
	return this->draw_parameter_from_auxiliary_source();
}




template <>
sg_ret ProfileView<Distance, Speed>::draw_gps_speeds(void)
{
	return this->draw_parameter_from_auxiliary_source();
}




template <>
sg_ret ProfileView<Time, Speed>::draw_gps_speeds(void)
{
	return this->draw_parameter_from_auxiliary_source();
}




template <>
Speed ProfileView<Distance, Speed>::get_tp_aux_value_uu(const Trackpoint & tp)
{
	return Speed(tp.gps_speed, SpeedType::Unit::user_unit());
}




template <>
Speed ProfileView<Time, Speed>::get_tp_aux_value_uu(const Trackpoint & tp)
{
	return Speed(tp.gps_speed, SpeedType::Unit::user_unit());
}




template <>
Altitude ProfileView<Distance, Altitude>::get_tp_aux_value_uu(const Trackpoint & tp)
{
	/* TODO_MAYBE: Getting elevation from DEM cache may be slow
	   when we do it for every TP on every redraw of view. */
	return DEMCache::get_elev_by_coord(tp.coord, DemInterpolation::Simple).convert_to_unit(Altitude::Unit::user_unit());
}




template <>
Altitude ProfileView<Time, Altitude>::get_tp_aux_value_uu(const Trackpoint & tp)
{
	/* TODO_MAYBE: Getting elevation from DEM cache may be slow
	   when we do it for every TP on every redraw of view. */
	return DEMCache::get_elev_by_coord(tp.coord, DemInterpolation::Simple).convert_to_unit(AltitudeType::Unit::user_unit());
}




}




void ProfileViewET::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_DEM_ELEVATION, this->show_dem_cb->checkState());
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




sg_ret ProfileViewET::draw_additional_indicators(Track & trk)
{
	if (this->show_dem_cb && this->show_dem_cb->checkState()) {
		return this->draw_dem_elevation();
	} else {
		return sg_ret::ok;
	}
}




void ProfileViewSD::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_SD_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




sg_ret ProfileViewSD::draw_additional_indicators(Track & trk)
{
	if (this->show_gps_speed_cb && this->show_gps_speed_cb->checkState()) {
		return this->draw_gps_speeds();
	} else {
		return sg_ret::ok;
	}
}




void ProfileViewED::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_DEM_ELEVATION, this->show_dem_cb->checkState());
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




sg_ret ProfileViewED::draw_additional_indicators(Track & trk)
{
	if (this->show_dem_cb && this->show_dem_cb->checkState()) {
		return this->draw_dem_elevation();
	} else {
		return sg_ret::ok;
	}
}




void ProfileViewGD::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_GD_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




sg_ret ProfileViewGD::draw_additional_indicators(Track & trk)
{
	if (this->show_gps_speed_cb && this->show_gps_speed_cb->checkState()) {
		/* Ensure some kind of max speed when not set. */
		if (!trk.get_max_speed().is_valid() || trk.get_max_speed().is_zero()) {
			trk.calculate_max_speed();
		}

		return this->draw_gps_speeds_relative(trk);
	} else {
		qDebug() << SG_PREFIX_E << "Can't draw relative GPS speeds - no widgets";
		return sg_ret::err;
	}
}




void ProfileViewST::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_ST_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}



sg_ret ProfileViewST::draw_additional_indicators(Track & trk)
{
	if (this->show_gps_speed_cb && this->show_gps_speed_cb->checkState()) {
		return this->draw_gps_speeds();
	} else {
		return sg_ret::ok;
	}
}



void ProfileViewDT::save_settings(void)
{
	ApplicationState::set_boolean(VIK_SETTINGS_TRACK_PROFILE_DT_SHOW_GPS_SPEED, this->show_gps_speed_cb->checkState());
}




sg_ret ProfileViewDT::draw_additional_indicators(Track & trk)
{
	if (this->show_gps_speed_cb && this->show_gps_speed_cb->checkState()) {
		/* Ensure some kind of max speed when not set. */
		if (!trk.get_max_speed().is_valid() || trk.get_max_speed().is_zero()) {
			trk.calculate_max_speed();
		}
		return this->draw_gps_speeds_relative(trk);
	} else {
		qDebug() << SG_PREFIX_E << "Can't draw relative GPS speeds - no widgets";
		return sg_ret::err;
	}
}




/**
   Look up view
*/
ProfileViewBase * TrackProfileDialog::find_view(Graph2D * graph_2d) const
{
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		ProfileViewBase * view = *iter;
		if (view->graph_2d == graph_2d) {
			return view;
		}
	}
	return NULL;
}




/**
   Draw all graphs
*/
void TrackProfileDialog::draw_all_views(bool resized)
{
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		ProfileViewBase * view = *iter;
		if (!view->graph_2d) {
			continue;
		}

		/* If dialog window is resized then saved image is no longer valid. */
		view->graph_2d->saved_pixmap_valid = !resized;
		view->draw_track_and_crosshairs(*this->trk);
	}
}




sg_ret ProfileViewBase::draw_track_and_crosshairs(Track & trk)
{
	sg_ret ret_trk;
	sg_ret ret_marks;

	sg_ret ret = this->draw_graph_without_crosshairs(trk);
	if (sg_ret::ok != ret) {
		qDebug() << SG_PREFIX_E << "Failed to draw graph without crosshairs";
		return ret;
	}


	/* Draw crosshairs. */
	if (1) {
		Crosshair2D hover_ch; /* Invalid, don't draw hover crosshair right after a view has been created or resized. */
		hover_ch.debug = "cursor pos";

		ret = this->draw_crosshairs(this->m_selection_ch, hover_ch);
		if (sg_ret::ok != ret) {
			qDebug() << SG_PREFIX_E << "Failed to draw crosshairs";
		}
	}


	return ret;
}




sg_ret TrackProfileDialog::handle_graph_resize_cb(ViewportPixmap * pixmap)
{
	Graph2D * graph_2d = (Graph2D *) pixmap;
	qDebug() << SG_PREFIX_SLOT << "Reacting to signal from graph" << QString(graph_2d->debug);

	ProfileViewBase * view = this->find_view(graph_2d);
	if (!view) {
		qDebug() << SG_PREFIX_E << "Can't find view";
		return sg_ret::err;
	}

	/*
	  Invalidate. Old crosshair would be invalid in graph with new
	  sizes.

	  TODO_MAYBE: selection of tp should survive resizing of
	  graphs. Maybe we should save not only crosshair position,
	  but also Distance or Time on x axis, and then, based on that
	  value we should re-calculate ::m_selection_ch.
	*/
	view->m_selection_ch = Crosshair2D();

	view->graph_2d->cached_central_n_columns = view->graph_2d->central_get_n_columns();
	view->graph_2d->cached_central_n_rows = view->graph_2d->central_get_n_rows();

	view->graph_2d->saved_pixmap_valid = true;
	view->draw_track_and_crosshairs(*this->trk);


	return sg_ret::ok;
}




void ProfileViewBase::create_graph_2d(void)
{
	this->graph_2d = new Graph2D(GRAPH_MARGIN_LEFT, GRAPH_MARGIN_RIGHT, GRAPH_MARGIN_TOP, GRAPH_MARGIN_BOTTOM, NULL);
	snprintf(this->graph_2d->debug, sizeof (this->graph_2d->debug), "%s", this->get_title().toUtf8().constData());

	return;
}




void TrackProfileDialog::save_settings(void)
{
	/* Session settings. */
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, this->profile_width);
	ApplicationState::set_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, this->profile_height);

	/* Just for this session. */
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		(*iter)->save_settings();
	}
}




void TrackProfileDialog::destroy_cb(void) /* Slot. */
{
	this->save_settings();
}




void TrackProfileDialog::dialog_response_cb(int resp) /* Slot. */
{
	bool keep_dialog = false;
	sg_ret ret = sg_ret::err;

	/* Note: destroying dialog (eg, parent window exit) won't give "response". */
	switch (resp) {
	case SG_TRACK_PROFILE_CANCEL:
		this->reject();
		break;

	case SG_TRACK_PROFILE_OK:
		this->trk->update_tree_item_properties();
		this->trw->emit_tree_item_changed("TRW - Track Profile Dialog - Profile OK");
		this->accept();
		break;

	case SG_TRACK_PROFILE_SPLIT_AT_MARKER:
		ret = this->trk->split_at_selected_trackpoint_cb();
		if (sg_ret::ok != ret) {
			Dialog::error(tr("Failed to split track. Track unchanged."), this->trw->get_window());
			keep_dialog = true;
		} else {
			this->trw->emit_tree_item_changed("A TRW Track has been split into several tracks (at marker)");
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Dialog response slot: unknown response" << resp;
		return;
	}

	/* Keep same behaviour for now: destroy dialog if click on any button. */
	if (!keep_dialog) {
		this->destroy_cb();
		this->accept();
	}
}




/**
 * Force a redraw when checkbutton has been toggled to show/hide that information.
 */
void TrackProfileDialog::checkbutton_toggle_cb(void)
{
	/* Even though not resized, we'll pretend it is -
	   as this invalidates the saved images (since the image may have changed). */
	this->draw_all_views(true);
}




/**
   Create the widgets for the given graph tab
*/
void ProfileViewBase::create_widgets_layout(void)
{
	this->widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->widget->setMinimumSize(500, 300);

	this->main_vbox = new QVBoxLayout();
	this->labels_grid = new QGridLayout();
	this->controls_vbox = new QVBoxLayout();

	QLayout * old = this->widget->layout();
	delete old;
	qDeleteAll(this->widget->children());
	this->widget->setLayout(this->main_vbox);

	this->main_vbox->addWidget(this->graph_2d);
	this->main_vbox->addLayout(this->labels_grid);
	this->main_vbox->addLayout(this->controls_vbox);

	return;
}




void SlavGPS::track_profile_dialog(Track * trk, GisViewport * main_gisview, QWidget * parent)
{
	TrackProfileDialog dialog(QObject::tr("Track Profile"), trk, main_gisview, parent);
	trk->set_profile_dialog(&dialog);
	dialog.exec();
	trk->clear_profile_dialog();
}




const QString & ProfileViewBase::get_title(void) const
{
	return this->title;
}








TrackProfileDialog::TrackProfileDialog(QString const & title, Track * new_trk, GisViewport * new_main_gisview, QWidget * parent) : QDialog(parent)
{
	this->setWindowTitle(tr("%1 - Track Profile").arg(new_trk->get_name()));

	this->trw = (LayerTRW *) new_trk->get_owning_layer();
	this->trk = new_trk;
	this->main_gisview = new_main_gisview;


	QLayout * old = this->layout();
	delete old;
	qDeleteAll(this->children());
	QVBoxLayout * vbox = new QVBoxLayout;
	this->setLayout(vbox);


	int profile_size_value;
	/* Ensure minimum values. */
	this->profile_width = 600;
	if (ApplicationState::get_integer(VIK_SETTINGS_TRACK_PROFILE_WIDTH, &profile_size_value)) {
		if (profile_size_value > this->profile_width) {
			this->profile_width = profile_size_value;
		}
	}

	this->profile_height = 300;
	if (ApplicationState::get_integer(VIK_SETTINGS_TRACK_PROFILE_HEIGHT, &profile_size_value)) {
		if (profile_size_value > this->profile_height) {
			this->profile_height = profile_size_value;
		}
	}

	this->views.push_back(new ProfileViewED(this));
	this->views.push_back(new ProfileViewGD(this));
	this->views.push_back(new ProfileViewST(this));
	this->views.push_back(new ProfileViewDT(this));
	this->views.push_back(new ProfileViewET(this));
	this->views.push_back(new ProfileViewSD(this));

	this->tabs = new QTabWidget();
	this->tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	this->trk->prepare_for_profile();

	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		ProfileViewBase * view = *iter;
		if (!view) {
			continue;
		}


		this->tabs->addTab(view->widget, view->get_title());

		qDebug() << SG_PREFIX_I << "Configuring signals for graph" << QString(view->graph_2d->debug) << "in view" << view->get_title();
		connect(view->graph_2d, SIGNAL (cursor_moved(ViewportPixmap *, QMouseEvent *)),    this, SLOT (handle_cursor_move_cb(ViewportPixmap *, QMouseEvent *)));
		connect(view->graph_2d, SIGNAL (button_released(ViewportPixmap *, QMouseEvent *)), this, SLOT (handle_mouse_button_release_cb(ViewportPixmap *, QMouseEvent *)));
		connect(view->graph_2d, SIGNAL (size_changed(ViewportPixmap *)), this, SLOT (handle_graph_resize_cb(ViewportPixmap *)));
	}


	this->button_box = new QDialogButtonBox();


	this->button_cancel = this->button_box->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
	this->button_split_at_marker = this->button_box->addButton(tr("Split at &Marker"), QDialogButtonBox::ActionRole);
	this->button_ok = this->button_box->addButton(tr("&OK"), QDialogButtonBox::AcceptRole);

	this->button_split_at_marker->setEnabled(1 == this->trk->get_selected_children().get_count()); /* Initially no trackpoint is selected. */

	this->signal_mapper = new QSignalMapper(this);
	connect(this->button_cancel,          SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_split_at_marker, SIGNAL (released()), signal_mapper, SLOT (map()));
	connect(this->button_ok,              SIGNAL (released()), signal_mapper, SLOT (map()));

	this->signal_mapper->setMapping(this->button_cancel,          SG_TRACK_PROFILE_CANCEL);
	this->signal_mapper->setMapping(this->button_split_at_marker, SG_TRACK_PROFILE_SPLIT_AT_MARKER);
	this->signal_mapper->setMapping(this->button_ok,              SG_TRACK_PROFILE_OK);

	connect(this->signal_mapper, SIGNAL (mapped(int)), this, SLOT (dialog_response_cb(int)));


	vbox->addWidget(this->tabs);
	vbox->addWidget(this->button_box);

	int i = 0;
	for (auto iter = this->views.begin(); iter != this->views.end(); iter++) {
		i++;
		ProfileViewBase * view = *iter;
		if (!view) {
			qDebug() << SG_PREFIX_E << "Can't find profile" << i << "in loop";
			continue;
		}
		view->generate_initial_track_data_wrapper(trk);
	}
}





void ProfileViewBase::configure_labels_x_distance()
{
	this->labels.x_label = new QLabel(QObject::tr("Distance From Start:"));
	this->labels.x_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

	/* Additional absolute timestamp to provide more information in UI. */
	this->labels.tp_timestamp_label = new QLabel(QObject::tr("Trackpoint timestamp:"));
	this->labels.tp_timestamp_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);
}

void ProfileViewBase::configure_labels_x_time()
{
	this->labels.x_label = new QLabel(QObject::tr("Time From Start:"));
	this->labels.x_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);

	/* Additional absolute timestamp to provide more information in UI. */
	this->labels.tp_timestamp_label = new QLabel(QObject::tr("Trackpoint timestamp:"));
	this->labels.tp_timestamp_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);
}


void ProfileViewBase::configure_labels_y_altitude()
{
	this->labels.y_label = new QLabel(QObject::tr("Track Height:"));
	this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);
}

void ProfileViewBase::configure_labels_y_gradient()
{
	this->labels.y_label = new QLabel(QObject::tr("Track Gradient:"));
	this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);
}

void ProfileViewBase::configure_labels_y_speed()
{
	this->labels.y_label = new QLabel(QObject::tr("Track Speed:"));
	this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);
}

void ProfileViewBase::configure_labels_y_distance()
{
	this->labels.y_label = new QLabel(QObject::tr("Distance From Start:"));
	this->labels.y_value = ui_label_new_selectable(QObject::tr("No Data"), NULL);
}



void ProfileViewBase::configure_labels_post(void)
{
	/* Use spacer item in last column to bring first two columns
	   (with parameter's name and parameter's value) close
	   together. */

	int row = 0;
	this->labels_grid->addWidget(this->labels.x_label, row, 0, Qt::AlignLeft);
	this->labels_grid->addWidget(this->labels.x_value, row, 1, Qt::AlignRight);
	this->labels_grid->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 3);
	row++;

	this->labels_grid->addWidget(this->labels.y_label, row, 0, Qt::AlignLeft);
	this->labels_grid->addWidget(this->labels.y_value, row, 1, Qt::AlignRight);
	this->labels_grid->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 3);
	row++;

	this->labels_grid->addWidget(this->labels.tp_timestamp_label, row, 0, Qt::AlignLeft);
	this->labels_grid->addWidget(this->labels.tp_timestamp_value, row, 1, Qt::AlignRight);
	this->labels_grid->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 3);
	row++;

	return;
}




void ProfileViewET::configure_controls(void)
{
	bool value;

	this->show_dem_cb = new QCheckBox(QObject::tr("Show DEM"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_DEM_ELEVATION, &value)) {
		this->show_dem_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_dem_cb);
	QObject::connect(this->show_dem_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed (relative)"), dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ET_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewSD::configure_controls(void)
{
	bool value;

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_SD_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewED::configure_controls(void)
{
	bool value;

	this->show_dem_cb = new QCheckBox(QObject::tr("Show DEM"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_DEM_ELEVATION, &value)) {
		this->show_dem_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_dem_cb);
	QObject::connect(this->show_dem_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed (relative)"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ED_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewGD::configure_controls(void)
{
	bool value;

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed (relative)"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_GD_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewST::configure_controls(void)
{
	bool value;

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_ST_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




void ProfileViewDT::configure_controls(void)
{
	bool value;

	this->show_gps_speed_cb = new QCheckBox(QObject::tr("Show GPS Speed (relative)"), this->dialog);
	if (ApplicationState::get_boolean(VIK_SETTINGS_TRACK_PROFILE_DT_SHOW_GPS_SPEED, &value)) {
		this->show_gps_speed_cb->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	}
	this->controls_vbox->addWidget(this->show_gps_speed_cb);
	QObject::connect(this->show_gps_speed_cb, SIGNAL (stateChanged(int)), this->dialog, SLOT (checkbutton_toggle_cb()));
}




ProfileViewBase::ProfileViewBase(GisViewportDomain new_x_domain, GisViewportDomain new_y_domain, TrackProfileDialog * new_dialog, QWidget * parent)
{
	this->widget = new QWidget(parent);
	this->widget->setProperty(MY_WIDGET_PROPERTY, QVariant::fromValue((qulonglong) this));
	this->dialog = new_dialog;

	this->main_values_valid_pen.setColor(dialog->trk->has_color ? dialog->trk->color : "blue");
	this->main_values_valid_pen.setWidth(1);

	this->main_values_invalid_pen.setColor(dialog->trk->color == "red" ? "black" : "red");
	this->main_values_invalid_pen.setWidth(1);

	this->aux_values_1_pen.setColor("green");
	this->aux_values_1_pen.setWidth(3);

	this->aux_values_2_pen.setColor("blue");
	this->aux_values_2_pen.setWidth(3);

	this->x_domain = new_x_domain;
	this->y_domain = new_y_domain;

	this->create_graph_2d();
	this->create_widgets_layout();
}




ProfileViewBase::~ProfileViewBase()
{
	/* TODO_MAYBE: delete ::widget? */
}




ProfileViewET::ProfileViewET(TrackProfileDialog * new_dialog) : ProfileView<Time, Altitude>(GisViewportDomain::TimeDomain, GisViewportDomain::ElevationDomain, new_dialog)
{
	this->title = QObject::tr("Elevation over time");
	this->configure_labels_x_time();
	this->configure_labels_y_altitude();
	this->configure_labels_post();
	this->configure_controls();
}
ProfileViewST::ProfileViewST(TrackProfileDialog * new_dialog) : ProfileView<Time, Speed>(GisViewportDomain::TimeDomain,    GisViewportDomain::SpeedDomain,     new_dialog)
{
	this->title = QObject::tr("Speed over time");
	this->configure_labels_x_time();
	this->configure_labels_y_speed();
	this->configure_labels_post();
	this->configure_controls();
}
ProfileViewDT::ProfileViewDT(TrackProfileDialog * new_dialog) : ProfileView<Time, Distance>(GisViewportDomain::TimeDomain, GisViewportDomain::DistanceDomain,  new_dialog)
{
	this->title = QObject::tr("Distance over time");
	this->configure_labels_x_time();
	this->configure_labels_y_distance();
	this->configure_labels_post();
	this->configure_controls();
}
ProfileViewSD::ProfileViewSD(TrackProfileDialog * new_dialog) : ProfileView<Distance, Speed>(GisViewportDomain::DistanceDomain,    GisViewportDomain::SpeedDomain,     new_dialog)
{
	this->title = QObject::tr("Speed over distance");
	this->configure_labels_x_distance();
	this->configure_labels_y_speed();
	this->configure_labels_post();
	this->configure_controls();
}
ProfileViewED::ProfileViewED(TrackProfileDialog * new_dialog) : ProfileView<Distance, Altitude>(GisViewportDomain::DistanceDomain, GisViewportDomain::ElevationDomain, new_dialog)
{
	this->title = QObject::tr("Elevation over distance");
	this->configure_labels_x_distance();
	this->configure_labels_y_altitude();
	this->configure_labels_post();
	this->configure_controls();
}
ProfileViewGD::ProfileViewGD(TrackProfileDialog * new_dialog) : ProfileView<Distance, Gradient>(GisViewportDomain::DistanceDomain, GisViewportDomain::GradientDomain,  new_dialog)
{
	this->title = QObject::tr("Gradient over distance");
	this->configure_labels_x_distance();
	this->configure_labels_y_gradient();
	this->configure_labels_post();
	this->configure_controls();
}




int ProfileViewBase::get_central_n_columns(void) const
{
	return this->graph_2d->cached_central_n_columns;
}




int ProfileViewBase::get_central_n_rows(void) const
{
	return this->graph_2d->cached_central_n_rows;
}




/**
   @reviewed-on tbd
*/
void Graph2D::mousePressEvent(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I << "Mouse CLICK event, button" << (int) ev->button();
	ev->accept();
}



/**
   @reviewed-on tbd
*/
void Graph2D::mouseMoveEvent(QMouseEvent * ev)
{
	//this->draw_mouse_motion_cb(ev);
	emit this->cursor_moved(this, ev);
	ev->accept();
}




/**
   @reviewed-on tbd
*/
void Graph2D::mouseReleaseEvent(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I << "called with button" << (int) ev->button();
	emit this->button_released(this, ev);
	ev->accept();
}
