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
 */

#ifndef _SG_UIBUILDER_H_
#define _SG_UIBUILDER_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdint>

#include <QString>

#include "globals.h"
#include "variant.h"




/* Parameters (for I/O and Properties) */
typedef int16_t param_id_t;




enum class WidgetType {
	CHECKBUTTON = 0,
	RADIOGROUP,
	RADIOGROUP_STATIC,
	SPINBOX_DOUBLE,
	SPINBUTTON,
	ENTRY,
	PASSWORD,
	FILEENTRY,
	FOLDERENTRY,
	HSCALE,
	COLOR,
	COMBOBOX,
	FILELIST,
	BUTTON,
	DATETIME,
	NONE
};


/* Default value has to be returned via a function
   because certain types value are can not be statically allocated
   (i.e. a string value that is dependent on other functions).
   Also easier for colours to be set via a function call rather than a static assignment. */
typedef SlavGPS::SGVariant (* LayerDefaultFunc) (void);

/* Convert between the value held internally and the value used for display
   e.g. keep the internal value in seconds yet use days in the display. */
typedef SlavGPS::SGVariant (* LayerConvertFunc) (SlavGPS::SGVariant);

typedef struct {
	//SlavGPS::LayerType layer_type;
	param_id_t id;
	const char *name;
	SlavGPS::SGVariantType type;
	int16_t group;
	const char *title;
	WidgetType widget_type;
	void * widget_data;
	void * extra_widget_data;
	const char *tooltip;
	LayerDefaultFunc hardwired_default_value; /* Program's internal, hardwired value that will be used if settings file doesn't contain a value for given parameter. */
	LayerConvertFunc convert_to_display;
	LayerConvertFunc convert_to_internal;
} Parameter;

enum {
	VIK_LAYER_NOT_IN_PROPERTIES=-2,
	VIK_LAYER_GROUP_NONE=-1,
	PARAMETER_GROUP_NONE=-1
};

typedef struct {
	double min;
	double max;
	double step;
	uint8_t digits;
} ParameterScale;


typedef enum {
	VL_SO_NONE = 0,
	VL_SO_ALPHABETICAL_ASCENDING,
	VL_SO_ALPHABETICAL_DESCENDING,
	VL_SO_DATE_ASCENDING,
	VL_SO_DATE_DESCENDING,
	VL_SO_LAST
} vik_layer_sort_order_t;




void uibuilder_run_setparam(SlavGPS::SGVariant * paramdatas, uint16_t i, SlavGPS::SGVariant data, Parameter * params);
SlavGPS::SGVariant uibuilder_run_getparam(SlavGPS::SGVariant * params_defaults, uint16_t i);
/* Frees data from last (if necessary). */
void a_uibuilder_free_paramdatas(SlavGPS::SGVariant * paramdatas, Parameter * params, uint16_t params_count);




typedef struct {
	char const * label;
	int32_t id;
} label_id_t;


#ifndef SLAVGPS_QT




typedef struct {
	void * layer;
	Parameter * param;
	int param_id;
	GtkWidget ** widgets;
	GtkWidget ** labels;
} ui_change_values;




GtkWidget *a_uibuilder_new_widget(LayerParam *param, SlavGPS::SGVariant data);
SlavGPS::SGVariant a_uibuilder_widget_get_value(GtkWidget *widget, LayerParam *param);
int a_uibuilder_properties_factory(const char *dialog_name,
				   Window * parent,
				   Parameter *params,
				   uint16_t params_count,
				   char **groups,
				   uint8_t groups_count,
				   bool (*setparam) (void *,uint16_t,SlavGPS::SGVariant,void *,bool), /* AKA LayerFuncSetParam in layer.h. */
				   void * pass_along1,
				   void * pass_along2,
				   SlavGPS::SGVariant (*getparam) (void *,uint16_t,bool),  /* AKA LayerFuncGetParam in layer.h. */
				   void * pass_along_getparam,
				   void (*changeparam) (GtkWidget*, ui_change_values *)); /* AKA LayerFuncChangeParam in layer.h. */
	/* pass_along1 and pass_along2 are for set_param first and last params. */

SlavGPS::SGVariant *a_uibuilder_run_dialog(const char *dialog_name, Window * parent, LayerParam *params,
				       uint16_t params_count, char **groups, uint8_t groups_count,
				       SlavGPS::SGVariant *params_defaults);




#endif




#endif /* #ifndef _SG_UIBUILDER_H_ */
