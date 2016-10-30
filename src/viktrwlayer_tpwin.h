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
 *
 */

#ifndef _SG_LAYER_TRW_TPWIN_H_
#define _SG_LAYER_TRW_TPWIN_H_




#include <list>
#include <cstdint>

#include <QWidget>
#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>


#include "track.h"



#if 0
/* Response codes. */
#define VIK_TRW_LAYER_TPWIN_CLOSE    6
#define VIK_TRW_LAYER_TPWIN_INSERT   5
#define VIK_TRW_LAYER_TPWIN_DELETE   4
#define VIK_TRW_LAYER_TPWIN_SPLIT    3
#define VIK_TRW_LAYER_TPWIN_BACK     1
#define VIK_TRW_LAYER_TPWIN_FORWARD  0

#define VIK_TRW_LAYER_TPWIN_DATA_CHANGED 100

#define VIK_TRW_LAYER_TPWIN_TYPE            (vik_trw_layer_tpwin_get_type ())
#define VIK_TRW_LAYER_TPWIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TRW_LAYER_TPWIN_TYPE, VikTrwLayerTpwin))
#define VIK_TRW_LAYER_TPWIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TRW_LAYER_TPWIN_TYPE, VikTrwLayerTpwinClass))
#define IS_VIK_TRW_LAYER_TPWIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TRW_LAYER_TPWIN_TYPE))
#define IS_VIK_TRW_LAYER_TPWIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TRW_LAYER_TPWIN_TYPE))

typedef struct _VikTrwLayerTpwin VikTrwLayerTpwin;
typedef struct _VikTrwLayerTpwinClass VikTrwLayerTpwinClass;

struct _VikTrwLayerTpwinClass
{
	GtkDialogClass vik_trw_layer_class;
};

GType vik_trw_layer_tpwin_get_type ();

VikTrwLayerTpwin * vik_trw_layer_tpwin_new(GtkWindow * parent);
void vik_trw_layer_tpwin_set_empty(VikTrwLayerTpwin * tpwin);
void vik_trw_layer_tpwin_disable_join(VikTrwLayerTpwin * tpwin);

#endif




namespace SlavGPS {




	class PropertiesDialogTP : public QDialog {
	public:
		PropertiesDialogTP();
		PropertiesDialogTP(QWidget * parent);
		~PropertiesDialogTP();

		void set_tp(Track * list, std::list<Trackpoint *>::iterator * iter, char const * track_name, bool is_route);
		void set_track_name(char const * track_name);

	private:

		void update_times(Trackpoint * tp);

		Trackpoint * cur_tp = NULL;
		bool sync_to_tp_block = false;

		QDialogButtonBox * button_box = NULL;

		QPushButton * button_close = NULL;
		QPushButton * button_insert_after = NULL;
		QPushButton * button_delete = NULL;
		QPushButton * button_split_here = NULL;
		QPushButton * button_back = NULL;
		QPushButton * button_forward = NULL;

		QVBoxLayout * vbox = NULL;
		QHBoxLayout * hbox = NULL;

		QWidget * left_area = NULL;
		QWidget * right_area = NULL;

		QLineEdit * trkpt_name = NULL;
		QDoubleSpinBox * lat = NULL;
		QDoubleSpinBox * lon = NULL;
		QDoubleSpinBox * alt = NULL;
		QLabel * course = NULL;
		QSpinBox * timestamp = NULL;

		QLabel * diff_dist = NULL;
		QLabel * diff_time = NULL;
		QLabel * diff_speed = NULL;
		QLabel * speed = NULL;
		QLabel * vdop = NULL;
		QLabel * hdop = NULL;
		QLabel * pdop = NULL;
		QLabel * sat = NULL;
	};




}




#endif /* #ifndef _SG_LAYER_TRW_TPWIN_H_ */
