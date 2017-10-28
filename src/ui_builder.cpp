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
#include "globals.h"
#include "widget_color_button.h"
#include "widget_file_list.h"
#include "widget_file_entry.h"
#include "widget_radio_group.h"
#include "widget_slider.h"
#include "date_time_dialog.h"
#include "preferences.h"




using namespace SlavGPS;




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
		param_id_t group_id = iter->second->group_id;

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



		SGVariant param_value = preferences->get_param_value(iter->first);
		QString label = QString(iter->second->title);
		QWidget * widget = this->new_widget(iter->second, param_value);
		form->addRow(label, widget);
		this->widgets.insert(std::pair<param_id_t, QWidget *>(iter->first, widget));
	}
}




void PropertiesDialog::fill(Layer * layer)
{
	qDebug() << "\nII: UI Builder: creating Properties Dialog from layer" << layer->get_name();

	for (auto iter = layer->get_interface()->parameter_specifications.begin(); iter != layer->get_interface()->parameter_specifications.end(); iter++) {
		param_id_t group_id = iter->second->group_id;

		auto form_iter = this->forms.find(group_id);
		QFormLayout * form = NULL;
		if (form_iter == this->forms.end() && group_id != PARAMETER_GROUP_HIDDEN) {

			/* Add new tab, but only for non-hidden parameters */

			QString page_label = layer->get_interface()->parameter_groups
				? layer->get_interface()->parameter_groups[group_id]
				: tr("Properties");
			form = this->insert_tab(page_label);

			this->forms.insert(std::pair<param_id_t, QFormLayout *>(group_id, form));

			qDebug() << "II: Parameters Builder: created tab" << page_label;
		} else {
			form = form_iter->second;
		}

		SGVariant param_value = layer->get_param_value(iter->first, false);
		QString label = QString(iter->second->title);
		QWidget * widget = this->new_widget(iter->second, param_value);

		/* We create widgets for hidden parameters, but don't put them in form.
		   We create them so that PropertiesDialog::get_param_value() works
		   correctly and consistently for both hidden and visible parameters. */

		if (group_id != PARAMETER_GROUP_HIDDEN) {
			form->addRow(label, widget);
		}

		this->widgets.insert(std::pair<param_id_t, QWidget *>(iter->first, widget));
	}
}




void PropertiesDialog::fill(LayerInterface * interface)
{
	qDebug() << "\nII: UI Builder: creating Properties Dialog from layer interface";

	std::map<param_id_t, SGVariant> * values = &interface->parameter_default_values;

	for (auto iter = interface->parameter_specifications.begin(); iter != interface->parameter_specifications.end(); iter++) {
		param_id_t group_id = iter->second->group_id;
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

		SGVariant param_value = values->at(iter->first);
		QString label = QString(iter->second->title);
		QWidget * widget = this->new_widget(iter->second, param_value);
		form->addRow(label, widget);
		this->widgets.insert(std::pair<param_id_t, QWidget *>(iter->first, widget));
	}
}




