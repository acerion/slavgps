/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2008, Quy Tonthat <qtonthat@gmail.com>
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


#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include "viking.h"
#include "garminsymbols.h"
#include "icons/icons.h"
#include "globals.h"

#include <string.h>
#include <stdlib.h>

static struct {
	char *sym;     /* icon names used by gpsbabel, garmin */
	char *old_sym; /* keep backward compatible */
	int num;
	char *desc;
	const GdkPixdata *data;
	const GdkPixdata *data_large;
	GdkPixbuf *icon;
} garmin_syms[] = {
	/* "sym" are in 'Title Case' like in gpsbabel. This is needed for
	 * devices like Garmin Oregon 450. Old exports with lower case
	 * identifiers will be automatically converted to the version defined
	 * inside the table by vikwaypoint.c:vik_waypoint_set_symbol().
	 * The hash itself tries to keep all operations case independent
	 * using str_equal_casefold() and str_hash_casefold(). This is
	 * necessary to allow a_get_hashed_sym() to match the lower case
	 * version with the identifier stored in "sym".
	 */
	/*---------------------------------------------------------------
	  Marine symbols
	  ---------------------------------------------------------------*/
	{(char *) "Marina",                          (char *) "anchor",               0,  (char *) "white anchor symbol",               &wp_anchor_pixbuf,          &wp_anchor_large_pixbuf,          NULL },
	{(char *) "Bell",                            (char *) "bell",                 1,  (char *) "white bell symbol",                 &wp_bell_pixbuf,            &wp_bell_large_pixbuf,            NULL },
	{(char *) "Green Diamon",                    (char *) "diamond_grn",          2,  (char *) "green diamond symbol",              &wp_diamond_grn_pixbuf,     NULL,                             NULL },
	{(char *) "Red Diamon",                      (char *) "diamond_red",          3,  (char *) "red diamond symbol",                &wp_diamond_red_pixbuf,     NULL,                             NULL },
	{(char *) "Diver Down Flag 1",               (char *) "dive1",                4,  (char *) "diver down flag 1",                 &wp_dive1_pixbuf,           &wp_dive1_large_pixbuf,           NULL },
	{(char *) "Diver Down Flag 2",               (char *) "dive2",                5,  (char *) "diver down flag 2",                 &wp_dive2_pixbuf,           &wp_dive2_large_pixbuf,           NULL },
	{(char *) "Bank",                            (char *) "dollar",               6,  (char *) "white dollar symbol",               &wp_dollar_pixbuf,          &wp_dollar_large_pixbuf,          NULL },
	{(char *) "Fishing Area",                    (char *) "fish",                 7,  (char *) "white fish symbol",                 &wp_fish_pixbuf,            &wp_fish_large_pixbuf,            NULL },
	{(char *) "Gas Station",                     (char *) "fuel",                 8,  (char *) "white fuel symbol",                 &wp_fuel_pixbuf,            &wp_fuel_large_pixbuf,            NULL },
	{(char *) "Horn",                            (char *) "horn",                 9,  (char *) "white horn symbol",                 &wp_horn_pixbuf,            &wp_horn_large_pixbuf,            NULL },
	{(char *) "Residence",                       (char *) "house",               10,  (char *) "white house symbol",                &wp_house_pixbuf,           &wp_house_large_pixbuf,           NULL },
	{(char *) "Restaurant",                      (char *) "knife",               11,  (char *) "white knife & fork symbol",         &wp_knife_pixbuf,           &wp_knife_large_pixbuf,           NULL },
	{(char *) "Light",                           (char *) "light",               12,  (char *) "white light symbol",                &wp_light_pixbuf,           &wp_light_large_pixbuf,           NULL },
	{(char *) "Bar",                             (char *) "mug",                 13,  (char *) "white mug symbol",                  &wp_mug_pixbuf,             &wp_mug_large_pixbuf,             NULL },
	{(char *) "Skull and Crossbones",            (char *) "skull",               14,  (char *) "white skull and crossbones symbol", &wp_skull_pixbuf,           &wp_skull_large_pixbuf,           NULL },
	{(char *) "Green Square",                    (char *) "square_grn",          15,  (char *) "green square symbol",               &wp_square_grn_pixbuf,      NULL,                             NULL },
	{(char *) "Red Square",                      (char *) "square_red",          16,  (char *) "red square symbol",                 &wp_square_red_pixbuf,      NULL,                             NULL },
	{(char *) "Buoy, White",                     (char *) "wbuoy",               17,  (char *) "white buoy waypoint symbol",        &wp_wbuoy_pixbuf,           &wp_wbuoy_large_pixbuf,           NULL },
	{(char *) "Waypoint",                        (char *) "wpt_dot",             18,  (char *) "waypoint dot",                      &wp_wpt_dot_pixbuf,         NULL,                             NULL },
	{(char *) "Shipwreck",                       (char *) "wreck",               19,  (char *) "white wreck symbol",                &wp_wreck_pixbuf,           &wp_wreck_large_pixbuf,           NULL },
	{(char *) "None",                            (char *) "null",                20,  (char *) "null symbol (transparent)",         &wp_null_pixbuf,            NULL,                             NULL },
	{(char *) "Man Overboard",                   (char *) "mob",                 21,  (char *) "man overboard symbol",              &wp_mob_pixbuf,             &wp_mob_large_pixbuf,             NULL },
	{(char *) "Navaid, Amber",                   (char *) "buoy_ambr",           22,  (char *) "amber map buoy symbol",             &wp_buoy_ambr_pixbuf,       &wp_buoy_ambr_large_pixbuf,       NULL },
	{(char *) "Navaid, Black",                   (char *) "buoy_blck",           23,  (char *) "black map buoy symbol",             &wp_buoy_blck_pixbuf,       &wp_buoy_blck_large_pixbuf,       NULL },
	{(char *) "Navaid, Blue",                    (char *) "buoy_blue",           24,  (char *) "blue map buoy symbol",              &wp_buoy_blue_pixbuf,       &wp_buoy_blue_large_pixbuf,       NULL },
	{(char *) "Navaid, Green",                   (char *) "buoy_grn",            25,  (char *) "green map buoy symbol",             &wp_buoy_grn_pixbuf,        &wp_buoy_grn_large_pixbuf,        NULL },
	{(char *) "Navaid, Green/Red",               (char *) "buoy_grn_red",        26,  (char *) "green/red map buoy symbol",         &wp_buoy_grn_red_pixbuf,    &wp_buoy_grn_red_large_pixbuf,    NULL },
	{(char *) "Navaid, Green/White",             (char *) "buoy_grn_wht",        27,  (char *) "green/white map buoy symbol",       &wp_buoy_grn_wht_pixbuf,    &wp_buoy_grn_wht_large_pixbuf,    NULL },
	{(char *) "Navaid, Orange",                  (char *) "buoy_orng",           28,  (char *) "orange map buoy symbol",            &wp_buoy_orng_pixbuf,       &wp_buoy_orng_large_pixbuf,       NULL },
	{(char *) "Navaid, Red",                     (char *) "buoy_red",            29,  (char *) "red map buoy symbol",               &wp_buoy_red_pixbuf,        &wp_buoy_red_large_pixbuf,        NULL },
	{(char *) "Navaid, Red/Green",               (char *) "buoy_red_grn",        30,  (char *) "red/green map buoy symbol",         &wp_buoy_red_grn_pixbuf,    &wp_buoy_red_grn_large_pixbuf,    NULL },
	{(char *) "Navaid, Red/White",               (char *) "buoy_red_wht",        31,  (char *) "red/white map buoy symbol",         &wp_buoy_red_wht_pixbuf,    &wp_buoy_red_wht_large_pixbuf,    NULL },
	{(char *) "Navaid, Violet",                  (char *) "buoy_violet",         32,  (char *) "violet map buoy symbol",            &wp_buoy_violet_pixbuf,     &wp_buoy_violet_large_pixbuf,     NULL },
	{(char *) "Navaid, White",                   (char *) "buoy_wht",            33,  (char *) "white map buoy symbol",             &wp_buoy_wht_pixbuf,        &wp_buoy_wht_large_pixbuf,        NULL },
	{(char *) "Navaid, White/Green",             (char *) "buoy_wht_grn",        34,  (char *) "white/green map buoy symbol",       &wp_buoy_wht_grn_pixbuf,    &wp_buoy_wht_grn_large_pixbuf,    NULL },
	{(char *) "Navaid, White/Red",               (char *) "buoy_wht_red",        35,  (char *) "white/red map buoy symbol",         &wp_buoy_wht_red_pixbuf,    &wp_buoy_wht_red_large_pixbuf,    NULL },
	{(char *) "White Dot",                       (char *) "dot",                 36,  (char *) "white dot symbol",                  &wp_dot_pixbuf,             NULL,            NULL },
	{(char *) "Radio Beacon",                    (char *) "rbcn",                37,  (char *) "radio beacon symbol",               &wp_rbcn_pixbuf,            &wp_rbcn_large_pixbuf,            NULL },
	{(char *) "Boat Ramp",                       (char *) "boat_ramp",          150,  (char *) "boat ramp symbol",                  &wp_boat_ramp_pixbuf,       &wp_boat_ramp_large_pixbuf,       NULL },
	{(char *) "Campground",                      (char *) "camp",               151,  (char *) "campground symbol",                 &wp_camp_pixbuf,            &wp_camp_large_pixbuf,            NULL },
	{(char *) "Restroom",                        (char *) "restrooms",          152,  (char *) "restrooms symbol",                  &wp_restroom_pixbuf,        &wp_restroom_large_pixbuf,        NULL },
	{(char *) "Shower",                          (char *) "showers",            153,  (char *) "shower symbol",                     &wp_shower_pixbuf,          &wp_shower_large_pixbuf,          NULL },
	{(char *) "Drinking Water",                  (char *) "drinking_wtr",       154,  (char *) "drinking water symbol",             &wp_drinking_wtr_pixbuf,    &wp_drinking_wtr_large_pixbuf,    NULL },
	{(char *) "Telephone",                       (char *) "phone",              155,  (char *) "telephone symbol",                  &wp_phone_pixbuf,           &wp_phone_large_pixbuf,           NULL },
	{(char *) "Medical Facility",                (char *) "1st_aid",            156,  (char *) "first aid symbol",                  &wp_1st_aid_pixbuf,         &wp_1st_aid_large_pixbuf,         NULL },
	{(char *) "Information",                     (char *) "info",               157,  (char *) "information symbol",                &wp_info_pixbuf,            &wp_info_large_pixbuf,            NULL },
	{(char *) "Parking Area",                    (char *) "parking",            158,  (char *) "parking symbol",                    &wp_parking_pixbuf,         &wp_parking_large_pixbuf,         NULL },
	{(char *) "Park",                            (char *) "park",               159,  (char *) "park symbol",                       &wp_park_pixbuf,            &wp_park_large_pixbuf,            NULL },
	{(char *) "Picnic Area",                     (char *) "picnic",             160,  (char *) "picnic symbol",                     &wp_picnic_pixbuf,          &wp_picnic_large_pixbuf,          NULL },
	{(char *) "Scenic Area",                     (char *) "scenic",             161,  (char *) "scenic area symbol",                &wp_scenic_pixbuf,          &wp_scenic_large_pixbuf,          NULL },
	{(char *) "Skiing Area",                     (char *) "skiing",             162,  (char *) "skiing symbol",                     &wp_skiing_pixbuf,          &wp_skiing_large_pixbuf,          NULL },
	{(char *) "Swimming Area",                   (char *) "swimming",           163,  (char *) "swimming symbol",                   &wp_swimming_pixbuf,        &wp_swimming_large_pixbuf,        NULL },
	{(char *) "Dam",                             (char *) "dam",                164,  (char *) "dam symbol",                        &wp_dam_pixbuf,             &wp_dam_large_pixbuf,             NULL },
	{(char *) "Controlled Area",                 (char *) "controlled",         165,  (char *) "controlled area symbol",            &wp_controlled_pixbuf,      &wp_controlled_large_pixbuf,      NULL },
	{(char *) "Danger Area",                     (char *) "danger",             166,  (char *) "danger symbol",                     &wp_danger_pixbuf,          &wp_danger_large_pixbuf,          NULL },
	{(char *) "Restricted Area",                 (char *) "restricted",         167,  (char *) "restricted area symbol",            &wp_restricted_pixbuf,      &wp_restricted_large_pixbuf,      NULL },
	{(char *) "Null 2",                          (char *) "null_2",             168,  (char *) "null symbol",                       NULL,                       NULL,                             NULL },  /* not exist */
	{(char *) "Ball Park",                       (char *) "ball",               169,  (char *) "ball symbol",                       &wp_ball_pixbuf,            &wp_ball_large_pixbuf,            NULL },
	{(char *) "Car",                             (char *) "car",                170,  (char *) "car symbol",                        &wp_car_pixbuf,             &wp_car_large_pixbuf,             NULL },
	{(char *) "Hunting Area",                    (char *) "deer",               171,  (char *) "deer symbol",                       &wp_deer_pixbuf,            &wp_deer_large_pixbuf,            NULL },
	{(char *) "Shopping Center",                 (char *) "shopping",           172,  (char *) "shopping cart symbol",              NULL,                       &wp_shopping_large_pixbuf,        NULL },
	{(char *) "Lodging",                         (char *) "lodging",            173,  (char *) "lodging symbol",                    NULL,                       &wp_lodging_large_pixbuf,         NULL },
	{(char *) "Mine",                            (char *) "mine",               174,  (char *) "mine symbol",                       &wp_mine_pixbuf,            &wp_mine_large_pixbuf,            NULL },
	{(char *) "Trail Head",                      (char *) "trail_head",         175,  (char *) "trail head symbol",                 NULL,                       &wp_trail_head_large_pixbuf,      NULL },
	{(char *) "Truck Stop",                      (char *) "truck_stop",         176,  (char *) "truck stop symbol",                 NULL,                       &wp_truck_stop_large_pixbuf,      NULL },
	{(char *) "Exit",                            (char *) "user_exit",          177,  (char *) "user exit symbol",                  NULL,                       &wp_exit_large_pixbuf,            NULL },
	{(char *) "Flag",                            (char *) "flag",               178,  (char *) "flag symbol",                       &wp_flag_pixbuf,            NULL,                             NULL },
	{(char *) "Circle with X",                   (char *) "circle_x",           179,  (char *) "circle with x in the center",       NULL,                       NULL,                             NULL },
	{(char *) "Open 24 Hours",                   (char *) "open_24hr",          180,  (char *) "open 24 hours symbol",              NULL,                       NULL,                             NULL },
	{(char *) "Fishing Hot Spot Facility",       (char *) "fhs_facility",       181,  (char *) "U Fishing Hot SpotsTM Facility",    NULL,                       &wp_fhs_facility_large_pixbuf,    NULL },
	{(char *) "Bottom Conditions",               (char *) "bot_cond",           182,  (char *) "Bottom Conditions",                 NULL,                       NULL,                             NULL },
	{(char *) "Tide/Current PRediction Station", (char *) "tide_pred_stn",      183,  (char *) "Tide/Current Prediction Station",   NULL,                       NULL,                             NULL },
	{(char *) "Anchor Prohibited",               (char *) "anchor_prohib",      184,  (char *) "U anchor prohibited symbol",        NULL,                       NULL,                             NULL },
	{(char *) "Beacon",                          (char *) "beacon",             185,  (char *) "U beacon symbol",                   NULL,                       NULL,                             NULL },
	{(char *) "Coast Guard",                     (char *) "coast_guard",        186,  (char *) "U coast guard symbol",              NULL,                       NULL,                             NULL },
	{(char *) "Reef",                            (char *) "reef",               187,  (char *) "U reef symbol",                     NULL,                       NULL,                             NULL },
	{(char *) "Weed Bed",                        (char *) "weedbed",            188,  (char *) "U weedbed symbol",                  NULL,                       NULL,                             NULL },
	{(char *) "Dropoff",                         (char *) "dropoff",            189,  (char *) "U dropoff symbol",                  NULL,                       NULL,                             NULL },
	{(char *) "Dock",                            (char *) "dock",               190,  (char *) "U dock symbol",                     NULL,                       NULL,                             NULL },
	{(char *) "U Marina",                        (char *) "marina",             191,  (char *) "U marina symbol",                   NULL,                       NULL,                             NULL },
	{(char *) "Bait and Tackle",                 (char *) "bait_tackle",        192,  (char *) "U bait and tackle symbol",          NULL,                       NULL,                             NULL },
	{(char *) "Stump",                           (char *) "stump",              193,  (char *) "U stump symbol",                    NULL,                       NULL,                             NULL },
	{(char *) "Ground Transportation",           (char *) "grnd_trans",         229,  (char *) "ground transportation",             NULL,                       &wp_grnd_trans_large_pixbuf,      NULL },
	/*---------------------------------------------------------------
	  User customizable symbols
	  The values from begin_custom to end_custom inclusive are
	  reserved for the identification of user customizable symbols.
	  ---------------------------------------------------------------*/
	{(char *) "custom begin placeholder",        (char *) "begin_custom",      7680,  (char *) "first user customizable symbol",    NULL,                       NULL,                             NULL },
	{(char *) "custom end placeholder",          (char *) "end_custom",        8191,  (char *) "last user customizable symbol",     NULL,                       NULL,                             NULL },
	/*---------------------------------------------------------------
	  Land symbols
	  ---------------------------------------------------------------*/
	{(char *) "Interstate Highway",   (char *) "is_hwy",         8192, (char *) "interstate hwy symbol",             NULL,                NULL,            NULL },   /* TODO: check symbol name */
	{(char *) "US hwy",           (char *) "us_hwy",         8193, (char *) "us hwy symbol",                     NULL,                NULL,            NULL },
	{(char *) "State Hwy",        (char *) "st_hwy",         8194, (char *) "state hwy symbol",                  NULL,                NULL,            NULL },
	{(char *) "Mile Marker",          (char *) "mi_mrkr",        8195, (char *) "mile marker symbol",                NULL,                NULL,            NULL },
	{(char *) "TracBack Point",       (char *) "trcbck",         8196, (char *) "TracBack (feet) symbol",            NULL,                NULL,            NULL },
	{(char *) "Golf Course",          (char *) "golf",           8197, (char *) "golf symbol",                       &wp_golf_pixbuf,            &wp_golf_large_pixbuf,            NULL },
	{(char *) "City (Small)",         (char *) "sml_cty",        8198, (char *) "small city symbol",                 &wp_sml_cty_pixbuf,         &wp_sml_cty_large_pixbuf,            NULL },
	{(char *) "City (Medium)",        (char *) "med_cty",        8199, (char *) "medium city symbol",                &wp_med_cty_pixbuf,         &wp_med_cty_large_pixbuf,            NULL },
	{(char *) "City (Large)",         (char *) "lrg_cty",        8200, (char *) "large city symbol",                 &wp_lrg_cty_pixbuf,         &wp_lrg_cty_large_pixbuf,            NULL },
	{(char *) "Intl freeway hwy",              (char *) "freeway",        8201, (char *) "intl freeway hwy symbol",           NULL,                NULL,            NULL },
	{(char *) "Intl national hwy",     (char *) "ntl_hwy",        8202, (char *) "intl national hwy symbol",          NULL,                NULL,            NULL },
	{(char *) "City (Capitol)",         (char *) "cap_cty",        8203, (char *) "capitol city symbol (star)",        &wp_cap_cty_pixbuf,         NULL,            NULL },
	{(char *) "Amusement Park",       (char *) "amuse_pk",       8204, (char *) "amusement park symbol",             NULL,                &wp_amuse_pk_large_pixbuf,            NULL },
	{(char *) "Bowling",               (char *) "bowling",        8205, (char *) "bowling symbol",                    NULL,                &wp_bowling_large_pixbuf,            NULL },
	{(char *) "Car Rental",           (char *) "car_rental",     8206, (char *) "car rental symbol",                 NULL,                &wp_car_rental_large_pixbuf,            NULL },
	{(char *) "Car Repair",           (char *) "car_repair",     8207, (char *) "car repair symbol",                 NULL,                &wp_car_repair_large_pixbuf,            NULL },
	{(char *) "Fast Food",            (char *) "fastfood",       8208, (char *) "fast food symbol",                  NULL,                &wp_fastfood_large_pixbuf,            NULL },
	{(char *) "Fitness Center",       (char *) "fitness",        8209, (char *) "fitness symbol",                    NULL,                &wp_fitness_large_pixbuf,            NULL },
	{(char *) "Movie Theater",        (char *) "movie",          8210, (char *) "movie symbol",                      NULL,                &wp_movie_large_pixbuf,            NULL },
	{(char *) "Museum",               (char *) "museum",         8211, (char *) "museum symbol",                     NULL,                &wp_museum_large_pixbuf,            NULL },
	{(char *) "Pharmacy",             (char *) "pharmacy",       8212, (char *) "pharmacy symbol",                   NULL,                &wp_pharmacy_large_pixbuf,            NULL },
	{(char *) "Pizza",                (char *) "pizza",          8213, (char *) "pizza symbol",                      NULL,                &wp_pizza_large_pixbuf,            NULL },
	{(char *) "Post Office",          (char *) "post_ofc",       8214, (char *) "post office symbol",                NULL,                &wp_post_ofc_large_pixbuf,            NULL },
	{(char *) "RV Park",              (char *) "rv_park",        8215, (char *) "RV park symbol",                    &wp_rv_park_pixbuf,  &wp_rv_park_large_pixbuf,            NULL },
	{(char *) "School",               (char *) "school",         8216, (char *) "school symbol",                     &wp_school_pixbuf,   &wp_school_large_pixbuf,            NULL },
	{(char *) "Stadium",              (char *) "stadium",        8217, (char *) "stadium symbol",                    NULL,                &wp_stadium_large_pixbuf,            NULL },
	{(char *) "Department Store",     (char *) "store",          8218, (char *) "dept. store symbol",                NULL,                &wp_store_large_pixbuf,            NULL },
	{(char *) "Zoo",                  (char *) "zoo",            8219, (char *) "zoo symbol",                        NULL,                &wp_zoo_large_pixbuf,            NULL },
	{(char *) "Convenience Store",    (char *) "conv_store",       8220, (char *) "convenience store symbol",          NULL,                &wp_conv_store_large_pixbuf,        NULL },
	{(char *) "Live Theater",         (char *) "theater",          8221, (char *) "live theater symbol",               NULL,                &wp_theater_large_pixbuf,            NULL },
	{(char *) "Ramp intersection",    (char *) "ramp_int",       8222, (char *) "ramp intersection symbol",          NULL,                NULL,            NULL },
	{(char *) "Street Intersection",  (char *) "st_int",         8223, (char *) "street intersection symbol",        NULL,                NULL,            NULL },
	{(char *) "Scales",               (char *) "weigh_station",     8226, (char *) "inspection/weigh station symbol",   NULL,               &wp_weigh_station_large_pixbuf,       NULL },
	{(char *) "Toll Booth",           (char *) "toll_booth",     8227, (char *) "toll booth symbol",                 NULL,                &wp_toll_booth_large_pixbuf,            NULL },
	{(char *) "Elevation point",      (char *) "elev_pt",        8228, (char *) "elevation point symbol",            NULL,                NULL,            NULL },
	{(char *) "Exit without services",(char *) "ex_no_srvc",     8229, (char *) "exit without services symbol",      NULL,                NULL,            NULL },
	{(char *) "Geographic place name, Man-made",(char *) "geo_place_mm",   8230, (char *) "Geographic place name, man-made",   NULL,                NULL,            NULL },
	{(char *) "Geographic place name, water", (char *) "geo_place_wtr",  8231, (char *) "Geographic place name, water",      NULL,                NULL,            NULL },
	{(char *) "Geographic place name, Land",(char *) "geo_place_lnd",  8232, (char *) "Geographic place name, land",       NULL,                NULL,            NULL },
	{(char *) "Bridge",               (char *) "bridge",         8233, (char *) "bridge symbol",                     &wp_bridge_pixbuf,          &wp_bridge_large_pixbuf,            NULL },
	{(char *) "Building",             (char *) "building",       8234, (char *) "building symbol",                   &wp_building_pixbuf,        &wp_building_large_pixbuf,        NULL },
	{(char *) "Cemetery",             (char *) "cemetery",       8235, (char *) "cemetery symbol",                   &wp_cemetery_pixbuf,        &wp_cemetery_large_pixbuf,            NULL },
	{(char *) "Church",               (char *) "church",         8236, (char *) "church symbol",                     &wp_church_pixbuf,          &wp_church_large_pixbuf,          NULL },
	{(char *) "Civil",                (char *) "civil",          8237, (char *) "civil location symbol",             NULL,                &wp_civil_large_pixbuf,            NULL },
	{(char *) "Crossing",             (char *) "crossing",       8238, (char *) "crossing symbol",                   NULL,                &wp_crossing_large_pixbuf,            NULL },
	{(char *) "Ghost Town",           (char *) "hist_town",      8239, (char *) "historical town symbol",            NULL,                NULL,            NULL },
	{(char *) "Levee",                (char *) "levee",          8240, (char *) "levee symbol",                      NULL,                NULL,            NULL },
	{(char *) "Military",             (char *) "military",       8241, (char *) "military location symbol",          &wp_military_pixbuf,        NULL,            NULL },
	{(char *) "Oil Field",            (char *) "oil_field",      8242, (char *) "oil field symbol",                  NULL,                &wp_oil_field_large_pixbuf,          NULL },
	{(char *) "Tunnel",               (char *) "tunnel",         8243, (char *) "tunnel symbol",                     &wp_tunnel_pixbuf,          &wp_tunnel_large_pixbuf,          NULL },
	{(char *) "Beach",                (char *) "beach",          8244, (char *) "beach symbol",                      &wp_beach_pixbuf,           &wp_beach_large_pixbuf,           NULL },
	{(char *) "Forest",               (char *) "forest",         8245, (char *) "forest symbol",                     &wp_forest_pixbuf,          &wp_forest_large_pixbuf,          NULL },
	{(char *) "Summit",               (char *) "summit",         8246, (char *) "summit symbol",                     &wp_summit_pixbuf,          &wp_summit_large_pixbuf,          NULL },
	{(char *) "Large Ramp intersection",(char *) "lrg_ramp_int", 8247, (char *) "large ramp intersection symbol",    NULL,                NULL,            NULL },
	{(char *) "Large exit without services",(char *) "lrg_ex_no_srvc", 8248, (char *) "large exit without services smbl",  NULL,                NULL,            NULL },
	{(char *) "Police Station",       (char *) "police",         8249, (char *) "police/official badge symbol",      NULL,                &wp_police_large_pixbuf,            NULL },
	{(char *) "Gambling/casino",      (char *) "cards",          8250, (char *) "gambling/casino symbol",            NULL,                NULL,            NULL },
	{(char *) "Ski Resort",           (char *) "ski_resort",     8251, (char *) "snow skiing symbol",                NULL,                &wp_ski_resort_large_pixbuf,          NULL },
	{(char *) "Ice Skating",          (char *) "ice_skating",    8252, (char *) "ice skating symbol",                &wp_ice_skating_pixbuf,     &wp_ice_skating_large_pixbuf,  NULL },
	{(char *) "Wrecker",              (char *) "wrecker",        8253, (char *) "tow truck (wrecker) symbol",        NULL,                &wp_wrecker_large_pixbuf,            NULL },
	{(char *) "Border Crossing (Port Of Entry)",(char *) "border",         8254, (char *) "border crossing (port of entry)",   NULL,                NULL,            NULL },
	{(char *) "Geocache",             (char *) "geocache",       8255, (char *) "geocache location",                 &wp_geocache_pixbuf,        &wp_geocache_large_pixbuf,        NULL },
	{(char *) "Geocache Found",       (char *) "geocache_fnd",   8256, (char *) "found geocache",                    &wp_geocache_fnd_pixbuf,    &wp_geocache_fnd_large_pixbuf,    NULL },
	{(char *) "Contact, Smiley",      (char *) "cntct_smiley",   8257, (char *) "Rino contact symbol,(char *) ""smiley""",   NULL,                NULL,            NULL },
	{(char *) "Contact, Ball Cap",    (char *) "cntct_ball_cap", 8258, (char *) "Rino contact symbol,(char *) ""ball cap""", NULL,                NULL,            NULL },
	{(char *) "Contact, Big Ears",     (char *) "cntct_big_ears", 8259, (char *) "Rino contact symbol,(char *) ""big ear""",  NULL,                NULL,            NULL },
	{(char *) "Contact, Spike",        (char *) "cntct_spike",    8260, (char *) "Rino contact symbol,(char *) ""spike""",    NULL,                NULL,            NULL },
	{(char *) "Contact, Goatee",       (char *) "cntct_goatee",   8261, (char *) "Rino contact symbol,(char *) ""goatee""",   NULL,                NULL,            NULL },
	{(char *) "Contact, Afro",         (char *) "cntct_afro",     8262, (char *) "Rino contact symbol,(char *) ""afro""",     NULL,                NULL,            NULL },
	{(char *) "Contact, Dreadlocks",   (char *) "cntct_dreads",   8263, (char *) "Rino contact symbol,(char *) ""dreads""",   NULL,                NULL,            NULL },
	{(char *) "Contact, Female1",      (char *) "cntct_female1",  8264, (char *) "Rino contact symbol,(char *) ""female 1""", NULL,                NULL,            NULL },
	{(char *) "Contact, Female2",      (char *) "cntct_female2",  8265, (char *) "Rino contact symbol,(char *) ""female 2""", NULL,                NULL,            NULL },
	{(char *) "Contact, Female3",      (char *) "cntct_female3",  8266, (char *) "Rino contact symbol,(char *) ""female 3""", NULL,                NULL,            NULL },
	{(char *) "Contact, Ranger",       (char *) "cntct_ranger",   8267, (char *) "Rino contact symbol,(char *) ""ranger""",   NULL,                NULL,            NULL },
	{(char *) "Contact, Kung-Fu",      (char *) "cntct_kung_fu",  8268, (char *) "Rino contact symbol,(char *) ""kung fu""",  NULL,                NULL,            NULL },
	{(char *) "Contact, Sumo",         (char *) "cntct_sumo",     8269, (char *) "Rino contact symbol,(char *) ""sumo""",     NULL,                NULL,            NULL },
	{(char *) "Contact, Pirate",       (char *) "cntct_pirate",   8270, (char *) "Rino contact symbol,(char *) ""pirate""",   NULL,                NULL,            NULL },
	{(char *) "Contact, Biker",        (char *) "cntct_biker",    8271, (char *) "Rino contact symbol,(char *) ""biker""",    NULL,                NULL,            NULL },
	{(char *) "Contact, Alien",        (char *) "cntct_alien",    8272, (char *) "Rino contact symbol,(char *) ""alien""",    NULL,                NULL,            NULL },
	{(char *) "Contact, Bug",          (char *) "cntct_bug",      8273, (char *) "Rino contact symbol,(char *) ""bug""",      NULL,                NULL,            NULL },
	{(char *) "Contact, Cat",          (char *) "cntct_cat",      8274, (char *) "Rino contact symbol,(char *) ""cat""",      NULL,                NULL,            NULL },
	{(char *) "Contact, Dog",          (char *) "cntct_dog",      8275, (char *) "Rino contact symbol,(char *) ""dog""",      NULL,                NULL,            NULL },
	{(char *) "Contact, Pig",          (char *) "cntct_pig",      8276, (char *) "Rino contact symbol,(char *) ""pig""",      NULL,                NULL,            NULL },
	{(char *) "Water Hydrant",         (char *) "hydrant",        8282, (char *) "water hydrant symbol",              NULL,                NULL,            NULL },
	{(char *) "Flag, Blue",            (char *) "flag_blue",      8284, (char *) "blue flag symbol",                  NULL,                &wp_flag_blue_large_pixbuf,            NULL },
	{(char *) "Flag, Green",           (char *) "flag_green",     8285, (char *) "green flag symbol",                 NULL,                &wp_flag_green_large_pixbuf,            NULL },
	{(char *) "Flag, Red",             (char *) "flag_red",       8286, (char *) "red flag symbol",                   NULL,                &wp_flag_red_large_pixbuf,            NULL },
	{(char *) "Pin, Blue",             (char *) "pin_blue",       8287, (char *) "blue pin symbol",                   NULL,                &wp_pin_blue_large_pixbuf,            NULL },
	{(char *) "Pin, Green",            (char *) "pin_green",      8288, (char *) "green pin symbol",                  NULL,                &wp_pin_green_large_pixbuf,            NULL },
	{(char *) "Pin, Red",              (char *) "pin_red",        8289, (char *) "red pin symbol",                    NULL,                &wp_pin_red_large_pixbuf,            NULL },
	{(char *) "Block, Blue",           (char *) "block_blue",     8290, (char *) "blue block symbol",                 NULL,                &wp_block_blue_large_pixbuf,            NULL },
	{(char *) "Block, Green",          (char *) "block_green",    8291, (char *) "green block symbol",                NULL,                &wp_block_green_large_pixbuf,           NULL },
	{(char *) "Block, Red",            (char *) "block_red",      8292, (char *) "red block symbol",                  NULL,                &wp_block_red_large_pixbuf,            NULL },
	{(char *) "Bike Trail",            (char *) "bike_trail",     8293, (char *) "bike trail symbol",                 NULL,                &wp_bike_trail_large_pixbuf,            NULL },
	{(char *) "Circle, Red",           (char *) "circle_red",     8294, (char *) "red circle symbol",                 NULL,                NULL,            NULL },
	{(char *) "Circle, Green",         (char *) "circle_green",   8295, (char *) "green circle symbol",               NULL,                NULL,            NULL },
	{(char *) "Circle, Blue",          (char *) "circle_blue",    8296, (char *) "blue circle symbol",                NULL,                NULL,            NULL },
	{(char *) "Diamond, Blue",         (char *) "diamond_blue",   8299, (char *) "blue diamond symbol",               NULL,                NULL,            NULL },
	{(char *) "Oval, Red",             (char *) "oval_red",       8300, (char *) "red oval symbol",                   NULL,                NULL,            NULL },
	{(char *) "Oval, Green",           (char *) "oval_green",     8301, (char *) "green oval symbol",                 NULL,                NULL,            NULL },
	{(char *) "Oval, Blue",            (char *) "oval_blue",      8302, (char *) "blue oval symbol",                  NULL,                NULL,            NULL },
	{(char *) "Rectangle, Red",        (char *) "rect_red",       8303, (char *) "red rectangle symbol",              NULL,                NULL,            NULL },
	{(char *) "Rectangle, Green",      (char *) "rect_green",     8304, (char *) "green rectangle symbol",            NULL,                NULL,            NULL },
	{(char *) "Rectangle, Blue",       (char *) "rect_blue",      8305, (char *) "blue rectangle symbol",             NULL,                NULL,            NULL },
	{(char *) "Square, Blue",          (char *) "square_blue",    8308, (char *) "blue square symbol",                NULL,                NULL,            NULL },
	{(char *) "Letter A, Red",         (char *) "letter_a_red",   8309, (char *) "red letter 'A' symbol",             NULL,                NULL,            NULL },
	{(char *) "Letter B, Red",         (char *) "letter_b_red",   8310, (char *) "red letter 'B' symbol",             NULL,                NULL,            NULL },
	{(char *) "Letter C, Red",         (char *) "letter_c_red",   8311, (char *) "red letter 'C' symbol",             NULL,                NULL,            NULL },
	{(char *) "Letter D, Red",         (char *) "letter_d_red",   8312, (char *) "red letter 'D' symbol",             NULL,                NULL,            NULL },
	{(char *) "Letter A, Green",       (char *) "letter_a_green", 8313, (char *) "green letter 'A' symbol",           NULL,                NULL,            NULL },
	{(char *) "Letter C, Green",       (char *) "letter_c_green", 8314, (char *) "green letter 'C' symbol",           NULL,                NULL,            NULL },
	{(char *) "Letter B, Green",       (char *) "letter_b_green", 8315, (char *) "green letter 'B' symbol",           NULL,                NULL,            NULL },
	{(char *) "Letter D, Green",       (char *) "letter_d_green", 8316, (char *) "green letter 'D' symbol",           NULL,                NULL,            NULL },
	{(char *) "Letter A, Blue",        (char *) "letter_a_blue",  8317, (char *) "blue letter 'A' symbol",            NULL,                NULL,            NULL },
	{(char *) "Letter B, Blue",        (char *) "letter_b_blue",  8318, (char *) "blue letter 'B' symbol",            NULL,                NULL,            NULL },
	{(char *) "Letter C, Blue",        (char *) "letter_c_blue",  8319, (char *) "blue letter 'C' symbol",            NULL,                NULL,            NULL },
	{(char *) "Letter D, Blue",        (char *) "letter_d_blue",  8320, (char *) "blue letter 'D' symbol",            NULL,                NULL,            NULL },
	{(char *) "Number 0, Red",         (char *) "number_0_red",   8321, (char *) "red number '0' symbol",             NULL,                NULL,            NULL },
	{(char *) "Number 1, Red",         (char *) "number_1_red",   8322, (char *) "red number '1' symbol",             NULL,                NULL,            NULL },
	{(char *) "Number 2, Red",         (char *) "number_2_red",   8323, (char *) "red number '2' symbol",             NULL,                NULL,            NULL },
	{(char *) "Number 3, Red",         (char *) "number_3_red",   8324, (char *) "red number '3' symbol",             NULL,                NULL,            NULL },
	{(char *) "Number 4, Red",         (char *) "number_4_red",   8325, (char *) "red number '4' symbol",             NULL,                NULL,            NULL },
	{(char *) "Number 5, Red",         (char *) "number_5_red",   8326, (char *) "red number '5' symbol",             NULL,                NULL,            NULL },
	{(char *) "Number 6, Red",         (char *) "number_6_red",   8327, (char *) "red number '6' symbol",             NULL,                NULL,            NULL },
	{(char *) "Number 7, Red",         (char *) "number_7_red",   8328, (char *) "red number '7' symbol",             NULL,                NULL,            NULL },
	{(char *) "Number 8, Red",         (char *) "number_8_red",   8329, (char *) "red number '8' symbol",             NULL,                NULL,            NULL },
	{(char *) "Number 9, Red",         (char *) "number_9_red",   8330, (char *) "red number '9' symbol",             NULL,                NULL,            NULL },
	{(char *) "Number 0, Green",       (char *) "number_0_green", 8331, (char *) "green number '0' symbol",           NULL,                NULL,            NULL },
	{(char *) "Number 1, Green",       (char *) "number_1_green", 8332, (char *) "green number '1' symbol",           NULL,                NULL,            NULL },
	{(char *) "Number 2, Green",       (char *) "number_2_green", 8333, (char *) "green number '2' symbol",           NULL,                NULL,            NULL },
	{(char *) "Number 3, Green",       (char *) "number_3_green", 8334, (char *) "green number '3' symbol",           NULL,                NULL,            NULL },
	{(char *) "Number 4, Green",       (char *) "number_4_green", 8335, (char *) "green number '4' symbol",           NULL,                NULL,            NULL },
	{(char *) "Number 5, Green",       (char *) "number_5_green", 8336, (char *) "green number '5' symbol",           NULL,                NULL,            NULL },
	{(char *) "Number 6, Green",       (char *) "number_6_green", 8337, (char *) "green number '6' symbol",           NULL,                NULL,            NULL },
	{(char *) "Number 7, Green",       (char *) "number_7_green", 8338, (char *) "green number '7' symbol",           NULL,                NULL,            NULL },
	{(char *) "Number 8, Green",       (char *) "number_8_green", 8339, (char *) "green number '8' symbol",           NULL,                NULL,            NULL },
	{(char *) "Number 9, Green",       (char *) "number_9_green", 8340, (char *) "green number '9' symbol",           NULL,                NULL,            NULL },
	{(char *) "Number 0, Blue",        (char *) "number_0_blue",  8341, (char *) "blue number '0' symbol",            NULL,                NULL,            NULL },
	{(char *) "Number 1, Blue",        (char *) "number_1_blue",  8342, (char *) "blue number '1' symbol",            NULL,                NULL,            NULL },
	{(char *) "Number 2, Blue",        (char *) "number_2_blue",  8343, (char *) "blue number '2' symbol",            NULL,                NULL,            NULL },
	{(char *) "Number 3, Blue",        (char *) "number_3_blue",  8344, (char *) "blue number '3' symbol",            NULL,                NULL,            NULL },
	{(char *) "Number 4, Blue",        (char *) "number_4_blue",  8345, (char *) "blue number '4' symbol",            NULL,                NULL,            NULL },
	{(char *) "Number 5, Blue",        (char *) "number_5_blue",  8346, (char *) "blue number '5' symbol",            NULL,                NULL,            NULL },
	{(char *) "Number 6, Blue",        (char *) "number_6_blue",  8347, (char *) "blue number '6' symbol",            NULL,                NULL,            NULL },
	{(char *) "Number 7, Blue",        (char *) "number_7_blue",  8348, (char *) "blue number '7' symbol",            NULL,                NULL,            NULL },
	{(char *) "Number 8, Blue",        (char *) "number_8_blue",  8349, (char *) "blue number '8' symbol",            NULL,                NULL,            NULL },
	{(char *) "Number 9, Blue",        (char *) "number_9_blue",  8350, (char *) "blue number '9' symbol",            NULL,                NULL,            NULL },
	{(char *) "Triangle, Blue",        (char *) "triangle_blue",  8351, (char *) "blue triangle symbol",              NULL,                NULL,            NULL },
	{(char *) "Triangle, Green",       (char *) "triangle_green", 8352, (char *) "green triangle symbol",             NULL,                NULL,            NULL },
	{(char *) "Triangle, Red",         (char *) "triangle_red",   8353, (char *) "red triangle symbol",               NULL,                NULL,            NULL },
	/*---------------------------------------------------------------
	  Aviation symbols
	  ---------------------------------------------------------------*/
	{(char *) "Airport",               (char *) "airport",        16384,(char *) "airport symbol",                    &wp_airplane_pixbuf,        &wp_airplane_large_pixbuf,        NULL },
	{(char *) "Intersection",          (char *) "int",            16385,(char *) "intersection symbol",               NULL,                NULL,            NULL },
	{(char *) "Non-directional beacon",(char *) "ndb",            16386,(char *) "non-directional beacon symbol",     NULL,                NULL,            NULL },
	{(char *) "VHF Omni-range",        (char *) "vor",            16387,(char *) "VHF omni-range symbol",             NULL,                NULL,            NULL },
	{(char *) "Heliport",              (char *) "heliport",       16388,(char *) "heliport symbol",                   NULL,                       &wp_helipad_large_pixbuf,         NULL },
	{(char *) "Private Field",         (char *) "private",        16389,(char *) "private field symbol",              NULL,                NULL,            NULL },
	{(char *) "Soft Field",            (char *) "soft_fld",       16390,(char *) "soft field symbol",                 NULL,                NULL,            NULL },
	{(char *) "Tall Tower",            (char *) "tall_tower",     16391,(char *) "tall tower symbol",                 NULL,                &wp_tall_tower_large_pixbuf,           NULL },
	{(char *) "Short Tower",           (char *) "short_tower",    16392,(char *) "short tower symbol",                NULL,                &wp_short_tower_large_pixbuf,          NULL },
	{(char *) "Glider Area",           (char *) "glider",         16393,(char *) "glider symbol",                     NULL,                &wp_glider_large_pixbuf,            NULL },
	{(char *) "Ultralight Area",       (char *) "ultralight",     16394,(char *) "ultralight symbol",                 NULL,                &wp_ultralight_large_pixbuf,            NULL },
	{(char *) "Parachute Area",        (char *) "parachute",      16395,(char *) "parachute symbol",                  NULL,                &wp_parachute_large_pixbuf,            NULL },
	{(char *) "VOR/TACAN",             (char *) "vortac",         16396,(char *) "VOR/TACAN symbol",                  NULL,                NULL,            NULL },
	{(char *) "VOR-DME",               (char *) "vordme",         16397,(char *) "VOR-DME symbol",                    NULL,                NULL,            NULL },
	{(char *) "First approach fix",    (char *) "faf",            16398,(char *) "first approach fix",                NULL,                NULL,            NULL },
	{(char *) "Localizer Outer Marker",(char *) "lom",            16399,(char *) "localizer outer marker",            NULL,                NULL,            NULL },
	{(char *) "Missed Approach Point", (char *) "map",            16400,(char *) "missed approach point",             NULL,                NULL,            NULL },
	{(char *) "TACAN",                 (char *) "tacan",          16401,(char *) "TACAN symbol",                      NULL,                NULL,            NULL },
	{(char *) "Seaplane Base",         (char *) "seaplane",       16402,(char *) "Seaplane Base",                     NULL,                NULL,            NULL }
};




