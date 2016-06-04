/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
/* WARNING: If you go beyond this point, we are NOT responsible for any ill effects on your sanity */
/* viktrwlayer.c -- 8000+ lines can make a difference in the state of things */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "viking.h"
#include "vikmapslayer.h"
#include "vikgpslayer.h"
#include "viktrwlayer_export.h"
#include "viktrwlayer_wpwin.h"
#include "viktrwlayer_propwin.h"
#include "viktrwlayer_analysis.h"
#include "viktrwlayer_tracklist.h"
#include "viktrwlayer_waypointlist.h"
#ifdef VIK_CONFIG_GEOTAG
#include "viktrwlayer_geotag.h"
#include "geotag_exif.h"
#endif
#include "garminsymbols.h"
#include "thumbnails.h"
#include "background.h"
#include "gpx.h"
#include "file.h"
#include "dialog.h"
#include "geojson.h"
#include "babel.h"
#include "dem.h"
#include "dems.h"
#include "geonamessearch.h"
#ifdef VIK_CONFIG_OPENSTREETMAP
#include "osm-traces.h"
#endif
#include "acquire.h"
#include "datasources.h"
#include "datasource_gps.h"
#include "vikexttools.h"
#include "vikexttool_datasources.h"
#include "ui_util.h"
#include "vikutils.h"
#include "gpspoint.h"
#include "clipboard.h"
#include "settings.h"
#include "util.h"
#include "globals.h"

#include "vikrouting.h"

#include "icons/icons.h"

#include "layer_trw_draw.h"

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>


#define POINTS 1
#define LINES 2

/* this is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5

#define MIN_STOP_LENGTH 15
#define MAX_STOP_LENGTH 86400

using namespace SlavGPS;

extern bool have_astro_program;
extern char * astro_program;
extern bool have_diary_program;
extern char *diary_program;
extern bool have_geojson_export;


typedef enum {
  MA_VTL = 0,
  MA_VLP,
  MA_SUBTYPE, // OR END for Layer only
  MA_SUBLAYER_ID,
  MA_CONFIRM,
  MA_VVP,
  MA_TV_ITER,
  MA_MISC,
  MA_LAST,
} menu_array_index;





void LayerTRW::add_menu_items(GtkMenu * menu, void * vlp)
{
	VikTrwLayer * vtl = (VikTrwLayer *) this->vl;

	static trw_menu_layer_t pass_along_data;
	pass_along_data.layer = this;
	pass_along_data.panel = (LayersPanel *) (((VikLayersPanel *) vlp)->panel_ref);
	static trw_menu_layer_t * pass_along = &pass_along_data;

	GtkWidget *item;
	GtkWidget *export_submenu;

	item = gtk_menu_item_new();
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	gtk_widget_show ( item );

	if (vtl->trw->current_track) {
		if (vtl->trw->current_track->is_route) {
			item = gtk_menu_item_new_with_mnemonic(_("_Finish Route"));
		} else {
			item = gtk_menu_item_new_with_mnemonic(_("_Finish Track"));
		}
		g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(trw_layer_finish_track), pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		// Add separator
		item = gtk_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
	}

	/* Now with icons */
	item = gtk_image_menu_item_new_with_mnemonic ( _("_View Layer") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_view), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );

	GtkWidget *view_submenu = gtk_menu_new();
	item = gtk_image_menu_item_new_with_mnemonic ( _("V_iew") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU) );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), view_submenu );

	item = gtk_menu_item_new_with_mnemonic ( _("View All _Tracks") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_tracks_view), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (view_submenu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new_with_mnemonic ( _("View All _Routes") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_routes_view), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (view_submenu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new_with_mnemonic ( _("View All _Waypoints") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_waypoints_view), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (view_submenu), item);
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_with_mnemonic ( _("_Goto Center of Layer") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_centerize), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new_with_mnemonic ( _("Goto _Waypoint...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_wp), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );

	export_submenu = gtk_menu_new ();
	item = gtk_image_menu_item_new_with_mnemonic ( _("_Export Layer") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_HARDDISK, GTK_ICON_SIZE_MENU) );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), export_submenu );

	item = gtk_menu_item_new_with_mnemonic ( _("Export as GPS_Point...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpspoint), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new_with_mnemonic ( _("Export as GPS_Mapper...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpsmapper), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new_with_mnemonic ( _("Export as _GPX...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpx), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new_with_mnemonic ( _("Export as _KML...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_kml), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
	gtk_widget_show ( item );

	if ( have_geojson_export ) {
		item = gtk_menu_item_new_with_mnemonic ( _("Export as GEO_JSON...") );
		g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_geojson), pass_along );
		gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
		gtk_widget_show ( item );
	}

	item = gtk_menu_item_new_with_mnemonic ( _("Export via GPSbabel...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_babel), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
	gtk_widget_show ( item );

	char* external1 = g_strdup_printf ( _("Open with External Program_1: %s"), a_vik_get_external_gpx_program_1() );
	item = gtk_menu_item_new_with_mnemonic ( external1 );
	free( external1 );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_external_gpx_1), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
	gtk_widget_show ( item );

	char* external2 = g_strdup_printf ( _("Open with External Program_2: %s"), a_vik_get_external_gpx_program_2() );
	item = gtk_menu_item_new_with_mnemonic ( external2 );
	free( external2 );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_external_gpx_2), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (export_submenu), item);
	gtk_widget_show ( item );

	GtkWidget *new_submenu = gtk_menu_new();
	item = gtk_image_menu_item_new_with_mnemonic ( _("_New") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), new_submenu);

	item = gtk_image_menu_item_new_with_mnemonic ( _("New _Waypoint...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wp), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (new_submenu), item);
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_with_mnemonic ( _("New _Track") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_track), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (new_submenu), item);
	gtk_widget_show ( item );
	// Make it available only when a new track *not* already in progress
	gtk_widget_set_sensitive ( item, ! (bool)KPOINTER_TO_INT(vtl->trw->current_track) );

	item = gtk_image_menu_item_new_with_mnemonic ( _("New _Route") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_route), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (new_submenu), item);
	gtk_widget_show ( item );
	// Make it available only when a new track *not* already in progress
	gtk_widget_set_sensitive ( item, ! (bool)KPOINTER_TO_INT(vtl->trw->current_track) );

