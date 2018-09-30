/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2018, Kamil Ignacak <acerion@wp.pl>
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




/*
 * Mostly imported from https://github.com/openstreetmap/mod_tile/
 * Release 0.4
 */




#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>




#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QObject>
#include <QDebug>




#include "osm_metatile.h"
#include "globals.h"
#include "mapcoord.h"





using namespace SlavGPS;




#define SG_MODULE "MetaTile"




#define META_MAGIC "META"
#define META_MAGIC_COMPRESSED "METZ"

/* Use this to enable meta-tiles which will render NxN tiles at once.
   Note: This should be a power of 2 (2, 4, 8, 16 ...). */
#define METATILE (8)




struct entry {
	int offset;
	int size;
};




struct metatile_header {
	char magic[4]; /* META_MAGIC or META_MAGIC_COMPRESSED. */
	int count;     /* METATILE ^ 2 */

	/* Lowest x,y of this metatile, plus z */
	int x;
	int y;
	int z;

	struct entry index[]; /* Array with metatile_header::count entries. */

	/* What follows the header is the tile data.
	   The index offsets are measured from the start of the file. */
};




/**
   Based on function from mod_tile/src/store_file_utils.c. Previously this has been called xyz_to_meta().

   Sets the path to the meta-tile and the offset within the meta-tile.
*/
Metatile::Metatile(const QString & dir, const TileInfo & tile_info)
{
	int x = tile_info.x;
	int y = tile_info.y;
	int z = tile_info.get_tile_zoom_level(); /* This is OSM metatile, so use TileInfo::get_tile_zoom_level() directly. */

	/* Each meta tile winds up in its own file, with several in each leaf directory
	   the .meta tile name is based on the sub-tile at (0,0). */
	unsigned char mask = METATILE - 1;
	this->offset = (x & mask) * METATILE + (y & mask);
	x &= ~mask;
	y &= ~mask;

	unsigned char hash[5];
	for (unsigned char i = 0; i < 5; i++) {
		hash[i] = ((x & 0x0f) << 4) | (y & 0x0f);
		x >>= 4;
		y >>= 4;
	}

	char path[PATH_MAX];
	snprintf(path, sizeof (path), "%s/%d/%u/%u/%u/%u/%u.meta", dir.toUtf8().constData(), z, hash[4], hash[3], hash[2], hash[1], hash[0]);
	this->file_full_path = QString(path);

	qDebug() << SG_PREFIX_I << "Dir path" << dir << "full path" << this->file_full_path;
}




/**
   Slightly reworked to use simplified code creating path in Metatile::Metatile() above.

   Reads into Metatile::buffer up to size specified by sizeof (Metatile::buffer).

   Sets Metatile::is_compressed to inform whether the file is in a
   compressed format (possibly only gzip).

   Error messages returned in log_msg.
*/
int Metatile::read_metatile(QString & log_msg)
{
	int fd = open(this->file_full_path.toUtf8().constData(), O_RDONLY);
	if (fd < 0) {
		log_msg = QObject::tr("Could not open metatile %1. Reason: %2\n").arg(this->file_full_path).arg(strerror(errno));
		return -1;
	}

	const size_t header_size = sizeof (struct metatile_header) + METATILE * METATILE * sizeof (struct entry);
	struct metatile_header * header = (struct metatile_header *) malloc(header_size);


	size_t header_read_bytes = 0;
	while (header_read_bytes < header_size) {
		size_t len = header_size - header_read_bytes;
		ssize_t got = read(fd, ((unsigned char *) header) + header_read_bytes, len);
		if (got < 0) {
			log_msg = QObject::tr("Failed to read complete header for metatile %1. Reason: %2\n").arg(this->file_full_path).arg(strerror(errno));
			close(fd);
			free(header);
			return -2;
		} else if (got > 0) {
			header_read_bytes += got;
		} else {
			break;
		}
	}
	if (header_read_bytes < header_size) {
		log_msg = QObject::tr("Meta file %1 too small to contain header\n").arg(this->file_full_path);
		close(fd);
		free(header);
		return -3;
	}
	if (memcmp(header->magic, META_MAGIC, strlen(META_MAGIC))) {
		if (memcmp(header->magic, META_MAGIC_COMPRESSED, strlen(META_MAGIC_COMPRESSED))) {
			log_msg = QObject::tr("Meta file %1 header magic mismatch\n").arg(this->file_full_path);
			close(fd);
			free(header);
			return -4;
		} else {
			this->is_compressed = true;
		}
	} else {
		this->is_compressed = false;
	}

	/* Currently this code only works with fixed metatile sizes (due to Metatile::Metatile() above). */
	if (header->count != (METATILE * METATILE)) {
		log_msg = QObject::tr("Meta file %1 header bad count %2 != %3\n").arg(this->file_full_path).arg(header->count).arg(METATILE * METATILE);
		free(header);
		close(fd);
		return -5;
	}

	size_t file_offset = header->index[this->offset].offset;
	size_t tile_size   = header->index[this->offset].size;

	free(header);

	if (tile_size > sizeof (this->buffer)) {
		log_msg = QObject::tr("Truncating tile %1 to fit buffer of %2\n").arg(tile_size).arg(sizeof (this->buffer));
		tile_size = sizeof (this->buffer);
		close(fd);
		return -6;
	}

	if (lseek(fd, file_offset, SEEK_SET) < 0) {
		log_msg = QObject::tr("Meta file %1 seek error: %2\n").arg(this->file_full_path).arg(strerror(errno));
		close(fd);
		return -7;
	}


	/* Read the actual tile data. */
	this->read_bytes = 0;
	while (this->read_bytes < tile_size) {
		size_t len = tile_size - this->read_bytes;
		ssize_t got = read(fd, this->buffer + this->read_bytes, len);
		if (got < 0) {
			log_msg = QObject::tr("Failed to read data from file %1. Reason: %2\n").arg(this->file_full_path).arg(strerror(errno));
			close(fd);
			return -8;
		} else if (got > 0) {
			this->read_bytes += got;
		} else {
			break;
		}
	}
	close(fd);
	return 0;
}
