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

#include "viewport.h"
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



#endif