#ifdef VIK_CONFIG_GEOTAG
	item = gtk_menu_item_new_with_mnemonic ( _("Geotag _Images...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
#endif

	GtkWidget *acquire_submenu = gtk_menu_new ();
	item = gtk_image_menu_item_new_with_mnemonic ( _("_Acquire") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_MENU) );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), acquire_submenu );

	item = gtk_menu_item_new_with_mnemonic ( _("From _GPS...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_gps_cb), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
	gtk_widget_show ( item );

	/* FIXME: only add menu when at least a routing engine has support for Directions */
	item = gtk_menu_item_new_with_mnemonic ( _("From _Directions...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_routing_cb), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
	gtk_widget_show ( item );

#ifdef VIK_CONFIG_OPENSTREETMAP
	item = gtk_menu_item_new_with_mnemonic ( _("From _OSM Traces...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_osm_cb), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new_with_mnemonic ( _("From _My OSM Traces...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_osm_my_traces_cb), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
	gtk_widget_show ( item );
#endif

	item = gtk_menu_item_new_with_mnemonic ( _("From _URL...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_url_cb), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
	gtk_widget_show ( item );

#ifdef VIK_CONFIG_GEONAMES
	GtkWidget *wikipedia_submenu = gtk_menu_new();
	item = gtk_image_menu_item_new_with_mnemonic ( _("From _Wikipedia Waypoints") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
	gtk_menu_shell_append(GTK_MENU_SHELL (acquire_submenu), item);
	gtk_widget_show(item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), wikipedia_submenu);

	item = gtk_image_menu_item_new_with_mnemonic ( _("Within _Layer Bounds") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wikipedia_wp_layer), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (wikipedia_submenu), item);
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_with_mnemonic ( _("Within _Current View") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_100, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wikipedia_wp_viewport), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (wikipedia_submenu), item);
	gtk_widget_show ( item );
#endif

#ifdef VIK_CONFIG_GEOCACHES
	item = gtk_menu_item_new_with_mnemonic ( _("From Geo_caching...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_geocache_cb), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
	gtk_widget_show ( item );
#endif

#ifdef VIK_CONFIG_GEOTAG
	item = gtk_menu_item_new_with_mnemonic ( _("From Geotagged _Images...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_geotagged_cb), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
	gtk_widget_show ( item );
#endif

	item = gtk_menu_item_new_with_mnemonic ( _("From _File...") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_acquire_file_cb), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (acquire_submenu), item);
	gtk_widget_set_tooltip_text (item, _("Import File With GPS_Babel..."));
	gtk_widget_show ( item );

	vik_ext_tool_datasources_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), GTK_MENU (acquire_submenu) );

	GtkWidget *upload_submenu = gtk_menu_new ();
	item = gtk_image_menu_item_new_with_mnemonic ( _("_Upload") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU) );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), upload_submenu );

	item = gtk_image_menu_item_new_with_mnemonic ( _("Upload to _GPS...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_gps_upload), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (upload_submenu), item);
	gtk_widget_show ( item );

#ifdef VIK_CONFIG_OPENSTREETMAP
	item = gtk_image_menu_item_new_with_mnemonic ( _("Upload to _OSM...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_osm_traces_upload_cb), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (upload_submenu), item);
	gtk_widget_show ( item );
#endif

	GtkWidget *delete_submenu = gtk_menu_new ();
	item = gtk_image_menu_item_new_with_mnemonic ( _("De_lete") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), delete_submenu );

	item = gtk_image_menu_item_new_with_mnemonic ( _("Delete All _Tracks") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_tracks), pass_along );
	gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_with_mnemonic ( _("Delete Tracks _From Selection...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_tracks_from_selection), pass_along );
	gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _All Routes") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_routes), pass_along );
	gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_with_mnemonic ( _("_Delete Routes From Selection...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_routes_from_selection), pass_along );
	gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_with_mnemonic ( _("Delete All _Waypoints") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_waypoints), pass_along );
	gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_with_mnemonic ( _("Delete Waypoints From _Selection...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_waypoints_from_selection), pass_along );
	gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
	gtk_widget_show ( item );

	VikLayersPanel * vlp_ = VIK_LAYERS_PANEL(vlp);
	item = a_acquire_trwlayer_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), (VikLayersPanel *) vlp,
					 (VikViewport *) vlp_->panel_ref->get_viewport()->vvp, vtl );
	if ( item ) {
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show ( item );
	}

	item = a_acquire_trwlayer_track_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), (VikLayersPanel *) vlp,
					       (VikViewport *) vlp_->panel_ref->get_viewport()->vvp, vtl );
	if ( item ) {
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show ( item );
	}

	item = gtk_image_menu_item_new_with_mnemonic ( _("Track _List...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_list_dialog), pass_along );
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	gtk_widget_show ( item );
	gtk_widget_set_sensitive ( item, (bool)(vtl->trw->tracks.size() + vtl->trw->routes.size()) );

	item = gtk_image_menu_item_new_with_mnemonic ( _("_Waypoint List...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_list_dialog), pass_along );
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	gtk_widget_show ( item );
	gtk_widget_set_sensitive ( item, (bool) (vtl->trw->waypoints.size()) );

	GtkWidget *external_submenu = create_external_submenu ( menu );
	// TODO: Should use selected layer's centre - rather than implicitly using the current viewport
	vik_ext_tools_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vtl)), GTK_MENU (external_submenu), NULL );
}




/* vlp can be NULL if necessary - i.e. right-click from a tool */
/* viewpoint is now available instead */
bool LayerTRW::sublayer_add_menu_items(GtkMenu * menu, void * panel, int subtype, void * sublayer, GtkTreeIter * iter, Viewport * viewport)
{
  VikTrwLayer * l = (VikTrwLayer *) this->vl;


  sg_uid_t uid = (sg_uid_t) ((long) sublayer);
  GtkWidget *item;
  bool rv = false;


  static trw_menu_sublayer_t pass_along_data;
  static trw_menu_sublayer_t * pass_along = &pass_along_data;

  pass_along->layer       = this;
  pass_along->panel       = (LayersPanel *) panel;
  pass_along->subtype     = subtype;
  pass_along->sublayer_id = uid;
  pass_along->confirm     = true; // Confirm delete request
  pass_along->viewport    = viewport;
  pass_along->tv_iter     = iter;
  pass_along->misc        = NULL; // For misc purposes - maybe track or waypoint

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT || subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE )
  {
    rv = true;

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_PROPERTIES, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_properties_item), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK) {
      Track * trk = l->trw->tracks.at(uid);
      if (trk && trk->property_dialog)
        gtk_widget_set_sensitive(GTK_WIDGET(item), false );
    }
    if (subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE) {
      Track * trk = l->trw->routes.at(uid);
      if (trk && trk->property_dialog)
        gtk_widget_set_sensitive(GTK_WIDGET(item), false );
    }

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_CUT, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_cut_item_cb), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_COPY, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_copy_item_cb), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_DELETE, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_item), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT )
    {
      // Always create separator as now there is always at least the transform menu option
      item = gtk_menu_item_new ();
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );

      /* could be a right-click using the tool */
      if ( panel != NULL ) {
        item = gtk_image_menu_item_new_with_mnemonic ( _("_Goto") );
        gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_waypoint), pass_along );
        gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
        gtk_widget_show ( item );
      }

      sg_uid_t wp_uid = (sg_uid_t) ((long) sublayer);
      Waypoint * wp = VIK_TRW_LAYER(l)->trw->waypoints.at(wp_uid);

      if ( wp && wp->name ) {
        if ( is_valid_geocache_name ( wp->name ) ) {
          item = gtk_menu_item_new_with_mnemonic ( _("_Visit Geocache Webpage") );
          g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_gc_webpage), pass_along );
          gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
          gtk_widget_show ( item );
        }
