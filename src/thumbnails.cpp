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

/*
 * Large (and important) sections of this file were adapted from
 * ROX-Filer source code, Copyright (C) 2003, the ROX-Filer team,
 * originally licensed under the GPL v2 or greater (as above).
 */




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <cstring>
#include <cstdlib>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>

#include <QDebug>
#include <QDir>

#include <glib.h>
#include <glib/gstdio.h>
#include "thumbnails.h"
//#include "file.h"
#include "globals.h"
#include "vikutils.h"




using namespace SlavGPS;




#define PREFIX " Thumbnails:" << __FUNCTION__ << __LINE__ << ">"




#undef MIN /* quit yer whining, gcc */
#undef MAX
#ifndef MAX
/* We need MAX macro and some system does not offer it */
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#define HOME_DIR g_get_home_dir()

#ifdef WINDOWS
#define THUMB_DIR "\\THUMBNAILS\\" /* viking maps default viking\maps */
#define THUMB_SUB_DIR "normal\\"
#else
#define THUMB_DIR "/.thumbnails/"
#define THUMB_SUB_DIR "normal/"
#endif




static QString md5_hash(const char * message);




bool Thumbnails::thumbnail_exists(const QString & original_file_full_path)
{
	QPixmap * pixmap = Thumbnails::get_thumbnail(original_file_full_path);
	if (pixmap) {
		delete pixmap;
		return true;
	}
	return false;
}




QPixmap * Thumbnails::get_default_thumbnail()
{
	return new QPixmap(":/thumbnails/not_loaded_yet.png");
}




void Thumbnails::generate_thumbnail_if_missing(const QString & original_file_full_path)
{
	if (!Thumbnails::thumbnail_exists(original_file_full_path)) {
		Thumbnails::generate_thumbnail(original_file_full_path);
	}
}




