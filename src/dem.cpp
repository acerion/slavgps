/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2008, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
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

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <glib.h>
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdlib.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "compression.h"
#include "dem.h"
#include "coords.h"
#include "fileutils.h"

/* Compatibility */
#if ! GLIB_CHECK_VERSION(2,22,0)
#define g_mapped_file_unref g_mapped_file_free
#endif

#define DEM_BLOCK_SIZE 1024
#define GET_COLUMN(dem,n) ((VikDEMColumn *)g_ptr_array_index( (dem)->columns, (n) ))

static bool get_double_and_continue ( char **buffer, double *tmp, bool warn )
{
  char *endptr;
  *tmp = g_strtod(*buffer, &endptr);
  if ( endptr == NULL|| endptr == *buffer ) {
    if ( warn )
      fprintf(stderr, _("WARNING: Invalid DEM\n"));
    return false;
  }
  *buffer=endptr;
  return true;
}


static bool get_int_and_continue ( char **buffer, int *tmp, bool warn )
{
  char *endptr;
  *tmp = strtol(*buffer, &endptr, 10);
  if ( endptr == NULL|| endptr == *buffer ) {
    if ( warn )
      fprintf(stderr, _("WARNING: Invalid DEM\n"));
    return false;
  }
  *buffer=endptr;
  return true;
}

static bool dem_parse_header ( char *buffer, VikDEM *dem )
{
  double val;
  int int_val;
  unsigned int i;
  char *tmp = buffer;

  /* incomplete header */
  if ( strlen(buffer) != 1024 )
    return false;

  /* fix Fortran-style exponentiation 1.0D5 -> 1.0E5 */
  while (*tmp) {
    if ( *tmp=='D')
      *tmp='E';
    tmp++;
  }

  /* skip name */
  buffer += 149;

  /* "DEM level code, pattern code, palaimetric reference system code" -- skip */
  get_int_and_continue(&buffer, &int_val, true);
  get_int_and_continue(&buffer, &int_val, true);
  get_int_and_continue(&buffer, &int_val, true);

  /* zone */
  get_int_and_continue(&buffer, &int_val, true);
  dem->utm_zone = int_val;
  /* TODO -- southern or northern hemisphere?! */
  dem->utm_letter = 'N';

  /* skip numbers 5-19  */
  for ( i = 0; i < 15; i++ ) {
    if ( ! get_double_and_continue(&buffer, &val, false) ) {
      fprintf(stderr, _("WARNING: Invalid DEM header\n"));
      return false;
    }
  }

  /* number 20 -- horizontal unit code (utm/ll) */
  get_double_and_continue(&buffer, &val, true);
  dem->horiz_units = val;
  get_double_and_continue(&buffer, &val, true);
  /* dem->orig_vert_units = val; now done below */

  /* TODO: do this for real. these are only for 1:24k and 1:250k USGS */
  if ( dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS ) {
    dem->east_scale = 10.0; /* meters */
    dem->north_scale = 10.0;
    dem->orig_vert_units = VIK_DEM_VERT_DECIMETERS;
  } else {
    dem->east_scale = 3.0; /* arcseconds */
    dem->north_scale = 3.0;
    dem->orig_vert_units = VIK_DEM_VERT_METERS;
  }

  /* skip next */
  get_double_and_continue(&buffer, &val, true);

  /* now we get the four corner points. record the min and max. */
  get_double_and_continue(&buffer, &val, true);
  dem->min_east = dem->max_east = val;
  get_double_and_continue(&buffer, &val, true);
  dem->min_north = dem->max_north = val;

  for ( i = 0; i < 3; i++ ) {
    get_double_and_continue(&buffer, &val, true);
    if ( val < dem->min_east ) dem->min_east = val;
    if ( val > dem->max_east ) dem->max_east = val;
    get_double_and_continue(&buffer, &val, true);
    if ( val < dem->min_north ) dem->min_north = val;
    if ( val > dem->max_north ) dem->max_north = val;
  }

  return true;
}

