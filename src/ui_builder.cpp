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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




#include <cstdlib>
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
#include "date_time_dialog.h"
#include "preferences.h"
#include "goto.h"




using namespace SlavGPS;




#define PREFIX " UI Builder:" << __FUNCTION__ << __LINE__ << ">"




extern Tree * g_tree;




ParameterSpecification & ParameterSpecification::operator=(const ParameterSpecification & other)
{
	if (&other == this) {
		/* Self-assignment. */
		return *this;
	}

	this->id = other.id;
	if (other.name_space) { /* .name_space may be NULL. */
		this->name_space = strdup(other.name_space);
	}
	this->name = strdup(other.name);
	this->type_id = other.type_id;
	this->group_id = other.group_id;
	this->ui_label = other.ui_label;
	this->widget_type = other.widget_type;
	this->widget_data = other.widget_data;

	this->hardwired_default_value = other.hardwired_default_value;
	this->extra = other.extra;
	this->tooltip = other.tooltip;

	return *this;
}




QString SlavGPS::widget_type_get_label(WidgetType type_id)
{
	QString result;

	switch (type_id) {
	case WidgetType::CheckButton:
		result = "CheckButton";
	case WidgetType::RadioGroup:
		result = "RadioGroup";
	case WidgetType::SpinBoxDouble:
		result = "SpinBoxDouble";
	case WidgetType::SpinBoxInt:
		result = "SpinBoxInt";
	case WidgetType::Entry:
		result = "Entry";
	case WidgetType::Password:
		result = "Password";
	case WidgetType::FileEntry:
		result = "FileEntry";
	case WidgetType::FolderEntry:
		result = "FolderEntry";
	case WidgetType::HScale:
		result = "HScale";
	case WidgetType::Color:
		result = "Color";
	case WidgetType::ComboBox:
		result = "ComboBox";
	case WidgetType::FileList:
		result = "FileList";
	case WidgetType::DateTime:
		result = "DateTime";
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

	//this->tabs->setTabBarAutoHide(true); /* TODO: enable when this method becomes widely available. */

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
	qDebug() << "\nII: UI Builder: creating Properties Dialog from preferences";

	for (auto iter = preferences->begin(); iter != preferences->end(); iter++) {
		param_id_t group_id = iter.value().group_id;

		auto form_iter = this->forms.find(group_id);
		QFormLayout * form = NULL;
		if (form_iter == this->forms.end()) {
			/* Create new tab in UI dialog. */

			const QString tab_label = preferences->group_names[group_id];
			form = this->insert_tab(tab_label);

			this->forms.insert(std::pair<param_id_t, QFormLayout *>(group_id, form));

			qDebug() << "II: Preferences Builder: created tab" << tab_label;
		} else {
			form = form_iter->second;
		}


		const ParameterSpecification & param_spec = iter.value();
		const QString param_name = iter.key(); /* TODO: isn't it the same as param_spec->name? */
		const SGVariant param_value = preferences->get_param_value(param_name);

		QWidget * widget = this->new_widget(param_spec, param_value);
		form->addRow(param_spec.ui_label, widget);
		this->widgets2.insert(param_name, widget);
	}
}




void PropertiesDialog::fill(Layer * layer)
{
	qDebug() << "\nII: UI Builder: creating Properties Dialog from layer" << layer->get_name();

	for (auto iter = layer->get_interface().parameter_specifications.begin(); iter != layer->get_interface().parameter_specifications.end(); iter++) {

		const ParameterSpecification & param_spec = *(iter->second);

		param_id_t group_id = param_spec.group_id;

		auto form_iter = this->forms.find(group_id);
		QFormLayout * form = NULL;
		if (form_iter == this->forms.end() && group_id != PARAMETER_GROUP_HIDDEN) {

			/* Add new tab, but only for non-hidden parameters */

			QString page_label = layer->get_interface().parameter_groups
				? layer->get_interface().parameter_groups[group_id]
				: tr("Properties");
			form = this->insert_tab(page_label);

			this->forms.insert(std::pair<param_id_t, QFormLayout *>(group_id, form));

			qDebug() << "II: Parameters Builder: created tab" << page_label;
		} else {
			form = form_iter->second;
		}

		const SGVariant param_value = layer->get_param_value(iter->first, false);
		QWidget * widget = this->new_widget(param_spec, param_value);

		/* We create widgets for hidden parameters, but don't put them in form.
		   We create them so that PropertiesDialog::get_param_value() works
		   correctly and consistently for both hidden and visible parameters. */

		if (group_id != PARAMETER_GROUP_HIDDEN) {
			form->addRow(param_spec.ui_label, widget);
		}

		this->widgets.insert(std::pair<param_id_t, QWidget *>(iter->first, widget));
	}
}




void PropertiesDialog::fill(LayerInterface * interface)
{
	qDebug() << "\nII: UI Builder: creating Properties Dialog from layer interface";

	std::map<param_id_t, SGVariant> * values = &interface->parameter_default_values;

	for (auto iter = interface->parameter_specifications.begin(); iter != interface->parameter_specifications.end(); iter++) {

		const ParameterSpecification & param_spec = *(iter->second);

		param_id_t group_id = param_spec.group_id;
		if (group_id == PARAMETER_GROUP_HIDDEN) {
			iter++;
			continue;
		}

		auto form_iter = this->forms.find(group_id);
		QFormLayout * form = NULL;
		if (form_iter == this->forms.end()) {
			QString page_label = interface->parameter_groups
				? interface->parameter_groups[group_id]
				: tr("Properties");
			form = this->insert_tab(page_label);

			this->forms.insert(std::pair<param_id_t, QFormLayout *>(group_id, form));

			qDebug() << "II: Parameters Builder: created tab" << page_label;
		} else {
			form = form_iter->second;
		}

		const SGVariant param_value = values->at(iter->first);
		QWidget * widget = this->new_widget(param_spec, param_value);
		form->addRow(param_spec.ui_label, widget);
		this->widgets.insert(std::pair<param_id_t, QWidget *>(iter->first, widget));
	}
}




void PropertiesDialog::fill(Waypoint * wp, const std::vector<const ParameterSpecification *> & param_specs, const QString & default_name)
{
	qDebug() << "\nII: UI Builder: creating Properties Dialog from waypoint";

	int i = 0;
	QFormLayout * form = this->insert_tab(tr("Properties"));
	this->forms.insert(std::pair<param_id_t, QFormLayout *>(param_specs[SG_WP_PARAM_NAME]->group_id, form));
	SGVariant param_value; // = layer->get_param_value(i, false);
	QWidget * widget = NULL;



	const ParameterSpecification * param_spec = param_specs[SG_WP_PARAM_NAME];
	param_value = SGVariant(default_name); /* TODO: This should be somehow taken from param_specs->default */
	widget = this->new_widget(*param_spec, param_value);
	form->addRow(param_spec->ui_label, widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));



	const LatLon lat_lon = wp->coord.get_latlon();



	param_spec = param_specs[SG_WP_PARAM_LAT];
	widget = this->new_widget(*param_spec, SGVariant(lat_lon.lat, SGVariantType::Latitude));
	form->addRow(param_spec->ui_label, widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));



	param_spec = param_specs[SG_WP_PARAM_LON];
	widget = this->new_widget(*param_spec, SGVariant(lat_lon.lon, SGVariantType::Longitude));
	form->addRow(param_spec->ui_label, widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));



	/* TODO: Consider if there should be a remove time button... */
	param_spec = param_specs[SG_WP_PARAM_TIME];
	widget = this->new_widget(*param_spec, SGVariant(wp->timestamp, SGVariantType::Timestamp));
	form->addRow(param_spec->ui_label, widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));
