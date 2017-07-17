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

#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSlider>

#include "layer.h"
#include "uibuilder_qt.h"
#include "globals.h"
#include "widget_color_button.h"
#include "widget_file_list.h"
#include "widget_file_entry.h"
#include "widget_radio_group.h"
#include "widget_slider.h"
#include "uibuilder.h"
#include "waypoint_properties.h"
#include "date_time_dialog.h"
#include "waypoint.h"




using namespace SlavGPS;



#if 0
GtkWidget *a_uibuilder_new_widget(LayerParam *param, SGVariant data)
{
}




SGVariant a_uibuilder_widget_get_value(GtkWidget *widget, LayerParam *param)
{

}




//static void draw_to_image_file_total_area_cb (QSpinBox * spinbutton, void * *pass_along)
int a_uibuilder_properties_factory(const char * dialog_name,
				   QWindow * parent,
				   Parameter * params,
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
		if (params[i].group != VIK_LAYER_NOT_IN_PROPERTIES) {
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
			if (params[i].group != VIK_LAYER_NOT_IN_PROPERTIES) {
				if (tables) {
					table = tables[MAX(0, params[i].group)]; /* Round up NOT_IN_GROUP, that's not reasonable here. */
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
				if (params[i].group != VIK_LAYER_NOT_IN_PROPERTIES) {
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
				if (params[i].group != VIK_LAYER_NOT_IN_PROPERTIES) {
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





SGVariant *a_uibuilder_run_dialog(const char *dialog_name, Window * parent, LayerParam *params,
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

#endif







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
		param_id_t group_id = iter->second->group;

		auto form_iter = this->forms.find(group_id);
		QFormLayout * form = NULL;
		if (form_iter == this->forms.end()) {
			QString page_label = tr("Properties");
			form = this->insert_tab(page_label);

			this->forms.insert(std::pair<param_id_t, QFormLayout *>(group_id, form));

			qDebug() << "II: Preferences Builder: created tab" << page_label;
		} else {
			form = form_iter->second;
		}



		SGVariant param_value = preferences->get_param_value(iter->first);
		QString label = QString(iter->second->title);
		QWidget * widget = this->new_widget(iter->second, param_value);
		form->addRow(label, widget);
		qDebug() << "II: UI Builder: adding widget #" << iter->first << iter->second->title << widget;
		this->widgets.insert(std::pair<param_id_t, QWidget *>(iter->first, widget));
	}
}




void PropertiesDialog::fill(Layer * layer)
{
	qDebug() << "\nII: UI Builder: creating Properties Dialog from layer" << layer->get_name();

	std::map<param_id_t, Parameter *> * params = layer->get_interface()->layer_parameters;

	for (auto iter = params->begin(); iter != params->end(); iter++) {
		param_id_t group_id = iter->second->group;
		if (group_id == VIK_LAYER_NOT_IN_PROPERTIES) {
			continue;
		}

		auto form_iter = this->forms.find(group_id);
		QFormLayout * form = NULL;
		if (form_iter == this->forms.end()) {
			/* Add new tab. */
			QString page_label = layer->get_interface()->params_groups
				? layer->get_interface()->params_groups[group_id]
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
		form->addRow(label, widget);
		qDebug() << "II: UI Builder: adding widget #" << iter->first << iter->second->title << widget;
		this->widgets.insert(std::pair<param_id_t, QWidget *>(iter->first, widget));
	}
}




void PropertiesDialog::fill(LayerInterface * interface)
{
	qDebug() << "\nII: UI Builder: creating Properties Dialog from layer interface";

	std::map<param_id_t, Parameter *> * params = interface->layer_parameters;
	std::map<param_id_t, SGVariant> * values = interface->parameter_value_defaults;

	for (auto iter = params->begin(); iter != params->end(); iter++) {
		param_id_t group_id = iter->second->group;
		if (group_id == VIK_LAYER_NOT_IN_PROPERTIES) {
			iter++;
			continue;
		}

		auto form_iter = this->forms.find(group_id);
		QFormLayout * form = NULL;
		if (form_iter == this->forms.end()) {
			QString page_label = interface->params_groups
				? interface->params_groups[group_id]
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
		qDebug() << "II: UI Builder: adding widget #" << iter->first << iter->second->title << widget;
		this->widgets.insert(std::pair<param_id_t, QWidget *>(iter->first, widget));
	}
}




void PropertiesDialog::fill(Waypoint * wp, Parameter * parameters)
{
	qDebug() << "\nII: UI Builder: creating Properties Dialog from waypoint";

	int i = 0;
	QFormLayout * form = this->insert_tab(tr("Properties"));
	this->forms.insert(std::pair<param_id_t, QFormLayout *>(parameters[SG_WP_PARAM_NAME].group, form));
	SGVariant param_value; // = layer->get_param_value(i, false);
	Parameter * param = NULL;
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

	param = &parameters[SG_WP_PARAM_NAME];
	param_value.s = wp->name;
	widget = this->new_widget(param, param_value);
	form->addRow(QString(param->title), widget);
	qDebug() << "II: UI Builder: adding widget #" << param->id << param->title << widget;
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param->id, widget));

	param = &parameters[SG_WP_PARAM_LAT];
	param_value.s = lat;
	widget = this->new_widget(param, param_value);
	form->addRow(QString(param->title), widget);
	qDebug() << "II: UI Builder: adding widget #" << param->id << param->title << widget;
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param->id, widget));

	param = &parameters[SG_WP_PARAM_LON];
	param_value.s = lon;
	widget = this->new_widget(param, param_value);
	form->addRow(QString(param->title), widget);
	qDebug() << "II: UI Builder: adding widget #" << param->id << param->title << widget;
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param->id, widget));

	param = &parameters[SG_WP_PARAM_TIME];
	param_value.u = wp->timestamp;
	widget = this->new_widget(param, param_value);
	form->addRow(QString(param->title), widget);
	qDebug() << "II: UI Builder: adding widget #" << param->id << param->title << widget;
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param->id, widget));

	param = &parameters[SG_WP_PARAM_ALT];
	param_value.s = alt;
	widget = this->new_widget(param, param_value);
	form->addRow(QString(param->title), widget);
	qDebug() << "II: UI Builder: adding widget #" << param->id << param->title << widget;
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param->id, widget));

	param = &parameters[SG_WP_PARAM_COMMENT];
	param_value.s = wp->comment;
	widget = this->new_widget(param, param_value);
	form->addRow(QString(param->title), widget);
	qDebug() << "II: UI Builder: adding widget #" << param->id << param->title << widget;
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param->id, widget));

	param = &parameters[SG_WP_PARAM_DESC];
	param_value.s = wp->description;
	widget = this->new_widget(param, param_value);
	form->addRow(QString(param->title), widget);
	qDebug() << "II: UI Builder: adding widget #" << param->id << param->title << widget;
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param->id, widget));

	param = &parameters[SG_WP_PARAM_IMAGE];
	param_value.s = wp->image;
	widget = this->new_widget(param, param_value);
	form->addRow(QString(param->title), widget);
	qDebug() << "II: UI Builder: adding widget #" << param->id << param->title << widget;
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param->id, widget));

	param = &parameters[SG_WP_PARAM_SYMBOL];
	param_value.s = wp->symbol;
	widget = this->new_widget(param, param_value);
	form->addRow(QString(param->title), widget);
	qDebug() << "II: UI Builder: adding widget #" << param->id << param->title << widget;
	this->widgets.insert(std::pair<param_id_t, QWidget *>(param->id, widget));
}



