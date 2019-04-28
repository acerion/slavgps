/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2008, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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




#include <cstring>
#include <cstdlib>
#include <cstdint>

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif




#include <gio/gio.h>




#include <QDebug>




#include "compression.h"




using namespace SlavGPS;




#define SG_MODULE "Compression"




#ifdef HAVE_LIBZ
/* Return size of unzip data or 0 if failed. */
static unsigned int uncompress_data(void * uncompressed_buffer, unsigned int uncompressed_size, void * compressed_data, unsigned int compressed_size)
{
	z_stream stream;
	int err;

	stream.next_in = (Bytef *) compressed_data;
	stream.avail_in = compressed_size;
	stream.next_out = (Bytef *) uncompressed_buffer;
	stream.avail_out = uncompressed_size;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;
	stream.opaque = (voidpf)0;

	/* Negative windowBits to inflateInit2 means "no header". */
	if ((err = inflateInit2(&stream, -MAX_WBITS)) != Z_OK) {
		fprintf(stderr, "WARNING: %s(): inflateInit2 failed\n", __PRETTY_FUNCTION__);
		return 0;
	}

	err = inflate(&stream, Z_FINISH);
	if ((err != Z_OK) && (err != Z_STREAM_END) && stream.msg) {
		fprintf(stderr, "WARNING: %s() inflate failed err=%d \"%s\"\n", __PRETTY_FUNCTION__, err, stream.msg == NULL ? "unknown" : stream.msg);
		inflateEnd(&stream);
		return 0;
	}

	inflateEnd(&stream);
	return(stream.total_out);
}
#endif




/**
 * @zip_file:   pointer to start of compressed data
 * @unzip_size: the size of the compressed data block
 *
 * Returns a pointer to uncompressed data (maybe NULL).
 */
void * SlavGPS::unzip_file(char * zip_file, size_t * unzip_size)
{
#ifndef HAVE_LIBZ
	void * unzip_data = NULL;
	goto end;
#else
	/* See http://en.wikipedia.org/wiki/Zip_(file_format) */
	struct _lfh {
		uint32_t sig;
		uint16_t extract_version;
		uint16_t flags;
		uint16_t comp_method;
		uint16_t time;
		uint16_t date;
		uint32_t crc_32;
		uint32_t compressed_size;
		uint32_t uncompressed_size;
		uint16_t filename_len;
		uint16_t extra_field_len;
	}  __attribute__ ((gcc_struct,__packed__)) *local_file_header = NULL;

	if (sizeof(struct _lfh) != 30) {
		fprintf(stderr, "CRITICAL: Incorrect internal zip header size, should be 30 but is %zd\n", sizeof(struct _lfh));
		return NULL;
	}

	local_file_header = (struct _lfh *) zip_file;
	if (GUINT32_FROM_LE(local_file_header->sig) != 0x04034b50) {
		fprintf(stderr, "WARNING: %s(): wrong zip format (%d)\n", __PRETTY_FUNCTION__, GUINT32_FROM_LE(local_file_header->sig));
		return NULL;
	}

	char * zip_data = zip_file + sizeof(struct _lfh)
		+ GUINT16_FROM_LE(local_file_header->filename_len)
		+ GUINT16_FROM_LE(local_file_header->extra_field_len);
	const unsigned long uncompressed_size = GUINT32_FROM_LE(local_file_header->uncompressed_size);
	void * unzip_data = malloc(uncompressed_size);

	/* Protection against malloc failures.
	   ATM not normally been checking malloc failures in Viking but sometimes using zip files can be quite large
	   (e.g. when using DEMs) so more potential for failure. */
	if (!unzip_data) {
		goto end;
	}

	fprintf(stderr, "DEBUG: %s: method %d: from size %d to %ld\n", __FUNCTION__, GUINT16_FROM_LE(local_file_header->comp_method), GUINT32_FROM_LE(local_file_header->compressed_size), uncompressed_size);

	if (GUINT16_FROM_LE(local_file_header->comp_method) == 0 &&
		(uncompressed_size == GUINT32_FROM_LE(local_file_header->compressed_size))) {
		/* Stored only - no need to 'uncompress'. Thus just copy. */
		memcpy(unzip_data, zip_data, uncompressed_size);
		*unzip_size = uncompressed_size;
		goto end;
	}

	if (!(*unzip_size = uncompress_data(unzip_data, uncompressed_size, zip_data, GUINT32_FROM_LE(local_file_header->compressed_size)))) {
		free(unzip_data);
		unzip_data = NULL;
		goto end;
	}

#endif
end:
	return unzip_data;
}




