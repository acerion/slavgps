#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define DEFAULT_BACKGROUND_COLOR "#CCCCCC"
#define DEFAULT_HIGHLIGHT_COLOR "#EEA500"
/* Default highlight in orange */

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <cstdlib>
#include <cassert>

#ifndef SLAVGPS_QT
#include <glib-object.h>
#endif

#include "vikviewport.h"
#include "coords.h"
#ifndef SLAVGPS_QT
#include "window.h"
#endif
#include "mapcoord.h"

/* For ALTI_TO_MPP. */
#include "globals.h"
#include "settings.h"
#ifndef SLAVGPS_QT
#include "dialog.h"
#endif




/* Mock functions used during migration of SlavGPS to QT. */

using namespace SlavGPS;




#ifdef SLAVGPS_QT


DistanceUnit a_vik_get_units_distance(void)
{
	return (DistanceUnit) 0;
}

vik_startup_method_t a_vik_get_startup_method()
{
	return VIK_STARTUP_METHOD_LAST_LOCATION;
}

void a_settings_set_double(char const*, double)
{

}

bool a_settings_get_double(char const*, double*)
{
	return true;
}

bool a_settings_get_integer(char const*, int*)
{
	return true;
}

double a_vik_get_default_lat()
{
	return 55.0;
}

double a_vik_get_default_long()
{
	return 16.0;
}

vik_degree_format_t a_vik_get_degree_format()
{
	return VIK_DEGREE_FORMAT_DDD;
}

HeightUnit a_vik_get_units_height()
{
	return HeightUnit::METRES;
}

#endif
