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

#ifndef _VIKING_LAYERS_PANEL_H
#define _VIKING_LAYERS_PANEL_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <stdbool.h>
#include <stdint.h>


#include "viklayer.h"
#include "vikaggregatelayer.h"


struct _VikLayersPanel;


namespace SlavGPS {

	class LayersPanel {
	public:

		VikAggregateLayer *toplayer;
		GtkTreeIter toplayer_iter;

		VikTreeview *vt;
		VikViewport * vvp; /* reference */

		_VikLayersPanel * gob;
	};



}

#ifdef __cplusplus
extern "C" {
#endif


#define VIK_LAYERS_PANEL_TYPE            (vik_layers_panel_get_type ())
#define VIK_LAYERS_PANEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_LAYERS_PANEL_TYPE, VikLayersPanel))
#define VIK_LAYERS_PANEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_LAYERS_PANEL_TYPE, VikLayersPanelClass))
#define IS_VIK_LAYERS_PANEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_LAYERS_PANEL_TYPE))
#define IS_VIK_LAYERS_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_LAYERS_PANEL_TYPE))

struct _VikLayersPanel {
	GtkVBox vbox;

	LayersPanel panel;


	LayersPanel * panel_ref;

	_VikLayersPanel * gob;
};

typedef struct _VikLayersPanel VikLayersPanel;
typedef struct _VikLayersPanelClass VikLayersPanelClass;

struct _VikLayersPanelClass
{
	GtkVBoxClass vbox_class;

	void (* update) (VikLayersPanel *vlp);
	void (* delete_layer) (VikLayersPanel *vlp); // NB Just before (actual layer *not* specified ATM) it is deleted
};

GType vik_layers_panel_get_type();
VikLayersPanel *vik_layers_panel_new();
void vik_layers_panel_free(VikLayersPanel *vlp);
void vik_layers_panel_add_layer(LayersPanel * panel, VikLayer *l);
void vik_layers_panel_draw_all(LayersPanel * panel);
VikLayer *vik_layers_panel_get_selected(LayersPanel * panel);
void vik_layers_panel_cut_selected(LayersPanel * panel);
void vik_layers_panel_copy_selected(LayersPanel * panel);
bool vik_layers_panel_paste_selected(LayersPanel * panel);
void vik_layers_panel_delete_selected(LayersPanel * panel);
VikLayer *vik_layers_panel_get_layer_of_type(LayersPanel * panel, VikLayerTypeEnum type);
void vik_layers_panel_set_viewport(LayersPanel * layers_panel, VikViewport *vvp);
//bool vik_layers_panel_tool(VikLayersPanel *vlp, uint16_t layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp);
VikViewport * vik_layers_panel_get_viewport(LayersPanel * panel);
void vik_layers_panel_emit_update(LayersPanel * panel);
bool vik_layers_panel_properties(LayersPanel * panel);
bool vik_layers_panel_new_layer(LayersPanel * panel, VikLayerTypeEnum type);
void vik_layers_panel_clear(LayersPanel * panel);
VikAggregateLayer *vik_layers_panel_get_top_layer(LayersPanel * panel);
void vik_layers_panel_change_coord_mode(LayersPanel * panel, VikCoordMode mode);
GList *vik_layers_panel_get_all_layers_of_type(LayersPanel * panel, int type, bool include_invisible);
VikTreeview *vik_layers_panel_get_treeview(LayersPanel * panel);

#ifdef __cplusplus
}
#endif

#endif
