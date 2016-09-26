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

#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>

#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>

#include <glib/gi18n.h>

#include "layer.h"
#include "uibuilder_qt.h"
#include "globals.h"
#include "widget_color_button.h"
#include "widget_file_list.h"
#include "widget_radio_group.h"




using namespace SlavGPS;



#if 0
GtkWidget *a_uibuilder_new_widget(LayerParam *param, LayerParamValue data)
{
}




LayerParamValue a_uibuilder_widget_get_value(GtkWidget *widget, LayerParam *param)
{

}




//static void draw_to_image_file_total_area_cb (GtkSpinButton *spinbutton, void * *pass_along)
int a_uibuilder_properties_factory(const char * dialog_name,
				   QWindow * parent,
				   LayerParam * params,
				   uint16_t params_count,
				   char ** groups,
				   uint8_t groups_count,
				   bool (* setparam) (void *, uint16_t, LayerParamValue, void *, bool),
				   void * pass_along1,
				   void * pass_along2,
				   LayerParamValue (* getparam) (void *, uint16_t, bool),
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
		GtkWidget **labels = (GtkWidget **) malloc(sizeof(GtkWidget *) * widget_count);
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
					gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tables[current_group], gtk_label_new(groups[current_group]));
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
					labels[j] = gtk_label_new(_(params[i].title));
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
						case LayerWidgetType::COMBOBOX:
							g_signal_connect(G_OBJECT(widgets[j]), "changed", G_CALLBACK(changeparam), &change_values[j]);
							break;
						case LayerWidgetType::CHECKBUTTON:
							g_signal_connect(G_OBJECT(widgets[j]), "toggled", G_CALLBACK(changeparam), &change_values[j]);
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





LayerParamValue *a_uibuilder_run_dialog(const char *dialog_name, GtkWindow *parent, LayerParam *params,
				       uint16_t params_count, char **groups, uint8_t groups_count,
				       LayerParamValue *params_defaults)
{
	LayerParamValue * paramdatas = (LayerParamValue *) malloc(params_count * sizeof (LayerParamValue));
	if (a_uibuilder_properties_factory(dialog_name,
					   parent,
					   params,
					   params_count,
					   groups,
					   groups_count,
					   (bool (*)(void*, uint16_t, LayerParamValue, void*, bool)) uibuilder_run_setparam,
					   paramdatas,
					   params,
					   (LayerParamValue (*)(void*, uint16_t, bool)) uibuilder_run_getparam,
					   params_defaults,
					   NULL) > 0) {

		return paramdatas;
	}
	free(paramdatas);
	return NULL;
}

#endif







