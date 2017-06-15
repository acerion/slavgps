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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "uibuilder.h"
#include "vikradiogroup.h"
#include "vikfileentry.h"
#include "vikfilelist.h"
#include "viking.h"
#include "globals.h"




using namespace SlavGPS;




GtkWidget *a_uibuilder_new_widget(LayerParam *param, ParameterValue data)
{
	/* Perform pre conversion if necessary. */
	ParameterValue vlpd = data;
	if (param->convert_to_display) {
		vlpd = param->convert_to_display(data);
	}

	GtkWidget *rv = NULL;
	switch (param->widget_type) {
	case WidgetType::COLOR:
		if (param->type == ParameterType::COLOR) {
			rv = gtk_color_button_new_with_color(&(vlpd.c));
		}
		break;
	case WidgetType::CHECKBUTTON:
	    if (param->type == ParameterType::BOOLEAN) {
		    //rv = gtk_check_button_new_with_label (//param->title);
		    rv = gtk_check_button_new();
		    if (vlpd.b) {
			    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rv), true);
		    }
	    }
	    break;
	case WidgetType::COMBOBOX:
		if (param->type == ParameterType::UINT && param->widget_data) {
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
					if (((unsigned int *)param->extra_widget_data)[i] == vlpd.u) {
						/* Match default value. */
						gtk_combo_box_set_active(rv, i);
						break;
					}
			} else {
				gtk_combo_box_set_active(rv, vlpd.u);
			}
		} else if (param->type == ParameterType::STRING && param->widget_data && !param->extra_widget_data) {
			/* Build a combobox with editable text. */
			char **pstr = (char **) param->widget_data;
			rv = new QComboBox();
			if (vlpd.s) {
				vik_combo_box_text_append(rv, vlpd.s);
			}

			while (*pstr) {
				vik_combo_box_text_append(rv, *(pstr++));
			}

			if (vlpd.s) {
				gtk_combo_box_set_active(rv, 0);
			}
		} else if (param->type == ParameterType::STRING && param->widget_data && param->extra_widget_data) {
			/* Build a combobox with fixed selections without editable text. */
			char **pstr = (char **) param->widget_data;
			rv = new QComboBox();
			while (*pstr) {
				vik_combo_box_text_append(rv, *(pstr++));
			}
			if (vlpd.s) {
				/* Set the effective default value. */
				/* In case of value does not exist, set the first value. */
				gtk_combo_box_set_active(rv, 0);
				int i;
				for (i = 0; ((const char **)param->widget_data)[i]; i++)
					if (strcmp(((const char **)param->extra_widget_data)[i], vlpd.s) == 0) {
						/* Match default value. */
						gtk_combo_box_set_active(rv, i);
						break;
					}
			} else {
				gtk_combo_box_set_active(rv, 0);
			}
		}
		break;
	case WidgetType::RADIOGROUP:
		/* widget_data and extra_widget_data are GList. */
		if (param->type == ParameterType::UINT && param->widget_data) {
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
	case WidgetType::RADIOGROUP_STATIC:
		if (param->type == ParameterType::UINT && param->widget_data) {
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
	case WidgetType::SPINBUTTON:
		if ((param->type == ParameterType::DOUBLE || param->type == ParameterType::UINT
		     || param->type == ParameterType::INT)  && param->widget_data) {

			double init_val = (param->type == ParameterType::DOUBLE) ? vlpd.d : (param->type == ParameterType::UINT ? vlpd.u : vlpd.i);
			ParameterScale * scale = (ParameterScale *) param->widget_data;
			rv = new QSpinBox(GTK_ADJUSTMENT(gtk_adjustment_new(init_val, scale->min, scale->max, scale->step, scale->step, 0)), scale->step, scale->digits);
		}
		break;
	case WidgetType::ENTRY:
		if (param->type == ParameterType::STRING) {
			rv = gtk_entry_new();
			if (vlpd.s) {
				gtk_entry_set_text(GTK_ENTRY(rv), vlpd.s);
			}
		}
		break;
	case WidgetType::PASSWORD:
		if (param->type == ParameterType::STRING) {
			rv = gtk_entry_new();
			gtk_entry_set_visibility(GTK_ENTRY(rv), false);
			if (vlpd.s) {
				gtk_entry_set_text(GTK_ENTRY(rv), vlpd.s);
			}
			rv->setToolTip(QObject::tr("Take care that this password will be stored clearly in a plain file."));
		}
		break;
	case WidgetType::FILEENTRY:
		if (param->type == ParameterType::STRING) {
			rv = vik_file_entry_new(GTK_FILE_CHOOSER_ACTION_OPEN, (vf_filter_type) KPOINTER_TO_INT(param->widget_data), NULL, NULL);
			if (vlpd.s) {
				rv->set_filename(vlpd.s);
			}
		}
		break;
	case WidgetType::FOLDERENTRY:
		if (param->type == ParameterType::STRING) {
			rv = vik_file_entry_new(GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, VF_FILTER_NONE, NULL, NULL);
			if (vlpd.s) {
				rv->set_filename(vlpd.s);
			}
		}
		break;

	case WidgetType::FILELIST:
		if (param->type == ParameterType::STRING_LIST) {
			rv = vik_file_list_new(_(param->title), NULL);
			vik_file_list_set_files(VIK_FILE_LIST(rv), vlpd.sl);
		}
		break;
	case WidgetType::HSCALE:
		if ((param->type == ParameterType::DOUBLE || param->type == ParameterType::UINT
		     || param->type == ParameterType::INT)  && param->widget_data) {

			double init_val = (param->type == ParameterType::DOUBLE) ? vlpd.d : (param->type == ParameterType::UINT ? vlpd.u : vlpd.i);
			ParameterScale * scale = (ParameterScale *) param->widget_data;
			rv = gtk_hscale_new_with_range(scale->min, scale->max, scale->step);
			gtk_scale_set_digits(GTK_SCALE(rv), scale->digits);
			gtk_range_set_value(GTK_RANGE(rv), init_val);
		}
		break;

	case WidgetType::BUTTON:
		if (param->type == ParameterType::PTR && param->widget_data) {
			rv = gtk_button_new_with_label((const char *) param->widget_data);
			QObject::connect(rv, SIGNAL("clicked"), param->extra_widget_data, SLOT (vlpd.ptr));
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




ParameterValue a_uibuilder_widget_get_value(GtkWidget *widget, LayerParam *param)
{
	ParameterValue rv;
	switch (param->widget_type) {
	case WidgetType::COLOR:
		gtk_color_button_get_color(GTK_COLOR_BUTTON(widget), &(rv.c));
		break;
	case WidgetType::CHECKBUTTON:
		rv.b = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
		break;
	case WidgetType::COMBOBOX:
		if (param->type == ParameterType::UINT) {
			rv.i = gtk_combo_box_get_active(widget);
			if (rv.i == -1) {
				rv.i = 0;
			}

			rv.u = rv.i;
			if (param->extra_widget_data) {
				rv.u = ((unsigned int *)param->extra_widget_data)[rv.u];
			}
		}
		if (param->type == ParameterType::STRING) {
			if (param->extra_widget_data) {
				/* Combobox displays labels and we want values from extra. */
				int pos = gtk_combo_box_get_active(widget);
				rv.s = ((const char **)param->extra_widget_data)[pos];
			} else {
				/* Return raw value. */
#if GTK_CHECK_VERSION (2, 24, 0)
				rv.s = gtk_entry_get_text(GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget))));
#else
				rv.s = gtk_combo_box_get_active_text(widget);
#endif
			}
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, rv.s);
		}
		break;
	case WidgetType::RADIOGROUP:
	case WidgetType::RADIOGROUP_STATIC:
		rv.u = vik_radio_group_get_selected(VIK_RADIO_GROUP(widget));
		if (param->extra_widget_data) {
			rv.u = KPOINTER_TO_UINT (g_list_nth_data((GList *) param->extra_widget_data, rv.u));
		}
		break;
	case WidgetType::SPINBUTTON:
		if (param->type == ParameterType::UINT) {
			rv.u = widget.value();
		} else if (param->type == ParameterType::INT) {
			rv.i = widget.value();
		} else {
			rv.d = widget.value();
		}
		break;
	case WidgetType::ENTRY:
	case WidgetType::PASSWORD:
		rv.s = gtk_entry_get_text(GTK_ENTRY(widget));
		break;
	case WidgetType::FILEENTRY:
	case WidgetType::FOLDERENTRY:
		rv.s = VIK_FILE_ENTRY(widget)->get_filename();
		break;
	case WidgetType::FILELIST:
		rv.sl = vik_file_list_get_files(VIK_FILE_LIST(widget));
		break;
	case WidgetType::HSCALE:
		if (param->type == ParameterType::UINT) {
			rv.u = (uint32_t) gtk_range_get_value(GTK_RANGE(widget));
		} else if (param->type == ParameterType::INT) {
			rv.i = (int32_t) gtk_range_get_value(GTK_RANGE(widget));
		} else {
			rv.d = gtk_range_get_value(GTK_RANGE(widget));
		}
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




//static void draw_to_image_file_total_area_cb (QSpinBox * spinbutton, void * *pass_along)
int a_uibuilder_properties_factory(const char *dialog_name,
				   Window * parent,
				   Parameter *params,
				   uint16_t params_count,
				   char **groups,
				   uint8_t groups_count,
				   bool (*setparam) (void *,uint16_t,ParameterValue,void *,bool),
				   void * pass_along1,
				   void * pass_along2,
				   ParameterValue (*getparam) (void *,uint16_t,bool),
				   void * pass_along_getparam,
				   void (*changeparam) (GtkWidget*, ui_change_values *))
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
		QLabels **labels = (QLabel **) malloc(sizeof(QLabel *) * widget_count);
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
							QObject::connect(widgets[j], SIGNAL("toggled"), &change_values[j], SLOT (changeparam));
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





ParameterValue *a_uibuilder_run_dialog(const char *dialog_name, Window * parent, LayerParam *params,
				       uint16_t params_count, char **groups, uint8_t groups_count,
				       ParameterValue *params_defaults)
{
	ParameterValue * paramdatas = (ParameterValue *) malloc(params_count * sizeof (ParameterValue));
	if (a_uibuilder_properties_factory(dialog_name,
					   parent,
					   params,
					   params_count,
					   groups,
					   groups_count,
					   (bool (*)(void*, uint16_t, ParameterValue, void*, bool)) uibuilder_run_setparam,
					   paramdatas,
					   params,
					   (ParameterValue (*)(void*, uint16_t, bool)) uibuilder_run_getparam,
					   params_defaults,
					   NULL) > 0) {

		return paramdatas;
	}
	free(paramdatas);
	return NULL;
}
