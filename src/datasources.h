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
 */

#ifndef _SG_DATASOURCES_H_
#define _SG_DATASOURCES_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




#ifdef VIK_CONFIG_GEOCACHES
#include "datasource_gc.h"
#endif

#include "datasource_file.h"
#include "datasource_osm.h"
#include "datasource_osm_my_traces.h"
#include "datasource_wikipedia.h"
#include "datasource_url.h"
#include "datasource_gc.h"
#include "datasource_geotag.h"
#include "datasource_geojson.h"
#include "datasource_routing.h"
#include "datasource_gps.h"
#include "datasource_bfilter.h"




#endif /* #ifndef _SG_DATASOURCES_H_ */