#ifdef K_FIXME_RESTORE
	QObject::connect(timevaluebutton, SIGNAL("button-release-event"), edit_wp, SLOT (time_edit_click));
#endif



	QString alt;
	const HeightUnit height_unit = Preferences::get_unit_height();
	switch (height_unit) {
	case HeightUnit::Metres:
		alt = QString("%1").arg(wp->altitude);
		break;
	case HeightUnit::Feet:
		alt = QString("%1").arg(VIK_METERS_TO_FEET(wp->altitude));
		break;
	default:
		alt = "???";
		qDebug() << "EE" PREFIX << "invalid height unit" << (int) height_unit;
		break;
	}

	param_spec = param_specs[SG_WP_PARAM_ALT];
	param_value = SGVariant(alt);
	widget = this->new_widget(*param_spec, param_value);
	form->addRow(param_spec->ui_label, widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));



	/* TODO: comment may contain URL. Make the label or input field clickable. */
	param_spec = param_specs[SG_WP_PARAM_COMMENT];
	param_value = SGVariant(wp->comment);
	widget = this->new_widget(*param_spec, param_value);
	form->addRow(param_spec->ui_label, widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));



	/* TODO: description may contain URL. Make the label or input field clickable. */
	param_spec = param_specs[SG_WP_PARAM_DESC];
	param_value = SGVariant(wp->description);
	widget = this->new_widget(*param_spec, param_value);
	form->addRow(param_spec->ui_label, widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));



	/* TODO: perhaps add file filter for image files? */
	param_spec = param_specs[SG_WP_PARAM_IMAGE];
	param_value = SGVariant(wp->image_full_path);
	widget = this->new_widget(*param_spec, param_value);
	form->addRow(param_spec->ui_label, widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));



	param_spec = param_specs[SG_WP_PARAM_SYMBOL];
	param_value = SGVariant(wp->symbol_name);
	widget = this->new_widget(*param_spec, param_value);
	form->addRow(param_spec->ui_label, widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));
}




