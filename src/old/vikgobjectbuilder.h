/*
 * viking
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * viking is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SG_GOBJECT_BUILDER_H_
#define _SG_GOBJECT_BUILDER_H_




#include <cstdint>

#include <glib-object.h>




#define VIK_TYPE_GOBJECT_BUILDER             (vik_gobject_builder_get_type ())
#define VIK_GOBJECT_BUILDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TYPE_GOBJECT_BUILDER, VikGobjectBuilder))
#define VIK_GOBJECT_BUILDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TYPE_GOBJECT_BUILDER, VikGobjectBuilderClass))
#define VIK_IS_GOBJECT_BUILDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TYPE_GOBJECT_BUILDER))
#define VIK_IS_GOBJECT_BUILDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TYPE_GOBJECT_BUILDER))
#define VIK_GOBJECT_BUILDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_TYPE_GOBJECT_BUILDER, VikGobjectBuilderClass))

typedef struct _VikGobjectBuilderClass VikGobjectBuilderClass;
typedef struct _VikGobjectBuilder VikGobjectBuilder;

struct _VikGobjectBuilderClass
{
	GObjectClass parent_class;

	/* Signals. */
	void(* new_object) (VikGobjectBuilder *self, GObject *object);
};

struct _VikGobjectBuilder
{
	GObject parent_instance;
};

GType vik_gobject_builder_get_type (void) G_GNUC_CONST;
VikGobjectBuilder *vik_gobject_builder_new(void);

void vik_gobject_builder_parse(VikGobjectBuilder * self, const char * filename);




#endif /* #ifndef _SG_GOBJECT_BUILDER_H_ */
