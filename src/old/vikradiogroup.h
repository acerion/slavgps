/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_RADIOGROUP_H_
#define _SG_RADIOGROUP_H_




#include <cstdint>

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>




#define VIK_RADIO_GROUP_TYPE            (vik_radio_group_get_type ())
#define VIK_RADIO_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_RADIO_GROUP_TYPE, VikRadioGroup))
#define VIK_RADIO_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_RADIO_GROUP_TYPE, VikRadioGroupClass))
#define IS_VIK_RADIO_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_RADIO_GROUP_TYPE))
#define IS_VIK_RADIO_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_RADIO_GROUP_TYPE))

typedef struct _VikRadioGroup VikRadioGroup;
typedef struct _VikRadioGroupClass VikRadioGroupClass;

struct _VikRadioGroupClass {
	GtkVBoxClass vbox_class;
};

GType vik_radio_group_get_type();




GtkWidget * vik_radio_group_new(GList * options);
void vik_radio_group_set_selected(VikRadioGroup * vrg, uint8_t i);
uint8_t vik_radio_group_get_selected(VikRadioGroup * vrg);
GtkWidget * vik_radio_group_new_static(char const ** options);




#endif /* #ifndef _SG_RADIOGROUP_H_ */
