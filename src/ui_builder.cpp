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




//#include <cstdlib>
#include <cassert>




#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStringList>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSlider>




#include "layer.h"
#include "layer_interface.h"
#include "layer_trw_waypoint_properties.h"
#include "layer_trw_waypoint.h"
#include "ui_builder.h"
#include "widget_color_button.h"
#include "widget_file_list.h"
#include "widget_file_entry.h"
#include "widget_radio_group.h"
#include "widget_slider.h"
#include "widget_measurement_entry.h"
#include "date_time_dialog.h"
#include "preferences.h"
//#include "goto.h"




using namespace SlavGPS;




#define SG_MODULE "UI Builder"




ParameterSpecification & ParameterSpecification::operator=(const ParameterSpecification & other)
{
	if (&other == this) {
		/* Self-assignment. */
		return *this;
	}

	this->id = other.id;
	this->name = other.name;
	this->type_id = other.type_id;
	this->group_id = other.group_id;
	this->ui_label = other.ui_label;
	this->widget_type = other.widget_type;
	this->widget_data = other.widget_data;
	this->hardcoded_default_value = other.hardcoded_default_value;
	this->tooltip = other.tooltip;

	return *this;
}




QString SlavGPS::widget_type_get_label(WidgetType type_id)
{
	QString result;

	switch (type_id) {
	case WidgetType::CheckButton:
		result = "CheckButton";
		break;
	case WidgetType::RadioGroup:
		result = "RadioGroup";
		break;
	case WidgetType::SpinBoxDouble:
		result = "SpinBoxDouble";
		break;
	case WidgetType::SpinBoxInt:
		result = "SpinBoxInt";
		break;
	case WidgetType::Entry:
		result = "Entry";
		break;
	case WidgetType::Password:
		result = "Password";
		break;
	case WidgetType::FileSelector:
		result = "FileSelector";
		break;
	case WidgetType::FolderEntry:
		result = "FolderEntry";
		break;
	case WidgetType::HScale:
		result = "HScale";
		break;
	case WidgetType::Color:
		result = "Color";
		break;
	case WidgetType::ComboBox:
		result = "ComboBox";
		break;
	case WidgetType::FileList:
		result = "FileList";
		break;
	case WidgetType::DateTime:
		result = "DateTime";
		break;
	case WidgetType::Altitude:
		result = "Altitude";
		break;
	case WidgetType::None:
	default:
		result = "None/Unknown";
	}

	return result;
}




