/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#include <cstdio>
#include <cstring>

#ifdef HAVE_MATH_H
#include <cmath>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

#include <QDebug>
#include <QXmlSimpleReader>
#include <QXmlInputSource>

#include "goto_tool_xml.h"




using namespace SlavGPS;




#define PREFIX ": GoTo XML:" << __FUNCTION__ << __LINE__ << ">"




static bool stack_is_path(const GSList * stack, const char * path);

static void start_element_handler(GMarkupParseContext * context, const char * element_name, const char ** attribute_names, const char ** attribute_values, void * user_data, GError ** error);
static void text_handler(GMarkupParseContext * context, const char * text, size_t text_len, void * user_data, GError ** error);




MyHandler::MyHandler(const QString & new_lat_path, const QString & new_lon_path)
{
	/* We will know that we found lat/lon values when, during
	   traversal of xml tree, we descend into place with given
	   "path", i.e. list of nested xml tags. */
	this->lat_path = QString(new_lat_path).split('/');
	this->lon_path = QString(new_lon_path).split('/');

	qDebug() << "II" PREFIX << "latitude path =" << this->lat_path;
	qDebug() << "II" PREFIX << "longitude path =" << this->lon_path;
}




bool MyHandler::startDocument(void)
{
	qDebug() << "II" PREFIX;
	return true;
}




bool MyHandler::endDocument(void)
{
	qDebug() << "II" PREFIX;
	return true;
}




bool MyHandler::startElement(const QString &namespaceURI, const QString &localName, const QString &qName, const QXmlAttributes &atts)
{
	this->stack.push_back(localName);
	qDebug() << "II" PREFIX << "localName =" << localName << "qName =" << qName;
	qDebug() << "II" PREFIX << "stack after pushing =" << this->stack;
	return true;
}




bool MyHandler::endElement(const QString &namespaceURI, const QString &localName, const QString &qName)
{
	qDebug() << "II" PREFIX << "localName =" << localName << "qName =" << qName;
	this->stack.pop_back();
	qDebug() << "II" PREFIX << "stack after popping =" << this->stack;
	return true;
}




bool MyHandler::characters(const QString & ch)
{
	if (this->stack == this->lat_path) {
		qDebug() << "II" PREFIX << "---- found latitude =" << ch;
	} else if (this->stack == this->lon_path) {
		qDebug() << "II" PREFIX << "---- found longitude =" << ch;
	} else {
		qDebug() << "II" PREFIX << "found other characters string =" << ch;
	}

	return true;
}




bool MyHandler::fatalError(const QXmlParseException & exception)
{
	qDebug() << "EE" PREFIX;
	return true;
}




GotoToolXML::GotoToolXML()
{
	this->url_format = strdup("<no-set>");
	this->lat_path = strdup("<no-set>");
	this->lat_attr = NULL;
	this->lon_path = strdup("<no-set>");
	this->lon_attr = NULL;

	this->ll.lat = NAN;
	this->ll.lon = NAN;
}




GotoToolXML::GotoToolXML(const QString & new_label, char const * new_url_format, const QString & new_lat_path, const QString & new_lon_path)
{
	this->label = new_label;
	this->set_url_format(new_url_format);

	this->xml_handler = new MyHandler(new_lat_path, new_lon_path);
}




GotoToolXML::GotoToolXML(const QString & new_label, char const * new_url_format, char const * new_lat_path, char const * new_lat_attr, char const * new_lon_path, char const * new_lon_attr)
{
	this->label = new_label;
	this->set_url_format(new_url_format);

	this->set_lat_path(new_lat_path);
	this->set_lat_attr(new_lat_attr);
	this->set_lon_path(new_lon_path);
	this->set_lon_attr(new_lon_attr);
}




GotoToolXML::~GotoToolXML()
{
	delete this->xml_handler;
}




void GotoToolXML::set_url_format(char const * new_format)
{
	if (this->url_format) {
		free(this->url_format);
	}
	this->url_format = strdup(new_format);
	return;
}




void GotoToolXML::set_lat_path(char const * new_value)
{
	char **splitted = g_strsplit(new_value, "@", 2);
	free(this->lat_path);
	this->lat_path = splitted[0];
	if (splitted[1]) {
		this->set_lat_attr(splitted[1]);
		free(splitted[1]);
	}
	/* only free the tab, not the strings */
	free(splitted);
	splitted = NULL;
	return;
}




void GotoToolXML::set_lat_attr(char const * new_value)
{
	/* Avoid to overwrite XPATH value */
	/* NB: This disable future overwriting,
	   but as property is CONSTRUCT_ONLY there is no matter */
	if (!this->lat_attr) {
		free(this->lat_attr);

	}
	this->lat_attr = strdup(new_value);
	return;
}




void GotoToolXML::set_lon_path(char const * new_value)
{
	char **splitted = g_strsplit(new_value, "@", 2);
	free(this->lon_path);
	this->lon_path = splitted[0];
	if (splitted[1]) {
		this->set_lon_attr(splitted[1]);
		free(splitted[1]);
	}
	/* only free the tab, not the strings */
	free(splitted);
	splitted = NULL;
	return;
}




void GotoToolXML::set_lon_attr(char const * new_value)
{
	/* Avoid to overwrite XPATH value */
	/* NB: This disable future overwriting,
	   but as property is CONSTRUCT_ONLY there is no matter */
	if (!this->lon_attr) {
		free(this->lon_attr);

	}
	this->lon_attr = strdup(new_value);
	return;
}