std::map<param_id_t, Parameter *>::iterator PropertiesDialog::add_widgets_to_tab(QFormLayout * form, Layer * layer, std::map<param_id_t, Parameter *>::iterator & iter, std::map<param_id_t, Parameter *>::iterator & end)
{
	param_id_t i = 0;
	int last_group = iter->second->group;

	qDebug() << "II: UI Builder: vvvvvvvvvv adding widgets to group" << last_group << ":";

	while (iter != end && iter->second->group == last_group) {

		if (!iter->second->title) {
			iter++;
			continue;
		}
		if (iter->second->group == VIK_LAYER_NOT_IN_PROPERTIES) {
			iter++;
			continue;
		}

		SGVariant param_value = layer->get_param_value(iter->first, false);

		QString label = QString(iter->second->title);
		QWidget * widget = this->new_widget(iter->second, param_value);

		form->addRow(label, widget);
		qDebug() << "II: UI Builder: adding widget" << widget;
		this->widgets.insert(std::pair<param_id_t, QWidget *>(iter->first, widget));

		last_group = iter->second->group;
		i++;
		iter++;
	}

	qDebug() << "II: UI Builder ^^^^^^^^^^ added new" << i << "widgets in this tab (" << widgets.size() << "in total)";
	return iter;
}




QWidget * PropertiesDialog::new_widget(Parameter * param, SGVariant param_value)
{
	/* Perform pre conversion if necessary. */
	SGVariant vlpd = param_value;
	if (param->convert_to_display) {
		vlpd = param->convert_to_display(param_value);
	}

	QWidget * widget = NULL;
	switch (param->widget_type) {

	case WidgetType::COLOR:
		if (param->type == SGVariantType::COLOR) {
			qDebug() << "II: UI Builder: creating color button with colors" << vlpd.c.r << vlpd.c.g << vlpd.c.b << vlpd.c.a;
			QColor color(vlpd.c.r, vlpd.c.g, vlpd.c.b, vlpd.c.a);
			SGColorButton * widget_ = new SGColorButton(color, NULL);

			//widget_->setStyleSheet("* { border: none; background-color: rgb(255,125,100) }");
			widget = widget_;
		}
		break;

	case WidgetType::CHECKBUTTON:
		if (param->type == SGVariantType::BOOLEAN) {
			QCheckBox * widget_ = new QCheckBox;
			if (vlpd.b) {
				widget_->setCheckState(Qt::Checked);
			}
			widget = widget_;
		}
		break;

	case WidgetType::COMBOBOX: {
		assert (param->widget_data);

		QComboBox * widget_ = new QComboBox(this);

		label_id_t * values = (label_id_t *) param->widget_data;
		int i = 0;
		int selected_idx = 0;
		while (values[i].label) {
			QString label(values[i].label);
			if (param->type == SGVariantType::UINT) {
				widget_->addItem(label, QVariant((uint32_t) values[i].id));
				if (param_value.u == (uint32_t) values[i].id) {
					selected_idx = i;
				}
			} else if (param->type == SGVariantType::INT) {
				widget_->addItem(label, QVariant((int32_t) values[i].id));
				if (param_value.i == (int32_t) values[i].id) {
					selected_idx = i;
				}
			} else if (param->type == SGVariantType::STRING) {
				/* TODO: implement. */
			} else {
				qDebug() << "EE: UI Builder: set: unsupported parameter type for combobox:" << (int) param->type;
			}

			i++;
		}

		widget_->setCurrentIndex(selected_idx);

		widget = widget_;
	}
		break;

#if 0
	case WidgetType::RADIOGROUP:
		/* widget_data and extra_widget_data are GList. */
		if (param->type == SGVariantType::UINT && param->widget_data) {
			rv = vik_radio_group_new((GList *) param->widget_data);
			if (param->extra_widget_data) { /* Map of alternate uint values for options. */
				int i;
				int nb_elem = g_list_length((GList *) param->widget_data);
				for (i = 0; i < nb_elem; i++)
					if (KPOINTER_TO_UINT (g_list_nth_data((GList *) param->extra_widget_data, i)) == vlpd.u) {
						vik_radio_group_set_selected(VIK_RADIO_GROUP(rv), i);
						break;
					}
			} else if (vlpd.u) { /* Zero is already default. */
				vik_radio_group_set_selected(VIK_RADIO_GROUP(rv), vlpd.u);
			}
		}
		break;
#endif


	case WidgetType::RADIOGROUP_STATIC:
#if 0
		rv = vik_radio_group_new_static((const char **) param->widget_data);
			if (param->extra_widget_data) { /* Map of alternate uint values for options. */
				int i;
				for (i = 0; ((const char **)param->widget_data)[i]; i++)
					if (((unsigned int *)param->extra_widget_data)[i] == vlpd.u) {
						vik_radio_group_set_selected(VIK_RADIO_GROUP(rv), i);
						break;
					}
			} else if (vlpd.u) { /* Zero is already default. */
				vik_radio_group_set_selected(VIK_RADIO_GROUP(rv), vlpd.u);
			}
		}
		break;
#else
		if (param->type == SGVariantType::UINT && param->widget_data) {
			QStringList labels;
			for (int i = 0; ((const char **)param->widget_data)[i]; i++) {
				QString label (((const char **)param->widget_data)[i]);
				labels << label;
			}
			QString title("");
			SGRadioGroup * widget_ = new SGRadioGroup(title, labels, this);

			widget = widget_;
		}
		break;
#endif
	case WidgetType::SPINBUTTON:
		if ((param->type == SGVariantType::UINT || param->type == SGVariantType::INT)
		    && param->widget_data) {

			int init_val = param->type == SGVariantType::UINT ? vlpd.u : vlpd.i;
			ParameterScale * scale = (ParameterScale *) param->widget_data;
			QSpinBox * widget_ = new QSpinBox();
			widget_->setMinimum(scale->min);
			widget_->setMaximum(scale->max);
			widget_->setSingleStep(scale->step);
			widget_->setValue(init_val);
			//scale->digits

			widget = widget_;
		}
		break;

	case WidgetType::SPINBOX_DOUBLE:
		if (param->type == SGVariantType::DOUBLE
		    && param->widget_data) {

			double init_val = vlpd.d;
			ParameterScale * scale = (ParameterScale *) param->widget_data;
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

	case WidgetType::ENTRY:
		if (param->type == SGVariantType::STRING) {
			QLineEdit * widget_ = new QLineEdit;
			if (vlpd.s) {
				widget_->insert(QString(vlpd.s));
			}
			widget = widget_;
		}
		break;
	case WidgetType::PASSWORD:
		if (param->type == SGVariantType::STRING) {
			QLineEdit * widget_ = new QLineEdit();
			widget_->setEchoMode(QLineEdit::Password);
			if (vlpd.s) {
				widget_->setText(QString(vlpd.s));
			}
			widget_->setToolTip(QObject::tr("Notice that this password will be stored clearly in a plain file."));
		}
		break;
	case WidgetType::FILEENTRY:
		if (param->type == SGVariantType::STRING) {
			QString title("Select file");
			SGFileEntry * widget_ = new SGFileEntry(QFileDialog::Option(0), QFileDialog::ExistingFile, title, NULL);
			if (vlpd.s) {
				QString filename(vlpd.s);
				widget_->set_filename(filename);
			}
			widget = widget_;
		}
		break;

	case WidgetType::FOLDERENTRY:
		if (param->type == SGVariantType::STRING) {
			QString title("Select file");
			SGFileEntry * widget_ = new SGFileEntry(QFileDialog::Option(0), QFileDialog::Directory, title, NULL);
			if (vlpd.s) {
				QString filename(vlpd.s);
				widget_->set_filename(filename);
			}
			widget = widget_;
		}
		break;
	case WidgetType::FILELIST:
		if (param->type == SGVariantType::STRING_LIST) {
			SGFileList * widget_ = new SGFileList(param->title, *vlpd.sl, this);
			widget = widget_;
		}
		break;
	case WidgetType::HSCALE:
		if ((param->type == SGVariantType::UINT || param->type == SGVariantType::INT)
		    && param->widget_data) {

			int init_value = (param->type == SGVariantType::UINT ? ((int) vlpd.u) : vlpd.i);
			ParameterScale * scale = (ParameterScale *) param->widget_data;

			SGSlider * widget_ = new SGSlider(*scale, Qt::Horizontal);
			widget_->set_value(init_value);
			widget = widget_;
		}
		break;
#ifdef K
	case WidgetType::HSCALE:
		if (param->type == SGVariantType::DOUBLE && param->widget_data) {

			double init_val = vlpd.d;
			ParameterScale * scale = (ParameterScale *) param->widget_data;
			rv = gtk_hscale_new_with_range(scale->min, scale->max, scale->step);
			gtk_scale_set_digits(GTK_SCALE(rv), scale->digits);
			gtk_range_set_value(GTK_RANGE(rv), init_val);
		}
		break;
	case WidgetType::BUTTON:
		if (param->type == SGVariantType::PTR && param->widget_data) {
			rv = gtk_button_new_with_label((const char *) param->widget_data);
			QObject::connect(rv, SIGNAL("clicked"), param->extra_widget_data, SLOT (vlpd.ptr));
		}
		break;
#endif
	case WidgetType::DATETIME: {
			SGDateTime * widget_ = new SGDateTime(param_value.u, this);
			widget = widget_;
		}
		break;
	default:
		break;
	}
#if 0
	if (rv && !gtk_widget_get_tooltip_text(rv)) {
		if (param->tooltip) {
			rv->setToolTip(QObject::tr(param->tooltip);
		}
	}
#endif
	return widget;
}




SGVariant PropertiesDialog::get_param_value(param_id_t id, Parameter * param)
{
	qDebug() << "DD: UI Builder: getting value of widget" << (int) id << "/" << (int) this->widgets.size();

	SGVariant rv = { 0 };
	QWidget * widget = this->widgets[id];
	if (!widget) {
		if (param->group == VIK_LAYER_NOT_IN_PROPERTIES) {
			qDebug() << "DD: UI Builder: widget is 'not in properties'";
		} else {
			qDebug() << "EE: UI Builder: widget not found";
		}
		return rv;
	}

	switch (param->widget_type) {
	case WidgetType::COLOR: {
		QColor c = ((SGColorButton *) widget)->get_color();
		rv.c.r = c.red();
		rv.c.g = c.green();
		rv.c.b = c.blue();
		rv.c.a = c.alpha();
	}
		break;
	case WidgetType::CHECKBUTTON:
		rv.b = ((QCheckBox *) widget)->isChecked();
		break;

	case WidgetType::COMBOBOX:
		if (param->type == SGVariantType::UINT) {
			rv.u = (uint32_t) ((QComboBox *) widget)->currentData().toUInt();
		} else if (param->type == SGVariantType::INT) {
			rv.i = (int32_t) ((QComboBox *) widget)->currentData().toInt();
		} else if (param->type == SGVariantType::STRING) {
			/* TODO: implement */
		} else {
			qDebug() << "EE: UI Builder: get: unsupported parameter type for combobox:" << (int) param->type;
		}

		break;

	case WidgetType::RADIOGROUP:
	case WidgetType::RADIOGROUP_STATIC:
		rv.u = ((SGRadioGroup *) widget)->get_selected();
		if (param->extra_widget_data) {
			rv.u = KPOINTER_TO_UINT (g_list_nth_data((GList *) param->extra_widget_data, rv.u));
		}
		break;
	case WidgetType::SPINBUTTON:
		if (param->type == SGVariantType::UINT) {
			rv.u = ((QSpinBox *) widget)->value();
		} else {
			rv.i = ((QSpinBox *) widget)->value();
		}
		break;

	case WidgetType::SPINBOX_DOUBLE:
		rv.d = ((QDoubleSpinBox *) widget)->value();
		qDebug() << "II: UI Builder: saving value of Spinbox Double:" << rv.d;
		break;

	case WidgetType::ENTRY:
	case WidgetType::PASSWORD:
		rv.s = strdup((char *) ((QLineEdit *) widget)->text().toUtf8().constData()); /* FIXME: memory management. */
		qDebug() << "II: UI Builder: saving value of Entry or Password:" << rv.s;
		break;

	case WidgetType::FILEENTRY:
	case WidgetType::FOLDERENTRY:
		rv.s = strdup(((SGFileEntry *) widget)->get_filename().toUtf8().constData()); /* FIXME: memory management. */
		break;

	case WidgetType::FILELIST:
		rv.sl = new QStringList(((SGFileList *) widget)->get_list());
		for (auto iter = rv.sl->constBegin(); iter != rv.sl->constEnd(); iter++) {
			qDebug() << "File on retrieved list: " << QString(*iter);
		}
		break;
	case WidgetType::HSCALE:
		if (param->type == SGVariantType::UINT) {
			rv.u = (uint32_t) ((SGSlider *) widget)->get_value();
		} else if (param->type == SGVariantType::INT) {
			rv.i = (int32_t) ((SGSlider *) widget)->get_value();
		} else {
#ifdef K
			rv.d = gtk_range_get_value(GTK_RANGE(widget));
#endif
		}
		break;
	case WidgetType::DATETIME:
		rv.u = ((SGDateTime *) widget)->value();
		qDebug() << "II: UI Builder: saving value of time stamp:" << rv.u;
		break;
	default:
		break;
	}

	/* Perform conversion if necessary. */
	if (param->convert_to_internal) {
		rv = param->convert_to_internal(rv);
	}

	return rv;
}