static GHashTable *icons = NULL;
static GHashTable *old_icons = NULL;




static int str_equal_casefold(gconstpointer v1, gconstpointer v2)
{
	bool equal;
	char *v1_lower;
	char *v2_lower;

	v1_lower = g_utf8_casefold((const char *) v1, -1);
	if (!v1_lower) {
		return false;
	}
	v2_lower = g_utf8_casefold((const char *) v2, -1);
	if (!v2_lower) {
		free(v1_lower);
		return false;
	}

	equal = g_str_equal(v1_lower, v2_lower);

	free(v1_lower);
	free(v2_lower);

	return equal;
}




static unsigned int str_hash_casefold(gconstpointer key)
{
	unsigned int h;
	char *key_lower;

	key_lower = g_utf8_casefold((const char *) key, -1);
	if (!key_lower) {
		return 0;
	}

	h = g_str_hash(key_lower);

	free(key_lower);

	return h;
}




static void init_icons()
{
	icons = g_hash_table_new_full(str_hash_casefold, str_equal_casefold, NULL, NULL);
	old_icons = g_hash_table_new_full(str_hash_casefold, str_equal_casefold, NULL, NULL);
	int i;
	for (i = 0; i < G_N_ELEMENTS(garmin_syms); i++) {
		g_hash_table_insert(icons, garmin_syms[i].sym, KINT_TO_POINTER (i));
		g_hash_table_insert(old_icons, garmin_syms[i].old_sym, KINT_TO_POINTER (i));
	}
}




