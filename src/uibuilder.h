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
#ifndef _SG_UIBUILDER_H_
#define _SG_UIBUILDER_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <list>
#include <cstdint>

#include "globals.h"


#ifndef SLAVGPS_QT
#include <gtk/gtk.h>
#endif


/* Parameters (for I/O and Properties) */

typedef union {
	double d;
	uint32_t u;
	int32_t i;
	bool b;
	const char *s;
	struct { int r; int g; int b; int a; } c;
	std::list<char *> * sl;
	void * ptr; // For internal usage - don't save this value in a file!
} LayerParamData;


typedef LayerParamData LayerParamValue;



enum class LayerWidgetType {
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
};


/* id is index. */
enum class LayerParamType {
	DOUBLE = 1,
	UINT,
	INT,

	/* In my_layer_set_param, if you want to use the string, you should dup it.
	 * In my_layer_get_param, the string returned will NOT be free'd, you are responsible for managing it (I think). */
	STRING,
	BOOLEAN,
	COLOR,

	/* NOTE: string list works uniquely: data.sl should NOT be free'd when
	 * the internals call get_param -- i.e. it should be managed w/in the layer.
	 * The value passed by the internals into set_param should also be managed
	 * by the layer -- i.e. free'd by the layer.
	 */

	STRING_LIST,
	PTR, /* Not really a 'parameter' but useful to route to extended configuration (e.g. toolbar order). */
};


/* Default value has to be returned via a function
   because certain types value are can not be statically allocated
   (i.e. a string value that is dependent on other functions).
   Also easier for colours to be set via a function call rather than a static assignment. */
typedef LayerParamData (* LayerDefaultFunc) (void);

/* Convert between the value held internally and the value used for display
   e.g. keep the internal value in seconds yet use days in the display. */
typedef LayerParamData (* LayerConvertFunc) (LayerParamData);

typedef struct {
	SlavGPS::LayerType layer_type;
	int16_t id;
	const char *name;
	LayerParamType type;
	int16_t group;
	const char *title;
	LayerWidgetType widget_type;
	void * widget_data;
	void * extra_widget_data;
	const char *tooltip;
	LayerDefaultFunc default_value;
	LayerConvertFunc convert_to_display;
	LayerConvertFunc convert_to_internal;
} LayerParam;

enum {
	VIK_LAYER_NOT_IN_PROPERTIES=-2,
	VIK_LAYER_GROUP_NONE=-1
};

typedef struct {
	double min;
	double max;
	double step;
	uint8_t digits;
} LayerParamScale;


typedef enum {
	VL_SO_NONE = 0,
	VL_SO_ALPHABETICAL_ASCENDING,
	VL_SO_ALPHABETICAL_DESCENDING,
	VL_SO_DATE_ASCENDING,
	VL_SO_DATE_DESCENDING,
	VL_SO_LAST
} vik_layer_sort_order_t;



/* Annoyingly 'C' cannot initialize unions properly. */
/* It's dependent on the standard used or the compiler support... */
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L || __GNUC__
#define VIK_LPD_BOOLEAN(X)     (LayerParamData) { .b = (X) }
#define VIK_LPD_INT(X)         (LayerParamData) { .u = (X) }
#define VIK_LPD_UINT(X)        (LayerParamData) { .i = (X) }
#define VIK_LPD_COLOR(X,Y,Z,A) (LayerParamData) { .c = { (X), (Y), (Z), (A) } }
#define VIK_LPD_DOUBLE(X)      (LayerParamData) { .d = (X) }
#else
#define VIK_LPD_BOOLEAN(X)     (LayerParamData) { (X) }
#define VIK_LPD_INT(X)         (LayerParamData) { (X) }
#define VIK_LPD_UINT(X)        (LayerParamData) { (X) }
#define VIK_LPD_COLOR(X,Y,Z,A) (LayerParamData) { (X), (Y), (Z), (A) }
#define VIK_LPD_DOUBLE(X)      (LayerParamData) { (X) }
#endif



LayerParamData vik_lpd_true_default(void);
LayerParamData vik_lpd_false_default(void);
void uibuilder_run_setparam(LayerParamData * paramdatas, uint16_t i, LayerParamData data, LayerParam * params);
LayerParamData uibuilder_run_getparam(LayerParamData * params_defaults, uint16_t i);
/* Frees data from last (if necessary). */
void a_uibuilder_free_paramdatas(LayerParamData * paramdatas, LayerParam * params, uint16_t params_count);




#ifndef SLAVGPS_QT




typedef struct {
	void * layer;
	LayerParam * param;
	int param_id;
	GtkWidget ** widgets;
	GtkWidget ** labels;
} ui_change_values;




GtkWidget *a_uibuilder_new_widget(LayerParam *param, LayerParamData data);
LayerParamData a_uibuilder_widget_get_value(GtkWidget *widget, LayerParam *param);
int a_uibuilder_properties_factory(const char *dialog_name,
				   GtkWindow *parent,
				   LayerParam *params,
				   uint16_t params_count,
				   char **groups,
				   uint8_t groups_count,
				   bool (*setparam) (void *,uint16_t,LayerParamData,void *,bool), /* AKA LayerFuncSetParam in layer.h. */
				   void * pass_along1,
				   void * pass_along2,
				   LayerParamData (*getparam) (void *,uint16_t,bool),  /* AKA LayerFuncGetParam in layer.h. */
				   void * pass_along_getparam,
				   void (*changeparam) (GtkWidget*, ui_change_values *)); /* AKA LayerFuncChangeParam in layer.h. */
	/* pass_along1 and pass_along2 are for set_param first and last params. */

LayerParamData *a_uibuilder_run_dialog(const char *dialog_name, GtkWindow *parent, LayerParam *params,
				       uint16_t params_count, char **groups, uint8_t groups_count,
				       LayerParamData *params_defaults);




#endif




#endif /* #ifndef _SG_UIBUILDER_H_ */
