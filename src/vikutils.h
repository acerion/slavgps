/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
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

#ifndef _SG_UTILS_H_
#define _SG_UTILS_H_




#include <cmath> /* NAN */




#include <QApplication>
#include <QString>
#include <QFile>
#include <QTemporaryFile>
#include <QAbstractButton>
#include <QColor>
#include <QPen>
#include <QDateTime>




#include "layer_map_source.h"
#include "coord.h"




#define TEST_BOOLEAN(str) (! ((str)[0] == '\0' || (str)[0] == '0' || (str)[0] == 'n' || (str)[0] == 'N' || (str)[0] == 'f' || (str)[0] == 'F'))




namespace SlavGPS {




	class Window;
	class GisViewport;
	class Layer;
	class LayerTRW;
	class Track;
	class Trackpoint;




	class CommandLineOptions {
	public:
		/**
		   Parse command line options passed to application. Save results.
		   @return false on errors
		   @return true otherwise
		*/
		bool parse(QCoreApplication & app);

		/**
		   Apply any startup values that have been specified from the command line.
		   Values are defaulted in such a manner not to be applied when they haven't been specified.
		*/
		void apply(Window * window);


		/* Default values that won't actually get applied unless changed by command line parameter values. */
		bool debug = false;
		bool verbose = false;
		bool version = false;

		LatLon lat_lon;
		int zoom_level_osm = -1;
		MapTypeID map_type_id = MapTypeID::Initial;

		QStringList files;
	};




	QString vu_trackpoint_formatted_message(const QString & format_code, Trackpoint * tp, Trackpoint * tp_prev, Track * trk, double climb);

	QString vu_get_canonical_filename(Layer * layer, const QString & path, const QString & reference_file_full_path);




	QString file_base_name(const QString & full_path);




	class TZLookup {
	public:
		static void init();
		static void uninit();
		static const QTimeZone * get_tz_at_location(const Coord & coord);
	private:
		static int load_from_dir(const QString & dir);
	};




	class SGUtils {
	public:
		static bool is_very_first_run(void);
		static void set_auto_features_on_first_run(void);

		static bool create_temporary_file(QTemporaryFile & file, const QString & name_pattern);
		static void copy_label_menu(QAbstractButton * button);



		static void color_to_string(char * buffer, size_t buffer_size, const QColor & color);
		static QPen new_pen(const QColor & color, int width);

		static QString get_canonical_path(const QString & path);

		/* Convert contents of QString representing a double value in C locale into double.
		   Return std::nan on errors. */
		static double c_to_double(const QString & string);

		/*
		  Convert double value into a string representation in
		  C locale.  Returns empty string if double value is
		  std::nan or if conversion fails.

		  In some context using dot as a decimal separator is
		  essential.  E.g in GPX specifications, decimal
		  values are xsd:decimal, so they must use the period
		  separator, not the localized one.
		*/
		static QString double_to_c(double d, int precision = 6);

		/**
		   Convert a geo coordinate @coord of item visible in
		   given @viewport to x/y coordinate on screen.
		*/
		static ScreenPos coord_to_point(const Coord & coord, const GisViewport * gisview);


		/* Generate an integer suitable for comparison of version numbers. */
		static int version_to_number(const QString & version);
	};
	/* Convert contents of QString representing a double value in C locale into double.
	   Return std::nan on errors.

	   Copy of SGUtils::c_to_double(), kept outside of class to
	   avoid header dependency problems.
	*/
	double c_to_double(const QString & string);




	/* Comparison class for std::maps with QString as a key. */
	struct qstring_compare {
		bool operator() (const QString & string1, const QString & string2) const { return string1 < string2; }
	};




}




#endif