void PropertiesDialog::fill(Waypoint * wp, ParameterSpecification * param_specs, const QString & default_name)
{
	qDebug() << "\nII: UI Builder: creating Properties Dialog from waypoint";

	int i = 0;
	QFormLayout * form = this->insert_tab(tr("Properties"));
	this->forms.insert(std::pair<param_id_t, QFormLayout *>(param_specs[SG_WP_PARAM_NAME].group_id, form));
	SGVariant param_value; // = layer->get_param_value(i, false);
	ParameterSpecification * param_spec = NULL;
	QWidget * widget = NULL;


	struct LatLon ll = wp->coord.get_latlon();

	/* FIXME: memory management. */
	char * lat = g_strdup_printf("%f", ll.lat);
	char * lon = g_strdup_printf("%f", ll.lon);
	char * alt = NULL;

	HeightUnit height_units = Preferences::get_unit_height();
	switch (height_units) {
	case HeightUnit::METRES:
		alt = g_strdup_printf("%f", wp->altitude);
		break;
	case HeightUnit::FEET:
		alt = g_strdup_printf("%f", VIK_METERS_TO_FEET(wp->altitude));
		break;
	default:
		alt = g_strdup_printf("%f", wp->altitude);
		fprintf(stderr, "CRITICAL: invalid height unit %d\n", (int) height_units);
	}

	param_spec = &param_specs[SG_WP_PARAM_NAME];
	param_value = SGVariant(default_name);
	widget = this->new_widget(param_spec, param_value);
	form->addRow(QString(param_spec->title), widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));

	param_spec = &param_specs[SG_WP_PARAM_LAT];
	param_value = SGVariant(lat);
	widget = this->new_widget(param_spec, param_value);
	form->addRow(QString(param_spec->title), widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));

	param_spec = &param_specs[SG_WP_PARAM_LON];
	param_value = SGVariant(lon);
	widget = this->new_widget(param_spec, param_value);
	form->addRow(QString(param_spec->title), widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));

	param_spec = &param_specs[SG_WP_PARAM_TIME];
	param_value.u = wp->timestamp;
	widget = this->new_widget(param_spec, param_value);
	form->addRow(QString(param_spec->title), widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));

	param_spec = &param_specs[SG_WP_PARAM_ALT];
	param_value = SGVariant(alt);
	widget = this->new_widget(param_spec, param_value);
	form->addRow(QString(param_spec->title), widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));

	param_spec = &param_specs[SG_WP_PARAM_COMMENT];
	param_value = SGVariant(wp->comment);
	widget = this->new_widget(param_spec, param_value);
	form->addRow(QString(param_spec->title), widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));

	param_spec = &param_specs[SG_WP_PARAM_DESC];
	param_value = SGVariant(wp->description);
	widget = this->new_widget(param_spec, param_value);
	form->addRow(QString(param_spec->title), widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));

	param_spec = &param_specs[SG_WP_PARAM_IMAGE];
	param_value = SGVariant(wp->image);
	widget = this->new_widget(param_spec, param_value);
	form->addRow(QString(param_spec->title), widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));

	param_spec = &param_specs[SG_WP_PARAM_SYMBOL];
	param_value = SGVariant(wp->symbol_name);
	widget = this->new_widget(param_spec, param_value);
	form->addRow(QString(param_spec->title), widget);
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param_spec->id, widget));
}



std::map<param_id_t, ParameterSpecification *>::iterator PropertiesDialog::add_widgets_to_tab(QFormLayout * form, Layer * layer, std::map<param_id_t, ParameterSpecification *>::iterator & iter, std::map<param_id_t, ParameterSpecification *>::iterator & end)
{
	param_id_t i = 0;
	param_id_t last_group_id = iter->second->group_id;

	qDebug() << "II: UI Builder: vvvvvvvvvv adding widgets to group" << last_group_id << ":";

	while (iter != end && iter->second->group_id == last_group_id) {

		if (!iter->second->title) {
			iter++;
			continue;
		}
		if (iter->second->group_id == PARAMETER_GROUP_HIDDEN) {
			iter++;
			continue;
		}

		SGVariant param_value = layer->get_param_value(iter->first, false);

		QString label = QString(iter->second->title);
		QWidget * widget = this->new_widget(iter->second, param_value);

		form->addRow(label, widget);
		this->widgets.insert(std::pair<param_id_t, QWidget *>(iter->first, widget));

		last_group_id = iter->second->group_id;
		i++;
		iter++;
	}

	qDebug() << "II: UI Builder ^^^^^^^^^^ added new" << i << "widgets in this tab (" << widgets.size() << "in total)";
	return iter;
}




