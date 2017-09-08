/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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




#ifdef VIK_CONFIG_GEOCACHES




#include <cstring>
#include <cstdlib>

#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "preferences.h"
#include "datasources.h"




/* Could have an array of programs instead... */
#define GC_PROGRAM1 "geo-nearest"
#define GC_PROGRAM2 "geo-html2gpx"

/* Params will be geocaching.username, geocaching.password
   We have to make sure these don't collide. */
#define PREFERENCES_GROUP_KEY_GC "geocaching"
#define PREFERENCES_NAMESPACE_GC "geocaching."




typedef struct {
	QSpinBox num_spin;
	QLineEdit center_entry; /* TODO make separate widgets for lat/lon. */
	QDoubleSpinBox miles_radius_spin;

	QPen circle_pen;
	Viewport * viewport;
	bool circle_onscreen;
	int circle_x, circle_y, circle_width;
} datasource_gc_widgets_t;




static void * datasource_gc_init(acq_vik_t *avt);
static void datasource_gc_add_setup_widgets(GtkWidget *dialog, Viewport * viewport, void * user_data);
static ProcessOptions * datasource_gc_get_process_options(datasource_gc_widgets_t *widgets, void * not_used, const char *not_used2, const char *not_used3);
static void datasource_gc_cleanup(datasource_gc_widgets_t *widgets);
static char *datasource_gc_check_existence();




#define METERSPERMILE 1609.344




VikDataSourceInterface vik_datasource_gc_interface = {
	N_("Download Geocaches"),
	N_("Geocaching.com Caches"),
	DatasourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true, /* Yes automatically update the display - otherwise we won't see the geocache waypoints! */
	true,
	true,

	(DataSourceInternalDialog)              NULL,
	(VikDataSourceInitFunc)		        datasource_gc_init,
	(VikDataSourceCheckExistenceFunc)	datasource_gc_check_existence,
	(VikDataSourceAddSetupWidgetsFunc)	datasource_gc_add_setup_widgets,
	(VikDataSourceGetProcessOptionsFunc)    datasource_gc_get_process_options,
	(VikDataSourceProcessFunc)              a_babel_convert_from,
	(VikDataSourceProgressFunc)		NULL,
	(VikDataSourceAddProgressWidgetsFunc)	NULL,
	(VikDataSourceCleanupFunc)		datasource_gc_cleanup,
	(VikDataSourceOffFunc)                  NULL,
};




static Parameter prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GC "username", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, N_("geocaching.com username:"), WidgetType::ENTRY, NULL, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_GC "password", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, N_("geocaching.com password:"), WidgetType::ENTRY, NULL, NULL, NULL, NULL },
};




void a_datasource_gc_init()
{
	Preferences::register_group(PREFERENCES_GROUP_KEY_GC, QObject::tr("Geocaching"));

	SGVariant tmp;
	tmp = SGVariant("username");
	Preferences::register_parameter(prefs, tmp, PREFERENCES_GROUP_KEY_GC);
	tmp = SGVariant("password");
	Preferences::register_parameter(prefs+1, tmp, PREFERENCES_GROUP_KEY_GC);
}




static void * datasource_gc_init(acq_vik_t *avt)
{
	datasource_gc_widgets_t *widgets = (datasource_gc_widgets_t *) malloc(sizeof(*widgets));
	return widgets;
}




static char *datasource_gc_check_existence()
{
	bool OK1 = false;
	bool OK2 = false;

	char *location1 = g_find_program_in_path(GC_PROGRAM1);
	if (location1) {
		free(location1);
		OK1 = true;
	}

	char *location2 = g_find_program_in_path(GC_PROGRAM2);
	if (location2) {
		free(location2);
		OK2 = true;
	}

	if (OK1 && OK2) {
		return NULL;
	}

	return g_strdup_printf(_("Can't find %s or %s in path! Check that you have installed it correctly."), GC_PROGRAM1, GC_PROGRAM2);
}




static void datasource_gc_draw_circle(datasource_gc_widgets_t *widgets)
{
	double lat, lon;
	if (widgets->circle_onscreen) {
		widgets->viewport->draw_arc(widgets->circle_pen,
					    widgets->circle_x - widgets->circle_width/2,
					    widgets->circle_y - widgets->circle_width/2,
					    widgets->circle_width, widgets->circle_width, 0, 360
					    false);
	}
	/* Calculate widgets circle_x and circle_y. */
	/* Split up lat,lon into lat and lon. */
	if (2 == sscanf(widgets->center_entry.text(), "%lf,%lf", &lat, &lon)) {

		int x, y;
		struct LatLon ll = { .lat = lat, .lon = lon };

		const Coord coord(ll, widgets->viewport->get_coord_mode());
		widgets->viewport->coord_to_screen(&cord, &x, &y);
		/* TODO: real calculation. */
		if (x > -1000 && y > -1000 && x < (widgets->viewport->get_width() + 1000) &&
		    y < (widgets->viewport->get_width() + 1000)) {

			widgets->circle_x = x;
			widgets->circle_y = y;

			/* Determine miles per pixel. */
			const Coord coord1 = widgets->viewport->screen_to_coord(0, widgets->viewport->get_height()/2);
			const Coord coord2 = widgets->viewport->screen_to_coord(widgets->viewport->get_width(), widgets->viewport->get_height()/2);
			double pixels_per_meter = ((double)widgets->viewport->get_width()) / Coord::distance(coord1, coord2);

			/* This is approximate. */
			widgets->circle_width = widgets->miles_radius_spin.value();
				* METERSPERMILE * pixels_per_meter * 2;

			widgets->viewport->draw_arc(widgets->circle_pen,
						    widgets->circle_x - widgets->circle_width/2,
						    widgets->circle_y - widgets->circle_width/2,
						    widgets->circle_width, widgets->circle_width, 0, 360,
						    false);

			widgets->circle_onscreen = true;
		} else {
			widgets->circle_onscreen = false;
		}
	}

	/* See if onscreen. */
	/* Okay. */
	widgets->viewport->sync();
}