QPixmap Thumbnails::scale_pixmap(const QPixmap & src, int max_w, int max_h)
{
	const int w = src.width();
	const int h = src.height();

	if (w <= max_w && h <= max_h) {
		return src;
	} else {
		float scale_x = ((float) w) / max_w;
		float scale_y = ((float) h) / max_h;
		float scale = MAX(scale_x, scale_y);
		int dest_w = w / scale;
		int dest_h = h / scale;

		return src.scaled(MAX(dest_w, 1), MAX(dest_h, 1), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
}




bool Thumbnails::generate_thumbnail(const QString & original_file_full_path)
{
	QPixmap original_image;
	if (!original_image.load(original_file_full_path)) {
		return false;
	}

#ifdef K_TODO
	QPixmap * tmpbuf = gdk_pixbuf_apply_embedded_orientation(original_image);
	g_object_unref(G_OBJECT(original_image));
	original_image = tmpbuf;
#endif

	struct stat info;
	if (stat(original_file_full_path.toUtf8().constData(), &info) != 0) {
		return false;
	}

	QPixmap thumb = Thumbnails::scale_pixmap(original_image, PIXMAP_THUMB_SIZE, PIXMAP_THUMB_SIZE);

	const QString canonical_path = SGUtils::get_canonical_path(original_file_full_path);
	const QString original_uri = QString("file://%1").arg(canonical_path);
	const QString md5 = md5_hash(original_uri.toUtf8().constData());


	QString target_full_path = QString("%1%2%3").arg(HOME_DIR).arg(THUMB_DIR).arg(THUMB_SUB_DIR);
	const QDir thumbs_dir(target_full_path); /* Here we are still using path to a directory, not a final-final path. */
	/* Create thumbnails directory (with all parent dirs if necessary).
	   TODO: viking used 0700 permissions for the directory. What should we do? Use umask? */
	if (!thumbs_dir.mkpath(".")) {
		qDebug() << "EE: Failed to create thumbnails directory" << target_full_path;
	}

	target_full_path += QString("%1.png").arg(md5);

	/* Truncate final path to this length when renaming.
	   We create the file ###.png.Viking-PID and rename it to avoid
	   a race condition if two programs create the same thumb at
	   once. */
	const int final_path_len = target_full_path.length();
#ifdef WINDOWS
	target_full_path += ".Viking";
#else
	target_full_path += QString(".Viking-%1").arg((long) getpid());
#endif


	mode_t old_mask = umask(0077);
	const bool success = thumb.save(target_full_path, "png");
	umask(old_mask);
	if (!success) {
		qDebug() << "EE:" PREFIX << "failed to save thumbnail";
		return false;
	}


	const QString swidth = QString("%1").arg((int) original_image.width());
	const QString sheight = QString("%1").arg((int) original_image.height());
	const QString ssize = QString("%1").arg((long long) info.st_size);
	const QString smtime = QString("%1").arg((long) info.st_mtime);
#ifdef K_TODO
	const char * orientation = gdk_pixbuf_get_option(original_image, "orientation");

	/*
	  Thumb::URI must be in ISO-8859-1 encoding otherwise gdk_pixbuf_save() will fail
	  - e.g. if characters such as 'ě' are encountered.
	  Also see http://en.wikipedia.org/wiki/ISO/IEC_8859-1.
	*/

	gdk_pixbuf_save(thumb, target_full_path, "png", &error,
			"tEXt::Thumb::Image::Width", swidth,
			"tEXt::Thumb::Image::Height", sheight,
			"tEXt::Thumb::Size", ssize,
			"tEXt::Thumb::MTime", smtime,
			"tEXt::Thumb::URI", original_uri.toLatin1().constData(),
			"tEXt::Software", PROJECT,
			"tEXt::Software::Orientation", orientation ? orientation : "0",
			NULL);
#endif

	const QString final_full_path = target_full_path.left(final_path_len);
	if (rename(target_full_path.toUtf8().constData(), final_full_path.toUtf8().constData())) {
		int e = errno;
		qDebug() << "EE:" PREFIX "failed to rename" << target_full_path << "to" << final_full_path << ":" << strerror(e);
		return false;
	}

	return true;
}




QPixmap * Thumbnails::get_thumbnail(const QString & original_file_full_path)
{
	const QString canonical_path = SGUtils::get_canonical_path(original_file_full_path);
	const QString original_uri = QString("file://%1").arg(canonical_path);
	const QString md5 = md5_hash(original_uri.toUtf8().constData());

	const QString thumb_path = QString("%1%2%3%4.png").arg(HOME_DIR).arg(THUMB_DIR).arg(THUMB_SUB_DIR).arg(md5);


	QPixmap * thumb = new QPixmap();
	if (!thumb->load(thumb_path)) {
		goto err;
	}

#ifdef K_TODO
	/* Note that these don't need freeing... */
	const char * ssize = gdk_pixbuf_get_option(thumb, "tEXt::Thumb::Size");
	if (!ssize) {
		goto err;
	}

	const char * smtime = gdk_pixbuf_get_option(thumb, "tEXt::Thumb::MTime");
	if (!smtime) {
		goto err;
	}

	struct stat info;
	if (stat(canonical_path.toUtf8().constData(), &info) != 0) {
		goto err;
	}

	if (info.st_mtime != atol(smtime) || info.st_size != atol(ssize)) {
		goto err;
	}
#endif

	goto out;
err:
	if (thumb) {
		delete thumb; /* Deleting because we are handling error here. */
		thumb = NULL;
	}

out:
	return thumb;
}




/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest. The original code was
 * written by Colin Plumb in 1993, and put in the public domain.
 *
 * Modified to use glib datatypes. Put under GPL to simplify
 * licensing for ROX-Filer. Taken from Debian's dpkg package.
 *
 */

#define md5byte unsigned char

typedef struct _MD5Context MD5Context;

struct _MD5Context {
	uint32_t buf[4];
	uint32_t bytes[2];
	uint32_t in[16];
};




static void MD5Init(MD5Context *ctx);
static void MD5Update(MD5Context *ctx, md5byte const *buf, unsigned len);
static char *MD5Final(MD5Context *ctx);
static void MD5Transform(uint32_t buf[4], uint32_t const in[16]);




#if G_BYTE_ORDER == G_BIG_ENDIAN
static void byteSwap(uint32_t * buf, unsigned words)
{
	md5byte *p = (md5byte *) buf;

	do {
		*buf++ = (uint32_t)((unsigned)p[3] << 8 | p[2]) << 16 |
			((unsigned)p[1] << 8 | p[0]);
		p += 4;
	} while (--words);
}
#else
#define byteSwap(buf,words)
#endif




/*
 * Start MD5 accumulation. Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
static void MD5Init(MD5Context * ctx)
{
	ctx->buf[0] = 0x67452301;
	ctx->buf[1] = 0xefcdab89;
	ctx->buf[2] = 0x98badcfe;
	ctx->buf[3] = 0x10325476;

	ctx->bytes[0] = 0;
	ctx->bytes[1] = 0;
}




/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void MD5Update(MD5Context * ctx, md5byte const * buf, unsigned len)
{
	/* Update byte count. */

	uint32_t t = ctx->bytes[0];
	if ((ctx->bytes[0] = t + len) < t) {
		ctx->bytes[1]++;	/* Carry from low to high. */
	}

	t = 64 - (t & 0x3f);	/* Space available in ctx->in (at least 1). */
	if (t > len) {
		memcpy((md5byte *)ctx->in + 64 - t, buf, len);
		return;
	}
	/* First chunk is an odd size. */
	memcpy((md5byte *)ctx->in + 64 - t, buf, t);
	byteSwap(ctx->in, 16);
	MD5Transform(ctx->buf, ctx->in);
	buf += t;
	len -= t;

	/* Process data in 64-byte chunks. */
	while (len >= 64) {
		memcpy(ctx->in, buf, 64);
		byteSwap(ctx->in, 16);
		MD5Transform(ctx->buf, ctx->in);
		buf += 64;
		len -= 64;
	}

	/* Handle any remaining bytes of data. */
	memcpy(ctx->in, buf, len);
}




/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first).
 * Returns the newly allocated string of the hash.
 */
