/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2011, Rob Norris <rw_norris@hotmail.com>
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "vikfilelist.h"




struct _VikFileList {
	GtkVBox parent;
	GtkWidget * treeview;
	GtkWidget * file_selector;
	GtkTreeModel * model;
	GtkFileFilter * filter;
};




static void file_list_add(VikFileList * vfl)
{
	GSList *files = NULL;
	GSList *fiter = NULL;

	if (!vfl->file_selector) {
		GtkWidget *win = gtk_widget_get_toplevel(GTK_WIDGET(vfl));
		assert (win);
		vfl->file_selector = gtk_file_chooser_dialog_new(_("Choose file(s)"),
								 GTK_WINDOW(win),
								 GTK_FILE_CHOOSER_ACTION_OPEN,
								 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
								 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
								 NULL);
		gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(vfl->file_selector), true);
		gtk_window_set_transient_for(GTK_WINDOW(vfl->file_selector), GTK_WINDOW(win));
		gtk_window_set_destroy_with_parent(GTK_WINDOW(vfl->file_selector), true);

		if (vfl->filter) {
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(vfl->file_selector), vfl->filter);
		}
	}

	if (gtk_dialog_run(GTK_DIALOG(vfl->file_selector)) == GTK_RESPONSE_ACCEPT) {
		files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(vfl->file_selector));
		fiter = files;
		GtkTreeIter iter;
		while (fiter) {
			char *file_name = (char *) fiter->data;

			gtk_list_store_append(GTK_LIST_STORE(vfl->model), &iter);
			gtk_list_store_set(GTK_LIST_STORE(vfl->model), &iter, 0, file_name, -1);

			free(file_name);

			fiter = g_slist_next(fiter);
		}
		g_slist_free(files);
	}
	gtk_widget_hide(vfl->file_selector);
}




static GtkTreeRowReference ** file_list_get_selected_refs(GtkTreeModel * model, GList * list)
{
	GtkTreeRowReference ** arr = (GtkTreeRowReference **) malloc((g_list_length (list) + 1) * sizeof (GtkTreeRowReference *));

	int pos = 0;
	for (GList * iter = g_list_first (list); iter != NULL; iter = g_list_next(iter)) {
		GtkTreePath *path = (GtkTreePath *)(iter->data);
		arr[pos] = gtk_tree_row_reference_new(model, path);
		pos++;
	}
	arr[pos] = NULL;

	return arr;
}




static void file_list_store_delete_refs(GtkTreeModel * model, GtkTreeRowReference * const * rrefs)
{
	GtkTreePath * path;
	GtkTreeIter iter;

	for (int rr = 0; rrefs[rr] != NULL; rr++) {
		path = gtk_tree_row_reference_get_path(rrefs[rr]);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_list_store_remove(GTK_LIST_STORE (model), &iter);
	}
}




static void file_list_del(VikFileList * vfl)
{
	GtkTreeSelection * ts = gtk_tree_view_get_selection(GTK_TREE_VIEW(vfl->treeview));
	GList * list = gtk_tree_selection_get_selected_rows(ts, &(vfl->model));

	/* For multi delete need to store references to selected rows. */
	GtkTreeRowReference **rrefs = file_list_get_selected_refs(vfl->model, list);
	/* And then delete each one individually as the path will have changed. */
	file_list_store_delete_refs(vfl->model, rrefs);

	/* Cleanup. */
	g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free(list);
	free(rrefs);
}




GType vik_file_list_get_type(void)
{
	static GType vs_type = 0;

	if (!vs_type) {
		static const GTypeInfo vs_info = {
			sizeof (VikFileListClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			NULL, /* class init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (VikFileList),
			0,
			NULL /* instance init */
		};
		vs_type = g_type_register_static(GTK_TYPE_VBOX, "VikFileList", &vs_info, (GTypeFlags) 0);
	}

	return vs_type;
}




/**
 * Support just one filter, as that's all that's needed ATM.
 * Probably need to use a GList of them if more than one is required.
 */
GtkWidget * vik_file_list_new(const char * title, GtkFileFilter * filter)
{
	GtkWidget *add_btn, *del_btn;
	GtkWidget *hbox, *scrolledwindow;
	VikFileList *vfl = VIK_FILE_LIST (g_object_new(VIK_FILE_LIST_TYPE, NULL));

	vfl->filter = filter;

	GtkTreeViewColumn *column;

	vfl->model = GTK_TREE_MODEL (gtk_list_store_new(1, G_TYPE_STRING));

	vfl->treeview = gtk_tree_view_new();
	gtk_tree_view_set_model(GTK_TREE_VIEW(vfl->treeview), vfl->model);
	column = gtk_tree_view_column_new_with_attributes(title, gtk_cell_renderer_text_new (), "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW (vfl->treeview), column);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection (GTK_TREE_VIEW(vfl->treeview)), GTK_SELECTION_MULTIPLE);

	gtk_widget_set_size_request(vfl->treeview, 200, 100);

	add_btn = gtk_button_new_with_label(_("Add..."));
	del_btn = gtk_button_new_with_label(_("Delete"));

	g_signal_connect_swapped(G_OBJECT(add_btn), "clicked", G_CALLBACK(file_list_add), vfl);
	g_signal_connect_swapped(G_OBJECT(del_btn), "clicked", G_CALLBACK(file_list_del), vfl);

	hbox = gtk_hbox_new(false, 2);

	scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolledwindow), GTK_WIDGET(vfl->treeview));

	gtk_box_pack_start(GTK_BOX(vfl), scrolledwindow, true, true, 3);


	gtk_box_pack_start(GTK_BOX(hbox), add_btn, true, true, 3);
	gtk_box_pack_start(GTK_BOX(hbox), del_btn, true, true, 3);
	gtk_box_pack_start(GTK_BOX(vfl), hbox, false, false, 3);
	gtk_widget_show_all(GTK_WIDGET(vfl));

	vfl->file_selector = NULL;

	return GTK_WIDGET(vfl);
}




static bool get_file_name(GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, std::list<char *> * list)
{
	char *str;
	gtk_tree_model_get(model, iter, 0, &str, -1);
	fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, str);
	list->push_back(str); /* NB str is already a copy. */
	return false;
}




std::list<char *> * vik_file_list_get_files(VikFileList * vfl)
{
	std::list<char *> * list = new std::list<char *>;
	gtk_tree_model_foreach(vfl->model, (GtkTreeModelForeachFunc) get_file_name, list);
	return list;
}




void vik_file_list_set_files(VikFileList * vfl, std::list<char *> * files)
{
	for (auto f = files->begin(); f != files->end(); f++) {
		GtkTreeIter iter;
		gtk_list_store_append(GTK_LIST_STORE (vfl->model), &iter);
		gtk_list_store_set(GTK_LIST_STORE (vfl->model), &iter, 0, *f, -1);
	}
}