static void dem_parse_block_as_cont ( char *buffer, VikDEM *dem, int *cur_column, int *cur_row )
{
  int tmp;
  while ( *cur_row < GET_COLUMN(dem, *cur_column)->n_points ) {
    if ( get_int_and_continue(&buffer, &tmp,false) ) {
      if ( dem->orig_vert_units == VIK_DEM_VERT_DECIMETERS )
        GET_COLUMN(dem, *cur_column)->points[*cur_row] = (int16_t) (tmp / 10);
      else
        GET_COLUMN(dem, *cur_column)->points[*cur_row] = (int16_t) tmp;
    } else
      return;
    (*cur_row)++;
  }
  *cur_row = -1; /* expecting new column */
}

static void dem_parse_block_as_header ( char *buffer, VikDEM *dem, int *cur_column, int *cur_row )
{
  unsigned int n_rows;
  int i;
  double east_west, south;
  double tmp;

  /* 1 x n_rows 1 east_west south x x x DATA */

  if ( (!get_double_and_continue(&buffer, &tmp, true)) || tmp != 1 ) {
    fprintf(stderr, _("WARNING: Incorrect DEM Class B record: expected 1\n"));
    return;
  }

  /* don't need this */
  if ( !get_double_and_continue(&buffer, &tmp, true ) ) return;

  /* n_rows */
  if ( !get_double_and_continue(&buffer, &tmp, true ) )
    return;
  n_rows = (unsigned int) tmp;

  if ( (!get_double_and_continue(&buffer, &tmp, true)) || tmp != 1 ) {
    fprintf(stderr, _("WARNING: Incorrect DEM Class B record: expected 1\n"));
    return;
  }
  
  if ( !get_double_and_continue(&buffer, &east_west, true) )
    return;
  if ( !get_double_and_continue(&buffer, &south, true) )
    return;

  /* next three things we don't need */
  if ( !get_double_and_continue(&buffer, &tmp, true)) return;
  if ( !get_double_and_continue(&buffer, &tmp, true)) return;
  if ( !get_double_and_continue(&buffer, &tmp, true)) return;


  dem->n_columns ++;
  (*cur_column) ++;

  /* empty spaces for things before that were skipped */
  (*cur_row) = (south - dem->min_north) / dem->north_scale;
  if ( south > dem->max_north || (*cur_row) < 0 )
    (*cur_row) = 0;

  n_rows += *cur_row;

  g_ptr_array_add ( dem->columns, malloc(sizeof(VikDEMColumn)) );
  GET_COLUMN(dem,*cur_column)->east_west = east_west;
  GET_COLUMN(dem,*cur_column)->south = south;
  GET_COLUMN(dem,*cur_column)->n_points = n_rows;
  GET_COLUMN(dem,*cur_column)->points = (int16_t *) malloc(sizeof(int16_t)*n_rows);

  /* no information for things before that */
  for ( i = 0; i < (*cur_row); i++ )
    GET_COLUMN(dem,*cur_column)->points[i] = VIK_DEM_INVALID_ELEVATION;

  /* now just continue */
  dem_parse_block_as_cont ( buffer, dem, cur_column, cur_row );


}

static void dem_parse_block ( char *buffer, VikDEM *dem, int *cur_column, int *cur_row )
{
  /* if haven't read anything or have read all items in a columns and are expecting a new column */
  if ( *cur_column == -1 || *cur_row == -1 ) {
    dem_parse_block_as_header(buffer, dem, cur_column, cur_row);
  } else {
    dem_parse_block_as_cont(buffer, dem, cur_column, cur_row);
  }
}