#ifdef VIK_CONFIG_GEOTAG
        item = gtk_menu_item_new_with_mnemonic ( _("Geotag _Images...") );
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging_waypoint), pass_along );
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        gtk_widget_set_tooltip_text (item, _("Geotag multiple images against this waypoint"));
        gtk_widget_show ( item );
#endif
      }

      if ( wp && wp->image )
      {
        // Set up image paramater
        pass_along->misc = wp->image;

        item = gtk_image_menu_item_new_with_mnemonic ( _("_Show Picture...") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-Show Picture", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_show_picture), pass_along );
        gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
        gtk_widget_show ( item );

#ifdef VIK_CONFIG_GEOTAG
	GtkWidget *geotag_submenu = gtk_menu_new ();
	item = gtk_image_menu_item_new_with_mnemonic ( _("Update Geotag on _Image") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), geotag_submenu );

	item = gtk_menu_item_new_with_mnemonic ( _("_Update") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging_waypoint_mtime_update), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (geotag_submenu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new_with_mnemonic ( _("Update and _Keep File Timestamp") );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging_waypoint_mtime_keep), pass_along );
	gtk_menu_shell_append (GTK_MENU_SHELL (geotag_submenu), item);
	gtk_widget_show ( item );
#endif
      }

      if ( wp )
      {
        if ( wp->url ||
             ( wp->comment && !strncmp(wp->comment, "http", 4) ) ||
             ( wp->description && !strncmp(wp->description, "http", 4) )) {
          item = gtk_image_menu_item_new_with_mnemonic ( _("Visit _Webpage") );
          gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU) );
          g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_webpage), pass_along );
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show ( item );
        }
      }
    }
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_TRACKS || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTES ) {
    item = gtk_image_menu_item_new_from_stock ( GTK_STOCK_PASTE, NULL );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_paste_item_cb), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
    // TODO: only enable if suitable item is in clipboard - want to determine *which* sublayer type
    if ( a_clipboard_type ( ) == VIK_CLIPBOARD_DATA_SUBLAYER )
      gtk_widget_set_sensitive ( item, true );
    else
      gtk_widget_set_sensitive ( item, false );

    // Add separator
    item = gtk_menu_item_new ();
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
  }

  if ( panel && (subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT) )
  {
    rv = true;
    item = gtk_image_menu_item_new_with_mnemonic ( _("_New Waypoint...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_wp), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS )
  {
    item = gtk_image_menu_item_new_with_mnemonic ( _("_View All Waypoints") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_waypoints_view), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Goto _Waypoint...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_wp), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _All Waypoints") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_waypoints), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Delete Waypoints From Selection...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_waypoints_from_selection), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    GtkWidget *vis_submenu = gtk_menu_new ();
    item = gtk_menu_item_new_with_mnemonic ( _("_Visibility") );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), vis_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Show All Waypoints") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoints_visibility_on), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Hide All Waypoints") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoints_visibility_off), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Toggle") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoints_visibility_toggle), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_List Waypoints...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_waypoint_list_dialog), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACKS )
  {
    rv = true;

    if ( l->trw->current_track && !l->trw->current_track->is_route ) {
      item = gtk_menu_item_new_with_mnemonic ( _("_Finish Track") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_finish_track), pass_along );
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show ( item );
      // Add separator
      item = gtk_menu_item_new ();
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    item = gtk_image_menu_item_new_with_mnemonic ( _("_View All Tracks") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_tracks_view), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_New Track") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_track), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    // Make it available only when a new track *not* already in progress
    gtk_widget_set_sensitive ( item, ! (bool)KPOINTER_TO_INT(l->trw->current_track) );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _All Tracks") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_tracks), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Delete Tracks From Selection...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_tracks_from_selection), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    GtkWidget *vis_submenu = gtk_menu_new ();
    item = gtk_menu_item_new_with_mnemonic ( _("_Visibility") );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), vis_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Show All Tracks") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_tracks_visibility_on), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Hide All Tracks") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_tracks_visibility_off), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Toggle") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_tracks_visibility_toggle), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_List Tracks...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_list_dialog_single), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("_Statistics") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_tracks_stats), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTES )
  {
    rv = true;

    if ( l->trw->current_track && l->trw->current_track->is_route ) {
      item = gtk_menu_item_new_with_mnemonic ( _("_Finish Route") );
      // Reuse finish track method
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_finish_track), pass_along );
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show ( item );
      // Add separator
      item = gtk_menu_item_new ();
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    item = gtk_image_menu_item_new_with_mnemonic ( _("_View All Routes") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_routes_view), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_New Route") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_new_route), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    // Make it available only when a new track *not* already in progress
    gtk_widget_set_sensitive ( item, ! (bool)KPOINTER_TO_INT(l->trw->current_track) );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _All Routes") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_all_routes), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Delete Routes From Selection...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_routes_from_selection), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    GtkWidget *vis_submenu = gtk_menu_new ();
    item = gtk_menu_item_new_with_mnemonic ( _("_Visibility") );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), vis_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Show All Routes") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_routes_visibility_on), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Hide All Routes") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_routes_visibility_off), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Toggle") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_routes_visibility_toggle), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(vis_submenu), item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_List Routes...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_list_dialog_single), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );

    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("_Statistics") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_routes_stats), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
  }


  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_TRACKS || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTES ) {
    GtkWidget *submenu_sort = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Sort") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu_sort );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Name _Ascending") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_ASCENDING, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_sort_order_a2z), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(submenu_sort), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Name _Descending") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_DESCENDING, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_sort_order_z2a), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(submenu_sort), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Date Ascending") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_ASCENDING, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_sort_order_timestamp_ascend), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(submenu_sort), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Date Descending") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SORT_DESCENDING, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_sort_order_timestamp_descend), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(submenu_sort), item );
    gtk_widget_show ( item );
  }

  GtkWidget *upload_submenu = gtk_menu_new ();

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE )
  {
    item = gtk_menu_item_new ();
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if ( l->trw->current_track && subtype == VIK_TRW_LAYER_SUBLAYER_TRACK && !l->trw->current_track->is_route )
      item = gtk_menu_item_new_with_mnemonic ( _("_Finish Track") );
    if ( l->trw->current_track && subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE && l->trw->current_track->is_route )
      item = gtk_menu_item_new_with_mnemonic ( _("_Finish Route") );
    if ( l->trw->current_track ) {
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_finish_track), pass_along );
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show ( item );

      // Add separator
      item = gtk_menu_item_new ();
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_image_menu_item_new_with_mnemonic ( _("_View Track") );
    else
      item = gtk_image_menu_item_new_with_mnemonic ( _("_View Route") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_auto_track_view), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("_Statistics") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_statistics), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    GtkWidget *goto_submenu;
    goto_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Goto") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), goto_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Startpoint") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_FIRST, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_startpoint), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("\"_Center\"") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_center), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Endpoint") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_LAST, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_endpoint), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Highest Altitude") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_TOP, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_max_alt), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Lowest Altitude") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GOTO_BOTTOM, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_min_alt), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
    gtk_widget_show ( item );

    // Routes don't have speeds
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Maximum Speed") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_MEDIA_FORWARD, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_goto_track_max_speed), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(goto_submenu), item );
      gtk_widget_show ( item );
    }

    GtkWidget *combine_submenu;
    combine_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("Co_mbine") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CONNECT, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), combine_submenu );

    // Routes don't have times or segments...
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      item = gtk_menu_item_new_with_mnemonic ( _("_Merge By Time...") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_merge_by_timestamp), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(combine_submenu), item );
      gtk_widget_show ( item );

      item = gtk_menu_item_new_with_mnemonic ( _("Merge _Segments") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_merge_by_segment), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(combine_submenu), item );
      gtk_widget_show ( item );
    }

    item = gtk_menu_item_new_with_mnemonic ( _("Merge _With Other Tracks...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_merge_with_other), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(combine_submenu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_menu_item_new_with_mnemonic ( _("_Append Track...") );
    else
      item = gtk_menu_item_new_with_mnemonic ( _("_Append Route...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_append_track), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(combine_submenu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_menu_item_new_with_mnemonic ( _("Append _Route...") );
    else
      item = gtk_menu_item_new_with_mnemonic ( _("Append _Track...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_append_other), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(combine_submenu), item );
    gtk_widget_show ( item );

    GtkWidget *split_submenu;
    split_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Split") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_DISCONNECT, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), split_submenu );

    // Routes don't have times or segments...
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      item = gtk_menu_item_new_with_mnemonic ( _("_Split By Time...") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_split_by_timestamp), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(split_submenu), item );
      gtk_widget_show ( item );

      // ATM always enable this entry - don't want to have to analyse the track before displaying the menu - to keep the menu speedy
      item = gtk_menu_item_new_with_mnemonic ( _("Split Se_gments") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_split_segments), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(split_submenu), item );
      gtk_widget_show ( item );
    }

    item = gtk_menu_item_new_with_mnemonic ( _("Split By _Number of Points...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_split_by_n_points), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(split_submenu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("Split at _Trackpoint") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_split_at_trackpoint), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(split_submenu), item );
    gtk_widget_show ( item );
    // Make it available only when a trackpoint is selected.
    gtk_widget_set_sensitive ( item, (bool)KPOINTER_TO_INT(l->trw->current_tpl) );

    GtkWidget *insert_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Insert Points") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), insert_submenu );

    item = gtk_menu_item_new_with_mnemonic ( _("Insert Point _Before Selected Point") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_insert_point_before), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(insert_submenu), item );
    gtk_widget_show ( item );
    // Make it available only when a point is selected
    gtk_widget_set_sensitive ( item, (bool)KPOINTER_TO_INT(l->trw->current_tpl) );

    item = gtk_menu_item_new_with_mnemonic ( _("Insert Point _After Selected Point") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_insert_point_after), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(insert_submenu), item );
    gtk_widget_show ( item );
    // Make it available only when a point is selected
    gtk_widget_set_sensitive ( item, (bool)KPOINTER_TO_INT(l->trw->current_tpl) );

    GtkWidget *delete_submenu;
    delete_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete Poi_nts") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), delete_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("Delete _Selected Point") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_point_selected), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
    gtk_widget_show ( item );
    // Make it available only when a point is selected
    gtk_widget_set_sensitive ( item, (bool)KPOINTER_TO_INT(l->trw->current_tpl) );

    item = gtk_menu_item_new_with_mnemonic ( _("Delete Points With The Same _Position") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_points_same_position), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
    gtk_widget_show ( item );

    item = gtk_menu_item_new_with_mnemonic ( _("Delete Points With The Same _Time") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_delete_points_same_time), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(delete_submenu), item );
    gtk_widget_show ( item );

    GtkWidget *transform_submenu;
    transform_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Transform") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CONVERT, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), transform_submenu );

    GtkWidget *dem_submenu;
    dem_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Apply DEM Data") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-DEM Download", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
    gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), dem_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Overwrite") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_apply_dem_data_all), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(dem_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Overwrite any existing elevation values with DEM values"));
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Keep Existing") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_apply_dem_data_only_missing), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(dem_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Keep existing elevation values, only attempt for missing values"));
    gtk_widget_show ( item );

    GtkWidget *smooth_submenu;
    smooth_submenu = gtk_menu_new ();
    item = gtk_menu_item_new_with_mnemonic ( _("_Smooth Missing Elevation Data") );
    gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), smooth_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Interpolated") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_missing_elevation_data_interp), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(smooth_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Interpolate between known elevation values to derive values for the missing elevations"));
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Flat") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_missing_elevation_data_flat), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(smooth_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Set unknown elevation values to the last known value"));
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_image_menu_item_new_with_mnemonic ( _("C_onvert to a Route") );
    else
      item = gtk_image_menu_item_new_with_mnemonic ( _("C_onvert to a Track") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CONVERT, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_convert_track_route), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
    gtk_widget_show ( item );

    // Routes don't have timestamps - so these are only available for tracks
    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Anonymize Times") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_anonymize_times), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
      gtk_widget_set_tooltip_text (item, _("Shift timestamps to a relative offset from 1901-01-01"));
      gtk_widget_show ( item );

      item = gtk_image_menu_item_new_with_mnemonic ( _("_Interpolate Times") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_interpolate_times), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
      gtk_widget_set_tooltip_text (item, _("Reset trackpoint timestamps between the first and last points such that track is traveled at equal speed"));
      gtk_widget_show ( item );
    }

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Reverse Track") );
    else
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Reverse Route") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_BACK, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_reverse), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("Refine Route...") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_route_refine), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    /* ATM This function is only available via the layers panel, due to the method in finding out the maps in use */
    if ( panel ) {
      if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
        item = gtk_image_menu_item_new_with_mnemonic ( _("Down_load Maps Along Track...") );
      else
        item = gtk_image_menu_item_new_with_mnemonic ( _("Down_load Maps Along Route...") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-Maps Download", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_download_map_along_track_cb), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Export Track as GPX...") );
    else
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Export Route as GPX...") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_HARDDISK, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_export_gpx_track), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK )
      item = gtk_image_menu_item_new_with_mnemonic ( _("E_xtend Track End") );
    else
      item = gtk_image_menu_item_new_with_mnemonic ( _("E_xtend Route End") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_extend_track_end), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );

    if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("Extend _Using Route Finder") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-Route Finder", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_extend_track_end_route_finder), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }

    // ATM can't upload a single waypoint but can do waypoints to a GPS
    if ( subtype != VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Upload") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU) );
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show ( item );
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), upload_submenu );

      item = gtk_image_menu_item_new_with_mnemonic ( _("_Upload to GPS...") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_gps_upload_any), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(upload_submenu), item );
      gtk_widget_show ( item );
    }
  }

  GtkWidget *external_submenu = create_external_submenu ( menu );

  // These are only made available if a suitable program is installed
  if ( (have_astro_program || have_diary_program) &&
       (subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT) ) {

    if ( have_diary_program ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Diary") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_SPELL_CHECK, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_diary), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(external_submenu), item );
      gtk_widget_set_tooltip_text (item, _("Open diary program at this date"));
      gtk_widget_show ( item );
    }

    if ( have_astro_program ) {
      item = gtk_image_menu_item_new_with_mnemonic ( _("_Astronomy") );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_astro), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(external_submenu), item );
      gtk_widget_set_tooltip_text (item, _("Open astronomy program at this date and location"));
      gtk_widget_show ( item );
    }
  }

  if ( l->trw->current_tpl || l->trw->current_wp ) {
    // For the selected point
    VikCoord *vc;
    if ( l->trw->current_tpl )
      vc = &(((Trackpoint *) l->trw->current_tpl->data)->coord);
    else
      vc = &(l->trw->current_wp->coord);
    vik_ext_tools_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l)), GTK_MENU (external_submenu), vc );
  }
  else {
    // Otherwise for the selected sublayer
    // TODO: Should use selected items centre - rather than implicitly using the current viewport
    vik_ext_tools_add_menu_items_to_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l)), GTK_MENU (external_submenu), NULL );
  }


