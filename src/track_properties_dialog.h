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

#ifndef _SG_TRACK_PROPERTIES_DIALOG_H_
#define _SG_TRACK_PROPERTIES_DIALOG_H_




#include <QObject>
#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QComboBox>
#include <QFormLayout>

#include "widget_color_button.h"




namespace SlavGPS {




	class Window;
	class LayerTRW;
	class Track;




	class TrackPropertiesDialog : public QDialog {
		Q_OBJECT
	public:
		TrackPropertiesDialog() {};
		TrackPropertiesDialog(QString const & title, LayerTRW * a_layer, Track * a_trk, bool start_on_stats, Window * a_parent = NULL);
		~TrackPropertiesDialog() {};

		void create_properties_page(void);
		void create_statistics_page(void);

	private slots:
		void dialog_accept_cb(void);

	private:
		LayerTRW * trw = NULL;
		Track * trk = NULL;

		QTabWidget * tabs = NULL;
		QFormLayout * properties_form = NULL;
		QFormLayout * statistics_form = NULL;
		QWidget * properties_area = NULL;
		QWidget * statistics_area = NULL;

		QDialogButtonBox * button_box = NULL;

		QPushButton * button_ok = NULL;
		QPushButton * button_cancel = NULL;

		QVBoxLayout * vbox = NULL;

		/* Track properties. */
		QLineEdit * w_comment = NULL;
		QLineEdit * w_description = NULL;
		QLineEdit * w_source = NULL;
		QLineEdit * w_type = NULL;
		SGColorButton * w_color = NULL;
		QComboBox * w_namelabel = NULL;
		QSpinBox * w_number_distlabels = NULL;

		/* Track statistics. */
		QLabel * w_track_length = NULL;
		QLabel * w_tp_count = NULL;
		QLabel * w_segment_count = NULL;
		QLabel * w_duptp_count = NULL;
		QLabel * w_max_speed = NULL;
		QLabel * w_avg_speed = NULL;
		QLabel * w_mvg_speed = NULL;
		QLabel * w_avg_dist = NULL;
		QLabel * w_elev_range = NULL;
		QLabel * w_elev_gain = NULL;
		QLabel * w_time_start = NULL;
		QLabel * w_time_end = NULL;
		QLabel * w_time_dur = NULL;

		double   track_length;
		double   track_length_inc_gaps;

		char     * tz = NULL; /* TimeZone at track's location. */
	};




	void track_properties_dialog(Window * parent, LayerTRW * layer, Track * trk, bool start_on_stats = false);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_PROPERTIES_DIALOG_H_ */