static char * MD5Final(MD5Context * ctx)
{
	char *retval;
	int count = ctx->bytes[0] & 0x3f;	/* Number of bytes in ctx->in. */
	md5byte *p = (md5byte *) ctx->in + count;
	uint8_t	*bytes;

	/* Set the first char of padding to 0x80.  There is always room. */
	*p++ = 0x80;

	/* Bytes of padding needed to make 56 bytes (-8..55). */
	count = 56 - 1 - count;

	if (count < 0) {	/* Padding forces an extra block. */
		memset(p, 0, count + 8);
		byteSwap(ctx->in, 16);
		MD5Transform(ctx->buf, ctx->in);
		p = (md5byte *)ctx->in;
		count = 56;
	}
	memset(p, 0, count);
	byteSwap(ctx->in, 14);

	/* Append length in bits and transform. */
	ctx->in[14] = ctx->bytes[0] << 3;
	ctx->in[15] = ctx->bytes[1] << 3 | ctx->bytes[0] >> 29;
	MD5Transform(ctx->buf, ctx->in);

	byteSwap(ctx->buf, 4);

	retval = (char *) malloc(33);
	bytes = (uint8_t *) ctx->buf;
	for (int i = 0; i < 16; i++) {
		sprintf(retval + (i * 2), "%02x", bytes[i]);
	}
	retval[32] = '\0';

	return retval;
}




# ifndef ASM_MD5

/* The four core functions - F1 is optimized somewhat. */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f,w,x,y,z,in,s) \
	 (w += f(x,y,z) + in, w = (w<<s | w>>(32-s)) + x)

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void MD5Transform(uint32_t buf[4], uint32_t const in[16])
{
	register uint32_t a, b, c, d;

	a = buf[0];
	b = buf[1];
	c = buf[2];
	d = buf[3];

	MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
	MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
	MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
	MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
	MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
	MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
	MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
	MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
	MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
	MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
	MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
	MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
	MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
	MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
	MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
	MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

	MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
	MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
	MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
	MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
	MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
	MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
	MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
	MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
	MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
	MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
	MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
	MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
	MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
	MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
	MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
	MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

	MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
	MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
	MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
	MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
	MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
	MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
	MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
	MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
	MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
	MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
	MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
	MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
	MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
	MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
	MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
	MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

	MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
	MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
	MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
	MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
	MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
	MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
	MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
	MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
	MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
	MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
	MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
	MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
	MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
	MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
	MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
	MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}

# endif /* ASM_MD5 */




static QString md5_hash(const char * message)
{
	MD5Context ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, (md5byte *) message, strlen(message));

	char * md5 = MD5Final(&ctx);
	const QString md5hash(md5);
	free(md5);

	return md5hash;
}