static VikDEM *vik_dem_read_srtm_hgt(const char *file_name, const char *basename, bool zip)
{
  int i, j;
  VikDEM *dem;
  off_t file_size;
  int16_t *dem_mem = NULL;
  char *dem_file = NULL;
  const int num_rows_3sec = 1201;
  const int num_rows_1sec = 3601;
  int num_rows;
  GMappedFile *mf;
  int arcsec;
  GError *error = NULL;

  dem = (VikDEM *) malloc(sizeof(VikDEM));

  dem->horiz_units = VIK_DEM_HORIZ_LL_ARCSECONDS;
  dem->orig_vert_units = VIK_DEM_VERT_DECIMETERS;

  /* TODO */
  dem->min_north = atoi(basename+1) * 3600;
  dem->min_east = atoi(basename+4) * 3600;
  if ( basename[0] == 'S' )
    dem->min_north = - dem->min_north;
  if ( basename[3] == 'W' )
    dem->min_east = - dem->min_east;

  dem->max_north = 3600 + dem->min_north;
  dem->max_east = 3600 + dem->min_east;

  dem->columns = g_ptr_array_new();
  dem->n_columns = 0;

  if ((mf = g_mapped_file_new(file_name, false, &error)) == NULL) {
    fprintf(stderr, _("CRITICAL: Couldn't map file %s: %s\n"), file_name, error->message);
    g_error_free(error);
    free(dem);
    return NULL;
  }
  file_size = g_mapped_file_get_length(mf);
  dem_file = g_mapped_file_get_contents(mf);
  
  if (zip) {
    void *unzip_mem = NULL;
    unsigned long ucsize;

    if ((unzip_mem = unzip_file(dem_file, &ucsize)) == NULL) {
      g_mapped_file_unref(mf);
      g_ptr_array_foreach ( dem->columns, (GFunc)g_free, NULL );
      g_ptr_array_free(dem->columns, true);
      free(dem);
      return NULL;
    }

    dem_mem = (int16_t *) unzip_mem;
    file_size = ucsize;
  }
  else
    dem_mem = (int16_t *)dem_file;

  if (file_size == (num_rows_3sec * num_rows_3sec * sizeof(int16_t)))
    arcsec = 3;
  else if (file_size == (num_rows_1sec * num_rows_1sec * sizeof(int16_t)))
    arcsec = 1;
  else {
    fprintf(stderr, "WARNING: %s(): file %s does not have right size\n", __PRETTY_FUNCTION__, basename);
    g_mapped_file_unref(mf);
    free(dem);
    return NULL;
  }

  num_rows = (arcsec == 3) ? num_rows_3sec : num_rows_1sec;
  dem->east_scale = dem->north_scale = arcsec;

  for ( i = 0; i < num_rows; i++ ) {
    dem->n_columns++;
    g_ptr_array_add ( dem->columns, malloc(sizeof(VikDEMColumn)) );
    GET_COLUMN(dem,i)->east_west = dem->min_east + arcsec*i;
    GET_COLUMN(dem,i)->south = dem->min_north;
    GET_COLUMN(dem,i)->n_points = num_rows;
    GET_COLUMN(dem,i)->points = (int16_t *) malloc(sizeof(int16_t)*num_rows);
  }

  int ent = 0;
  for ( i = (num_rows - 1); i >= 0; i-- ) {
    for ( j = 0; j < num_rows; j++ ) {
      GET_COLUMN(dem,j)->points[i] = GINT16_FROM_BE(dem_mem[ent]);
      ent++;
    }

  }

  if (zip)
    free(dem_mem);
  g_mapped_file_unref(mf);
  return dem;
}

#define IS_SRTM_HGT(fn) (strlen((fn))==11 && (fn)[7]=='.' && (fn)[8]=='h' && (fn)[9]=='g' && (fn)[10]=='t' && ((fn)[0]=='N' || (fn)[0]=='S') && ((fn)[3]=='E' || (fn)[3]=='W'))