QWidget * PropertiesDialog::new_widget(ParameterSpecification * param_spec, const SGVariant & param_value)
{
	/* Perform pre conversion if necessary. */
	SGVariant var = param_value;
	if (param_spec->extra && param_spec->extra->convert_to_display) {
		var = param_spec->extra->convert_to_display(param_value);
	}

	QWidget * widget = NULL;
	switch (param_spec->widget_type) {

	case WidgetType::COLOR:
		if (param_spec->type == SGVariantType::COLOR) {
			qDebug() << "II: UI Builder: creating color button with colors" << var.c.r << var.c.g << var.c.b << var.c.a;
			QColor color;
			var.to_qcolor(color);
			SGColorButton * widget_ = new SGColorButton(color, NULL);

			//widget_->setStyleSheet("* { border: none; background-color: rgb(255,125,100) }");
			widget = widget_;
		}
		qDebug() << "II: UI Builder: adding widget Color, label:" << param_spec->title << "success:" << (bool) widget;
		break;

	case WidgetType::CHECKBUTTON:
		if (param_spec->type == SGVariantType::BOOLEAN) {
			QCheckBox * widget_ = new QCheckBox;
			if (var.b) {
				widget_->setCheckState(Qt::Checked);
			}
			widget = widget_;
		}
		qDebug() << "II: UI Builder: adding widget CheckButton, label:" << param_spec->title << "success:" << (bool) widget;
		break;

	case WidgetType::COMBOBOX: {
		assert (param_spec->widget_data);
		assert (param_spec->type == SGVariantType::INT || param_spec->type == SGVariantType::STRING);

		QComboBox * widget_ = new QComboBox(this);

		const std::vector<SGLabelID> * items = (std::vector<SGLabelID> *) param_spec->widget_data;
		int i = 0;
		int selected_idx = 0;

		for (auto iter = items->begin(); iter != items->end(); iter++) {
			if (param_spec->type == SGVariantType::INT) {
				widget_->addItem((*iter).label, QVariant((int32_t) (*iter).id));
				if (param_value.i == (int32_t) (*iter).id) {
					selected_idx = i;
				}
			} else if (param_spec->type == SGVariantType::STRING) {
				/* TODO: implement. */
			} else {
				qDebug() << "EE: UI Builder: set: unsupported parameter type for combobox:" << (int) param_spec->type;
			}

			i++;
		}

		widget_->setCurrentIndex(selected_idx);

		widget = widget_;
	}
		qDebug() << "II: UI Builder: adding widget ComboBox, label:" << param_spec->title << "success:" << (bool) widget;
		break;

	case WidgetType::RADIOGROUP:
		if (param_spec->type == SGVariantType::INT && param_spec->widget_data) {
			const std::vector<SGLabelID> * items = (const std::vector<SGLabelID> *) param_spec->widget_data;
			assert (items);
			SGRadioGroup * widget_ = new SGRadioGroup("", items, this);
			widget = widget_;
		}
		qDebug() << "II: UI Builder: adding widget RadioGroup Static, label:" << param_spec->title << "success:" << (bool) widget;
		break;

	case WidgetType::SPINBOX_INT:

		assert (param_spec->type == SGVariantType::INT);
		if (param_spec->type == SGVariantType::INT && param_spec->widget_data) {

			int32_t init_val = var.i;
			ParameterScale * scale = (ParameterScale *) param_spec->widget_data;
			QSpinBox * widget_ = new QSpinBox();
			widget_->setMinimum(scale->min);
			widget_->setMaximum(scale->max);
			widget_->setSingleStep(scale->step);
			widget_->setValue(init_val);
			//scale->digits

			widget = widget_;
		}
		qDebug() << "II: UI Builder: adding widget SpinBox (int), label:" << param_spec->title << "success:" << (bool) widget;
		break;

	case WidgetType::SPINBOX_DOUBLE:

		assert (param_spec->type == SGVariantType::DOUBLE);
		if (param_spec->type == SGVariantType::DOUBLE && param_spec->widget_data) {

			double init_val = var.d;
			ParameterScale * scale = (ParameterScale *) param_spec->widget_data;
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
		qDebug() << "II: UI Builder: adding widget SpinBox (double), label:" << param_spec->title << "success:" << (bool) widget;
		break;

	case WidgetType::ENTRY:
		if (param_spec->type == SGVariantType::STRING) {
			QLineEdit * widget_ = new QLineEdit;
			if (!var.s.isEmpty()) {
				widget_->insert(var.s);
			}

			widget = widget_;
		}
		qDebug() << "II: UI Builder: adding widget Entry, label:" << param_spec->title << "success:" << (bool) widget;
		break;
	case WidgetType::PASSWORD:
		if (param_spec->type == SGVariantType::STRING) {
			QLineEdit * widget_ = new QLineEdit();
			widget_->setEchoMode(QLineEdit::Password);
			if (!var.s.isEmpty()) {
				widget_->setText(var.s);
			}
			widget_->setToolTip(QObject::tr("Notice that this password will be stored clearly in a plain file."));

			widget = widget_;
		}
		qDebug() << "II: UI Builder: adding widget Password, label:" << param_spec->title << "success:" << (bool) widget;
		break;
	case WidgetType::FILEENTRY:
		if (param_spec->type == SGVariantType::STRING) {

			SGFileTypeFilter file_type_filter = SGFileTypeFilter::ANY;
			if (param_spec->widget_data) {
				file_type_filter = *((SGFileTypeFilter *) param_spec->widget_data); /* Pointer to a table of size one. */
			}

			SGFileEntry * widget_ = new SGFileEntry(QFileDialog::Option(0), QFileDialog::ExistingFile, file_type_filter, tr("Select file"), NULL);
			if (!var.s.isEmpty()) {
				widget_->set_filename(var.s);
			}

			widget = widget_;
		}
		qDebug() << "II: UI Builder: adding widget FileEntry, label:" << param_spec->title << "success:" << (bool) widget;
		break;

	case WidgetType::FOLDERENTRY:
		if (param_spec->type == SGVariantType::STRING) {
			SGFileEntry * widget_ = new SGFileEntry(QFileDialog::Option(0), QFileDialog::Directory, SGFileTypeFilter::ANY, tr("Select folder"), NULL);
			if (!var.s.isEmpty()) {
				widget_->set_filename(var.s);
			}

			widget = widget_;
		}
		qDebug() << "II: UI Builder: adding widget FolderEntry, label:" << param_spec->title << "success:" << (bool) widget;
		break;

	case WidgetType::FILELIST:
		if (param_spec->type == SGVariantType::STRING_LIST) {
			SGFileList * widget_ = new SGFileList(param_spec->title, var.sl, this);

			widget = widget_;
		}
		qDebug() << "II: UI Builder: adding widget FileList, label:" << param_spec->title << "success:" << (bool) widget;
		break;

	case WidgetType::HSCALE:
		assert (param_spec->type == SGVariantType::INT || param_spec->type == SGVariantType::DOUBLE);
		if ((param_spec->type == SGVariantType::INT || param_spec->type == SGVariantType::DOUBLE) && param_spec->widget_data) {

			ParameterScale * scale = (ParameterScale *) param_spec->widget_data;

			/* TODO: implement a slider for values of type 'double'. */
			SGSlider * widget_ = new SGSlider(*scale, Qt::Horizontal);

			if (param_spec->type == SGVariantType::INT) {
				widget_->set_value(var.i);
				widget = widget_;
			} else {
				widget_->set_value(var.d);
				widget = widget_;
			}
		}
		qDebug() << "II: UI Builder: adding widget HScale (" << (param_spec->type == SGVariantType::INT ? "int" : "double") << "), label:" << param_spec->title << "success:" << (bool) widget;
		break;
#ifdef K
	case WidgetType::BUTTON:
		if (param_spec->type == SGVariantType::PTR && param_spec->widget_data) {
			rv = gtk_button_new_with_label((const char *) param_spec->widget_data);
			QObject::connect(rv, SIGNAL (triggered(bool)), param_spec->extra_widget_data, SLOT (var.ptr));

			widget = widget_;
		}
		qDebug() << "II: UI Builder: adding widget Button, label:" << param_spec->title << "success:" << (bool) widget;
		break;
#endif
	case WidgetType::DATETIME: {
			SGDateTime * widget_ = new SGDateTime(param_value.u, this);

			widget = widget_;
		}
		qDebug() << "II: UI Builder: adding widget DateTime, label:" << param_spec->title << "success:" << (bool) widget;
		break;
	default:
		break;
	}

	if (widget && widget->toolTip().isEmpty()) {
		if (param_spec->tooltip) {
			widget->setToolTip(QObject::tr(param_spec->tooltip));
		}
	}

	return widget;
}




SGVariant PropertiesDialog::get_param_value(param_id_t id, ParameterSpecification * param_spec)
{
	SGVariant rv;

	QWidget * widget = this->widgets[id];
	if (!widget) {
		if (param_spec->group_id == PARAMETER_GROUP_HIDDEN) {
			qDebug() << "II: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << param_spec->name << "widget is 'not in properties'";
		} else {
			qDebug() << "EE: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << param_spec->name << "widget not found";
		}
		return rv;
	}

	switch (param_spec->widget_type) {
	case WidgetType::COLOR: {
		QColor c = ((SGColorButton *) widget)->get_color();
		rv = SGVariant(c);
		qDebug() << "II: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << "type: Color" << "label:" << param_spec->title << "value:" << c.red() << c.green() << c.blue();
	}
		break;
	case WidgetType::CHECKBUTTON:
		rv = SGVariant((bool) ((QCheckBox *) widget)->isChecked());
		qDebug() << "II: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << "type: Checkbox" << "label:" << param_spec->title << "value:" << rv.b;
		break;

	case WidgetType::COMBOBOX:
		assert (param_spec->type == SGVariantType::INT || param_spec->type == SGVariantType::STRING);

		if (param_spec->type == SGVariantType::INT) {
			rv = SGVariant((int32_t) ((QComboBox *) widget)->currentData().toInt());
			qDebug() << "II: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << "type: ComboBox (I)" << "label:" << param_spec->title << "value:" << rv.i;

		} else if (param_spec->type == SGVariantType::STRING) {
			/* TODO: implement */

			/* Implementation in old code: */
#if 0
			if (param_spec->extra_widget_data) {
				/* Combobox displays labels and we want values from extra. */
				int pos = widget->currentIndex();
				rv = SGVariant((char *) ((const char **) param_spec->extra_widget_data)[pos]);
			} else {
				/* Return raw value. */
				rv = SGVariant((char *) widget->currentText()); /* TODO: verify this assignment. */
			}
#endif
		} else {
			qDebug() << "EE: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << "unsupported parameter type for combobox:" << (int) param_spec->type;
		}

		break;

	case WidgetType::RADIOGROUP:
		/* get_id_of_selected() returns arbitrary ID. */
		rv = SGVariant((int32_t) ((SGRadioGroup *) widget)->get_id_of_selected());
		qDebug() << "II: UI Builder: saving value of widget" << id << "/" << this->widgets.size() << "type: RadioGroup" << "label:" << param_spec->title << "value:" << rv.i;
		break;

	case WidgetType::SPINBOX_INT:
		assert (param_spec->type == SGVariantType::INT);
		if (param_spec->type == SGVariantType::INT) {
			rv = SGVariant((int32_t) ((QSpinBox *) widget)->value());
			qDebug() << "II: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << "type: SpinBox (int)" << "label:" << param_spec->title << "value:" << rv.i;
		}
		break;

	case WidgetType::SPINBOX_DOUBLE:
		assert (param_spec->type == SGVariantType::DOUBLE);
		rv = SGVariant((double) ((QDoubleSpinBox *) widget)->value());
		qDebug() << "II: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << "type: SpinBox (double)" << "label:" << param_spec->title << "value:" << rv.d;
		break;

	case WidgetType::ENTRY:
	case WidgetType::PASSWORD:
		rv = SGVariant(((QLineEdit *) widget)->text());
		qDebug() << "II: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << "type: LineEdit" << "label:" << param_spec->title << "value:" << rv.s;
		break;

	case WidgetType::FILEENTRY:
	case WidgetType::FOLDERENTRY:
		rv = SGVariant(((SGFileEntry *) widget)->get_filename());
		qDebug() << "II: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << "type: File/Dir" << "label:" << param_spec->title << "value:" << rv.s;
		break;

	case WidgetType::FILELIST:
		qDebug() << "II: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << "type: FileList" << "label:" << param_spec->title;
		rv = SGVariant(((SGFileList *) widget)->get_list());
		for (auto iter = rv.sl.constBegin(); iter != rv.sl.constEnd(); iter++) {
			qDebug() << "II: UI Builder: file on retrieved list: " << *iter;
		}

		break;

	case WidgetType::HSCALE:
		assert (param_spec->type == SGVariantType::INT || param_spec->type == SGVariantType::DOUBLE);
		if (param_spec->type == SGVariantType::INT) {
			rv = SGVariant((int32_t) ((SGSlider *) widget)->get_value());
			qDebug() << "II: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << "type: HScale (int)" << "label:" << param_spec->title << "value:" << rv.i;
		} else if (param_spec->type == SGVariantType::DOUBLE) {
#ifdef K
			rv = SGVariant((double) gtk_range_get_value(GTK_RANGE(widget)));
#endif
		} else {
			;
		}
		break;
	case WidgetType::DATETIME:
		rv = SGVariant((uint32_t) ((SGDateTime *) widget)->value());
		qDebug() << "II: UI Builder: saving value of widget" << (int) id << "/" << (int) this->widgets.size() << "type: DateTime" << "label:" << param_spec->title << "value:" << rv.u;
		break;
	default:
		break;
	}

	/* Perform conversion if necessary. */
	if (param_spec->extra && param_spec->extra->convert_to_internal) {
		rv = param_spec->extra->convert_to_internal(rv);
	}

	return rv;
}




void uibuilder_run_setparam(SGVariant * paramdatas, uint16_t i, const SGVariant & data, ParameterSpecification * param_specs)
{
	/* Could have to copy it if it's a string! */
	switch (param_specs[i].type) {
	case SGVariantType::STRING:
		paramdatas[i] = SGVariant(data.s);
		break;
	default:
		paramdatas[i] = data; /* Dtring list will have to be freed by layer. anything else not freed. */
	}
}




SGVariant uibuilder_run_getparam(SGVariant * params_defaults, uint16_t i)
{
	return params_defaults[i];
}




/* Frees data from last (if necessary). */
void a_uibuilder_free_paramdatas(SGVariant * param_table, ParameterSpecification * param_specs, uint16_t param_specs_count)
{
	int i;
	/* May have to free strings, etc. */
	for (i = 0; i < param_specs_count; i++) {
		switch (param_specs[i].type) {
		case SGVariantType::STRING_LIST:
			param_table[i].sl.clear();
			break;
		default:
			break;
		}
	}
	free(param_table);
}




bool SlavGPS::parameter_get_hardwired_value(SGVariant & value, const ParameterSpecification & param_spec)
{
	SGVariant param_value;
	if (param_spec.widget_type == WidgetType::SPINBOX_DOUBLE
	    || param_spec.widget_type == WidgetType::SPINBOX_INT
	    || param_spec.widget_type == WidgetType::HSCALE) {

		/* This will be overwritten below by value from settings file. */
		ParameterScale * scale = (ParameterScale *) param_spec.widget_data;
		value = scale->initial;
		return true;
	} else {
		if (param_spec.hardwired_default_value) {
			/* This will be overwritten below by value from settings file. */
			value = param_spec.hardwired_default_value();
			return true;
		}
	}

	return false;
}




#if 0




//static void draw_to_image_file_total_area_cb (QSpinBox * spinbutton, void * *pass_along)
int a_uibuilder_properties_factory(const char * dialog_name,
				   QWindow * parent,
				   ParameterSpecification * params,
				   uint16_t params_count,
				   char ** groups,
				   uint8_t groups_count,
				   bool (* setparam) (void *, uint16_t, SGVariant, void *, bool),
				   void * pass_along1,
				   void * pass_along2,
				   SGVariant (* getparam) (void *, uint16_t, bool),
				   void * pass_along_getparam,
				   void (* changeparam) (GtkWidget*, ui_change_values *))
/* pass_along1 and pass_along2 are for set_param first and last params */
{
	uint16_t i, j, widget_count = 0;
	bool must_redraw = false;

	if (!params) {
		return 1; /* No params == no options, so all is good. */
	}

	for (i = 0; i < params_count; i++) {
		if (params[i].group_id != PARAMETER_GROUP_HIDDEN) {
			widget_count++;
		}
	}

	if (widget_count == 0) {
		return 0; /* TODO -- should be one? */
	} else {
		/* Create widgets and titles; place in table. */
		GtkWidget *dialog = gtk_dialog_new_with_buttons(dialog_name,
								parent,
								(GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
								GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
								GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
		gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
		GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
		response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
#endif
		int resp;

		GtkWidget *table = NULL;
		GtkWidget **tables = NULL; /* For more than one group. */

		GtkWidget *notebook = NULL;
		QLabel ** labels = (QLabel **) malloc(sizeof(QLabel *) * widget_count);
		GtkWidget **widgets = (GtkWidget **) malloc(sizeof(GtkWidget *) * widget_count);
		ui_change_values * change_values = (ui_change_values *) malloc(sizeof (ui_change_values) * widget_count);

		if (groups && groups_count > 1) {
			uint8_t current_group;
			uint16_t tab_widget_count;
			notebook = gtk_notebook_new();
			/* Switch to vertical notebook mode when many groups. */
			if (groups_count > 4) {
				gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);
			}
			gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), notebook, false, false, 0);
			tables = (GtkWidget **) malloc(sizeof(GtkWidget *) * groups_count);
			for (current_group = 0; current_group < groups_count; current_group++) {
				tab_widget_count = 0;
				for (j = 0; j < params_count; j ++) {
					if (params[j].group == current_group) {
						tab_widget_count++;
					}
				}

				if (tab_widget_count) {
					tables[current_group] = gtk_table_new(tab_widget_count, 1, false);
					gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tables[current_group], new QLabel(groups[current_group]));
				}
			}
		} else {
			table = gtk_table_new(widget_count, 1, false);
			gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), table, false, false, 0);
		}

		for (i = 0, j = 0; i < params_count; i++) {
			if (params[i].group_id != PARAMETER_GROUP_HIDDEN) {
				if (tables) {
					table = tables[MAX(0, params[i].group_id)]; /* Round up NOT_IN_GROUP, that's not reasonable here. */
				}

				widgets[j] = a_uibuilder_new_widget (&(params[i]), getparam(pass_along_getparam, i, false));

				if (widgets[j]) {
					labels[j] = new QLabel(QObject::tr(params[i].title));
					gtk_table_attach(GTK_TABLE(table), labels[j], 0, 1, j, j+1, (GtkAttachOptions) 0, (GtkAttachOptions) 0, 0, 0);
					gtk_table_attach(GTK_TABLE(table), widgets[j], 1, 2, j, j+1, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 2, 2);

					if (changeparam) {
						change_values[j].layer = pass_along1;
						change_values[j].param = &params[i];
						change_values[j].param_id = (int) i;
						change_values[j].widgets = widgets;
						change_values[j].labels = labels;

						switch (params[i].widget_type) {
							/* Change conditions for other widget types can be added when needed. */
						case WidgetType::COMBOBOX:
							QObject::connect(widgets[j], SIGNAL("changed"), &change_values[j], SLOT (changeparam));
							break;
						case WidgetType::CHECKBUTTON:
							QObject::connect(widgets[j], SIGNAL("toggled"), SLOT (changeparam));
							break;
						default:
							break;
						}
					}
				}
				j++;
			}
		}

		/* Repeat run through to force changeparam callbacks now that the widgets have been created.
		   This primarily so the widget sensitivities get set up. */
		if (changeparam) {
			for (i = 0, j = 0; i < params_count; i++) {
				if (params[i].group_id != PARAMETER_GROUP_HIDDEN) {
					if (widgets[j]) {
						changeparam(widgets[j], &change_values[j]);
					}
					j++;
				}
			}
		}

		if (response_w) {
			gtk_widget_grab_focus(response_w);
		}

		gtk_widget_show_all(dialog);

		resp = gtk_dialog_run(GTK_DIALOG (dialog));
		if (resp == GTK_RESPONSE_ACCEPT){
			for (i = 0, j = 0; i < params_count; i++) {
				if (params[i].group_id != PARAMETER_GROUP_HIDDEN) {
					if (setparam(pass_along1,
						     i,
						     a_uibuilder_widget_get_value(widgets[j], &(params[i])),
						     pass_along2,
						     false)) {

						must_redraw = true;
					}
					j++;
				}
			}

			free(widgets);
			free(labels);
			free(change_values);
			if (tables) {
				free(tables);
			}

			gtk_widget_destroy(dialog); /* Hide before redrawing. */

			return must_redraw ? 2 : 3; /* user clicked OK */
		}

		free(widgets);
		free(labels);
		free(change_values);
		if (tables) {
			free(tables);
		}
		gtk_widget_destroy(dialog);

		return 0;
	}
}





