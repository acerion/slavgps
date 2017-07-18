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
 *
 */

/*
 * Large (and important) sections of this file were adapted from
 * ROX-Filer source code, Copyright (C) 2003, the ROX-Filer team,
 * originally licensed under the GPL v2 or greater (as above).
 *
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

#include <glib.h>
#include <glib/gstdio.h>
typedef int GdkPixdata; /* TODO: remove sooner or later. */
#include "thumbnails.h"
#include "file.h"
#include "icons/icons.h"
#include "globals.h"




using namespace SlavGPS;




#ifdef __CYGWIN__
#ifdef __CYGWIN_USE_BIG_TYPES__
#define ST_SIZE_FMT "%lld"
#else
#define ST_SIZE_FMT "%ld"
#endif
#else
/* FIXME -- on some systems this may need to me "lld", see ROX-Filer code */
#define ST_SIZE_FMT "%ld"
#endif

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

#define PIXMAP_THUMB_SIZE  128




static char *md5_hash(const char * message);
static QPixmap *save_thumbnail(const char * pathname, QPixmap * full);
static QPixmap *child_create_thumbnail(const char * path);




bool a_thumbnails_exists(const char * filename)
{
	QPixmap * pixmap = a_thumbnails_get(filename);
	if (pixmap) {
#ifdef K
		g_object_unref(G_OBJECT (pixmap));
#endif
		return true;
	}
	return false;
}




QPixmap * a_thumbnails_get_default()
{
	QPixmap * pixmap = NULL;
#ifdef K
	pixmap = return gdk_pixbuf_from_pixdata(&thumbnails_pixmap, false, NULL);
#endif
	return pixmap;
}




/* Filename must be absolute. you could have a function to make sure it exists and absolutize it. */
void a_thumbnails_create(const char * filename)
{
	QPixmap * pixmap = a_thumbnails_get(filename);

	if (!pixmap) {
		pixmap = child_create_thumbnail(filename);
	}

	if (pixmap) {
#ifdef K
		g_object_unref(G_OBJECT (pixmap));
#endif
	}
}




