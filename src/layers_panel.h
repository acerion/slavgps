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

#ifndef _SG_LAYERS_PANEL_H_
#define _SG_LAYERS_PANEL_H_




#include <QtWidgets>
#include <QWidget>
#include <QVBoxLayout>
#include <QToolBar>

#include "layer.h"




namespace SlavGPS {




	class LayerAggregate;
	class Window;




	class LayersPanel : public QWidget {
		Q_OBJECT
	public:

		LayersPanel(QWidget * parent, Window * window_);
		~LayersPanel();

		void add_layer(Layer * layer);
		void draw_all();
		Layer * get_selected_layer();

		Layer * get_layer_of_type(LayerType layer_type);


		bool new_layer(LayerType layer_type);
		void clear();

		void change_coord_mode(CoordMode mode);
		std::list<Layer const *> * get_all_layers_of_type(LayerType type, bool include_invisible);

		void set_visible(bool visible);
		bool get_visible(void);

		LayerAggregate * get_top_layer();
		TreeView * get_treeview();
		Window * get_window(void);
		Viewport * get_viewport();
		void set_viewport(Viewport * viewport);

		void contextMenuEvent(QContextMenuEvent * event);


		/* This should be somehow private. */
		void item_toggled(TreeIndex const & index);
		void item_edited(TreeIndex const & index, char const * new_text);
		bool key_press(QKeyEvent * ev);
		void move_item(bool up);
		bool button_press_cb(QMouseEvent * event);

	private:
		void show_context_menu(TreeIndex const & index, Layer * layer);
		void show_context_menu_layer_specific(TreeIndex const & index, Layer * layer);
		void show_context_menu_new_layer();
		QMenu * create_context_menu(uint16_t layer_menu_items);
		QMenu * add_submenu_new_layer(QMenu * menu);

		LayerAggregate * toplayer = NULL;
		TreeIndex toplayer_item;
		TreeView * tree_view = NULL;
		Window * window = NULL; /* Reference. */
		Viewport * viewport = NULL; /* Reference. */


		QAction * qa_layer_add = NULL;
		QAction * qa_layer_remove = NULL;
		QAction * qa_layer_move_up = NULL;
		QAction * qa_layer_move_down = NULL;
		QAction * qa_layer_cut = NULL;
		QAction * qa_layer_copy = NULL;
		QAction * qa_layer_paste = NULL;

		QVBoxLayout * panel_box = NULL;
		QToolBar * tool_bar = NULL;

	public slots:
		void add_layer_cb();
		bool properties_cb(void);
		void cut_selected_cb(void);
		void copy_selected_cb(void);
		bool paste_selected_cb(void);
		void delete_selected_cb(void);
		void emit_update_window_cb();

		void move_item_up_cb(void);
		void move_item_down_cb(void);

	signals:
		void update_window(void);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYERS_PANEL_H_ */