static GdkPixbuf * get_wp_sym_from_index(int i)
{
	// Ensure data exists to either directly load icon or scale from the other set
	if (!garmin_syms[i].icon && (garmin_syms[i].data || garmin_syms[i].data_large)) {
		if (a_vik_get_use_large_waypoint_icons()) {
			if (garmin_syms[i].data_large) {
				// Directly load icon
				garmin_syms[i].icon = gdk_pixbuf_from_pixdata(garmin_syms[i].data_large, false, NULL);
			} else {
				// Up sample from small image
				garmin_syms[i].icon = gdk_pixbuf_scale_simple(gdk_pixbuf_from_pixdata(garmin_syms[i].data, false, NULL), 30, 30, GDK_INTERP_BILINEAR);
			}
		} else {
			if (garmin_syms[i].data) {
				// Directly use small symbol
				garmin_syms[i].icon = gdk_pixbuf_from_pixdata(garmin_syms[i].data, false, NULL);
			} else {
				// Down size large image
				garmin_syms[i].icon = gdk_pixbuf_scale_simple(gdk_pixbuf_from_pixdata(garmin_syms[i].data_large, false, NULL), 18, 18, GDK_INTERP_BILINEAR);
			}
		}
	}
	return garmin_syms[i].icon;
}




GdkPixbuf *a_get_wp_sym(const char *sym)
{
	void * gp;
	void * x;

	if (!sym) {
		return NULL;
	}
	if (!icons) {
		init_icons();
	}
	if (g_hash_table_lookup_extended(icons, sym, &x, &gp)) {
		return get_wp_sym_from_index(KPOINTER_TO_INT(gp));
	} else if (g_hash_table_lookup_extended(old_icons, sym, &x, &gp)) {
		return get_wp_sym_from_index(KPOINTER_TO_INT(gp));
	} else {
		return NULL;
	}
}