LayerPropertiesDialog::LayerPropertiesDialog(QWidget * parent) : QDialog(parent)
{
	this->button_box = new QDialogButtonBox();
	this->ok = this->button_box->addButton("OK", QDialogButtonBox::AcceptRole);
	this->cancel = this->button_box->addButton("Cancel", QDialogButtonBox::RejectRole);


	this->tabs = new QTabWidget();
	this->vbox = new QVBoxLayout;


	//this->vbox->addLayout(this->form);
	this->vbox->addWidget(this->tabs);
	this->vbox->addWidget(this->button_box);

	QLayout * old = this->layout();
	delete old;

	this->setLayout(this->vbox);

        connect(button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(button_box, SIGNAL(rejected()), this, SLOT(reject()));
}




QFormLayout * LayerPropertiesDialog::insert_tab(QString & label)
{
	QFormLayout * form = new QFormLayout();
	QWidget * page = new QWidget();
	QLayout * l = page->layout();
	delete l;
	page->setLayout(form);

	this->tabs->addTab(page, label);

	return form;
}


LayerPropertiesDialog::~LayerPropertiesDialog()
{
	delete this->ok;
	delete this->cancel;
	delete this->button_box;
}




void LayerPropertiesDialog::fill(LayerParam * params, uint16_t params_count)
{
	QString label("page");
	QFormLayout * form = this->insert_tab(label);
	LayerParamValue param_value;
	for (uint16_t i = 0; i < params_count; i++) {
		QString label = QString(params[i].title);
		QWidget * widget = this->new_widget(&params[i], param_value);
		form->addRow(label, widget);
		qDebug() << ">>>> pushing back %x" << widget;
		this->widgets.insert(std::pair<layer_param_id_t, QWidget *>(i, widget));
	}
	qDebug() << "^^^^^^^^^ there are " << this->widgets.size() << "widgets";
}



void LayerPropertiesDialog::fill(Layer * layer)
{
	std::map<layer_param_id_t, LayerParam *> * params = layer->get_interface()->layer_parameters;
	if (!params) {
		return;
	}

	QString tab_label("page");
	QFormLayout * form = this->insert_tab(tab_label);

	for (auto iter = params->begin(); iter != params->end(); iter++) {

		LayerParamValue param_value = layer->get_param_value(iter->first, false);

		QString label = QString(iter->second->title);
		QWidget * widget = this->new_widget(iter->second, param_value);

		form->addRow(label, widget);
		qDebug() << ">>>> inserting parameter widget" << widget;
		this->widgets.insert(std::pair<layer_param_id_t, QWidget *>(iter->first, widget));
	}
	qDebug() << "^^^^^^^^^ there are " << this->widgets.size() << "widgets";
}




QWidget * LayerPropertiesDialog::new_widget(LayerParam * param, LayerParamValue param_value)
{
	/* Perform pre conversion if necessary. */
	LayerParamValue vlpd = param_value;
	if (param->convert_to_display) {
		vlpd = param->convert_to_display(param_value);
	}

	QWidget * widget = NULL;
	switch (param->widget_type) {
#if 1
	case LayerWidgetType::COLOR:
		if (param->type == LayerParamType::COLOR) {
			QColor color(vlpd.c.r, vlpd.c.g, vlpd.c.b, vlpd.c.a);
			SGColorButton * widget_ = new SGColorButton(color, NULL);

			//widget_->setStyleSheet("* { border: none; background-color: rgb(255,125,100) }");
			widget = widget_;
		}
		break;
#endif
	case LayerWidgetType::CHECKBUTTON:
	    if (param->type == LayerParamType::BOOLEAN) {
		    QCheckBox * widget_ = new QCheckBox;
		    if (vlpd.b) {
			    widget_->setCheckState(Qt::Checked);
		    }
		    widget = widget_;
	    }
	    break;
#if 0
	case LayerWidgetType::COMBOBOX:
		if (param->type == LayerParamType::UINT && param->widget_data) {
			/* Build a simple combobox. */
			char **pstr = (char **) param->widget_data;
			rv = vik_combo_box_text_new();
			while (*pstr) {
				vik_combo_box_text_append(rv, *(pstr++));
			}

			if (param->extra_widget_data) { /* Map of alternate uint values for options. */
				/* Set the effective default value. */
				int i;
				for (i = 0; ((const char **)param->widget_data)[i]; i++)
					if (((unsigned int *)param->extra_widget_data)[i] == vlpd.u) {
						/* Match default value. */
						gtk_combo_box_set_active(GTK_COMBO_BOX(rv), i);
						break;
					}
			} else {
				gtk_combo_box_set_active(GTK_COMBO_BOX (rv), vlpd.u);
			}
		} else if (param->type == LayerParamType::STRING && param->widget_data && !param->extra_widget_data) {
			/* Build a combobox with editable text. */
			char **pstr = (char **) param->widget_data;
#if GTK_CHECK_VERSION (2, 24, 0)
			rv = gtk_combo_box_text_new_with_entry();
#else
			rv = gtk_combo_box_entry_new_text();
#endif
			if (vlpd.s) {
				vik_combo_box_text_append(rv, vlpd.s);
			}

			while (*pstr) {
				vik_combo_box_text_append(rv, *(pstr++));
			}

			if (vlpd.s) {
				gtk_combo_box_set_active(GTK_COMBO_BOX (rv), 0);
			}
		} else if (param->type == LayerParamType::STRING && param->widget_data && param->extra_widget_data) {
			/* Build a combobox with fixed selections without editable text. */
			char **pstr = (char **) param->widget_data;
			rv = GTK_WIDGET (vik_combo_box_text_new());
			while (*pstr) {
				vik_combo_box_text_append(rv, *(pstr++));
			}
			if (vlpd.s) {
				/* Set the effective default value. */
				/* In case of value does not exist, set the first value. */
				gtk_combo_box_set_active(GTK_COMBO_BOX (rv), 0);
				int i;
				for (i = 0; ((const char **)param->widget_data)[i]; i++)
					if (strcmp(((const char **)param->extra_widget_data)[i], vlpd.s) == 0) {
						/* Match default value. */
						gtk_combo_box_set_active(GTK_COMBO_BOX (rv), i);
						break;
					}
			} else {
				gtk_combo_box_set_active(GTK_COMBO_BOX (rv), 0);
			}
		}
		break;
	case LayerWidgetType::RADIOGROUP:
		/* widget_data and extra_widget_data are GList. */
		if (param->type == LayerParamType::UINT && param->widget_data) {
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


	case LayerWidgetType::RADIOGROUP_STATIC:
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
		if (param->type == LayerParamType::UINT && param->widget_data) {
			std::list<QString> labels;
			for (int i = 0; ((const char **)param->widget_data)[i]; i++) {
				QString label (((const char **)param->widget_data)[i]);
				labels.push_back(label);
			}
			QString title("");
			SGRadioGroup * widget_ = new SGRadioGroup(title, labels, this);

			widget = widget_;
		}
		break;
#endif
	case LayerWidgetType::SPINBUTTON:
		if ((param->type == LayerParamType::UINT || param->type == LayerParamType::INT)
		    && param->widget_data) {

			int init_val = param->type == LayerParamType::UINT ? vlpd.u : vlpd.i;
			LayerParamScale * scale = (LayerParamScale *) param->widget_data;
			QSpinBox * widget_ = new QSpinBox();
			widget_->setValue(init_val);
			widget_->setMinimum(scale->min);
			widget_->setMaximum(scale->max);
			widget_->setSingleStep(scale->step);
			//scale->digits

			widget = widget_;
		}
		break;

	case LayerWidgetType::SPINBOX_DOUBLE:
		if (param->type == LayerParamType::DOUBLE
		    && param->widget_data) {

			double init_val = vlpd.d;
			LayerParamScale * scale = (LayerParamScale *) param->widget_data;
			QDoubleSpinBox * widget_ = new QDoubleSpinBox();
			widget_->setValue(init_val);
			widget_->setMinimum(scale->min);
			widget_->setMaximum(scale->max);
			widget_->setSingleStep(scale->step);
			widget_->setDecimals(scale->digits);

			widget = widget_;
		}
		break;

	case LayerWidgetType::ENTRY:
		if (param->type == LayerParamType::STRING) {
			QLineEdit * widget_ = new QLineEdit;
			if (vlpd.s) {
				widget_->insert(QString(vlpd.s));
			}
			widget = widget_;
		}
		break;
#if 0
	case LayerWidgetType::PASSWORD:
		if (param->type == LayerParamType::STRING) {
			rv = gtk_entry_new();
			gtk_entry_set_visibility(GTK_ENTRY(rv), false);
			if (vlpd.s) {
				gtk_entry_set_text(GTK_ENTRY(rv), vlpd.s);
			}
			gtk_widget_set_tooltip_text(GTK_WIDGET(rv),
						    _("Take care that this password will be stored clearly in a plain file."));
		}
		break;
	case LayerWidgetType::FILEENTRY:
		if (param->type == LayerParamType::STRING) {
			rv = vik_file_entry_new(GTK_FILE_CHOOSER_ACTION_OPEN, (vf_filter_type) KPOINTER_TO_INT(param->widget_data), NULL, NULL);
			if (vlpd.s) {
				vik_file_entry_set_filename(VIK_FILE_ENTRY(rv), vlpd.s);
			}
		}
		break;
	case LayerWidgetType::FOLDERENTRY:
		if (param->type == LayerParamType::STRING) {
			rv = vik_file_entry_new(GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, VF_FILTER_NONE, NULL, NULL);
			if (vlpd.s) {
				vik_file_entry_set_filename(VIK_FILE_ENTRY(rv), vlpd.s);
			}
		}
		break;
#endif
	case LayerWidgetType::FILELIST:
		if (param->type == LayerParamType::STRING_LIST) {
			SGFileList * widget_ = new SGFileList(param->title, vlpd.sl, this);
			widget = widget_;
		}
		break;
#if 0
	case LayerWidgetType::HSCALE:
		if ((param->type == LayerParamType::DOUBLE || param->type == LayerParamType::UINT
		     || param->type == LayerParamType::INT)  && param->widget_data) {

			double init_val = (param->type == LayerParamType::DOUBLE) ? vlpd.d : (param->type == LayerParamType::UINT ? vlpd.u : vlpd.i);
			LayerParamScale *scale = (LayerParamScale *) param->widget_data;
			rv = gtk_hscale_new_with_range(scale->min, scale->max, scale->step);
			gtk_scale_set_digits(GTK_SCALE(rv), scale->digits);
			gtk_range_set_value(GTK_RANGE(rv), init_val);
		}
		break;
	case LayerWidgetType::BUTTON:
		if (param->type == LayerParamType::PTR && param->widget_data) {
			rv = gtk_button_new_with_label((const char *) param->widget_data);
			g_signal_connect(G_OBJECT(rv), "clicked", G_CALLBACK (vlpd.ptr), param->extra_widget_data);
		}
		break;
#endif
	default: break;
	}
#if 0
	if (rv && !gtk_widget_get_tooltip_text(rv)) {
		if (param->tooltip) {
			gtk_widget_set_tooltip_text(rv, _(param->tooltip));
		}
	}
#endif
	return widget;
}




LayerParamValue LayerPropertiesDialog::get_param_value(layer_param_id_t id, LayerParam * param)
{
	qDebug() << "vvvvvvvvvvv there are " << this->widgets.size() << "widgets";

	QWidget * widget = this->widgets[id];

	LayerParamValue rv;
	switch (param->widget_type) {
	case LayerWidgetType::COLOR: {
		QColor c = ((SGColorButton *) widget)->get_color();
		rv.c.r = c.red();
		rv.c.g = c.green();
		rv.c.b = c.blue();
		rv.c.a = c.alpha();
	}
		break;
	case LayerWidgetType::CHECKBUTTON:
		rv.b = ((QCheckBox *) widget)->isChecked();
		break;
#if 0
	case LayerWidgetType::COMBOBOX:
		if (param->type == LayerParamType::UINT) {
			rv.i = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
			if (rv.i == -1) {
				rv.i = 0;
			}

			rv.u = rv.i;
			if (param->extra_widget_data) {
				rv.u = ((unsigned int *)param->extra_widget_data)[rv.u];
			}
		}
		if (param->type == LayerParamType::STRING) {
			if (param->extra_widget_data) {
				/* Combobox displays labels and we want values from extra. */
				int pos = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
				rv.s = ((const char **)param->extra_widget_data)[pos];
			} else {
				/* Return raw value. */
#if GTK_CHECK_VERSION (2, 24, 0)
				rv.s = gtk_entry_get_text(GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget))));
#else
				rv.s = gtk_combo_box_get_active_text(GTK_COMBO_BOX(widget));
#endif
			}
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, rv.s);
		}
		break;