QPixmap * a_thumbnails_scale_pixmap(QPixmap * src, int max_w, int max_h)
{
	int w = src->width();
	int h = src->height();

#ifdef K
	if (w <= max_w && h <= max_h) {
		g_object_ref(G_OBJECT (src));
		return src;
	} else {
		float scale_x = ((float) w) / max_w;
		float scale_y = ((float) h) / max_h;
		float scale = MAX(scale_x, scale_y);
		int dest_w = w / scale;
		int dest_h = h / scale;

		return src->scaled(MAX(dest_w, 1), MAX(dest_h, 1), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
#else
	return NULL;
#endif
}




static QPixmap * child_create_thumbnail(const char * path)
{
	QPixmap * image = new QPixmap();
	if (!image->load(path)) {
		delete image;
		image = NULL;
		return NULL;
	}
#ifdef K

	QPixmap * tmpbuf = gdk_pixbuf_apply_embedded_orientation(image);
	g_object_unref(G_OBJECT(image));
	image = tmpbuf;

	if (image) {
		QPixmap * thumb = save_thumbnail(path, image);
		g_object_unref(G_OBJECT (image));
		return thumb;
	}
#endif
	return NULL;
}




static QPixmap * save_thumbnail(const char * pathname, QPixmap * full)
{
	struct stat info;
	if (stat(pathname, &info) != 0) {
		return NULL;
	}

	QPixmap * thumb = a_thumbnails_scale_pixmap(full, PIXMAP_THUMB_SIZE, PIXMAP_THUMB_SIZE);
#ifdef K
	const char * orientation = gdk_pixbuf_get_option(full, "orientation");

	int original_width = full->width();
	int original_height = full->height();


	char * swidth = g_strdup_printf("%d", original_width);
	char * sheight = g_strdup_printf("%d", original_height);
	char * ssize = g_strdup_printf(ST_SIZE_FMT, info.st_size);
	char * smtime = g_strdup_printf("%ld", (long) info.st_mtime);

	char * path = file_realpath_dup(pathname);
	char * uri = g_strconcat("file://", path, NULL);
	char * md5 = md5_hash(uri);
	free(path);

	GString * to = g_string_new(HOME_DIR);
	g_string_append(to, THUMB_DIR);
	g_string_append(to, THUMB_SUB_DIR);
	if (g_mkdir_with_parents(to->str, 0700) != 0) {
		fprintf(stderr, "WARNING: %s: Failed to mkdir %s\n", __FUNCTION__, to->str);
	}

	g_string_append(to, md5);
	int name_len = to->len + 4; /* Truncate to this length when renaming. */
#ifdef WINDOWS
	g_string_append_printf(to, ".png.Viking");
#else
	g_string_append_printf(to, ".png.Viking-%ld", (long) getpid());
#endif

	free(md5);

	/* Thumb::URI must be in ISO-8859-1 encoding otherwise gdk_pixbuf_save() will fail
	   - e.g. if characters such as 'ě' are encountered.
	   Also see http://en.wikipedia.org/wiki/ISO/IEC_8859-1.
	   ATM GLIB Manual doesn't specify in which version this function became available,
	   find out that it's fairly recent so may break builds without this test. */
#if GLIB_CHECK_VERSION(2,40,0)
	char *thumb_uri = g_str_to_ascii (uri, NULL);
#else
	char *thumb_uri = g_strdup(uri);
#endif
	mode_t old_mask = umask(0077);
	GError *error = NULL;
	gdk_pixbuf_save(thumb, to->str, "png", &error,
	                "tEXt::Thumb::Image::Width", swidth,
	                "tEXt::Thumb::Image::Height", sheight,
	                "tEXt::Thumb::Size", ssize,
	                "tEXt::Thumb::MTime", smtime,
	                "tEXt::Thumb::URI", thumb_uri,
	                "tEXt::Software", PROJECT,
	                "tEXt::Software::Orientation", orientation ? orientation : "0",
	                NULL);
	umask(old_mask);
	free(thumb_uri);

	if (error) {
		fprintf(stderr, "WARNING: %s::%s\n", __FUNCTION__, error->message);
		g_error_free(error);
		g_object_unref(G_OBJECT(thumb));
		thumb = NULL; /* return NULL */
	} else {
		/* We create the file ###.png.Viking-PID and rename it to avoid
		 * a race condition if two programs create the same thumb at
		 * once.
		 */
		char *final;

		final = g_strndup(to->str, name_len);
		if (rename(to->str, final)) {
			int e = errno;
			fprintf(stderr, "WARNING: Failed to rename '%s' to '%s': %s\n", to->str, final, strerror(e));
			g_object_unref(G_OBJECT(thumb));
			thumb = NULL; /* Return NULL */
		}

		free(final);
	}

	g_string_free(to, true);
	free(swidth);
	free(sheight);
	free(ssize);
	free(smtime);
	free(uri);
#endif
	return thumb;
}




QPixmap * SlavGPS::a_thumbnails_get(const char * pathname)
{
	char * path = file_realpath_dup(pathname);
	char * uri = g_strconcat("file://", path, NULL);
	char * md5 = md5_hash(uri);
	free(uri);

	char * thumb_path = g_strdup_printf("%s%s%s%s.png", HOME_DIR, THUMB_DIR, THUMB_SUB_DIR, md5);

	free(md5);

	const char * ssize;
	const char * smtime;


	QPixmap * thumb = new QPixmap();
#ifdef K
	if (!thumb->load(thumb_path)) {
		goto err;
	}

	/* Note that these don't need freeing... */
	ssize = gdk_pixbuf_get_option(thumb, "tEXt::Thumb::Size");
	if (!ssize) {
		goto err;
	}

	smtime = gdk_pixbuf_get_option(thumb, "tEXt::Thumb::MTime");
	if (!smtime) {
		goto err;
	}

	struct stat info;
	if (stat(path, &info) != 0) {
		goto err;
	}

	if (info.st_mtime != atol(smtime) || info.st_size != atol(ssize)) {
		goto err;
	}

	goto out;
err:
	if (thumb) {
		g_object_unref(G_OBJECT (thumb));
	}
	thumb = NULL;
out:
	free(path);
	free(thumb_path);
#endif
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




static char * md5_hash(const char * message)
{
	MD5Context ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, (md5byte *) message, strlen(message));
	return MD5Final(&ctx);
}
