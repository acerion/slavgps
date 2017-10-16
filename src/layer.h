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

#ifndef _SG_LAYER_H_
#define _SG_LAYER_H_




#include <cstdint>

#include <QObject>
#include <QIcon>
#include <QMouseEvent>
#include <QMenu>
#include <QString>
#include <QPen>

#include "tree_view.h"
#include "viewport.h"
#include "globals.h"
#include "ui_builder.h"
#include "variant.h"




namespace SlavGPS {




	class Window;
	class LayerTRW;
	class LayerTool;
	class LayerInterface;
	class LayersPanel;




	/* Which items shall be present in context menu for a layer? */
	enum class LayerMenuItem {
		NONE       = 0x0000,
		PROPERTIES = 0x0001,
		CUT        = 0x0002,
		COPY       = 0x0004,
		PASTE      = 0x0008,
		DELETE     = 0x0010,
		NEW        = 0x0020,
		ALL        = 0xffff,
	};
#if 0
	LayerMenuItem operator&(const LayerMenuItem& arg1, const LayerMenuItem& arg2);
	LayerMenuItem operator|(const LayerMenuItem& arg1, const LayerMenuItem& arg2);
	LayerMenuItem operator~(const LayerMenuItem& arg);
#endif



	typedef void (* LayerRefCB) (void * ptr, void * dead_vml);




	class Layer : public TreeItem {
		Q_OBJECT

	public:

		Layer();
		~Layer();

		static void    marshall(Layer * layer, uint8_t ** data, size_t * data_len);
		void           marshall_params(uint8_t ** data, size_t * data_len);
		static Layer * unmarshall(uint8_t * data, size_t data_len, Viewport * viewport);
		void           unmarshall_params(uint8_t * data, size_t data_len);

		static Layer * construct_layer(LayerType layer_type, Viewport * viewport, bool interactive = false);

		void emit_layer_changed(void);
		void emit_layer_changed_although_invisible(void);

		LayerInterface * get_interface(void);
		static LayerInterface * get_interface(LayerType layer_type);
		void configure_interface(LayerInterface * intf, Parameter * parameters);
		static void preconfigure_interfaces(void);

		/* Rarely used, this is called after a read operation
		   or properties box is run.  usually used to create
		   GC's that depend on params, but GC's can also be
		   created from create() or set_param(). */
		virtual void post_read(Viewport * viewport, bool from_file);

		virtual void draw(Viewport * viewport) { return; };
		void draw_if_visible(Viewport * viewport);

		virtual QString get_tooltip(void);

		bool handle_selection_in_tree(void);

		/* Methods for generic "Select" tool. */
		virtual bool handle_select_tool_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool)         { return false; };
		virtual bool handle_select_tool_double_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool)  { return false; };
		virtual bool handle_select_tool_move(QMouseEvent * event, Viewport * viewport, LayerTool * tool)          { return false; };
		virtual bool handle_select_tool_release(QMouseEvent * event, Viewport * viewport, LayerTool * tool)       { return false; };
		virtual bool handle_select_tool_context_menu(QMouseEvent * event, Viewport * viewport)                    { return false; };

		/* kamilTODO: consider removing them from Layer. They are overriden only in LayerTRW. */
		virtual void set_menu_selection(LayerMenuItem selection) { return; };
		virtual LayerMenuItem get_menu_selection(void) const { return LayerMenuItem::NONE; };

		virtual void cut_sublayer(TreeItem * item) { return; };
		virtual void copy_sublayer(TreeItem * item, uint8_t ** data, unsigned int * len) { return; };
		virtual bool paste_sublayer(TreeItem * item, uint8_t * data, size_t len) { return false; };
		virtual void delete_sublayer(TreeItem * item) { return; };

		virtual void change_coord_mode(CoordMode dest_mode) { return; };

		virtual time_t get_timestamp(void) const { return 0; };

		/* Treeview drag and drop method. called on the
		   destination layer. it is given a source and
		   destination layer, and the source and destination
		   iters in the tree view. */
		virtual void drag_drop_request(Layer * src, TreeIndex & src_item_index, void * GtkTreePath_dest_path);

		virtual int read_file(FILE * f, char const * dirpath);
		virtual void write_file(FILE * f) const;

		virtual void add_menu_items(QMenu & menu);

		virtual bool properties_dialog(Viewport * viewport);
		virtual bool properties_dialog();

		/* Get current, per-instance-of-layer, value of a layer parameter. The parameter is specified by its id.
		   @is_file_operation denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual SGVariant get_param_value(param_id_t id, bool is_file_operation) const;

		/* Returns true if needs to redraw due to changed param. */
		/* bool denotes if for file I/O, as opposed to display/cut/copy etc... operations. */
		virtual bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);


		/* "type id string" means Layer's internal, fixed string
		   that can be used in .vik file operations and to
		   create internal IDs of objects. */
		static LayerType type_from_type_id_string(const QString & type_id_string);
		static QString get_type_id_string(LayerType type);
		QString get_type_id_string(void) const;

		/* "type ui label" means human-readable label that is
		   suitable for using in UI or in debug messages. */
		static QString get_type_ui_label(LayerType type);
		QString get_type_ui_label(void) const;


		static bool compare_timestamp_descending(Layer * first, Layer * second); /* kamilTODO: make arguments const. */
		static bool compare_timestamp_ascending(Layer * first, Layer * second);

		static bool compare_name_descending(Layer * first, Layer * second); /* kamilTODO: make arguments const. */
		static bool compare_name_ascending(Layer * first, Layer * second);



		const QString get_name(void) const;
		void set_name(const QString & new_name);


		void set_initial_parameter_values(void);

		Window * get_window(void);

		void ref();
		void unref();
		void weak_ref(LayerRefCB cb, void * obj);
		void weak_unref(LayerRefCB cb, void * obj);

		bool the_same_object(const Layer * layer) const;

		/* GUI. */
		QIcon get_icon(void);
		LayerMenuItem get_menu_items_selection(void);

		/* QString name; */ /* Inherited from TreeItem. */

		LayerType type;

	protected:
		virtual void marshall(uint8_t ** data, size_t * data_len);

		LayerInterface * interface = NULL;
		QMenu * right_click_menu = NULL;

	public slots:
		void child_layer_changed_cb(void);


	protected slots:
		virtual void location_info_cb(void);

	signals:
		void layer_changed(void);

	private:
		sg_uid_t layer_instance_uid = SG_UID_INITIAL;
	};




	void layer_init(void);




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Layer*)




#endif /* #ifndef _SG_LAYER_H_ */
