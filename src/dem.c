#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <math.h>
#include <stdlib.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>


#include "dem.h"
#include "file.h"

#define DEM_BLOCK_SIZE 1024
#define GET_COLUMN(dem,n) ((VikDEMColumn *)g_ptr_array_index( (dem)->columns, (n) ))

static gboolean get_double_and_continue ( gchar **buffer, gdouble *tmp, gboolean warn )
{
  gchar *endptr;
  *tmp = g_strtod(*buffer, &endptr);
  if ( endptr == NULL|| endptr == *buffer ) {
    if ( warn )
      g_warning("Invalid DEM");
    return FALSE;
  }
  *buffer=endptr;
  return TRUE;
}


static gboolean get_int_and_continue ( gchar **buffer, gint *tmp, gboolean warn )
{
  gchar *endptr;
  *tmp = strtol(*buffer, &endptr, 10);
  if ( endptr == NULL|| endptr == *buffer ) {
    if ( warn )
      g_warning("Invalid DEM");
    return FALSE;
  }
  *buffer=endptr;
  return TRUE;
}

static gboolean dem_parse_header ( gchar *buffer, VikDEM *dem )
{
  gdouble val;
  gint int_val;
  guint i;
  gchar *tmp = buffer;

  /* incomplete header */
  if ( strlen(buffer) != 1024 )
    return FALSE;

  /* fix Fortran-style exponentiation 1.0D5 -> 1.0E5 */
  while (*tmp) {
    if ( *tmp=='D')
      *tmp='E';
    tmp++;
  }

  /* skip name */
  buffer += 149;

  /* "DEM level code, pattern code, palaimetric reference system code" -- skip */
  get_int_and_continue(&buffer, &int_val, TRUE);
  get_int_and_continue(&buffer, &int_val, TRUE);
  get_int_and_continue(&buffer, &int_val, TRUE);

  /* zone */
  get_int_and_continue(&buffer, &int_val, TRUE);
  dem->utm_zone = int_val;
  /* TODO -- southern or northern hemisphere?! */
  dem->utm_letter = 'N';

  /* skip numbers 5-19  */
  for ( i = 0; i < 15; i++ ) {
    if ( ! get_double_and_continue(&buffer, &val, FALSE) ) {
      g_warning ("Invalid DEM header");
      return FALSE;
    }
  }

  /* number 20 -- horizontal unit code (utm/ll) */
  get_double_and_continue(&buffer, &val, TRUE);
  dem->horiz_units = val;
  get_double_and_continue(&buffer, &val, TRUE);
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
  get_double_and_continue(&buffer, &val, TRUE);

  /* now we get the four corner points. record the min and max. */
  get_double_and_continue(&buffer, &val, TRUE);
  dem->min_east = dem->max_east = val;
  get_double_and_continue(&buffer, &val, TRUE);
  dem->min_north = dem->max_north = val;

  for ( i = 0; i < 3; i++ ) {
    get_double_and_continue(&buffer, &val, TRUE);
    if ( val < dem->min_east ) dem->min_east = val;
    if ( val > dem->max_east ) dem->max_east = val;
    get_double_and_continue(&buffer, &val, TRUE);
    if ( val < dem->min_north ) dem->min_north = val;
    if ( val > dem->max_north ) dem->max_north = val;
  }

  return TRUE;
}

static void dem_parse_block_as_cont ( gchar *buffer, VikDEM *dem, gint *cur_column, gint *cur_row )
{
  gint tmp;
  while ( *cur_row < GET_COLUMN(dem, *cur_column)->n_points ) {
    if ( get_int_and_continue(&buffer, &tmp,FALSE) ) {
      if ( dem->orig_vert_units == VIK_DEM_VERT_DECIMETERS )
        GET_COLUMN(dem, *cur_column)->points[*cur_row] = (gint16) (tmp / 10);
      else
        GET_COLUMN(dem, *cur_column)->points[*cur_row] = (gint16) tmp;
    } else
      return;
    (*cur_row)++;
  }
  *cur_row = -1; /* expecting new column */
}