/**
 * @name: The name of the file to attempt to decompress
 *
 * Returns: The name of the uncompressed file (in a temporary location) or NULL.
 * Free the returned name after use.
 *
 * Also see: http://www.bzip.org/1.0.5/bzip2-manual-1.0.5.html
 */
sg_ret SlavGPS::uncompress_bzip2(QString & uncompressed_file_full_path, const QString & archive_file_full_path)
{
#ifdef HAVE_BZLIB_H
	qDebug() << SG_PREFIX_D << "bzip2 version" << BZ2_bzlibVersion();

	FILE * ff = fopen(archive_file_full_path.toUtf8().constData(), "rb");
	if (!ff) {
		return sg_ret::err;
	}

	int bzerror;
	BZFILE * bf = BZ2_bzReadOpen(&bzerror, ff, 0, 0, NULL, 0); /* This should take care of the bz2 file header. */
	if (bzerror != BZ_OK) {
		BZ2_bzReadClose(&bzerror, bf);
		/* Handle error. */
		qDebug() << SG_PREFIX_W << "BZ ReadOpen error on" << archive_file_full_path;
		return sg_ret::err;
	}

	GFileIOStream * gios;
	GError * error = NULL;
	char * tmpname = NULL;
#if GLIB_CHECK_VERSION(2,32,0)
	GFile * gf = g_file_new_tmp("vik-bz2-tmp.XXXXXX", &gios, &error);
	tmpname = g_file_get_path(gf);
#else
	int fd = g_file_open_tmp("vik-bz2-tmp.XXXXXX", &tmpname, &error);
	if (error) {
		qDebug() << SG_PREFIX_W << error->message;
		g_error_free(error);
		return sg_ret::err;
	}
	gios = g_file_open_readwrite(g_file_new_for_path (tmpname), NULL, &error);
	if (error) {
		qDebug() << SG_PREFIX_W << error->message;
		g_error_free(error);
		return sg_ret::err;
	}
#endif

	GOutputStream *gos = g_io_stream_get_output_stream(G_IO_STREAM(gios));

	/* Process in arbitary sized chunks. */
	char buf[4096];
	bzerror = BZ_OK;
	int nBuf = 0;
	/* Now process the actual compression data. */
	while (bzerror == BZ_OK) {
		nBuf = BZ2_bzRead(&bzerror, bf, buf, 4096);
		if (bzerror == BZ_OK || bzerror == BZ_STREAM_END) {
			/* Do something with buf[0 .. nBuf-1] */
			if (g_output_stream_write(gos, buf, nBuf, NULL, &error) < 0) {
				qDebug() << SG_PREFIX_E << "Couldn't write bz2 tmp" << tmpname << "file due to" << error->message;
				g_error_free(error);
				BZ2_bzReadClose(&bzerror, bf);
				goto end;
			}
		}
	}
	if (bzerror != BZ_STREAM_END) {
		/* Handle error... */
		qDebug() << SG_PREFIX_W << "BZ error: " << bzerror << "read" << nBuf;
	}
	BZ2_bzReadClose(&bzerror, bf);
	g_output_stream_close(gos, NULL, &error);

 end:
	g_object_unref(gios);
	fclose(ff);

	uncompressed_file_full_path = QString(tmpname);
	g_free(tmpname);

	return sg_ret::ok;
#else
	return sg_ret::err;
#endif
}
