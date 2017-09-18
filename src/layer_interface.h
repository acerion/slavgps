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

#ifndef _SG_LAYER_INTERFACE_H_
#define _SG_LAYER_INTERFACE_H_




#include <map>
#include <cstdint>

#include <QIcon>
#include <QMouseEvent>
#include <QMenu>
#include <QString>
#include <QPen>

#include "tree_view.h"
#include "viewport.h"
#include "globals.h"
#include "vikutils.h"




namespace SlavGPS {




	class Layer;
	class Viewport;
	class LayerTool;
	enum class LayerMenuItem;




	typedef std::map<QString, LayerTool *, qstring_compare> LayerToolContainer;




	class LayerInterface {

	friend class Layer;

	public:
		LayerInterface() {};

		virtual Layer * unmarshall(uint8_t * data, int len, Viewport * viewport) { return NULL; };
		virtual void change_param(void * gtk_widget, ui_change_values *) { };

		QKeySequence action_accelerator;
		QIcon action_icon;

		/* Create a container with layer-type-specific tools.
		   The container is owned by caller.
		   Pointers in the container are owned by caller.

		   By default a layer-type has no layer-specific tools. */
		virtual LayerToolContainer * create_tools(Window * window, Viewport * viewport) { return NULL; };

		/* Menu items (actions) to be created and put into a
		   context menu for given layer type. */
		LayerMenuItem menu_items_selection = LayerMenuItem::NONE;


		/* Specification of parameters in each layer type is
		   stored in 'parameters_c' C array.  During
		   application startup, in Layer::preconfigure_interfaces(),
		   pointers to these parameters in C array are stored
		   in 'parameters' container. The parameters can be later
		   accessed in C++-like fashion.

		   Each layer type stores (here, in layer interface) a
		   set of default values of parameters, to be used
		   when user creates a new instance of layer of type X.

		   Parameters can be combined into groups, they names
		   of the groups are in parameter_groups. */
		Parameter * parameters_c = NULL;
		std::map<param_id_t, Parameter *> parameters;
		std::map<param_id_t, SGVariant>  parameter_default_values;
		const char ** parameter_groups = NULL;

		struct {
			QString new_layer;      /* Menu "Layers" -> "New type-X Layer". */
			QString layer_type;     /* Stand-alone label for layer's type. Not to be concatenated with other string to form longer labels. */
			QString layer_defaults; /* Title of "Default settings of layer type X" dialog window. */
		} ui_labels;

	protected:
		QString fixed_layer_type_string; /* Used in .vik files - this should never change to maintain file compatibility. */
	};




} /* namespace SlavGPS */





#endif /* #ifndef _SG_LAYER_INTERFACE_H_ */