static void dem_parse_block_as_header ( gchar *buffer, VikDEM *dem, gint *cur_column, gint *cur_row )
{
  guint n_rows;
  gint i;
  gdouble east_west, south;
  gdouble tmp;

  /* 1 x n_rows 1 east_west south x x x DATA */

  if ( (!get_double_and_continue(&buffer, &tmp, TRUE)) || tmp != 1 ) {
    g_warning("Incorrect DEM Class B record: expected 1");
    return;
  }

  /* don't need this */
  if ( !get_double_and_continue(&buffer, &tmp, TRUE ) ) return;

  /* n_rows */
  if ( !get_double_and_continue(&buffer, &tmp, TRUE ) )
    return;
  n_rows = (guint) tmp;

  if ( (!get_double_and_continue(&buffer, &tmp, TRUE)) || tmp != 1 ) {
    g_warning("Incorrect DEM Class B record: expected 1");
    return;
  }
  
  if ( !get_double_and_continue(&buffer, &east_west, TRUE) )
    return;
  if ( !get_double_and_continue(&buffer, &south, TRUE) )
    return;

  /* next three things we don't need */
  if ( !get_double_and_continue(&buffer, &tmp, TRUE)) return;
  if ( !get_double_and_continue(&buffer, &tmp, TRUE)) return;
  if ( !get_double_and_continue(&buffer, &tmp, TRUE)) return;


  dem->n_columns ++;
  (*cur_column) ++;

  /* empty spaces for things before that were skipped */
  (*cur_row) = (south - dem->min_north) / dem->north_scale;
  if ( south > dem->max_north || (*cur_row) < 0 )
    (*cur_row) = 0;

  n_rows += *cur_row;

  g_ptr_array_add ( dem->columns, g_malloc(sizeof(VikDEMColumn)) );
  GET_COLUMN(dem,*cur_column)->east_west = east_west;
  GET_COLUMN(dem,*cur_column)->south = south;
  GET_COLUMN(dem,*cur_column)->n_points = n_rows;
  GET_COLUMN(dem,*cur_column)->points = g_malloc(sizeof(gint16)*n_rows);

  /* no information for things before that */
  for ( i = 0; i < (*cur_row); i++ )
    GET_COLUMN(dem,*cur_column)->points[i] = VIK_DEM_INVALID_ELEVATION;

  /* now just continue */
  dem_parse_block_as_cont ( buffer, dem, cur_column, cur_row );


}

static void dem_parse_block ( gchar *buffer, VikDEM *dem, gint *cur_column, gint *cur_row )
{
  /* if haven't read anything or have read all items in a columns and are expecting a new column */
  if ( *cur_column == -1 || *cur_row == -1 ) {
    dem_parse_block_as_header(buffer, dem, cur_column, cur_row);
  } else {
    dem_parse_block_as_cont(buffer, dem, cur_column, cur_row);
  }
}

/* return size of unzip data or 0 if failed */
/* can be made generic to uncompress zip, gzip, bzip2 data */
static guint uncompress_data(void *uncompressed_buffer, guint uncompressed_size, void *compressed_data, guint compressed_size)
{
	z_stream stream;
	int err;

	stream.next_in = compressed_data;
	stream.avail_in = compressed_size;
	stream.next_out = uncompressed_buffer;
	stream.avail_out = uncompressed_size;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;
	stream.opaque = (voidpf)0;

	/* negative windowBits to inflateInit2 means "no header" */
	if ((err = inflateInit2(&stream, -MAX_WBITS)) != Z_OK) {
		g_warning("%s(): inflateInit2 failed", __PRETTY_FUNCTION__);
		return 0;
	}

	err = inflate(&stream, Z_FINISH);
	if ((err != Z_OK) && (err != Z_STREAM_END) && stream.msg) {
		g_warning("%s() inflate failed err=%d \"%s\"", __PRETTY_FUNCTION__, err, stream.msg == NULL ? "unknown" : stream.msg);
		inflateEnd(&stream);
		return 0;
	}

	inflateEnd(&stream);
	return(stream.total_out);
}

