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

#ifndef _SG_UI_BUILDER_H_
#define _SG_UI_BUILDER_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <map>
#include <cstdint>

#include <QObject>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QToolButton>
#include <QString>
#include <QTabWidget>
#include <QVector>

#include "globals.h"
#include "variant.h"
//#include "preferences.h"




namespace SlavGPS {




	class Layer;
	class LayerInterface;
	class Waypoint;
	class Preferences;




	typedef int GtkWidget; /* TODO: remove sooner or later. */

	typedef int16_t param_id_t; /* This shall be a signed type. */




	enum class WidgetType {
		CHECKBUTTON = 0,
		RADIOGROUP,
		SPINBOX_DOUBLE,  /* SGVariantType::DOUBLE */
		SPINBOX_INT,     /* SGVariantType::INT */
		ENTRY,
		PASSWORD,
		FILEENTRY,
		FOLDERENTRY,
		HSCALE,          /* SGVariantType::DOUBLE or SGVariantType::INT */
		COLOR,
		COMBOBOX,        /* SGVariantType::STRING or SGVariantType::INT */
		FILELIST,
		BUTTON,
		DATETIME,
		NONE
	};


	/* Default value has to be returned via a function
	   because certain types value are can not be statically allocated
	   (i.e. a string value that is dependent on other functions).
	   Also easier for colors to be set via a function call rather than a static assignment. */
	typedef SGVariant (* LayerDefaultFunc) (void);

	/* Convert between the value held internally and the value used for display
	   e.g. keep the internal value in seconds yet use days in the display. */
	typedef SGVariant (* LayerConvertFunc) (SGVariant);


	typedef struct ParameterExtra_e {
		LayerConvertFunc convert_to_display;
		LayerConvertFunc convert_to_internal;
		void * extra_widget_data; /* Even more widget data, in addition to Parameter::widget_data. */
	} ParameterExtra;

	typedef struct {
		// LayerType layer_type;
		param_id_t id;
		const char *name;
		SGVariantType type;
		param_id_t group_id; /* Every parameter belongs to a group of related parameters. Related parameters are put into the same tab in UI dialog. */
		const char *title;
		WidgetType widget_type;
		void * widget_data;

		LayerDefaultFunc hardwired_default_value; /* Program's internal, hardwired value that will be used if settings file doesn't contain a value for given parameter. */
		ParameterExtra * extra;
		const char *tooltip;
	} Parameter;

	enum {
		PARAMETER_GROUP_HIDDEN  = -2,  /* This parameter won't be displayed in UI. */
		PARAMETER_GROUP_GENERIC = -1   /* All parameters in given module are in one category, so there is no point in creating more than one distinct group. There is only one group. */
	};

	typedef struct {
		double min;
		double max;
		SGVariant initial;
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
	} sort_order_t;




	void uibuilder_run_setparam(SGVariant * paramdatas, uint16_t i, SGVariant data, Parameter * params);
	SGVariant uibuilder_run_getparam(SGVariant * params_defaults, uint16_t i);
	/* Frees data from last (if necessary). */
	void a_uibuilder_free_paramdatas(SGVariant * paramdatas, Parameter * params, uint16_t params_count);

	bool parameter_get_hardwired_value(SGVariant & value, const Parameter & param);




	class SGLabelID {
	public:
		SGLabelID(const QString & label_, int id_) { label = label_; id = id_; }
		QString label;
		int id;
	};




	class PropertiesDialog : public QDialog {
	public:
		PropertiesDialog(QString const & title = "Properties", QWidget * parent = NULL);
		~PropertiesDialog();

		void fill(Preferences * preferences);
		void fill(Layer * layer);
		void fill(LayerInterface * interface);
		void fill(Waypoint * wp, Parameter * parameters, const QString & default_name);

		SGVariant get_param_value(param_id_t id, Parameter * param);

	private:
		QWidget * new_widget(Parameter * param, SGVariant param_value);

		QFormLayout * insert_tab(QString const & label);
		std::map<param_id_t, Parameter *>::iterator add_widgets_to_tab(QFormLayout * form, Layer * layer, std::map<param_id_t, Parameter *>::iterator & iter, std::map<param_id_t, Parameter *>::iterator & end);

		QDialogButtonBox * button_box = NULL;
		QPushButton * ok = NULL;
		QPushButton * cancel = NULL;
		QVBoxLayout * vbox = NULL;

		std::map<param_id_t, QWidget *> widgets;
		std::map<param_id_t, QFormLayout *> forms;

		QTabWidget * tabs = NULL;
	};




	typedef struct {
		void * layer;
		Parameter * param;
		int param_id;
		GtkWidget ** widgets;
		GtkWidget ** labels;
	} ui_change_values;




#ifdef K
	GtkWidget *a_uibuilder_new_widget(Parameter *param, SGVariant data);
	SGVariant a_uibuilder_widget_get_value(GtkWidget *widget, Parameter *param);
	int a_uibuilder_properties_factory(const char *dialog_name,
					   Window * parent,
					   Parameter *params,
					   uint16_t params_count,
					   char **groups,
					   uint8_t groups_count,
					   bool (*setparam) (void *,uint16_t, SGVariant,void *,bool), /* AKA LayerFuncSetParam in layer.h. */
					   void * pass_along1,
					   void * pass_along2,
					   SGVariant (*getparam) (void *,uint16_t,bool),  /* AKA LayerFuncGetParam in layer.h. */
					   void * pass_along_getparam,
					   void (*changeparam) (GtkWidget*, ui_change_values *)); /* AKA LayerFuncChangeParam in layer.h. */
	/* pass_along1 and pass_along2 are for set_param first and last params. */

	SGVariant *a_uibuilder_run_dialog(const char *dialog_name, Window * parent, Parameter *params,
					  uint16_t params_count, char **groups, uint8_t groups_count,
					  SGVariant *params_defaults);
#endif




}




#endif /* #ifndef _SG_UI_BUILDER_H_ */
