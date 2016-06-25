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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <assert.h>

#include <gtk/gtk.h>

#include "vikgobjectbuilder.h"

/* FIXME use private fields */
GType gtype = 0;
char *property_name = NULL;
GParameter *parameters = NULL;
int nb_parameters = 0;

/* signals */
enum
{
	NEW_OBJECT,

	LAST_SIGNAL
};


static unsigned int gobject_builder_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (VikGobjectBuilder, vik_gobject_builder, G_TYPE_OBJECT);

static void
vik_gobject_builder_init (VikGobjectBuilder *object)
{
	/* TODO: Add initialization code here */
}

static void
vik_gobject_builder_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */

	G_OBJECT_CLASS (vik_gobject_builder_parent_class)->finalize (object);
}

static void
vik_gobject_builder_new_object (VikGobjectBuilder *self, GObject *object)
{
	/* TODO: Add default signal handler implementation here */
}

static void
vik_gobject_builder_class_init (VikGobjectBuilderClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = vik_gobject_builder_finalize;

	klass->new_object = vik_gobject_builder_new_object;

	gobject_builder_signals[NEW_OBJECT] =
		g_signal_new ("new-object",
		              G_OBJECT_CLASS_TYPE (klass),
		              (GSignalFlags) 0,
		              G_STRUCT_OFFSET (VikGobjectBuilderClass, new_object),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              G_TYPE_OBJECT);
}

/* Called for open tags <foo bar="baz"> */
static void
_start_element (GMarkupParseContext *context,
                const char         *element_name,
                const char        **attribute_names,
                const char        **attribute_values,
                void *             user_data,
                GError             **error)
{
	if (strcmp(element_name, "object") == 0)
	{
		char *class_name = g_strdup(attribute_values[0]);
		gtype = g_type_from_name (class_name);
		if (gtype == 0)
		{
			fprintf(stderr, "WARNING: Unknown GObject type '%s'\n", class_name);
			return;
		}
		free(class_name);
	}
	if (strcmp(element_name, "property") == 0 && gtype != 0)
	{
		int i=0;
		while (attribute_names[i] != NULL)
		{
			if (strcmp (attribute_names[i], "name") == 0)
			{
				free(property_name);
				property_name = g_strdup(attribute_values[i]);
			}
			i++;
		}
	}
}

/* Called for close tags </foo> */
static void
_end_element (GMarkupParseContext *context,
              const char         *element_name,
              void *             user_data,
              GError             **error)
{
	VikGobjectBuilder *self = VIK_GOBJECT_BUILDER (user_data);
	void * object = NULL;
	if (strcmp(element_name, "object") == 0 && gtype != 0)
	{
		object = g_object_newv(gtype, nb_parameters, parameters);
		if (object != NULL)
		{
			fprintf(stderr, "DEBUG: VikGobjectBuilder: new GObject of type %s\n", g_type_name(gtype));
			g_signal_emit ( G_OBJECT(self), gobject_builder_signals[NEW_OBJECT], 0, object );
			g_object_unref (object);
		}
		/* Free memory */
		int i = 0;
		for (i = 0 ; i < nb_parameters ; i++)
		{
			free((char *)parameters[i].name);
			g_value_unset (&(parameters[i].value));
		}
		free(parameters);
		parameters = NULL;
		nb_parameters = 0;
		gtype = 0;
	}
	if (strcmp(element_name, "property") == 0)
	{
		free(property_name);
		property_name = NULL;
	}
}

/* Called for character data */
/* text is not nul-terminated */
static void
_text (GMarkupParseContext *context,
       const char         *text,
       size_t                text_len,
       void *             user_data,
       GError             **error)
{
	if (strcmp (g_markup_parse_context_get_element (context), "property") == 0)
	{
		GValue gvalue = {0};
		bool found = false;
		if (gtype != 0 && property_name != NULL)
		{
			/* parameter value */
			/* We have to retrieve the expected type of the value
			 * in order to do the correct transformation */
			GObjectClass *oclass;
			oclass = (GObjectClass *) g_type_class_ref (gtype);
			assert (oclass != NULL);
			GParamSpec *pspec;
			pspec = g_object_class_find_property (G_OBJECT_CLASS (oclass), property_name);
			if (!pspec)
			{
				fprintf(stderr, "WARNING: Unknown property: %s.%s\n", g_type_name (gtype), property_name);
				return;
			}
			char *value = g_strndup (text, text_len);
			found = gtk_builder_value_from_string_type(NULL, pspec->value_type, value, &gvalue, NULL);
			free(value);
		}
		if (G_IS_VALUE (&gvalue) && found == true)
		{
			/* store new parameter */
			fprintf(stderr, "DEBUG: VikGobjectBuilder: store new GParameter for %s: (%s)%s=%*s\n",
			        g_type_name(gtype), g_type_name(G_VALUE_TYPE(&gvalue)), property_name, (int)text_len, text);
			nb_parameters++;
			parameters = (GParameter *) g_realloc(parameters, sizeof(GParameter)*nb_parameters);
			/* parameter name */
			parameters[nb_parameters-1].name = g_strdup(property_name);
			/* parameter value */
			parameters[nb_parameters-1].value = gvalue;
		}
	}
}

VikGobjectBuilder *
vik_gobject_builder_new (void)
{
	return (VikGobjectBuilder *) g_object_new (VIK_TYPE_GOBJECT_BUILDER, NULL);
}

void
vik_gobject_builder_parse (VikGobjectBuilder *self, const char *filename)
{
	GMarkupParser xml_parser;
	GMarkupParseContext *xml_context;
	GError *error = NULL;

	FILE *file = fopen(filename, "r");
	if (file == NULL)
		/* TODO emit warning */
		return;

	/* setup context parse (ie callbacks) */
	xml_parser.start_element = &_start_element;
	xml_parser.end_element = &_end_element;
	xml_parser.text = &_text;
	xml_parser.passthrough = NULL;
	xml_parser.error = NULL;

	xml_context = g_markup_parse_context_new(&xml_parser, (GMarkupParseFlags) 0, self, NULL);

	char buff[BUFSIZ];
	size_t nb;
	while ((nb = fread (buff, sizeof(char), BUFSIZ, file)) > 0)
	{
		if (!g_markup_parse_context_parse(xml_context, buff, nb, &error))
			fprintf(stderr, "WARNING: %s: parsing error: %s\n", __FUNCTION__,
			          error != NULL ? error->message : "???");
	}
	/* cleanup */
	if (!g_markup_parse_context_end_parse(xml_context, &error))
		fprintf(stderr, "WARNING: %s: errors occurred reading file '%s': %s\n", __FUNCTION__, filename,
		          error != NULL ? error->message : "???");

	g_markup_parse_context_free(xml_context);
	fclose (file);
}