#endif
	case LayerWidgetType::RADIOGROUP:
	case LayerWidgetType::RADIOGROUP_STATIC:
		rv.u = ((SGRadioGroup *) widget)->value();
		if (param->extra_widget_data) {
			rv.u = KPOINTER_TO_UINT (g_list_nth_data((GList *) param->extra_widget_data, rv.u));
		}
		break;
	case LayerWidgetType::SPINBUTTON:
		if (param->type == LayerParamType::UINT) {
			rv.u = ((QSpinBox *) widget)->value();
		} else {
			rv.i = ((QSpinBox *) widget)->value();
		}
		break;

	case LayerWidgetType::SPINBOX_DOUBLE:
		rv.d = ((QDoubleSpinBox *) widget)->value();
		break;

	case LayerWidgetType::ENTRY:
	case LayerWidgetType::PASSWORD:
		rv.s = (char *) ((QLineEdit *) widget)->text().data(); /* kamilFIXME */
		break;
#if 0
	case LayerWidgetType::FILEENTRY:
	case LayerWidgetType::FOLDERENTRY:
		rv.s = vik_file_entry_get_filename(VIK_FILE_ENTRY(widget));
		break;
#endif
	case LayerWidgetType::FILELIST:
		rv.sl = ((SGFileList *) widget)->get_list();
		for (auto iter = rv.sl->begin(); iter != rv.sl->end(); iter++) {
			qDebug() << "File on retrieved list: " << QString(*iter);
		}
		break;
#if 0
	case LayerWidgetType::HSCALE:
		if (param->type == LayerParamType::UINT) {
			rv.u = (uint32_t) gtk_range_get_value(GTK_RANGE(widget));
		} else if (param->type == LayerParamType::INT) {
			rv.i = (int32_t) gtk_range_get_value(GTK_RANGE(widget));
		} else {
			rv.d = gtk_range_get_value(GTK_RANGE(widget));
		}
		break;
#endif
	default:
		break;
	}

	/* Perform conversion if necessary. */
	if (param->convert_to_internal) {
		rv = param->convert_to_internal(rv);
	}

	return rv;
}