static void datasource_gc_add_setup_widgets(GtkWidget *dialog, Viewport * viewport, void * user_data)
{
	datasource_gc_widgets_t *widgets = (datasource_gc_widgets_t *)user_data;

	QLabel * num_label = new QLabel(QObject::tr("Number geocaches:"));
	widgets->num_spin.setMinimum(1);
	widgets->num_spin.setMaximum(1000);
	widgets->num_spin.setSingleStep(10);
	widgets->num_spin.setValue(20);


	QLabel * center_label = new QLabel(QObject::tr("Centered around:"));
	QLabel * miles_radius_label = new QLabel(QObject::tr("Miles Radius:"));
	widgets->miles_radius_spin.setMinimum(1);
	widgets->miles_radius_spin.setMaximum(1000);
	widgets->miles_radius_spin.setSingleStep(1);
	widgets->miles_radius_spin.setValue(5);

	struct LatLon ll = viewport->get_center()->get_latlon();
	char * s_ll = g_strdup_printf("%f,%f", ll.lat, ll.lon);
	widgets->center_entry.setText(QString(s_ll));
	free(s_ll);


	widgets->viewport = viewport;
	widgets->circle_pen = viewport->new_pen("#000000", 3);
	gdk_gc_set_function (widgets->circle_pen, GDK_INVERT);
	widgets->circle_onscreen = true;
	datasource_gc_draw_circle(widgets);

	QObject::connect(&widgets->center_entry, SIGNAL(editingFinished), widgets, SLOT (datasource_gc_draw_circle));
	QObject::connect(widgets->miles_radius_spin, SIGNAL("value-changed"), widgets, SLOT (datasource_gc_draw_circle));

	/* Packing all these widgets */
	GtkBox *box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	box->addWidget(num_label);
	box->addWidget(widgets->num_spin);
	box->addWidget(center_label);
	box->addWidget(&widgets->center_entry);
	box->addWidget(miles_radius_label);
	box->addWidget(widgets->miles_radius_spin);
	gtk_widget_show_all(dialog);
}




static ProcessOptions * datasource_gc_get_process_options(datasource_gc_widgets_t *widgets, void * not_used, const char *not_used2, const char *not_used3)
{
	ProcessOptions * po = new ProcessOptions();

	//char *safe_string = g_shell_quote (widgets->center_entry.text());
	char *safe_user = g_shell_quote(a_preferences_get(PREFERENCES_NAMESPACE_GC "username")->s);
	char *safe_pass = g_shell_quote(a_preferences_get(PREFERENCES_NAMESPACE_GC "password")->s);
	char *slat, *slon;
	double lat, lon;
	if (2 != sscanf(widgets->center_entry.text(), "%lf,%lf", &lat, &lon)) {
		fprintf(stderr, _("WARNING: Broken input - using some defaults\n"));
		lat = Preferences::get_default_lat();
		lon = Preferences::get_default_lon();
	}
	/* Convert double as string in C locale. */
	slat = a_coords_dtostr(lat);
	slon = a_coords_dtostr(lon);

	/* Unix specific shell commands
	   1. Remove geocache webpages (maybe be from different location).
	   2, Gets upto n geocaches as webpages for the specified user in radius r Miles.
	   3. Converts webpages into a single waypoint file, ignoring zero location waypoints '-z'.
	      Probably as they are premium member only geocaches and user is only a basic member.
	   Final output is piped into GPSbabel - hence removal of *html is done at beginning of the command sequence. */
	po->shell_command = g_strdup_printf("rm -f ~/.geo/caches/*.html ; %s -H ~/.geo/caches -P -n%d -r%.1fM -u %s -p %s %s %s ; %s -z ~/.geo/caches/*.html ",
					    GC_PROGRAM1,
					    widgets->num_spin.value(),
					    widgets->miles_radius_spin.value(),
					    safe_user,
					    safe_pass,
					    slat, slon,
					    GC_PROGRAM2);
	//free(safe_string);
	free(safe_user);
	free(safe_pass);
	free(slat);
	free(slon);

	return po;
}




static void datasource_gc_cleanup(datasource_gc_widgets_t *widgets)
{
	if (widgets->circle_onscreen) {
		widgets->viewport->draw_arc(widgets->circle_pen,
					    widgets->circle_x - widgets->circle_width/2,
					    widgets->circle_y - widgets->circle_width/2,
					    widgets->circle_width, widgets->circle_width, 0, 360,
					    false);
		widgets->viewport->sync();
	}
	free(widgets);
}




#endif /* #ifdef VIK_CONFIG_GEOCACHES */
