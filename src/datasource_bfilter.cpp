/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2014-2015, Rob Norris <rw_norris@hotmail.com>
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
 * See: http://www.gpsbabel.org/htmldoc-development/Data_Filters.html
 */




#include <cstring>
#include <cstdlib>
#include <cassert>




#include <glib.h>




#include "datasource_bfilter.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "application_state.h"
#include "preferences.h"
#include "util.h"




using namespace SlavGPS;




/************************************ Simplify (Count) *****************************/




#define VIK_SETTINGS_BFILTER_SIMPLIFY "bfilter_simplify"
static bool bfilter_simplify_default_set = false;
static SGVariant bfilter_simplify_params_defaults[] = { SGVariant((int32_t) 100) };




BFilterSimplify::BFilterSimplify()
{
	this->window_title = QObject::tr("Simplify All Tracks...");
	this->layer_title = QObject::tr("Simplified Tracks");
	this->mode = DataSourceMode::CreateNewLayer;
	this->input_type = DataSourceInputType::TRWLayer;
	this->autoview = true;
	this->keep_dialog_open = false;
	this->is_thread = true;

	if (!bfilter_simplify_default_set) {
		int32_t tmp;
		if (!ApplicationState::get_integer(VIK_SETTINGS_BFILTER_SIMPLIFY, &tmp)) {
			tmp = 100;
		}

		bfilter_simplify_params_defaults[0] = SGVariant(tmp);
		bfilter_simplify_default_set = true;
	}
}