#ifdef VIK_CONFIG_GOOGLE
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE &&  ( l->trw->is_valid_google_route((sg_uid_t) ((long) sublayer)) ))
  {
    item = gtk_image_menu_item_new_with_mnemonic ( _("_View Google Directions") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_google_route_webpage), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
  }
#endif

  // Some things aren't usable with routes
  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK ) {
#ifdef VIK_CONFIG_OPENSTREETMAP
    item = gtk_image_menu_item_new_with_mnemonic ( _("Upload to _OSM...") );
    // Convert internal pointer into track
    sg_uid_t uid = (sg_uid_t) ((long) sublayer);
    pass_along->misc = l->trw->tracks.at(uid);
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_osm_traces_upload_track_cb), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(upload_submenu), item );
    gtk_widget_show ( item );
#endif

    // Currently filter with functions all use shellcommands and thus don't work in Windows
#ifndef WINDOWS
    item = gtk_image_menu_item_new_with_mnemonic ( _("Use with _Filter") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_track_use_with_filter), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
    gtk_widget_show ( item );
#endif

    /* ATM This function is only available via the layers panel, due to needing a panel */
    if ( panel ) {
      sg_uid_t uid = (sg_uid_t) ((long) sublayer);
      item = a_acquire_track_menu ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(l)), (VikLayersPanel *) ((LayersPanel *) panel)->gob,
                                    (VikViewport *) ((LayersPanel *) panel)->get_viewport()->vvp,
                                    l->trw->tracks.at(uid));
      if ( item ) {
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        gtk_widget_show ( item );
      }
    }