static void *unzip_hgt_file(gchar *zip_file, gulong *unzip_size)
{
	void *unzip_data = NULL;
	gchar *zip_data;
	struct _lfh {
		guint32 sig;
		guint16 extract_version;
		guint16 flags;
		guint16 comp_method;
		guint16 time;
		guint16 date;
		guint32 crc_32;
		guint32 compressed_size;
		guint32 uncompressed_size;
		guint16 filename_len;
		guint16 extra_field_len;
	}  __attribute__ ((__packed__)) *local_file_header = NULL;


	local_file_header = (struct _lfh *) zip_file;
	if (local_file_header->sig != 0x04034b50) {
		g_warning("%s(): wrong format\n", __PRETTY_FUNCTION__);
		g_free(unzip_data);
		goto end;
	}

	zip_data = zip_file + sizeof(struct _lfh) + local_file_header->filename_len + local_file_header->extra_field_len;
	unzip_data = g_malloc(local_file_header->uncompressed_size);
	gulong uncompressed_size = local_file_header->uncompressed_size;

	if (!(*unzip_size = uncompress_data(unzip_data, uncompressed_size, zip_data, local_file_header->compressed_size))) {
		g_free(unzip_data);
		unzip_data = NULL;
		goto end;
	}

end:
	return(unzip_data);
}