char * GotoToolXML::get_url_format()
{
	return this->url_format;
}




bool GotoToolXML::parse_file_for_latlon(const QString & file_full_path, LatLon & lat_lon)
{
	qDebug() << "DD" PREFIX << "paths/attributes" << this->lat_path << this->lat_attr << this->lon_path << this->lon_attr;
#if 1

	QFile file(file_full_path);
	QXmlSimpleReader xml_reader;
	QXmlInputSource source(&file);
	xml_reader.setContentHandler(this->xml_handler);
	xml_reader.setErrorHandler(this->xml_handler);


	if (!xml_reader.parse(&source)) {
		qDebug() << "EE" PREFIX << "failed to parse xml file";
		return false;
	}

	return true;

#else
	GMarkupParser xml_parser;
	GMarkupParseContext *xml_context = NULL;
	GError * error = NULL;



	FILE *file = fopen(file_full_path.toUtf8().constData(), "r");
	if (file == NULL) {
		/* TODO emit warning */
		return false;
	}

	/* setup context parse (ie callbacks) */
	if (this->lat_attr != NULL || this->lon_attr != NULL) {
		// At least one coordinate uses an attribute
		xml_parser.start_element = start_element_handler;
	} else {
		xml_parser.start_element = NULL;
	}

	xml_parser.end_element = NULL;
	if (this->lat_attr == NULL || this->lon_attr == NULL) {
		// At least one coordinate uses a raw element
		xml_parser.text = text_handler;
	} else {
		xml_parser.text = NULL;
	}
	xml_parser.passthrough = NULL;
	xml_parser.error = NULL;

	xml_context = g_markup_parse_context_new(&xml_parser, (GMarkupParseFlags) 0, this, NULL);

	/* setup result */
	this->ll.lat = NAN;
	this->ll.lon = NAN;

	char buff[BUFSIZ];
	size_t nb;
	while (xml_context && (nb = fread (buff, sizeof(char), BUFSIZ, file)) > 0) {

		if (!g_markup_parse_context_parse(xml_context, buff, nb, &error)) {
			fprintf(stderr, "%s: parsing error: %s.\n", __FUNCTION__, error->message);
			g_markup_parse_context_free(xml_context);
			xml_context = NULL;
		}
		g_clear_error (&error);
	}
	/* cleanup */
	if (xml_context && !g_markup_parse_context_end_parse(xml_context, &error)) {
		fprintf(stderr, "%s: errors occurred while reading file: %s.\n", __FUNCTION__, error->message);
	}
	g_clear_error (&error);

	if (xml_context) {
		g_markup_parse_context_free(xml_context);
	}
	xml_context = NULL;
	fclose (file);

	lat_lon = this->ll;

	if (std::isnan(this->ll.lat) || std::isnan(this->ll.lat)) {
		/* At least one coordinate not found */
		return false;
	} else {
		return true;
	}
#endif
}




static bool stack_is_path(const GSList * stack, const char * path)
{
	bool equal = true;
	int stack_len = g_list_length((GList *)stack);
	int i = stack_len - 1;
	while (equal == true && i >= 0) {
		if (*path != '/') {
			equal = false;
		} else {
			path++;
		}
		const char *current = (const char *) g_list_nth_data((GList *)stack, i);
		size_t len = strlen(current);
		if (strncmp(path, current, len) != 0) {
			equal = false;
		} else {
			path += len;
		}
		i--;
	}
	if (*path != '\0') {
		equal = false;
	}
	return equal;
}




/* Called for open tags <foo bar="baz"> */
static void start_element_handler(GMarkupParseContext * context,
			   const char          * element_name,
			   const char         ** attribute_names,
			   const char         ** attribute_values,
			   void                * user_data,
			   GError             ** error)
{
	GotoToolXML * self = (GotoToolXML *) user_data;
	const GSList * stack = g_markup_parse_context_get_element_stack(context);
	/* Longitude */
	if (self->lon_attr != NULL && std::isnan(self->ll.lon) && stack_is_path(stack, self->lon_path)) {
		int i=0;
		while (attribute_names[i] != NULL) {
			if (strcmp(attribute_names[i], self->lon_attr) == 0) {
				self->ll.lon = g_ascii_strtod(attribute_values[i], NULL);
			}
			i++;
		}
	}
	/* Latitude */
	if (self->lat_attr != NULL && std::isnan(self->ll.lat) && stack_is_path(stack, self->lat_path)) {
		int i=0;
		while (attribute_names[i] != NULL) {
			if (strcmp(attribute_names[i], self->lat_attr) == 0) {
				self->ll.lat = g_ascii_strtod(attribute_values[i], NULL);
			}
			i++;
		}
	}
}




/* Called for character data */
/* text is not nul-terminated */
static void text_handler(GMarkupParseContext * context, const char * text, size_t text_len, void * user_data, GError ** error)
{
	GotoToolXML * self = (GotoToolXML *) user_data;

	const GSList *stack = g_markup_parse_context_get_element_stack(context);

	char *textl = g_strndup(text, text_len);
	/* Store only first result */
	if (self->lat_attr == NULL && std::isnan(self->ll.lat) && stack_is_path (stack, self->lat_path)) {
		self->ll.lat = g_ascii_strtod(textl, NULL);
	}
	if (self->lon_attr == NULL && std::isnan(self->ll.lon) && stack_is_path (stack, self->lon_path)) {
		self->ll.lon = g_ascii_strtod(textl, NULL);
	}
	free(textl);
	return;
}
