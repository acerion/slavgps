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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cstdlib>

#include "datasource_bfilter.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "application_state.h"
#include "globals.h"
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
	this->mode = DataSourceMode::CREATE_NEW_LAYER;
	this->inputtype = DatasourceInputtype::TRWLAYER;
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




DataSourceDialog * BFilterSimplify::create_setup_dialog(Viewport * viewport, void * user_data)
{
	DataSourceDialog * dialog = new BFilterSimplifyDialog();
	dialog->setWindowTitle(this->window_title);
	return dialog;
}




BFilterSimplifyDialog::BFilterSimplifyDialog()
{
	static const ParameterScale scale = { 1, 10000, SGVariant((int32_t) 100), 10, 0 }; /* TODO: verify the hardcoded default value. */

	this->grid->addWidget(new QLabel(tr("Max number of points:")), 0, 0);

	const int32_t init_val = bfilter_simplify_params_defaults[0].val_int;
	this->spin = new QSpinBox();
	this->spin->setMinimum(scale.min);
	this->spin->setMaximum(scale.max);
	this->spin->setSingleStep(scale.step);
	this->spin->setValue(init_val);
	this->grid->addWidget(this->spin, 0, 1);
}




/* FIXME: caller of the function may pass value of first argument with type uint32_t, but the function expects int32_t. */
ProcessOptions * BFilterSimplifyDialog::get_process_options(const QString & input_filename, const QString & not_used)
{
	ProcessOptions * po = new ProcessOptions();
	const int32_t value = this->spin->value();

	po->babel_args = "-i gpx";
	po->input_file_name = input_filename;
	po->babel_filters = QString("-x simplify,count=%1").arg(value);

	/* Store for subsequent default use. */
	bfilter_simplify_params_defaults[0] = SGVariant(value);

	return po;
}




/**************************** Compress (Simplify by Error Factor Method) *****************************/




#define VIK_SETTINGS_BFILTER_COMPRESS "bfilter_compress"
static bool bfilter_compress_default_set = false;
static SGVariant bfilter_compress_params_defaults[] = { SGVariant(0.001) };