VikDEM *vik_dem_new_from_file(const char *file)
{
  FILE *f=NULL;
  VikDEM *rv;
  char buffer[DEM_BLOCK_SIZE+1];

  /* use to record state for dem_parse_block */
  int cur_column = -1;
  int cur_row = -1;
  const char *basename = a_file_basename(file);

  if ( g_access ( file, R_OK ) != 0 )
    return NULL;

  if ( (strlen(basename)==11 || ((strlen(basename) == 15) && (basename[11] == '.' && basename[12] == 'z' && basename[13] == 'i' && basename[14] == 'p'))) &&
       basename[7]=='.' && basename[8]=='h' && basename[9]=='g' && basename[10]=='t' &&
       (basename[0] == 'N' || basename[0] == 'S') && (basename[3] == 'E' || basename[3] =='W')) {
    bool is_zip_file = (strlen(basename) == 15);
    rv = vik_dem_read_srtm_hgt(file, basename, is_zip_file);
    return(rv);
  }

      /* Create Structure */
  rv = (VikDEM *) malloc(sizeof(VikDEM));

      /* Header */
  f = g_fopen(file, "r");
  if ( !f ) {
    free( rv );
    return NULL;
  }
  buffer[fread(buffer, 1, DEM_BLOCK_SIZE, f)] = '\0';
  if ( ! dem_parse_header ( buffer, rv ) ) {
    free( rv );
    fclose(f);
    return NULL;
  }
  /* TODO: actually use header -- i.e. GET # OF COLUMNS EXPECTED */

  rv->columns = g_ptr_array_new();
  rv->n_columns = 0;

      /* Column -- Data */
  while (! feof(f) ) {
     char *tmp;

     /* read block */
     buffer[fread(buffer, 1, DEM_BLOCK_SIZE, f)] = '\0';

     /* Fix Fortran-style exponentiation */
     tmp = buffer;
     while (*tmp) {
       if ( *tmp=='D')
         *tmp='E';
       tmp++;
     }

     dem_parse_block(buffer, rv, &cur_column, &cur_row);
  }

     /* TODO - class C records (right now says 'Invalid' and dies) */

  fclose(f);
  f = NULL;

  /* 24k scale */
  if ( rv->horiz_units == VIK_DEM_HORIZ_UTM_METERS && rv->n_columns >= 2 )
    rv->north_scale = rv->east_scale = GET_COLUMN(rv, 1)->east_west - GET_COLUMN(rv,0)->east_west;

  /* FIXME bug in 10m DEM's */
  if ( rv->horiz_units == VIK_DEM_HORIZ_UTM_METERS && rv->north_scale == 10 ) {
    rv->min_east -= 100;
    rv->min_north += 200;
  }


  return rv;
}

void vik_dem_free ( VikDEM *dem )
{
  unsigned int i;
  for ( i = 0; i < dem->n_columns; i++)
    free( GET_COLUMN(dem, i)->points );
  g_ptr_array_foreach ( dem->columns, (GFunc)g_free, NULL );
  g_ptr_array_free ( dem->columns, true );
  free( dem );
}

int16_t vik_dem_get_xy ( VikDEM *dem, unsigned int col, unsigned int row )
{
  if ( col < dem->n_columns )
    if ( row < GET_COLUMN(dem, col)->n_points )
      return GET_COLUMN(dem, col)->points[row];
  return VIK_DEM_INVALID_ELEVATION;
}

int16_t vik_dem_get_east_north ( VikDEM *dem, double east, double north )
{
  int col, row;

  if ( east > dem->max_east || east < dem->min_east ||
      north > dem->max_north || north < dem->min_north )
    return VIK_DEM_INVALID_ELEVATION;

  col = (int) floor((east - dem->min_east) / dem->east_scale);
  row = (int) floor((north - dem->min_north) / dem->north_scale);

  return vik_dem_get_xy ( dem, col, row );
}

