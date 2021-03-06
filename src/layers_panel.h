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




#include <QWidget>
#include <QVBoxLayout>
#include <QToolBar>




#include "globals.h"




namespace SlavGPS {




	class LayerAggregate;
	class Window;
	class GisViewport;
	class TreeView;
	class TreeItem;
	class Layer;
	class StandardMenuOperations;
	enum class LayerKind;
	enum class CoordMode;




	class LayersPanel : public QWidget {
		Q_OBJECT
	public:

		LayersPanel(QWidget * parent, Window * window);
		~LayersPanel();

		void add_layer(Layer * layer, const CoordMode & viewport_coord_mode);
		void draw_tree_items(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);

		/* If a layer is selected, get the layer.
		   If a sublayer is selected, get the sublayer's owning/parent layer. */
		Layer * selected_layer(void) const;

		Layer * get_layer_of_kind(LayerKind layer_kind);


		/**
		   \brief Remove all layers
		*/
		void clear(void);

		void change_coord_mode(CoordMode mode);
		std::list<const Layer *> get_all_layers_of_kind(LayerKind layer_kind, bool include_invisible) const;
		bool has_any_layer_of_kind(LayerKind layer_kind);

		/**
		   @brief Change visibility of layers panel
		*/
		void set_visibility(bool visible);

		/**
		   @brief Get visibility of layers panel
		*/
		bool visibility(void) const;

		LayerAggregate * top_layer(void) const;
		TreeView * tree_view(void) const;

		sg_ret context_menu_add_standard_operations(QMenu & menu, const StandardMenuOperations & ops);

		void contextMenuEvent(QContextMenuEvent * event);
		void keyPressEvent(QKeyEvent * event);


	private:
		void context_menu_show_for_new_layer();
		void context_menu_add_new_layer_submenu(QMenu & menu);


		Layer * go_up_to_layer(const TreeItem * tree_item, LayerKind layer_kind);

		void move_item(bool up);

		LayerAggregate * m_toplayer = nullptr;
		TreeView * m_tree_view = nullptr;
		Window * m_window = nullptr; /* Reference. */


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
		void cut_selected_cb(void);
		void copy_selected_cb(void);
		bool paste_selected_cb(void);
		void delete_selected_cb(void);
		void emit_items_tree_updated_cb(const QString & trigger_name);

		void move_item_up_cb(void);
		void move_item_down_cb(void);

		void activate_buttons_cb(void);

	signals:
		void items_tree_updated(void);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYERS_PANEL_H_ */