static VikDEM *vik_dem_read_srtm_hgt(FILE *f, const gchar *basename, gboolean zip)
{
  gint i, j;
  VikDEM *dem;
  struct stat stat;
  off_t file_size;
  gint16 *dem_mem = NULL;
  gint16 *dem_file = NULL;
  const gint num_rows_3sec = 1201;
  const gint num_rows_1sec = 3601;
  gint num_rows;
  int fd = fileno(f);
  gint arcsec;

  dem = g_malloc(sizeof(VikDEM));

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

  if (fstat(fd, &stat) == -1)
    g_error("%s(): fstat failed on %s\n", __PRETTY_FUNCTION__, basename);
  if ((dem_file = mmap(NULL, stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == (void *) -1)
    g_error("%s(): mmap failed on %s\n", __PRETTY_FUNCTION__, basename);

  file_size = stat.st_size;
  dem_mem = dem_file;
  if (zip) {
    void *unzip_mem = NULL;
    gulong ucsize;

    if ((unzip_mem = unzip_hgt_file((gchar *)dem_file, &ucsize)) == NULL) {
      munmap(dem_file, file_size);
      g_ptr_array_free(dem->columns, TRUE);
      g_free(dem);
      return NULL;
    }

    dem_mem = unzip_mem;
    file_size = ucsize;
  }

  if (file_size == (num_rows_3sec * num_rows_3sec * sizeof(gint16)))
    arcsec = 3;
  else if (file_size == (num_rows_1sec * num_rows_1sec * sizeof(gint16)))
    arcsec = 1;
  else {
    g_warning("%s(): file %s does not have right size\n", __PRETTY_FUNCTION__, basename);
    munmap(dem_file, file_size);
    g_ptr_array_free(dem->columns, TRUE);
    g_free(dem);
    return NULL;
  }

  num_rows = (arcsec == 3) ? num_rows_3sec : num_rows_1sec;
  dem->east_scale = dem->north_scale = arcsec;

  for ( i = 0; i < num_rows; i++ ) {
    dem->n_columns++;
    g_ptr_array_add ( dem->columns, g_malloc(sizeof(VikDEMColumn)) );
    GET_COLUMN(dem,i)->east_west = dem->min_east + arcsec*i;
    GET_COLUMN(dem,i)->south = dem->min_north;
    GET_COLUMN(dem,i)->n_points = num_rows;
    GET_COLUMN(dem,i)->points = g_malloc(sizeof(gint16)*num_rows);
  }

  int ent = 0;
  for ( i = (num_rows - 1); i >= 0; i-- ) {
    for ( j = 0; j < num_rows; j++ ) {
      GET_COLUMN(dem,j)->points[i] = GINT16_FROM_BE(dem_mem[ent]);
      ent++;
    }

  }

  if (zip)
    g_free(dem_mem);
  munmap(dem_file, stat.st_size);
  return dem;
}

#define IS_SRTM_HGT(fn) (strlen((fn))==11 && (fn)[7]=='.' && (fn)[8]=='h' && (fn)[9]=='g' && (fn)[10]=='t' && ((fn)[0]=='N' || (fn)[0]=='S') && ((fn)[3]=='E' || (fn)[3]=='W'))

VikDEM *vik_dem_new_from_file(const gchar *file)
{
  FILE *f;
  VikDEM *rv;
  gchar buffer[DEM_BLOCK_SIZE+1];

  /* use to record state for dem_parse_block */
  gint cur_column = -1;
  gint cur_row = -1;
  const gchar *basename = a_file_basename(file);

      /* FILE IO */
  f = fopen(file, "r");
  if ( !f )
    return NULL;

  if ( (strlen(basename)==11 || ((strlen(basename) == 15) && (basename[11] == '.' && basename[12] == 'z' && basename[13] == 'i' && basename[14] == 'p'))) &&
       basename[7]=='.' && basename[8]=='h' && basename[9]=='g' && basename[10]=='t' &&
       (basename[0] == 'N' || basename[0] == 'S') && (basename[3] == 'E' || basename[3] =='W')) {
    gboolean is_zip_file = (strlen(basename) == 15);
    rv = vik_dem_read_srtm_hgt(f, basename, is_zip_file);
    fclose(f);
    return(rv);
  }

      /* Create Structure */
  rv = g_malloc(sizeof(VikDEM));

      /* Header */
  buffer[fread(buffer, 1, DEM_BLOCK_SIZE, f)] = '\0';
  if ( ! dem_parse_header ( buffer, rv ) ) {
    g_free ( rv );
    return NULL;
  }
  /* TODO: actually use header -- i.e. GET # OF COLUMNS EXPECTED */

  rv->columns = g_ptr_array_new();
  rv->n_columns = 0;

      /* Column -- Data */
  while (! feof(f) ) {
     gchar *tmp;

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
  guint i;
  for ( i = 0; i < dem->n_columns; i++)
    g_free ( GET_COLUMN(dem, i)->points );
  g_ptr_array_free ( dem->columns, TRUE );
  g_free ( dem );
}

gint16 vik_dem_get_xy ( VikDEM *dem, guint col, guint row )
{
  if ( col < dem->n_columns )
    if ( row < GET_COLUMN(dem, col)->n_points )
      return GET_COLUMN(dem, col)->points[row];
  return VIK_DEM_INVALID_ELEVATION;
}

gint16 vik_dem_get_east_north ( VikDEM *dem, gdouble east, gdouble north )
{
  gint col, row;

  if ( east > dem->max_east || east < dem->min_east ||
      north > dem->max_north || north < dem->min_north )
    return VIK_DEM_INVALID_ELEVATION;

  col = (gint) floor((east - dem->min_east) / dem->east_scale);
  row = (gint) floor((north - dem->min_north) / dem->north_scale);

  return vik_dem_get_xy ( dem, col, row );
}

void vik_dem_east_north_to_xy ( VikDEM *dem, gdouble east, gdouble north, guint *col, guint *row )
{
  *col = (gint) floor((east - dem->min_east) / dem->east_scale);
  *row = (gint) floor((north - dem->min_north) / dem->north_scale);
  if ( *col < 0 ) *col = 0;
  if ( *row < 0 ) *row = 0;
}

