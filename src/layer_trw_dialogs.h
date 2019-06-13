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
#ifndef _SG_LAYER_TRW_DIALOGS_
#define _SG_LAYER_TRW_DIALOGS_




#include <cstdint>




#include <QDialog>
#include <QString>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QComboBox>




#include "widget_radio_group.h"
#include "dialog.h"




namespace SlavGPS {




	class VikingScale;




	QString a_dialog_new_track(const QString & default_name, bool is_route, QWidget * parent);
	bool a_dialog_time_threshold(const QString & title, const QString & label, uint32_t * thr, QWidget * parent = NULL);
	bool a_dialog_map_and_zoom(const QStringList & map_labels, unsigned int default_map_idx, const std::vector<VikingScale> & viking_scales, unsigned int default_zoom_idx, unsigned int * selected_map_idx, unsigned int * selected_zoom_idx, QWidget * parent);




	class MapAndZoomDialog : public BasicDialog {
		Q_OBJECT
	public:
		MapAndZoomDialog() {};
		MapAndZoomDialog(const QString & title, const QStringList & map_labels, const std::vector<VikingScale> & viking_scaless, QWidget * parent = NULL);

		void preselect(int map_idx, int zoom_idx);
		int get_map_idx(void) const;
		int get_zoom_idx(void) const;

	private:
		QComboBox * map_combo = NULL;
		QComboBox * zoom_combo = NULL;
	};




	class TimeThresholdDialog : public QDialog {
		Q_OBJECT
	public:
		TimeThresholdDialog() {};
		TimeThresholdDialog(const QString & title, const QString & label, uint32_t custom_threshold, QWidget * parent = NULL);
		~TimeThresholdDialog() {};

		void get_value(uint32_t * custom_threshold);

	private slots:
		void spin_changed_cb(int unused);

	private:
		QDialogButtonBox button_box;
		QSpinBox custom_spin;
		QVBoxLayout * vbox = NULL;
		RadioGroupWidget * radio_group = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_DIALOGS_ */
