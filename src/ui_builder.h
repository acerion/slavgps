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
#include <vector>
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
#include <QHash>

#include "globals.h"
#include "variant.h"




#define SMALL_ICON_SIZE 18




namespace SlavGPS {




	class Layer;
	class LayerInterface;
	class Waypoint;
	class Preferences;




	enum class WidgetType {
		CheckButton = 0,
		RadioGroup,
		SpinBoxDouble,  /* SGVariantType::Double */
		SpinBoxInt,     /* SGVariantType::Int */
		Entry,
		Password,
		FileEntry,
		FolderEntry,
		HScale,         /* SGVariantType::Double or SGVariantType::Int */
		Color,
		ComboBox,       /* SGVariantType::String or SGVariantType::Int */
		FileList,
		DateTime,
		None
	};

	QString widget_type_get_label(WidgetType type_id);




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
		void * extra_widget_data; /* Even more widget data, in addition to ParameterSpecification::widget_data. */
	} ParameterExtra;

	class ParameterSpecification {
	public:
		param_id_t id;
		const char * name_space;
		const char * name;
		SGVariantType type_id;
		param_id_t group_id; /* Every parameter belongs to a group of related parameters. Related parameters are put into the same tab in UI dialog. */
		QString ui_label;
		WidgetType widget_type;
		void * widget_data;

		LayerDefaultFunc hardwired_default_value; /* Program's internal, hardwired value that will be used if settings file doesn't contain a value for given parameter. */
		ParameterExtra * extra;
		const char * tooltip;

		ParameterSpecification & operator=(const ParameterSpecification & other);
		bool get_hardwired_value(SGVariant & value) const;
	};




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


	void uibuilder_run_setparam(SGVariant * paramdatas, uint16_t i, SGVariant data, ParameterSpecification * param_specs);
	SGVariant uibuilder_run_getparam(SGVariant * params_defaults, uint16_t i);




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
		void fill(Waypoint * wp, const std::vector<const ParameterSpecification *> & param_specs, const QString & default_name);

		SGVariant get_param_value(param_id_t param_id, const ParameterSpecification & param_spec);
		SGVariant get_param_value(const QString & param_name, const ParameterSpecification & param_spec);

		/* Referencing parameters (and widgets that are
		   related to the parameters) can be done through
		   either parameter name (QString) or parameter id
		   (param_id_t). */
		QHash<QString, QWidget *> widgets2;
		std::map<param_id_t, QWidget *> widgets;

	private:
		QWidget * new_widget(const ParameterSpecification & param_spec, const SGVariant & param_value);

		QFormLayout * insert_tab(QString const & label);
		std::map<param_id_t, ParameterSpecification *>::iterator add_widgets_to_tab(QFormLayout * form, Layer * layer, std::map<param_id_t, ParameterSpecification *>::iterator & iter, std::map<param_id_t, ParameterSpecification *>::iterator & end);

		SGVariant get_param_value_from_widget(QWidget * widget, const ParameterSpecification & param_spec);

		QDialogButtonBox * button_box = NULL;
		QPushButton * ok = NULL;
		QPushButton * cancel = NULL;
		QVBoxLayout * vbox = NULL;

		std::map<param_id_t, QFormLayout *> forms;

		QTabWidget * tabs = NULL;
	};




}




#endif /* #ifndef _SG_UI_BUILDER_H_ */