int BFilterSimplify::run_config_dialog(AcquireProcess * acquire_context)
{
	BFilterSimplifyDialog config_dialog(this->window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->acquire_options = config_dialog.create_acquire_options_layer(acquire_context->trw);
		this->download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




BFilterSimplifyDialog::BFilterSimplifyDialog(const QString & window_title) : DataSourceDialog(window_title)
{
	static const ParameterScale<int> scale(1, 10000, SGVariant((int32_t) 100), 10, 0); /* TODO_2_LATER: verify the hardcoded default value. */

	this->grid->addWidget(new QLabel(tr("Max number of points:")), 0, 0);

	const int32_t init_val = bfilter_simplify_params_defaults[0].u.val_int;
	this->spin = new QSpinBox();
	this->spin->setMinimum(scale.min);
	this->spin->setMaximum(scale.max);
	this->spin->setSingleStep(scale.step);
	this->spin->setValue(init_val);
	this->grid->addWidget(this->spin, 0, 1);
}




BabelOptions * BFilterSimplifyDialog::get_acquire_options_layer(const QString & input_layer_filename)
{
	BabelOptions * babel_options = new BabelOptions(BabelOptionsMode::FromFile);
	const int32_t value = this->spin->value();

	babel_options->input = input_layer_filename;
	babel_options->babel_args = "-i gpx";
	babel_options->babel_filters = QString("-x simplify,count=%1").arg(value);

	/* Store for subsequent default use. */
	bfilter_simplify_params_defaults[0] = SGVariant(value);

	return babel_options;
}




/**************************** Compress (Simplify by Error Factor Method) *****************************/




#define VIK_SETTINGS_BFILTER_COMPRESS "bfilter_compress"
static bool bfilter_compress_default_set = false;
static SGVariant bfilter_compress_params_defaults[] = { SGVariant(0.001) };




BFilterCompress::BFilterCompress()
{
	this->window_title = QObject::tr("Compress Tracks...");
	this->layer_title = QObject::tr("Compressed Tracks");
	this->mode = DataSourceMode::CreateNewLayer;
	this->input_type = DataSourceInputType::TRWLayer;
	this->autoview = true;
	this->keep_dialog_open = false;
	this->is_thread = true;

	if (!bfilter_compress_default_set) {
		double tmp;
		if (!ApplicationState::get_double(VIK_SETTINGS_BFILTER_COMPRESS, &tmp)) {
			tmp = 0.001;
		}

		bfilter_compress_params_defaults[0] = SGVariant(tmp);
		bfilter_compress_default_set = true;
	}
}




int BFilterCompress::run_config_dialog(AcquireProcess * acquire_context)
{
	BFilterCompressDialog config_dialog(this->window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->acquire_options = config_dialog.create_acquire_options_layer(acquire_context->trw);
		this->download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




BFilterCompressDialog::BFilterCompressDialog(const QString & window_title) : DataSourceDialog(window_title)
{
	static const ParameterScale<double> scale(0.0, 1.000, SGVariant(0.001), 0.001, 3); /* TODO_2_LATER: verify the hardcoded default value. */

	this->grid->addWidget(new QLabel(tr("Error Factor:")), 0, 0);

	const double init_val = bfilter_compress_params_defaults[0].u.val_double;
	this->spin = new QDoubleSpinBox();
	/* Order of fields is important. Use setDecimals() before using setValue(). */
	this->spin->setDecimals(scale.n_digits);
	this->spin->setMinimum(scale.min);
	this->spin->setMaximum(scale.max);
	this->spin->setSingleStep(scale.step);
	this->spin->setValue(init_val);
	this->spin->setToolTip(tr("Specifies the maximum allowable error that may be introduced by removing a single point by the crosstrack method. See the manual or GPSBabel Simplify Filter documentation for more detail."));
	this->grid->addWidget(this->spin, 0, 1);
}




/**
   http://www.gpsbabel.org/htmldoc-development/filter_simplify.html
*/
BabelOptions * BFilterCompressDialog::get_acquire_options_layer(const QString & input_layer_filename)
{
	BabelOptions * babel_options = new BabelOptions(BabelOptionsMode::FromFile);
	const double value = this->spin->value();

	const char units = Preferences::get_unit_distance() == DistanceUnit::Kilometres ? 'k' : ' ';
	/* I toyed with making the length,crosstrack or relative methods selectable.
	   However several things:
	   - mainly that typical values to use for the error relate to method being used - so hard to explain and then give a default sensible value in the UI.
	   - also using relative method fails when track doesn't have HDOP info - error reported to stderr - which we don't capture ATM.
	   - options make this more complicated to use - is even that useful to be allowed to change the error value?
	   NB units not applicable if relative method used - defaults to Miles when not specified. */
	babel_options->input = input_layer_filename;
	babel_options->babel_args = "-i gpx";
	babel_options->babel_filters = QString("-x simplify,crosstrack,error=%1%2").arg(value, -1, 'f', 5).arg(units); /* '-1' for left-align. */

	/* Store for subsequent default use. */
	bfilter_compress_params_defaults[0] = SGVariant(value);

	return babel_options;
}




/************************************ Duplicate Location ***********************************/




BFilterDuplicates::BFilterDuplicates()
{
	this->window_title = QObject::tr("Remove Duplicate Waypoints");
	this->layer_title = QObject::tr("Remove Duplicate Waypoints");
	this->mode = DataSourceMode::CreateNewLayer;
	this->input_type = DataSourceInputType::TRWLayer;
	this->autoview = true;
	this->keep_dialog_open = false;
	this->is_thread = true;
}




int BFilterDuplicates::run_config_dialog(AcquireProcess * acquire_context)
{
	BFilterDuplicatesDialog config_dialog(this->window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->acquire_options = config_dialog.create_acquire_options_layer(acquire_context->trw);
		this->download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




BabelOptions * BFilterDuplicatesDialog::get_acquire_options_layer(const QString & input_layer_filename)
{
	BabelOptions * babel_options = new BabelOptions(BabelOptionsMode::FromFile);

	babel_options->input = input_layer_filename;
	babel_options->babel_args = "-i gpx";
	babel_options->babel_filters = QString("-x duplicate,location");

	return babel_options;
}




/************************************ Swap Lat<->Lon ***********************************/




static SGVariant bfilter_manual_params_defaults[] = { SGVariant("") };




BFilterManual::BFilterManual()
{
	this->window_title = QObject::tr("Manual filter");
	this->layer_title = QObject::tr("Manual filter");
	this->mode = DataSourceMode::CreateNewLayer;
	this->input_type = DataSourceInputType::TRWLayer;
	this->autoview = true;
	this->keep_dialog_open = false;
	this->is_thread = true;
}




int BFilterManual::run_config_dialog(AcquireProcess * acquire_context)
{
	BFilterManualDialog config_dialog(this->window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->acquire_options = config_dialog.create_acquire_options_layer(acquire_context->trw);
		this->download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




BFilterManualDialog::BFilterManualDialog(const QString & window_title) : DataSourceDialog(window_title)
{
	this->grid->addWidget(new QLabel(tr("Manual filter:")), 0, 0);

	this->entry = new QLineEdit(bfilter_manual_params_defaults[0].to_string());
	this->entry->setToolTip(tr("Manual filter command: e.g. 'swap'."));
	this->grid->addWidget(this->entry, 0, 1);
}




BabelOptions * BFilterManualDialog::get_acquire_options_layer(const QString & input_layer_filename)
{
	BabelOptions * babel_options = new BabelOptions(BabelOptionsMode::FromFile);
	const QString value = this->entry->text();

	babel_options->input = input_layer_filename;
	babel_options->babel_args = "-i gpx";
	babel_options->babel_filters = QString("-x %1").arg(value);

	return babel_options;
}




/************************************ Polygon ***********************************/




BFilterPolygon::BFilterPolygon()
{
	this->window_title = QObject::tr("Waypoints Inside This");
	this->layer_title = QObject::tr("Polygonized Layer");
	this->mode = DataSourceMode::CreateNewLayer;
	this->input_type = DataSourceInputType::TRWLayerTrack;
	this->autoview = true;
	this->keep_dialog_open = false;
	this->is_thread = true;
}




/* FIXME: shell_escape stuff. */
BabelOptions * BFilterPolygonDialog::get_acquire_options_layer_track(const QString & layer_input_file_full_path, const QString & track_input_file_full_path)
{
	BabelOptions * babel_options = new BabelOptions(BabelOptionsMode::FromShellCommand);

	const QString command1 = QString("gpsbabel -i gpx -f %1 -o arc -F - ").arg(track_input_file_full_path);
	const QString command2 = QString("gpsbabel -i gpx -f %2 -x polygon,file=- -o gpx -F -").arg(layer_input_file_full_path);

	babel_options->shell_command = command1 + " | " + command2;

	return babel_options;
}




int BFilterPolygon::run_config_dialog(AcquireProcess * acquire_context)
{
	/* There is no *real* dialog for which to call ::exec(). */

	DataSourceDialog config_dialog(this->window_title);
	this->acquire_options = config_dialog.create_acquire_options_layer_track(acquire_context->trw, acquire_context->trk);

	return QDialog::Accepted;
}




/************************************ Exclude Polygon ***********************************/




BFilterExcludePolygon::BFilterExcludePolygon()
{
	this->window_title = QObject::tr("Waypoints Outside This");
	this->layer_title = QObject::tr("Polygonzied Layer");
	this->mode = DataSourceMode::CreateNewLayer;
	this->input_type = DataSourceInputType::TRWLayerTrack;
	this->autoview = true;
	this->keep_dialog_open = false;
	this->is_thread = true;
}





/* FIXME: shell_escape stuff */
BabelOptions * BFilterExcludePolygonDialog::get_acquire_options_layer_track(const QString & layer_input_file_full_path, const QString & track_input_file_full_path)
{
	BabelOptions * babel_options = new BabelOptions(BabelOptionsMode::FromShellCommand);
	babel_options->shell_command = QString("gpsbabel -i gpx -f %1 -o arc -F - | gpsbabel -i gpx -f %2 -x polygon,exclude,file=- -o gpx -F -").arg(track_input_file_full_path).arg(layer_input_file_full_path);

	return babel_options;
}



int BFilterExcludePolygon::run_config_dialog(AcquireProcess * acquire_context)
{
	/* There is no *real* dialog for which to call ::exec(). */

	DataSourceDialog config_dialog(this->window_title);
	this->acquire_options = config_dialog.create_acquire_options_layer_track(acquire_context->trw, acquire_context->trk);

	return QDialog::Accepted;
}
