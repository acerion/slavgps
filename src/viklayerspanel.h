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

#ifndef _SG_LAYERS_PANEL_H_
#define _SG_LAYERS_PANEL_H_




#include <cstdint>

#ifdef SLAVGPS_QT
#include <QtWidgets>
#include <QWidget>
#include <QVBoxLayout>
#include <QToolBar>
#else
#include <gtk/gtk.h>
#endif

#include "viklayer.h"
#include "vikaggregatelayer.h"




namespace SlavGPS {




	class LayerAggregate;




#ifdef SLAVGPS_QT
	class LayersPanel : public QWidget {
		Q_OBJECT
#else
	class LayersPanel {
#endif
	public:

#ifdef SLAVGPS_QT
		LayersPanel(QWidget * parent);
#else
		LayersPanel();
#endif
		~LayersPanel();

		void add_layer(Layer * layer);
		void draw_all();
		Layer * get_selected();
		void cut_selected();
		void copy_selected();
		bool paste_selected();
		void delete_selected();
		Layer * get_layer_of_type(LayerType layer_type);
		void set_viewport(Viewport * viewport);
		Viewport * get_viewport();
		void emit_update();
		bool properties();
		bool new_layer(LayerType layer_type);
		void clear();
		LayerAggregate * get_top_layer();
		void change_coord_mode(VikCoordMode mode);
		std::list<Layer *> * get_all_layers_of_type(LayerType type, bool include_invisible);
		TreeView * get_treeview();

		void set_visible(bool visible);
		bool get_visible();

		GtkWindow * get_toolkit_window(void);
		GtkWidget * get_toolkit_widget(void);


		LayerAggregate * toplayer = NULL;
		QStandardItem * toplayer_item = NULL;

		TreeView * tree_view = NULL;
		Viewport * viewport = NULL; /* Reference. */



		/* This should be somehow private. */
		void item_toggled(GtkTreeIter * iter);
		void item_edited(GtkTreeIter * iter, char const * new_text);
		void popup(GtkTreeIter * iter, MouseButton mouse_button);
		bool button_press(GdkEventButton * event);
		bool key_press(GdkEventKey * event);
		void move_item(bool up);

#ifdef SLAVGPS_QT
		QVBoxLayout * panel_box_ = NULL;
		QToolBar * tool_bar_ = NULL;
#else
		GtkVBox * panel_box_ = NULL;
#endif
	};




	void layers_panel_init(void);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYERS_PANEL_H_ */