BFilterCompress::BFilterCompress()
{
	this->window_title = QObject::tr("Compress Tracks...");
	this->layer_title = QObject::tr("Compressed Tracks");
	this->mode = DataSourceMode::CREATE_NEW_LAYER;
	this->inputtype = DatasourceInputtype::TRWLAYER;
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




DataSourceDialog * BFilterCompress::create_setup_dialog(Viewport * viewport, void * user_data)
{
	DataSourceDialog * dialog = new BFilterCompressDialog();
	dialog->setWindowTitle(this->window_title);
	return dialog;
}




BFilterCompressDialog::BFilterCompressDialog()
{
	static const ParameterScale scale = { 0.0, 1.000, SGVariant(0.001), 0.001, 3 }; /* TODO: verify the hardcoded default value. */

	this->grid->addWidget(new QLabel(tr("Error Factor:")), 0, 0);

	const double init_val = bfilter_compress_params_defaults[0].val_double;
	this->spin = new QDoubleSpinBox();
	/* Order of fields is important. Use setDecimals() before using setValue(). */
	this->spin->setDecimals(scale.digits);
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
ProcessOptions * BFilterCompressDialog::get_process_options(const QString & input_filename, const QString & not_used)
{
	ProcessOptions * po = new ProcessOptions();
	const double value = this->spin->value();

	char units = Preferences::get_unit_distance() == DistanceUnit::KILOMETRES ? 'k' : ' ';
	/* I toyed with making the length,crosstrack or relative methods selectable.
	   However several things:
	   - mainly that typical values to use for the error relate to method being used - so hard to explain and then give a default sensible value in the UI.
	   - also using relative method fails when track doesn't have HDOP info - error reported to stderr - which we don't capture ATM.
	   - options make this more complicated to use - is even that useful to be allowed to change the error value?
	   NB units not applicable if relative method used - defaults to Miles when not specified. */
	po->babel_args = "-i gpx";
	po->input_file_name = input_filename;
	po->babel_filters = QString(g_strdup_printf("-x simplify,crosstrack,error=%-.5f%c", value, units)); /* FIXME: memory leak. */

	/* Store for subsequent default use. */
	bfilter_compress_params_defaults[0] = SGVariant(value);

	return po;
}




/************************************ Duplicate Location ***********************************/




BFilterDuplicates::BFilterDuplicates()
{
	this->window_title = QObject::tr("Remove Duplicate Waypoints");
	this->layer_title = QObject::tr("Remove Duplicate Waypoints");
	this->mode = DataSourceMode::CREATE_NEW_LAYER;
	this->inputtype = DatasourceInputtype::TRWLAYER;
	this->autoview = true;
	this->keep_dialog_open = false;
	this->is_thread = true;
}




DataSourceDialog * BFilterDuplicates::create_setup_dialog(Viewport * viewport, void * user_data)
{
	return new BFilterDuplicatesDialog();
}




ProcessOptions * BFilterDuplicatesDialog::get_process_options(const QString & input_filename, const QString & not_used)
{
	ProcessOptions * po = new ProcessOptions();

	po->babel_args = "-i gpx";
	po->input_file_name = input_filename;
	po->babel_filters = QString("-x duplicate,location");

	return po;
}




/************************************ Swap Lat<->Lon ***********************************/




static SGVariant bfilter_manual_params_defaults[] = { SGVariant("") };




BFilterManual::BFilterManual()
{
	this->window_title = QObject::tr("Manual filter");
	this->layer_title = QObject::tr("Manual filter");
	this->mode = DataSourceMode::CREATE_NEW_LAYER;
	this->inputtype = DatasourceInputtype::TRWLAYER;
	this->autoview = true;
	this->keep_dialog_open = false;
	this->is_thread = true;
}




DataSourceDialog * BFilterManual::create_setup_dialog(Viewport * viewport, void * user_data)
{
	DataSourceDialog * dialog = new BFilterManualDialog();
	dialog->setWindowTitle(this->window_title);
	return dialog;
}




BFilterManualDialog::BFilterManualDialog()
{
	this->grid->addWidget(new QLabel(tr("Manual filter:")), 0, 0);

	this->entry = new QLineEdit(bfilter_manual_params_defaults[0].to_string());
	this->entry->setToolTip(tr("Manual filter command: e.g. 'swap'."));
	this->grid->addWidget(this->entry, 0, 1);
}




ProcessOptions * BFilterManualDialog::get_process_options(const QString & input_filename, const QString & not_used)
{
	ProcessOptions * po = new ProcessOptions();
	const QString value = this->entry->text();

	po->babel_args = "-i gpx";
	po->input_file_name = input_filename;
	po->babel_filters = QString("-x %1").arg(value);

	return po;
}




/************************************ Polygon ***********************************/




BFilterPolygon::BFilterPolygon()
{
	this->window_title = QObject::tr("Waypoints Inside This");
	this->layer_title = QObject::tr("Polygonized Layer");
	this->mode = DataSourceMode::CREATE_NEW_LAYER;
	this->inputtype = DatasourceInputtype::TRWLAYER_TRACK;
	this->autoview = true;
	this->keep_dialog_open = false;
	this->is_thread = true;
}




/* TODO: shell_escape stuff. */
ProcessOptions * BFilterPolygonDialog::get_process_options(const QString & input_filename, const QString & input_track_filename)
{
	ProcessOptions * po = new ProcessOptions();

	po->shell_command = g_strdup_printf("gpsbabel -i gpx -f %s -o arc -F - | gpsbabel -i gpx -f %s -x polygon,file=- -o gpx -F -", input_track_filename.toUtf8().constData(), input_filename.toUtf8().constData());

	return po;
}




/************************************ Exclude Polygon ***********************************/




BFilterExcludePolygon::BFilterExcludePolygon()
{
	this->window_title = QObject::tr("Waypoints Outside This");
	this->layer_title = QObject::tr("Polygonzied Layer");
	this->mode = DataSourceMode::CREATE_NEW_LAYER;
	this->inputtype = DatasourceInputtype::TRWLAYER_TRACK;
	this->autoview = true;
	this->keep_dialog_open = false;
	this->is_thread = true;
}




/* TODO: shell_escape stuff */
ProcessOptions * BFilterExcludePolygonDialog::get_process_options(const QString & input_filename, const QString & input_track_filename)
{
	ProcessOptions * po = new ProcessOptions();
	po->shell_command = g_strdup_printf("gpsbabel -i gpx -f %s -o arc -F - | gpsbabel -i gpx -f %s -x polygon,exclude,file=- -o gpx -F -", input_track_filename.toUtf8().constData(), input_filename.toUtf8().constData());

	return po;
}