#ifdef VIK_CONFIG_GEOTAG
    item = gtk_menu_item_new_with_mnemonic ( _("Geotag _Images...") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_geotagging_track), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
#endif
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_TRACK || subtype == VIK_TRW_LAYER_SUBLAYER_ROUTE ) {
    // Only show on viewport popmenu when a trackpoint is selected
    if ( ! panel && l->trw->current_tpl ) {
      // Add separator
      item = gtk_menu_item_new ();
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );

      item = gtk_image_menu_item_new_with_mnemonic ( _("_Edit Trackpoint") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU) );
      g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_edit_trackpoint), pass_along );
      gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
      gtk_widget_show ( item );
    }
  }

  if ( subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINTS || subtype == VIK_TRW_LAYER_SUBLAYER_WAYPOINT ) {
    GtkWidget *transform_submenu;
    transform_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Transform") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_CONVERT, GTK_ICON_SIZE_MENU) );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), transform_submenu );

    GtkWidget *dem_submenu;
    dem_submenu = gtk_menu_new ();
    item = gtk_image_menu_item_new_with_mnemonic ( _("_Apply DEM Data") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock ("vik-icon-DEM Download", GTK_ICON_SIZE_MENU) ); // Own icon - see stock_icons in vikwindow.c
    gtk_menu_shell_append ( GTK_MENU_SHELL(transform_submenu), item );
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), dem_submenu );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Overwrite") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_apply_dem_data_wpt_all), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(dem_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Overwrite any existing elevation values with DEM values"));
    gtk_widget_show ( item );

    item = gtk_image_menu_item_new_with_mnemonic ( _("_Keep Existing") );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(trw_layer_apply_dem_data_wpt_only_missing), pass_along );
    gtk_menu_shell_append ( GTK_MENU_SHELL(dem_submenu), item );
    gtk_widget_set_tooltip_text (item, _("Keep existing elevation values, only attempt for missing values"));
    gtk_widget_show ( item );
  }

  gtk_widget_show_all ( GTK_WIDGET(menu) );

  return rv;
}