const char *a_get_hashed_sym(const char *sym)
{
	void * gp;
	void * x;

	if (!sym) {
		return NULL;
	}
	if (!icons) {
		init_icons();
	}
	if (g_hash_table_lookup_extended(icons, sym, &x, &gp)) {
		return garmin_syms[KPOINTER_TO_INT(gp)].sym;
	} else if (g_hash_table_lookup_extended(old_icons, sym, &x, &gp)) {
		return garmin_syms[KPOINTER_TO_INT(gp)].sym;
	} else {
		return NULL;
	}
}




void a_populate_sym_list(GtkListStore *list) {
	int i;
	for (i = 0; i < G_N_ELEMENTS (garmin_syms); i++) {
		// Ensure at least one symbol available - the other can be auto generated
		if (garmin_syms[i].data || garmin_syms[i].data_large) {
			GtkTreeIter iter;
			gtk_list_store_append(list, &iter);
			gtk_list_store_set(list, &iter, 0, garmin_syms[i].sym, 1, get_wp_sym_from_index(i), -1);
		}
	}
}





/* Use when preferences have changed to reset icons*/
void clear_garmin_icon_syms()
{
	fprintf(stderr, "DEBUG: garminsymbols: clear_garmin_icon_syms\n");
	int i;
	for (i = 0; i < G_N_ELEMENTS (garmin_syms); i++) {
		if (garmin_syms[i].icon) {
			g_object_unref(garmin_syms[i].icon);
			garmin_syms[i].icon = NULL;
		}
	}
}