static bool dem_get_ref_points_elev_dist(VikDEM *dem,
    double east, double north, /* in seconds */
    int16_t *elevs, int16_t *dists)
{
  int i;
  int cols[4], rows[4];
  struct LatLon ll[4];
  struct LatLon pos;

  if ( east > dem->max_east || east < dem->min_east ||
      north > dem->max_north || north < dem->min_north )
    return false;  /* got nothing */

  pos.lon = east/3600;
  pos.lat = north/3600;

  /* order of the data: sw, nw, ne, se */
  /* sw */
  cols[0] = (int) floor((east - dem->min_east) / dem->east_scale);
  rows[0] = (int) floor((north - dem->min_north) / dem->north_scale);
  ll[0].lon = (dem->min_east + dem->east_scale*cols[0])/3600;
  ll[0].lat = (dem->min_north + dem->north_scale*rows[0])/3600;
  /* nw */
  cols[1] = cols[0];
  rows[1] = rows[0] + 1;
  ll[1].lon = ll[0].lon;
  ll[1].lat = ll[0].lat + (double)dem->north_scale/3600;
  /* ne */
  cols[2] = cols[0] + 1;
  rows[2] = rows[0] + 1;
  ll[2].lon = ll[0].lon + (double)dem->east_scale/3600;
  ll[2].lat = ll[0].lat + (double)dem->north_scale/3600;
  /* se */
  cols[3] = cols[0] + 1;
  rows[3] = rows[0];
  ll[3].lon = ll[0].lon + (double)dem->east_scale/3600;
  ll[3].lat = ll[0].lat;

  for (i = 0; i < 4; i++) {
    if ((elevs[i] = vik_dem_get_xy(dem, cols[i], rows[i])) == VIK_DEM_INVALID_ELEVATION)
      return false;
    dists[i] = a_coords_latlon_diff(&pos, &ll[i]);
  }

#if 0  /* debug */
  for (i = 0; i < 4; i++)
    fprintf(stderr, "%f:%f:%d:%d  \n", ll[i].lat, ll[i].lon, dists[i], elevs[i]);
  fprintf(stderr, "   north_scale=%f\n", dem->north_scale);
#endif

  return true;  /* all OK */
}

int16_t vik_dem_get_simple_interpol ( VikDEM *dem, double east, double north )
{
  int i;
  int16_t elevs[4], dists[4];

  if (!dem_get_ref_points_elev_dist(dem, east, north, elevs, dists))
    return VIK_DEM_INVALID_ELEVATION;

  for (i = 0; i < 4; i++) {
    if (dists[i] < 1) {
      return(elevs[i]);
    }
  }

  double t = (double)elevs[0]/dists[0] + (double)elevs[1]/dists[1] + (double)elevs[2]/dists[2] + (double)elevs[3]/dists[3];
  double b = 1.0/dists[0] + 1.0/dists[1] + 1.0/dists[2] + 1.0/dists[3];

  return(t/b);
}

int16_t vik_dem_get_shepard_interpol ( VikDEM *dem, double east, double north )
{
  int i;
  int16_t elevs[4], dists[4];
  int16_t max_dist;
  double t = 0.0;
  double b = 0.0;

  if (!dem_get_ref_points_elev_dist(dem, east, north, elevs, dists))
    return VIK_DEM_INVALID_ELEVATION;

  max_dist = 0;
  for (i = 0; i < 4; i++) {
    if (dists[i] < 1) {
      return(elevs[i]);
    }
    if (dists[i] > max_dist)
      max_dist = dists[i];
  }

  double tmp;
#if 0 /* derived method by Franke & Nielson. Does not seem to work too well here */
  for (i = 0; i < 4; i++) {
    tmp = pow((1.0*(max_dist - dists[i])/max_dist*dists[i]), 2);
    t += tmp*elevs[i];
    b += tmp;
  }
#endif

  for (i = 0; i < 4; i++) {
    tmp = pow((1.0/dists[i]), 2);
    t += tmp*elevs[i];
    b += tmp;
  }

  // fprintf(stderr, "DEBUG: tmp=%f t=%f b=%f %f\n", tmp, t, b, t/b);

  return(t/b);

}

void vik_dem_east_north_to_xy ( VikDEM *dem, double east, double north, unsigned int *col, unsigned int *row )
{
  *col = (unsigned int) floor((east - dem->min_east) / dem->east_scale);
  *row = (unsigned int) floor((north - dem->min_north) / dem->north_scale);
}

