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




#include "datasource_bfilter.h"
#include "babel.h"
#include "gpx.h"
#include "layer_trw_import.h"
#include "application_state.h"
#include "preferences.h"
#include "util.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource BFilter"




/************************************ Simplify (Count) *****************************/




#define VIK_SETTINGS_BFILTER_SIMPLIFY "bfilter_simplify"
static bool bfilter_simplify_default_set = false;
static const int32_t simplify_default_value = 100;
static SGVariant bfilter_simplify_params_defaults[] = { SGVariant(simplify_default_value, SGVariantType::Int) };




BFilterSimplify::BFilterSimplify()
{
	this->m_window_title = QObject::tr("Simplify All Tracks...");
	this->m_layer_title = QObject::tr("Simplified Tracks");
	this->m_layer_mode = TargetLayerMode::CreateNewLayer;
	this->m_autoview = true;
	this->m_keep_dialog_open_after_success = false;
	this->m_filter_type = DataSourceBabelFilter::Type::TRWLayer;

	if (!bfilter_simplify_default_set) {
		int32_t tmp;
		if (!ApplicationState::get_integer(VIK_SETTINGS_BFILTER_SIMPLIFY, &tmp)) {
			tmp = simplify_default_value;
		}

		bfilter_simplify_params_defaults[0] = SGVariant(tmp, SGVariantType::Int);
		bfilter_simplify_default_set = true;
	}
}




SGObjectTypeID BFilterSimplify::get_source_id(void) const
{
	return BFilterSimplify::source_id();
}
SGObjectTypeID BFilterSimplify::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.bfilter.simplify");
	return id;
}