SGVariant *a_uibuilder_run_dialog(const char *dialog_name, Window * parent, ParameterSpecification *params,
				       uint16_t params_count, char **groups, uint8_t groups_count,
				       SGVariant *params_defaults)
{
	SGVariant * paramdatas = (SGVariant *) malloc(params_count * sizeof (SGVariant));
	if (a_uibuilder_properties_factory(dialog_name,
					   parent,
					   params,
					   params_count,
					   groups,
					   groups_count,
					   (bool (*)(void*, uint16_t, SGVariant, void*, bool)) uibuilder_run_setparam,
					   paramdatas,
					   params,
					   (SGVariant (*)(void*, uint16_t, bool)) uibuilder_run_getparam,
					   params_defaults,
					   NULL) > 0) {

		return paramdatas;
	}
	free(paramdatas);
	return NULL;
}




GtkWidget *a_uibuilder_new_widget(ParameterSpecification *param, SGVariant data)
{
	/* Perform pre conversion if necessary. */
	SGVariant var = data;
	if (param->convert_to_display) {
		var = param->convert_to_display(data);
	}

	GtkWidget *rv = NULL;
	switch (param->widget_type) {
	case WidgetType::COMBOBOX:
		if (param->type == SGVariantType::UINT && param->widget_data) {
			/* Build a simple combobox. */
			char **pstr = (char **) param->widget_data;
			rv = new QComboBox();
			while (*pstr) {
				vik_combo_box_text_append(rv, *(pstr++));
			}

			if (param->extra_widget_data) { /* Map of alternate uint values for options. */
				/* Set the effective default value. */
				int i;
				for (i = 0; ((const char **)param->widget_data)[i]; i++)
					if (((unsigned int *)param->extra_widget_data)[i] == var.u) {
						/* Match default value. */
						rv->setCurrentIndex(i);
						break;
					}
			} else {
				rv->setCurrentIndex(var.u);
			}
		} else if (param->type == SGVariantType::STRING && param->widget_data && !param->extra_widget_data) {
			/* Build a combobox with editable text. */
			char **pstr = (char **) param->widget_data;
			rv = new QComboBox();
			if (var.s) {
				vik_combo_box_text_append(rv, var.s);
			}

			while (*pstr) {
				vik_combo_box_text_append(rv, *(pstr++));
			}

			if (var.s) {
				rv->setCurrentIndex(0);
			}
		} else if (param->type == SGVariantType::STRING && param->widget_data && param->extra_widget_data) {
			/* Build a combobox with fixed selections without editable text. */
			char **pstr = (char **) param->widget_data;
			rv = new QComboBox();
			while (*pstr) {
				vik_combo_box_text_append(rv, *(pstr++));
			}
			if (var.s) {
				/* Set the effective default value. */
				/* In case of value does not exist, set the first value. */
				rv->setCurrentIndex(0);
				int i;
				for (i = 0; ((const char **)param->widget_data)[i]; i++)
					if (strcmp(((const char **)param->extra_widget_data)[i], var.s) == 0) {
						/* Match default value. */
						rv->setCurrentIndex(i);
						break;
					}
			} else {
				rv->setCurrentIndex(0);
			}
		}
		break;
	case WidgetType::RADIO_unused_GROUP:
		/* widget_data and extra_widget_data are GList. */
		if (param->type == SGVariantType::UINT && param->widget_data) {
			rv = vik_radio_group_new((GList *) param->widget_data);
			if (param->extra_widget_data) { /* Map of alternate uint values for options. */
				int nb_elem = g_list_length((GList *) param->widget_data);
				for (int i = 0; i < nb_elem; i++)
					if (KPOINT_unused_ER_TO_UINT (g_list_nth_data((GList *) param->extra_widget_data, i)) == var.u) {
						vik_radio_group_set_selected(VIK_RADIO_GROUP(rv), i);
						break;
					}
			} else if (var.u) { /* Zero is already default. */
				vik_radio_group_set_selected(VIK_RADIO_GROUP(rv), var.u);
			}
		}
		break;
	case WidgetType::RADIO_unused_GROUP_STATIC:
		if (param->type == SGVariantType::UINT && param->widget_data) {
			rv = vik_radio_group_new_static((const char **) param->widget_data);
			if (param->extra_widget_data) { /* Map of alternate uint values for options. */
				for (int i = 0; ((const char **)param->widget_data)[i]; i++)
					if (((unsigned int *)param->extra_widget_data)[i] == var.u) {
						vik_radio_group_set_selected(VIK_RADIO_GROUP(rv), i);
						break;
					}
			} else if (var.u) { /* Zero is already default. */
				vik_radio_group_set_selected(VIK_RADIO_GROUP(rv), var.u);
			}
		}
		break;
	default: break;
	}
	if (rv && !gtk_widget_get_tooltip_text(rv)) {
		if (param->tooltip) {
			rv->setToolTip(param->tooltip);
		}
	}
	return rv;
}




#endif
