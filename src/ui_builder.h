/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2016 - 2019 Kamil Ignacak <acerion@wp.pl>
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
	class Preferences;




	enum class WidgetType {
		None = 0,

		CheckButton,
		RadioGroup,     /* SGVariantType::Enumeration. */
		SpinBoxDouble,  /* SGVariantType::Double */
		SpinBoxInt,     /* SGVariantType::Int */
		Entry,
		Password,
		FileSelector,
		FolderEntry,
		HScale,         /* SGVariantType::Double or SGVariantType::Int */
		Color,
		FileList,
		DateTime,
		DurationType,   /* SGVariantType::DurationType. */

		/*
		  For list of unique strings. These strings can be
		  directly shown to user in the widget.
		  Examples:
		  - list of serial line devices in /dev/,
		  - list of Routing engine names or identifiers,
		  - list of gps device protocols.
		*/
		StringEnumeration,    /* Corresponding variant type: SGVariantType::String. */

		/*
		  For list of unique integers. Such integers can't be
		  directly shown to user (because integers themselves
		  aren't user-friendly), and have to be accompanied by
		  user-friendly text labels.

		  Examples:
		  - list of Speed units,
		  - list of Degrees formats,
		  - list of shapes of Waypoint symbols.
		*/
		IntEnumeration,    /* Corresponding variant type: SGVariantType::Enumeration. */

		Latitude,
		Longitude,
		AltitudeWidget,
		ImageAlphaWidget
	};

	QString widget_type_get_label(WidgetType type_id);




	typedef SGVariant (* ParameterFunc) (void);

	class ParameterSpecification {
	public:
		param_id_t id;
		QString name;
		SGVariantType type_id;

		/* Every parameter belongs to a group of related
		   parameters. Related parameters are put into the
		   same tab in UI dialog. */
		param_id_t group_id;

		QString ui_label;
		WidgetType widget_type;
		void * widget_data;

		/* Program's internal, hardcoded value that will be
		   used if settings file doesn't contain a value for
		   given parameter.

		   Default value has to be returned via a function
		   because certain types value are can not be
		   statically allocated (i.e. a string value that is
		   dependent on other functions).  Also easier for
		   colors to be set via a function call rather than a
		   static assignment. */
		ParameterFunc hardcoded_default_value;

		QString tooltip;

		ParameterSpecification & operator=(const ParameterSpecification & other);
		SGVariant get_hardcoded_value(void) const;
	};




	enum {
		PARAMETER_GROUP_HIDDEN  = -2,  /* This parameter won't be displayed in UI. */
		PARAMETER_GROUP_GENERIC = -1   /* All parameters in given module are in one category, so there is no point in creating more than one distinct group. There is only one group. */
	};


	template <class T>
	class ParameterScale {
	public:
		ParameterScale(const T & new_min, const T & new_max, const SGVariant & new_initial, const T & new_step, int new_n_digits) :
			min(new_min), max(new_max), initial(new_initial), step(new_step), n_digits(new_n_digits) {}

		bool is_in_range(const T & value) const { return (value >= this->min && value <= this->max); }

		T min;
		T max;

		SGVariant initial;
		T step;
		int n_digits;
	};




	template <class T>
	class MeasurementScale {
	public:
		MeasurementScale(typename T::LL min, typename T::LL max, typename T::LL initial, typename T::LL step, typename T::Unit unit, int precision) :
			m_min(min, unit), m_max(max, unit), m_initial(initial, unit), m_step(step, unit), m_precision(precision), m_unit(unit) {}

		bool is_in_range(const T & value) const { return (value >= this->m_min && value <= this->m_max); }

		T m_min;
		T m_max;

		T m_initial;
		T m_step;

		int m_precision; /* When T_ll is equivalent of float, how many digits after comma to use? */

		typename T::Unit m_unit;
	};




	class SGLabelID {
	public:
		SGLabelID(const QString & label_, int id_) : label(label_), id(id_) {};
		QString label;
		int id = 0; /* This shall be forever 'int' type. */
	};




	/* Data type for values used to create WidgetType::IntEnumeration. */
	class WidgetIntEnumerationData {
	public:
		std::vector<SGLabelID> values;
		int default_id; /* This shall be forever 'int' type. This is not an array index, but enumeration: one of enumerations in ::values. */
	};




	/* Data type for values used to create WidgetType::StringEnumeration. */
	class WidgetStringEnumerationData {
	public:
		std::vector<QString> values;
		QString default_string;
	};




	class PropertiesDialog : public QDialog {
	public:
		PropertiesDialog(const QString & title = tr("Properties"), QWidget * parent = NULL);
		~PropertiesDialog();

		void fill(Preferences * preferences);
		void fill(const std::map<param_id_t, ParameterSpecification *> & parameter_specifications, const std::map<param_id_t, SGVariant> & values, const std::vector<SGLabelID> & parameter_groups);

		SGVariant get_param_value(const ParameterSpecification & param_spec);
		QWidget * get_widget(const ParameterSpecification & param_spec);

	private:
		QWidget * make_widget(const ParameterSpecification & param_spec, const SGVariant & param_value);

		QFormLayout * insert_tab(const QString & label);
		std::map<param_id_t, ParameterSpecification *>::iterator add_widgets_to_tab(QFormLayout * form, Layer * layer, std::map<param_id_t, ParameterSpecification *>::iterator & iter, std::map<param_id_t, ParameterSpecification *>::iterator & end);

		SGVariant get_param_value_from_widget(QWidget * widget, const ParameterSpecification & param_spec);

		QDialogButtonBox * button_box = NULL;
		QPushButton * ok = NULL;
		QPushButton * cancel = NULL;
		QVBoxLayout * vbox = NULL;

		QHash<QString, QWidget *> widgets; /* Parameter name (ParameterSpecification::name) -> widget. */

		std::map<param_id_t, QFormLayout *> forms;

		QTabWidget * tabs = NULL;
	};




}




#endif /* #ifndef _SG_UI_BUILDER_H_ */