std::map<param_id_t, ParameterSpecification *>::iterator PropertiesDialog::add_widgets_to_tab(QFormLayout * form, Layer * layer, std::map<param_id_t, ParameterSpecification *>::iterator & iter, std::map<param_id_t, ParameterSpecification *>::iterator & end)
{
	param_id_t i = 0;
	param_id_t last_group_id = iter->second->group_id;

	qDebug() << "II: UI Builder: vvvvvvvvvv adding widgets to group" << last_group_id << ":";

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

		QWidget * widget = this->new_widget(param_spec, param_value);

		form->addRow(param_spec.ui_label, widget);
		this->widgets.insert(std::pair<param_id_t, QWidget *>(iter->first, widget));

		last_group_id = param_spec.group_id;
		i++;
		iter++;
	}

	qDebug() << "II: UI Builder ^^^^^^^^^^ added new" << i << "widgets in this tab (" << widgets.size() << "in total)";
	return iter;
}




QWidget * PropertiesDialog::new_widget(const ParameterSpecification & param_spec, const SGVariant & param_value)
{
	/* Perform pre conversion if necessary. */
	SGVariant value = param_value;
	if (param_spec.extra && param_spec.extra->convert_to_display) {
		value = param_spec.extra->convert_to_display(param_value);
	}

	/* Print this debug before attempting to create a widget. If
	   application crashes before a widget is created, this debug
	   will tell us which widget caused problems. */
	qDebug() << "II:" PREFIX << "will create new" <<  widget_type_get_label(param_spec.widget_type) << "for" << param_spec.ui_label;

	QWidget * widget = NULL;
	switch (param_spec.widget_type) {

	case WidgetType::Color:
		if (param_spec.type_id == SGVariantType::Color) {
			qDebug() << "II: UI Builder: creating color button with colors" << value;
			SGColorButton * widget_ = new SGColorButton(value.val_color, NULL);

			//widget_->setStyleSheet("* { border: none; background-color: rgb(255,125,100) }");
			widget = widget_;
		}
		break;

	case WidgetType::CheckButton:
		if (param_spec.type_id == SGVariantType::Boolean) {
			QCheckBox * widget_ = new QCheckBox;
			if (value.u.val_bool) {
				widget_->setCheckState(Qt::Checked);
			}
			widget = widget_;
		}
		break;

	case WidgetType::ComboBox: {

		assert (param_spec.type_id == SGVariantType::Int || param_spec.type_id == SGVariantType::String);

		QComboBox * widget_ = new QComboBox(this);

		int selected_idx = 0;

		if (param_spec.widget_data) {
			const std::vector<SGLabelID> * items = (std::vector<SGLabelID> *) param_spec.widget_data;
			int i = 0;
			for (auto iter = items->begin(); iter != items->end(); iter++) {
				if (param_spec.type_id == SGVariantType::Int) {
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
					qDebug() << "EE: UI Builder: set: unsupported parameter spec type for combobox:" << param_spec.type_id;
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
		if (param_spec.type_id == SGVariantType::Int && param_spec.widget_data) {
			const std::vector<SGLabelID> * items = (const std::vector<SGLabelID> *) param_spec.widget_data;
			assert (items);
			SGRadioGroup * widget_ = new SGRadioGroup("", items, this);
			widget = widget_;
		}
		break;

	case WidgetType::SpinBoxInt:

		assert (param_spec.type_id == SGVariantType::Int);
		if (param_spec.type_id == SGVariantType::Int && param_spec.widget_data) {

			int32_t init_val = value.u.val_int;
			ParameterScale * scale = (ParameterScale *) param_spec.widget_data;
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

			const double init_val = value.u.val_double;
			ParameterScale * scale = (ParameterScale *) param_spec.widget_data;
			QDoubleSpinBox * widget_ = new QDoubleSpinBox();
			/* Order of fields is important. Use setDecimals() before using setValue(). */
			widget_->setDecimals(scale->digits);
			widget_->setMinimum(scale->min);
			widget_->setMaximum(scale->max);
			widget_->setSingleStep(scale->step);
			widget_->setValue(init_val);
			qDebug() << "II: UI Builder: new SpinBoxDouble with initial value" << init_val;

			widget = widget_;
		}
		break;

	case WidgetType::Entry:
		widget = new QLineEdit(value.to_string());
		break;

	case WidgetType::Password:
		if (param_spec.type_id == SGVariantType::String) {
			QLineEdit * widget_ = new QLineEdit();
			widget_->setEchoMode(QLineEdit::Password);
			if (!value.val_string.isEmpty()) {
				widget_->setText(value.val_string);
			}
			widget = widget_;
		}

		break;
	case WidgetType::FileEntry:
		if (param_spec.type_id == SGVariantType::String) {

			SGFileTypeFilter file_type_filter = SGFileTypeFilter::ANY;
			if (param_spec.widget_data) {
				file_type_filter = *((SGFileTypeFilter *) param_spec.widget_data); /* Pointer to a table of size one. */
			}

			SGFileEntry * widget_ = new SGFileEntry(QFileDialog::Option(0), QFileDialog::ExistingFile, file_type_filter, tr("Select file"), NULL);
			if (!value.val_string.isEmpty()) {
				widget_->preselect_file_full_path(value.val_string);
			}

			widget = widget_;
		}
		break;

	case WidgetType::FolderEntry:
		if (param_spec.type_id == SGVariantType::String) {
			SGFileEntry * widget_ = new SGFileEntry(QFileDialog::Option(0), QFileDialog::Directory, SGFileTypeFilter::ANY, tr("Select folder"), NULL);
			if (!value.val_string.isEmpty()) {
				widget_->preselect_file_full_path(value.val_string);
			}

			widget = widget_;
		}
		break;

	case WidgetType::FileList:
		if (param_spec.type_id == SGVariantType::StringList) {
			SGFileList * widget_ = new SGFileList(param_spec.ui_label, value.val_string_list, this);

			widget = widget_;
		}
		break;

	case WidgetType::HScale:
		assert (param_spec.type_id == SGVariantType::Int || param_spec.type_id == SGVariantType::Double);
		if ((param_spec.type_id == SGVariantType::Int || param_spec.type_id == SGVariantType::Double) && param_spec.widget_data) {

			ParameterScale * scale = (ParameterScale *) param_spec.widget_data;

			/* TODO: implement a slider for values of type 'double'. */
			SGSlider * widget_ = new SGSlider(*scale, Qt::Horizontal);

			if (param_spec.type_id == SGVariantType::Int) {
				widget_->set_value(value.u.val_int);
				widget = widget_;
			} else {
				widget_->set_value(value.u.val_double);
				widget = widget_;
			}
		}
		break;

	case WidgetType::DateTime:
		/* TODO: zero timestamp may still be a valid timestamp. */
		if (param_value.get_timestamp() != 0) {
			SGDateTimeButton * widget_ = new SGDateTimeButton(param_value.get_timestamp(), this);
			widget = widget_;
		} else {
			SGDateTimeButton * widget_ = new SGDateTimeButton(this);
			widget = widget_;
		}
		break;
	default:
		break;
	}

	if (widget) {
		qDebug() << "II: UI Builder: New Widget: success for:" << widget_type_get_label(param_spec.widget_type) << ", label =" << param_spec.ui_label;
	} else {
		qDebug() << "EE: UI Builder: New Widget: failure for:" << widget_type_get_label(param_spec.widget_type) << ", label =" << param_spec.ui_label;
	}

	if (widget && widget->toolTip().isEmpty()) {
		if (param_spec.tooltip) {
			widget->setToolTip(QObject::tr(param_spec.tooltip));
		}
	}

	return widget;
}




SGVariant PropertiesDialog::get_param_value(param_id_t param_id, const ParameterSpecification & param_spec)
{
	QWidget * widget = this->widgets[param_id];
	if (!widget) {
		if (param_spec.group_id == PARAMETER_GROUP_HIDDEN) {
			qDebug() << "II: UI Builder: Get Param Value: hidden widget:" << (int) param_id << "/" << this->widgets.size() << param_spec.name;
		} else {
			qDebug() << "EE: UI Builder: Get Param Value: widget not found:" << (int) param_id << "/" << this->widgets.size() << param_spec.name;
		}
		return SGVariant();
	}

	return this->get_param_value_from_widget(widget, param_spec);
}




SGVariant PropertiesDialog::get_param_value(const QString & param_name, const ParameterSpecification & param_spec)
{
	auto iter = this->widgets2.find(param_name);
	if (iter == this->widgets2.end() || !(*iter)) {
		if (param_spec.group_id == PARAMETER_GROUP_HIDDEN) {
			qDebug() << "II: UI Builder: saving value of widget" << this->widgets.size() << param_spec.name << "widget is 'not in properties'";
		} else {
			qDebug() << "EE: UI Builder: saving value of widget" << this->widgets.size() << param_spec.name << "widget not found";
		}
		return SGVariant();
	}

	QWidget * widget = *iter;

	return this->get_param_value_from_widget(widget, param_spec);
}




SGVariant PropertiesDialog::get_param_value_from_widget(QWidget * widget, const ParameterSpecification & param_spec)
{
	SGVariant rv;

	switch (param_spec.widget_type) {
	case WidgetType::Color:
		rv = SGVariant(((SGColorButton *) widget)->get_color());
		break;
	case WidgetType::CheckButton:
		rv = SGVariant((bool) ((QCheckBox *) widget)->isChecked());
		break;

	case WidgetType::ComboBox:
		assert (param_spec.type_id == SGVariantType::Int || param_spec.type_id == SGVariantType::String);

		if (param_spec.type_id == SGVariantType::Int) {
			rv = SGVariant((int32_t) ((QComboBox *) widget)->currentData().toInt());

		} else if (param_spec.type_id == SGVariantType::String) {
			rv = SGVariant(((QComboBox *) widget)->currentText());

			/* TODO: look at old implementation below: */
			/* Implementation in old code: */
#ifdef K_OLD_IMPLEMENTATION
			if (param_spec.extra_widget_data) {
				/* Combobox displays labels and we want values from extra. */
				int pos = widget->currentIndex();
				rv = SGVariant((char *) ((const char **) param_spec.extra_widget_data)[pos]);
			} else {
				/* Return raw value. */
				rv = SGVariant((char *) widget->currentText()); /* TODO: verify this assignment. */
			}
#endif
		} else {
			qDebug() << "EE: UI Builder: saving value of widget" << widget_type_get_label(param_spec.widget_type) << ", unsupported parameter spec type for combobox:" << param_spec.type_id;
		}

		break;

	case WidgetType::RadioGroup:
		/* get_id_of_selected() returns arbitrary ID. */
		rv = SGVariant((int32_t) ((SGRadioGroup *) widget)->get_id_of_selected());
		break;

	case WidgetType::SpinBoxInt:
		assert (param_spec.type_id == SGVariantType::Int);
		rv = SGVariant((int32_t) ((QSpinBox *) widget)->value());
		break;

	case WidgetType::SpinBoxDouble:
		assert (param_spec.type_id == SGVariantType::Double);
		rv = SGVariant((double) ((QDoubleSpinBox *) widget)->value());
		break;

	case WidgetType::Entry:
		rv = SGVariant(param_spec.type_id, ((QLineEdit *) widget)->text()); /* String representation -> variant. */
		break;

	case WidgetType::Password:
		rv = SGVariant(((QLineEdit *) widget)->text());
		break;

	case WidgetType::FileEntry:
	case WidgetType::FolderEntry:
		rv = SGVariant(((SGFileEntry *) widget)->get_selected_file_full_path());
		break;

	case WidgetType::FileList:
		rv = SGVariant(((SGFileList *) widget)->get_list());
		for (auto iter = rv.val_string_list.constBegin(); iter != rv.val_string_list.constEnd(); iter++) {
			qDebug() << "II: UI Builder: file on retrieved list: " << *iter;
		}

		break;

	case WidgetType::HScale:
		assert (param_spec.type_id == SGVariantType::Int || param_spec.type_id == SGVariantType::Double);
		if (param_spec.type_id == SGVariantType::Int) {
			rv = SGVariant((int32_t) ((SGSlider *) widget)->get_value());
		} else if (param_spec.type_id == SGVariantType::Double) {
#ifdef K_FIXME_RESTORE
			rv = SGVariant((double) gtk_range_get_value(GTK_RANGE(widget)));
#endif
		} else {
			;
		}
		break;
	case WidgetType::DateTime:
		rv = SGVariant(((SGDateTimeButton *) widget)->get_value(), SGVariantType::Timestamp);
		break;
	default:
		break;
	}

	/* Perform conversion if necessary. */
	if (param_spec.extra && param_spec.extra->convert_to_internal) {
		rv = param_spec.extra->convert_to_internal(rv);
	}

	qDebug() << "II: UI Builder:" << __FUNCTION__ << "widget type =" << widget_type_get_label(param_spec.widget_type) << ", label =" << param_spec.ui_label << ", saved value =" << rv << ", expected value type =" << param_spec.type_id;

	return rv;
}




SGVariant uibuilder_run_getparam(SGVariant * params_defaults, uint16_t i)
{
	return params_defaults[i];
}




bool ParameterSpecification::get_hardwired_value(SGVariant & value) const
{
	SGVariant param_value;
	if (this->widget_type == WidgetType::SpinBoxDouble
	    || this->widget_type == WidgetType::SpinBoxInt
	    || this->widget_type == WidgetType::HScale) {

		/* This will be overwritten below by value from settings file. */
		ParameterScale * scale = (ParameterScale *) this->widget_data;
		value = scale->initial;
		return true;
	} else {
		if (this->hardwired_default_value) {
			/* This will be overwritten below by value from settings file. */
			value = this->hardwired_default_value();
			return true;
		}
	}

	return false;
}