int BFilterSimplify::run_config_dialog(AcquireContext & acquire_context)
{
	BFilterSimplifyDialog config_dialog(this->m_window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->m_acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->m_download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




BFilterSimplifyDialog::BFilterSimplifyDialog(const QString & window_title) : DataSourceDialog(window_title)
{
	static const ParameterScale<int> scale(1, 10000, SGVariant(simplify_default_value, SGVariantType::Int), 10, 0);

	this->grid->addWidget(new QLabel(tr("Max number of points:")), 0, 0);

	const int32_t init_val = bfilter_simplify_params_defaults[0].u.val_int;
	this->spin = new QSpinBox();
	this->spin->setMinimum(scale.min);
	this->spin->setMaximum(scale.max);
	this->spin->setSingleStep(scale.step);
	this->spin->setValue(init_val);
	this->grid->addWidget(this->spin, 0, 1);
}




AcquireOptions * BFilterSimplifyDialog::create_acquire_options(AcquireContext & acquire_context)
{
	QString layer_file_full_path;

	SaveStatus save_status = GPX::write_layer_to_tmp_file(layer_file_full_path, acquire_context.get_trw(), nullptr);
	if (SaveStatus::Code::Success != save_status) {
		qDebug() << SG_PREFIX_E << "Failed to save layer to tmp file";
		return NULL;
	}
	const int32_t value = this->spin->value();

	AcquireOptions * options = new AcquireOptions();
	options->babel_process = new BabelProcess();
	options->babel_process->set_input("gpx", layer_file_full_path);
	options->babel_process->set_filters(QString("-x simplify,count=%1").arg(value));

	/* Store for subsequent default use. */
	bfilter_simplify_params_defaults[0] = SGVariant(value, SGVariantType::Int);

	Util::add_to_deletion_list(layer_file_full_path);

	return options;
}




/**************************** Compress (Simplify by Error Factor Method) *****************************/




#define VIK_SETTINGS_BFILTER_COMPRESS "bfilter_compress"
static bool bfilter_compress_default_set = false;
static SGVariant bfilter_compress_params_defaults[] = { SGVariant(0.001) };




BFilterCompress::BFilterCompress()
{
	this->m_window_title = QObject::tr("Compress Tracks...");
	this->m_layer_title = QObject::tr("Compressed Tracks");
	this->m_layer_mode = TargetLayerMode::CreateNewLayer;
	this->m_autoview = true;
	this->m_keep_dialog_open_after_success = false;
	this->m_filter_type = DataSourceBabelFilter::Type::TRWLayer;

	if (!bfilter_compress_default_set) {
		double tmp;
		if (!ApplicationState::get_double(VIK_SETTINGS_BFILTER_COMPRESS, &tmp)) {
			tmp = 0.001;
		}

		bfilter_compress_params_defaults[0] = SGVariant(tmp);
		bfilter_compress_default_set = true;
	}
}




SGObjectTypeID BFilterCompress::get_source_id(void) const
{
	return BFilterCompress::source_id();
}
SGObjectTypeID BFilterCompress::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.bfilter.compress");
	return id;
}




int BFilterCompress::run_config_dialog(AcquireContext & acquire_context)
{
	BFilterCompressDialog config_dialog(this->m_window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->m_acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->m_download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




BFilterCompressDialog::BFilterCompressDialog(const QString & window_title) : DataSourceDialog(window_title)
{
	static const ParameterScale<double> scale(0.0, 1.000, SGVariant(0.001), 0.001, 3);

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




/* http://www.gpsbabel.org/htmldoc-development/filter_simplify.html */
AcquireOptions * BFilterCompressDialog::create_acquire_options(AcquireContext & acquire_context)
{
	QString layer_file_full_path;
	SaveStatus save_status = GPX::write_layer_to_tmp_file(layer_file_full_path, acquire_context.get_trw(), nullptr);
	if (SaveStatus::Code::Success != save_status) {
		qDebug() << SG_PREFIX_E << "Failed to save layer to tmp file";
		return NULL;
	}
	const double value = this->spin->value();

	const char units = Preferences::get_unit_distance().u == DistanceType::Unit::E::Kilometres ? 'k' : ' ';
	/* I toyed with making the length,crosstrack or relative methods selectable.
	   However several things:
	   - mainly that typical values to use for the error relate to method being used - so hard to explain and then give a default sensible value in the UI.
	   - also using relative method fails when track doesn't have HDOP info - error reported to stderr - which we don't capture ATM.
	   - options make this more complicated to use - is even that useful to be allowed to change the error value?
	   NB units not applicable if relative method used - defaults to Miles when not specified. */

	AcquireOptions * options = new AcquireOptions();
	options->babel_process = new BabelProcess();
	options->babel_process->set_input("gpx", layer_file_full_path);
	options->babel_process->set_filters(QString("-x simplify,crosstrack,error=%1%2").arg(value, -1, 'f', 5).arg(units)); /* '-1' for left-align. */

	/* Store for subsequent default use. */
	bfilter_compress_params_defaults[0] = SGVariant(value);

	Util::add_to_deletion_list(layer_file_full_path);

	return options;
}




/************************************ Duplicate Location ***********************************/




BFilterDuplicates::BFilterDuplicates()
{
	this->m_window_title = QObject::tr("Remove Duplicate Waypoints");
	this->m_layer_title = QObject::tr("Remove Duplicate Waypoints");
	this->m_layer_mode = TargetLayerMode::CreateNewLayer;
	this->m_autoview = true;
	this->m_keep_dialog_open_after_success = false;
	this->m_filter_type = DataSourceBabelFilter::Type::TRWLayer;
}




SGObjectTypeID BFilterDuplicates::get_source_id(void) const
{
	return BFilterDuplicates::source_id();
}
SGObjectTypeID BFilterDuplicates::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.bfilter.remove_duplicated_wp");
	return id;
}




int BFilterDuplicates::run_config_dialog(AcquireContext & acquire_context)
{
	BFilterDuplicatesDialog config_dialog(this->m_window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->m_acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->m_download_options = new DownloadOptions; /* With default values. */
	}

	return answer;
}




AcquireOptions * BFilterDuplicatesDialog::create_acquire_options(AcquireContext & acquire_context)
{
	QString layer_file_full_path;
	SaveStatus save_status = GPX::write_layer_to_tmp_file(layer_file_full_path, acquire_context.get_trw(), nullptr);
	if (SaveStatus::Code::Success != save_status) {
		qDebug() << SG_PREFIX_E << "Failed to save layer to tmp file";
		return NULL;
	}

	AcquireOptions * options = new AcquireOptions();
	options->babel_process = new BabelProcess();
	options->babel_process->set_input("gpx", layer_file_full_path);
	options->babel_process->set_filters("-x duplicate,location");

	Util::add_to_deletion_list(layer_file_full_path);

	return options;
}




/************************************ Swap Lat<->Lon ***********************************/




static SGVariant bfilter_manual_params_defaults[] = { SGVariant("") };




BFilterManual::BFilterManual()
{
	this->m_window_title = QObject::tr("Manual filter");
	this->m_layer_title = QObject::tr("Manual filter");
	this->m_layer_mode = TargetLayerMode::CreateNewLayer;
	this->m_autoview = true;
	this->m_keep_dialog_open_after_success = false;
	this->m_filter_type = DataSourceBabelFilter::Type::TRWLayer;
}




SGObjectTypeID BFilterManual::get_source_id(void) const
{
	return BFilterManual::source_id();
}
SGObjectTypeID BFilterManual::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.bfilter.manual");
	return id;
}




int BFilterManual::run_config_dialog(AcquireContext & acquire_context)
{
	BFilterManualDialog config_dialog(this->m_window_title);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->m_acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->m_download_options = new DownloadOptions; /* With default values. */
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




AcquireOptions * BFilterManualDialog::create_acquire_options(AcquireContext & acquire_context)
{
	QString layer_file_full_path;
	SaveStatus save_status = GPX::write_layer_to_tmp_file(layer_file_full_path, acquire_context.get_trw(), nullptr);
	if (SaveStatus::Code::Success != save_status) {
		qDebug() << SG_PREFIX_E << "Failed to save layer to tmp file";
		return NULL;
	}

	const QString value = this->entry->text();

	AcquireOptions * options = new AcquireOptions();
	options->babel_process = new BabelProcess();
	options->babel_process->set_input("gpx", layer_file_full_path);
	options->babel_process->set_filters(QString("-x %1").arg(value));

	Util::add_to_deletion_list(layer_file_full_path);

	return options;
}




/************************************ Polygon ***********************************/




BFilterPolygon::BFilterPolygon()
{
	this->m_window_title = QObject::tr("Waypoints Inside This");
	this->m_layer_title = QObject::tr("Polygonized Layer");
	this->m_layer_mode = TargetLayerMode::CreateNewLayer;
	this->m_autoview = true;
	this->m_keep_dialog_open_after_success = false;
	this->m_filter_type = DataSourceBabelFilter::Type::TRWLayerWithTrack;
}




SGObjectTypeID BFilterPolygon::get_source_id(void) const
{
	return BFilterPolygon::source_id();
}
SGObjectTypeID BFilterPolygon::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.bfilter.polygon");
	return id;
}




int BFilterPolygon::run_config_dialog(AcquireContext & acquire_context)
{
	/* There is no *real* dialog for which to call ::exec(). */

	BFilterPolygonDialog config_dialog(this->m_window_title);

	this->m_acquire_options = config_dialog.create_acquire_options(acquire_context);

	return QDialog::Accepted;
}




AcquireOptions * BFilterPolygonDialog::create_acquire_options(AcquireContext & acquire_context)
{
	QString layer_file_full_path;
	QString track_file_full_path;
	SaveStatus save_status;

	save_status = GPX::write_layer_to_tmp_file(layer_file_full_path, acquire_context.get_trw(), nullptr);
	if (SaveStatus::Code::Success != save_status) {
		qDebug() << SG_PREFIX_E << "Failed to write layer to tmp file";
		return NULL;
	}
	save_status = GPX::write_track_to_tmp_file(track_file_full_path, acquire_context.get_trk(), nullptr);
	if (SaveStatus::Code::Success != save_status) {
		qDebug() << SG_PREFIX_E << "Failed to write track to tmp file";
		return NULL;
	}


	/* FIXME: shell_escape stuff. */
	const QString command1 = QString("gpsbabel -i gpx -f %1 -o arc -F - ").arg(track_file_full_path);
	const QString command2 = QString("gpsbabel -i gpx -f %2 -x polygon,file=- -o gpx -F -").arg(layer_file_full_path);
	AcquireOptions * acquire_options = new AcquireOptions(AcquireOptions::Mode::FromShellCommand);
	acquire_options->shell_command = command1 + " | " + command2;


	Util::add_to_deletion_list(layer_file_full_path);
	Util::add_to_deletion_list(track_file_full_path);

	return acquire_options;
}




/************************************ Exclude Polygon ***********************************/




BFilterExcludePolygon::BFilterExcludePolygon()
{
	this->m_window_title = QObject::tr("Waypoints Outside This");
	this->m_layer_title = QObject::tr("Polygonzied Layer");
	this->m_layer_mode = TargetLayerMode::CreateNewLayer;
	this->m_autoview = true;
	this->m_keep_dialog_open_after_success = false;
	this->m_filter_type = DataSourceBabelFilter::Type::TRWLayerWithTrack;
}




SGObjectTypeID BFilterExcludePolygon::get_source_id(void) const
{
	return BFilterExcludePolygon::source_id();
}
SGObjectTypeID BFilterExcludePolygon::source_id(void)
{
	/* Using 'static' to ensure that a type ID will be created
	   only once for this class of objects. */
	static SGObjectTypeID id("sg.datasource.bfilter.exclude_polygon");
	return id;
}




int BFilterExcludePolygon::run_config_dialog(AcquireContext & acquire_context)
{
	/* There is no *real* dialog for which to call ::exec(). */

	BFilterExcludePolygonDialog config_dialog(this->m_window_title);

	this->m_acquire_options = config_dialog.create_acquire_options(acquire_context);

	return QDialog::Accepted;
}




AcquireOptions* BFilterExcludePolygonDialog::create_acquire_options(AcquireContext & acquire_context)
{
	QString layer_file_full_path;
	QString track_file_full_path;
	SaveStatus save_status;

	save_status = GPX::write_layer_to_tmp_file(layer_file_full_path, acquire_context.get_trw(), nullptr);
	if (SaveStatus::Code::Success != save_status) {
		qDebug() << SG_PREFIX_E << "Failed to write layer to tmp file";
	}
	save_status = GPX::write_track_to_tmp_file(track_file_full_path, acquire_context.get_trk(), nullptr);
	if (SaveStatus::Code::Success != save_status) {
		qDebug() << SG_PREFIX_E << "Failed to write track to tmp file";
	}


	/* FIXME: shell_escape stuff. */
	const QString command1 = QString("gpsbabel -i gpx -f %1 -o arc -F -").arg(track_file_full_path);
	const QString command2 = QString("gpsbabel -i gpx -f %2 -x polygon,exclude,file=- -o gpx -F -").arg(layer_file_full_path);
	AcquireOptions * options = new AcquireOptions(AcquireOptions::Mode::FromShellCommand);
	options->shell_command = command1 + "|" + command2;


	Util::add_to_deletion_list(layer_file_full_path);
	Util::add_to_deletion_list(track_file_full_path);

	return options;
}
