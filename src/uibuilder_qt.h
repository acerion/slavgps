/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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
#ifndef _SG_UIBUILDER_QT_H_
#define _SG_UIBUILDER_QT_H_


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <list>
#include <map>
#include <cstdint>

#include <QObject>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QToolButton>

#include <QTabWidget>

#include <QVector>

#include "globals.h"
#include "uibuilder.h"
#include "layer.h"




namespace SlavGPS {




	class Layer;




	class LayerPropertiesDialog : public QDialog {
	public:
		LayerPropertiesDialog(QWidget * parent);
		~LayerPropertiesDialog();

		void fill(LayerParam * params, uint16_t params_count);
		void fill(Layer * layer);

		LayerParamValue get_param_value(layer_param_id_t id, LayerParam * param);




	private:
		QWidget * new_widget(LayerParam * param, LayerParamValue param_value);

		QFormLayout * insert_tab(QString & label);
		uint16_t add_widgets_to_tab(QFormLayout * form, LayerParam * params, uint16_t start);
		std::map<layer_param_id_t, LayerParam *>::iterator add_widgets_to_tab(QFormLayout * form, Layer * layer, std::map<layer_param_id_t, LayerParam *>::iterator & iter, std::map<layer_param_id_t, LayerParam *>::iterator & end);

		QDialogButtonBox * button_box = NULL;
		QPushButton * ok = NULL;
		QPushButton * cancel = NULL;
		QVBoxLayout * vbox = NULL;

		std::map<layer_param_id_t, QWidget *> widgets;

		QTabWidget * tabs = NULL;
	};




}




#endif /* #ifndef _SG_UIBUILDER_QT_H_ */