PropertiesDialog::PropertiesDialog(QString const & title, QWidget * parent_widget) : QDialog(parent_widget)
{
	this->setWindowTitle(title);

	this->button_box = new QDialogButtonBox();
	this->ok = this->button_box->addButton("OK", QDialogButtonBox::AcceptRole);
	this->cancel = this->button_box->addButton("Cancel", QDialogButtonBox::RejectRole);


	this->tabs = new QTabWidget();
	this->vbox = new QVBoxLayout;

	//this->tabs->setTabBarAutoHide(true); /* TODO_MAYBE: enable when this method becomes widely available. */

	//this->vbox->addLayout(this->form);
	this->vbox->addWidget(this->tabs);
	this->vbox->addWidget(this->button_box);

	QLayout * old = this->layout();
	delete old;

	this->setLayout(this->vbox);

        connect(button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(button_box, SIGNAL(rejected()), this, SLOT(reject()));
}




QFormLayout * PropertiesDialog::insert_tab(QString const & label)
{
	QFormLayout * form = new QFormLayout();
	QWidget * page = new QWidget();
	QLayout * l = page->layout();
	delete l;
	page->setLayout(form);

	this->tabs->addTab(page, label);

	return form;
}


PropertiesDialog::~PropertiesDialog()
{
	delete this->ok;
	delete this->cancel;
	delete this->button_box;
}




void PropertiesDialog::fill(Preferences * preferences)
{
	qDebug() << "\n" SG_PREFIX_I << "Creating Properties Dialog from preferences";

	for (auto iter = preferences->begin(); iter != preferences->end(); iter++) {
		param_id_t group_id = iter.value().group_id;

		auto form_iter = this->forms.find(group_id);
		QFormLayout * form = NULL;
		if (form_iter == this->forms.end()) {
			/* Create new tab in UI dialog. */

			const QString tab_label = preferences->group_names[group_id];
			form = this->insert_tab(tab_label);

			this->forms.insert(std::pair<param_id_t, QFormLayout *>(group_id, form));

			qDebug() << SG_PREFIX_I << "Created tab" << tab_label;
		} else {
			form = form_iter->second;
		}


		const ParameterSpecification & param_spec = iter.value();
		const QString param_name = iter.key();
		/* Remember that iter.ke() is <some namespace><dot>param_spec.name.
		   This is not the best place to re-create map's key by concatenating
		   some strings. Just use iter.key() and be done with it. */
		const SGVariant param_value = preferences->get_param_value(param_name);

		QWidget * widget = this->make_widget(param_spec, param_value);
		form->addRow(param_spec.ui_label, widget);
		this->widgets.insert(param_name, widget);
	}
}




void PropertiesDialog::fill(const std::map<param_id_t, ParameterSpecification *> & param_specs,
			    const std::map<param_id_t, SGVariant> & current_parameter_values,
			    const std::vector<SGLabelID> & param_groups)
{
	qDebug() << "\n" SG_PREFIX_I << "Creating Properties Dialog";

	for (auto param_iter = param_specs.begin(); param_iter != param_specs.end(); ++param_iter) {

		const ParameterSpecification & param_spec = *(param_iter->second);

		const SGVariant param_value = current_parameter_values.at(param_iter->first);
		QWidget * widget = this->make_widget(param_spec, param_value);

		const param_id_t group_id = param_spec.group_id;

		if (group_id != PARAMETER_GROUP_HIDDEN) {
			/* We created widget for hidden parameter above, but don't put it in UI form.
			   We have created the widget so that PropertiesDialog::get_param_value() works
			   correctly and consistently for both hidden and visible parameters. */

			QFormLayout * form = NULL;
			auto form_iter = this->forms.find(group_id);
			if (form_iter == this->forms.end()) {

				const bool use_generic_label = param_groups.empty() || group_id >= (int) param_groups.size();
				const QString page_label = use_generic_label ? tr("Properties") : param_groups[group_id].label;

				form = this->insert_tab(page_label);

				this->forms.insert(std::pair<param_id_t, QFormLayout *>(group_id, form));

				qDebug() << SG_PREFIX_I << "Created tab" << page_label;
			} else {
				form = form_iter->second;
			}

			form->addRow(param_spec.ui_label, widget);
		}

		/* Name of parameter in parameter specification is
		   unique in a layer, so we can use it as a key. */
		this->widgets.insert(QString(param_spec.name), widget);
	}
}




std::map<param_id_t, ParameterSpecification *>::iterator PropertiesDialog::add_widgets_to_tab(QFormLayout * form, Layer * layer, std::map<param_id_t, ParameterSpecification *>::iterator & iter, std::map<param_id_t, ParameterSpecification *>::iterator & end)
{
	int n_widgets = 0;
	param_id_t last_group_id = iter->second->group_id;

	qDebug() << SG_PREFIX_I << "vvvvvvvvvv adding widgets to group" << last_group_id << ":";

	while (iter != end && iter->second->group_id == last_group_id) {

		const ParameterSpecification & param_spec = *(iter->second);

		if (param_spec.ui_label == "") {
			iter++;
			continue;
		}
		if (param_spec.group_id == PARAMETER_GROUP_HIDDEN) {
			iter++;
			continue;
		}

		const SGVariant param_value = layer->get_param_value(iter->first, false);

		QWidget * widget = this->make_widget(param_spec, param_value);

		form->addRow(param_spec.ui_label, widget);
		this->widgets.insert(QString(param_spec.name), widget);

		last_group_id = param_spec.group_id;
		n_widgets++;
		iter++;
	}

	qDebug() << SG_PREFIX_I << "^^^^^^^^^^ added new" << n_widgets << "widgets in this tab (" << widgets.size() << "in total)";
	return iter;
}




QWidget * PropertiesDialog::make_widget(const ParameterSpecification & param_spec, const SGVariant & param_value)
{
	/* Print this debug before attempting to create a widget. If
	   application crashes before a widget is created, this debug
	   will tell us which widget caused problems. */
	qDebug() << SG_PREFIX_I << "Will create new" << widget_type_get_label(param_spec.widget_type) << "for" << param_spec.ui_label << param_spec.type_id;

	QWidget * widget = NULL;
	switch (param_spec.widget_type) {

	case WidgetType::Color:
		if (param_spec.type_id == SGVariantType::Color) {
			qDebug() << SG_PREFIX_I << "Creating color button with colors" << param_value;
			ColorButtonWidget * widget_ = new ColorButtonWidget(param_value.val_color, NULL);

			//widget_->setStyleSheet("* { border: none; background-color: rgb(255,125,100) }");
			widget = widget_;
		}
		break;

	case WidgetType::CheckButton:
		if (param_spec.type_id == SGVariantType::Boolean) {
			QCheckBox * widget_ = new QCheckBox;
			if (param_value.u.val_bool) {
				widget_->setCheckState(Qt::Checked);
			}
			widget = widget_;
		}
		break;

	case WidgetType::ComboBox:
	case WidgetType::Enumeration: {

		assert (param_spec.type_id == SGVariantType::Enumeration
			|| param_spec.type_id == SGVariantType::String);

		QComboBox * widget_ = new QComboBox(this);

		int selected_idx = 0;

		if (param_spec.widget_data) {
			const WidgetEnumerationData * enum_data = (WidgetEnumerationData *) param_spec.widget_data;
			int i = 0;
			for (auto iter = enum_data->values.begin(); iter != enum_data->values.end(); iter++) {
				if (param_spec.type_id == SGVariantType::Int || param_spec.type_id == SGVariantType::Enumeration) {
					widget_->addItem((*iter).label, QVariant((int32_t) (*iter).id));
					if (param_value.u.val_int == (int32_t) (*iter).id) {
						selected_idx = i;
					}
				} else if (param_spec.type_id == SGVariantType::String) {
					widget_->addItem((*iter).label, QVariant((int32_t) (*iter).id));
					if (param_value.val_string == (*iter).label) {
						selected_idx = i;
					}
				} else {
					qDebug() << SG_PREFIX_E << "Unsupported parameter spec type for combobox:" << param_spec.type_id;
				}

				i++;
			}
		} else {
			; /* For some combo boxes the algorithm of adding items may be  non-standard. */
		}

		widget_->setCurrentIndex(selected_idx);

		widget = widget_;
	}
		break;

	case WidgetType::RadioGroup:
		if ((param_spec.type_id == SGVariantType::Int)
		    && param_spec.widget_data) {
			const std::vector<SGLabelID> * items = (const std::vector<SGLabelID> *) param_spec.widget_data;
			assert (items);
			RadioGroupWidget * widget_ = new RadioGroupWidget("", items, this);
			widget = widget_;
		}
		break;

	case WidgetType::SpinBoxInt:

		assert (param_spec.type_id == SGVariantType::Int);
		if (param_spec.type_id == SGVariantType::Int && param_spec.widget_data) {

			const int32_t init_val = param_value.u.val_int;
			ParameterScale<int> * scale = (ParameterScale<int> *) param_spec.widget_data;
			QSpinBox * widget_ = new QSpinBox();
			widget_->setMinimum(scale->min);
			widget_->setMaximum(scale->max);
			widget_->setSingleStep(scale->step);
			widget_->setValue(init_val);
			//scale->digits

			widget = widget_;
		}
		break;

	case WidgetType::SpinBoxDouble:

		assert (param_spec.type_id == SGVariantType::Double);

		if (param_spec.type_id == SGVariantType::Double && param_spec.widget_data) {

			const double init_val = param_value.u.val_double;
			ParameterScale<double> * scale = (ParameterScale<double> *) param_spec.widget_data;
			QDoubleSpinBox * widget_ = new QDoubleSpinBox();
			/* Order of calls is important. Use setDecimals() before using setValue(). */
			widget_->setDecimals(scale->n_digits);
			widget_->setMinimum(scale->min);
			widget_->setMaximum(scale->max);
			widget_->setSingleStep(scale->step);
			widget_->setValue(init_val);
			qDebug() << SG_PREFIX_I << "New SpinBoxDouble with initial value" << init_val;

			widget = widget_;
		}
		break;

	case WidgetType::Entry:
		widget = new QLineEdit(param_value.to_string());
		break;

	case WidgetType::Password:
		if (param_spec.type_id == SGVariantType::String) {
			QLineEdit * widget_ = new QLineEdit();
			widget_->setEchoMode(QLineEdit::Password);
			if (!param_value.val_string.isEmpty()) {
				widget_->setText(param_value.val_string);
			}
			widget = widget_;
		}

		break;
	case WidgetType::FileSelector:
		if (param_spec.type_id == SGVariantType::String) {

			FileSelectorWidget::FileTypeFilter file_type_filter = FileSelectorWidget::FileTypeFilter::Any;
			if (param_spec.widget_data) {
				file_type_filter = *((FileSelectorWidget::FileTypeFilter *) param_spec.widget_data); /* Pointer to a table of size one. */
			}

			FileSelectorWidget * widget_ = new FileSelectorWidget(QFileDialog::Option(0), QFileDialog::ExistingFile, tr("Select file"), NULL);
			widget_->set_file_type_filter(file_type_filter);
			if (!param_value.val_string.isEmpty()) {
				widget_->preselect_file_full_path(param_value.val_string);
			}

			widget = widget_;
		}
		break;

	case WidgetType::FolderEntry:
		if (param_spec.type_id == SGVariantType::String) {
			FileSelectorWidget * widget_ = new FileSelectorWidget(QFileDialog::Option(0), QFileDialog::Directory, tr("Select folder"), NULL);
			if (!param_value.val_string.isEmpty()) {
				widget_->preselect_file_full_path(param_value.val_string);
			}

			widget = widget_;
		}
		break;

	case WidgetType::FileList:
		if (param_spec.type_id == SGVariantType::StringList) {
			FileListWidget * widget_ = new FileListWidget(param_spec.ui_label, param_value.val_string_list, this);

			widget = widget_;
		}
		break;

	case WidgetType::HScale:
		assert (param_spec.type_id == SGVariantType::Int || param_spec.type_id == SGVariantType::Double);
		if (param_spec.widget_data) {

			if (param_spec.type_id == SGVariantType::Int) {
				ParameterScale<int> * scale = (ParameterScale<int> *) param_spec.widget_data;
				SliderWidget * widget_ = new SliderWidget(*scale, Qt::Horizontal);
				widget_->set_value(param_value.u.val_int);
				widget = widget_;
			} else if (param_spec.type_id == SGVariantType::Double) {
				ParameterScale<double> * scale = (ParameterScale<double> *) param_spec.widget_data;
				SliderWidget * widget_ = new SliderWidget(*scale, Qt::Horizontal);
				widget_->set_value(param_value.u.val_double);
				widget = widget_;
			} else {
				qDebug() << SG_PREFIX_E << "Unexpected param spec type" << param_spec.type_id;
			}
		}
		break;

	case WidgetType::DateTime:
		widget = new SGDateTimeButton(param_value.get_timestamp(), this);
		break;

	case WidgetType::Altitude:
		assert (param_spec.type_id == SGVariantType::Altitude);
		if (param_spec.type_id == SGVariantType::Altitude) {
			const ParameterScale<double> * scale = (ParameterScale<double> *) param_spec.widget_data; /* May be NULL. */
			MeasurementEntryWidget * widget_ = new MeasurementEntryWidget(param_value, scale, this);
			qDebug() << SG_PREFIX_I << "New MeasurementEntryWidget with initial value" << param_value.get_altitude();

			widget = widget_;
		}
		break;

	default:
		break;
	}

	if (widget) {
		qDebug() << SG_PREFIX_I << "Created" << widget_type_get_label(param_spec.widget_type) << ", label =" << param_spec.ui_label;
	} else {
		qDebug() << SG_PREFIX_E << "Failed to create" << widget_type_get_label(param_spec.widget_type) << ", label =" << param_spec.ui_label;
	}

	if (widget && widget->toolTip().isEmpty()) {
		if (!param_spec.tooltip.isEmpty()) {
			widget->setToolTip(param_spec.tooltip);
		}
	}

	return widget;
}




SGVariant PropertiesDialog::get_param_value(const ParameterSpecification & param_spec)
{
	const QString param_name(param_spec.name);

	auto iter = this->widgets.find(param_name);
	if (iter == this->widgets.end() || !(*iter)) {
		qDebug() << SG_PREFIX_E << "Not returning value of" << param_name << ", widget not found";
		if (param_spec.group_id == PARAMETER_GROUP_HIDDEN) {
			qDebug() << SG_PREFIX_E << param_spec.name << "parameter is hidden, but we should have been able to find it";
		}
		return SGVariant();
	}

	QWidget * widget = *iter;

	return this->get_param_value_from_widget(widget, param_spec);
}




QWidget * PropertiesDialog::get_widget(const ParameterSpecification & param_spec)
{
	auto iter = this->widgets.find(QString(param_spec.name));
	if (iter == this->widgets.end()) {
		qDebug() << SG_PREFIX_E << "Failed to find widget for param spec" << param_spec.name << widget_type_get_label(param_spec.widget_type);
		return NULL;
	} else {
		qDebug() << SG_PREFIX_I << "Returning widget for param spec" << param_spec.name << widget_type_get_label(param_spec.widget_type);
		return (*iter);
	}
}




SGVariant PropertiesDialog::get_param_value_from_widget(QWidget * widget, const ParameterSpecification & param_spec)
{
	SGVariant rv;

	switch (param_spec.widget_type) {
	case WidgetType::Color:
		rv = SGVariant(((ColorButtonWidget *) widget)->get_color());
		break;
	case WidgetType::CheckButton:
		rv = SGVariant((bool) ((QCheckBox *) widget)->isChecked());
		break;

	case WidgetType::ComboBox:
	case WidgetType::Enumeration:
		assert (param_spec.type_id == SGVariantType::Enumeration
			|| param_spec.type_id == SGVariantType::String);

		if (param_spec.type_id == SGVariantType::Enumeration) {
			rv = SGVariant(((QComboBox *) widget)->currentData().toInt(), SGVariantType::Enumeration);

		} else if (param_spec.type_id == SGVariantType::String) {
			rv = SGVariant(((QComboBox *) widget)->currentText());

			/* TODO_LATER: look at old implementation below.
			   Verify if in places that use SGVariantType::String +
			   WidgetType::ComboBox this code is still needed. */
#ifdef K_OLD_IMPLEMENTATION
			if (param_spec.extra_widget_data) {
				/* Combobox displays labels and we want values from extra. */
				const int pos = widget->currentIndex();
				const QStringList * strings = (const QStringList *) param_spec.extra_widget_data;
				rv = SGVariant(strings->at(pos));
			} else {
				/* Return value of UI label of selected combo box. */
				rv = SGVariant(widget->currentText());
			}
#endif
		} else {
			qDebug() << SG_PREFIX_E << "Saving value of widget" << widget_type_get_label(param_spec.widget_type) << ", unsupported parameter spec type for combobox:" << param_spec.type_id;
		}

		break;

	case WidgetType::RadioGroup:
		/* get_id_of_selected() returns arbitrary ID. */
		rv = SGVariant((int32_t) ((RadioGroupWidget *) widget)->get_id_of_selected());
		break;

	case WidgetType::SpinBoxInt:
		assert (param_spec.type_id == SGVariantType::Int);
		rv = SGVariant((int32_t) ((QSpinBox *) widget)->value());
		break;

	case WidgetType::SpinBoxDouble:
		assert (param_spec.type_id == SGVariantType::Double);
		rv = SGVariant((double) ((QDoubleSpinBox *) widget)->value());
		break;

	case WidgetType::Entry: {
		QLineEdit * edit = (QLineEdit *) widget;
		rv = SGVariant(param_spec.type_id, edit->text()); /* String representation -> variant. */
		break;
	}


	case WidgetType::Password:
		rv = SGVariant(((QLineEdit *) widget)->text());
		break;

	case WidgetType::FileSelector:
	case WidgetType::FolderEntry:
		rv = SGVariant(((FileSelectorWidget *) widget)->get_selected_file_full_path());
		break;

	case WidgetType::FileList:
		rv = SGVariant(((FileListWidget *) widget)->get_list());
		for (auto iter = rv.val_string_list.constBegin(); iter != rv.val_string_list.constEnd(); iter++) {
			qDebug() << SG_PREFIX_I << "File on retrieved list:" << *iter;
		}

		break;

	case WidgetType::HScale:
		assert (param_spec.type_id == SGVariantType::Int || param_spec.type_id == SGVariantType::Double);
		if (param_spec.type_id == SGVariantType::Int) {
			rv = SGVariant((int32_t) ((SliderWidget *) widget)->get_value());
		} else if (param_spec.type_id == SGVariantType::Double) {
			rv = SGVariant((double) ((SliderWidget *) widget)->get_value());
		} else {
			qDebug() << SG_PREFIX_E << "Unexpected param spec type" << param_spec.type_id;
		}
		break;

	case WidgetType::DateTime:
		rv = SGVariant(((SGDateTimeButton *) widget)->get_value());
		break;

	case WidgetType::Altitude:
		assert (param_spec.type_id == SGVariantType::Altitude);
		rv = ((MeasurementEntryWidget *) widget)->get_value_iu();
		break;

	default:
		break;
	}

	assert (rv.type_id == param_spec.type_id);

	qDebug() << SG_PREFIX_I << "Widget" << widget_type_get_label(param_spec.widget_type) << "/" << param_spec.ui_label << "returns value" << rv;

	return rv;
}




SGVariant ParameterSpecification::get_hardcoded_value(void) const
{
	SGVariant param_value;

	switch (this->widget_type) {
	case WidgetType::SpinBoxDouble:
	case WidgetType::Altitude:
		if (this->widget_data) {
			const ParameterScale<double> * scale = (ParameterScale<double> *) this->widget_data;
			param_value = scale->initial;
		}
		break;

	case WidgetType::SpinBoxInt:
	case WidgetType::HScale:
		if (this->widget_data) {
			const ParameterScale<int> * scale = (ParameterScale<int> *) this->widget_data;
			param_value = scale->initial;
		}
		break;

	default:
		if (this->hardcoded_default_value) {
			param_value = this->hardcoded_default_value();
		}
		break;
	}

	return param_value; /* param_value.is_valid() may or may not return true. */
}
