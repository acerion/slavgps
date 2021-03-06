/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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




#include <cmath>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QDebug>
#include <QDir>
#include <QFileDialog>




#include "window.h"
#include "viewport_internal.h"
#include "viewport_to_image.h"
#include "preferences.h"
#include "util.h"
#include "application_state.h"
#include "statusbar.h"
#include "layers_panel.h"
#include "kmz.h"
#include "widget_radio_group.h"




using namespace SlavGPS;




#define SG_MODULE "Viewport To Image"

#define VIK_SETTINGS_VIEWPORT_SAVE_WIDTH     "window_save_image_width"
#define VIK_SETTINGS_VIEWPORT_SAVE_HEIGHT    "window_save_image_height"
#define VIK_SETTINGS_VIEWPORT_SAVE_FORMAT    "window_viewport_save_format"

#define VIEWPORT_SAVE_DEFAULT_WIDTH    1280
#define VIEWPORT_SAVE_DEFAULT_HEIGHT   1024
#define VIEWPORT_SAVE_DEFAULT_FORMAT   ViewportToImage::FileFormat::PNG




/* The last used directory for saving viewport to image(s). */
static QUrl g_last_folder_images_url;




/**
   @reviewed-on tbd
*/
void ViewportSaveDialog::get_size_from_viewport_cb(void) /* Slot */
{
	/* Temporarily block signals sent by spinboxes. */
	this->width_spin->blockSignals(true);
	this->height_spin->blockSignals(true);

	this->width_spin->setValue(this->original_total_width);
	this->height_spin->setValue(this->original_total_height);

	this->width_spin->blockSignals(false);
	this->height_spin->blockSignals(false);

	return;
}




/**
   @reviewed-on tbd
*/
void ViewportSaveDialog::calculate_total_area_cb(void)
{
	QString label_text;
	double width = this->width_spin->value() * this->original_viking_scale.get_x();
	double height = this->height_spin->value() * this->original_viking_scale.get_y();
	if (this->tiles_width_spin) { /* save many images; find TOTAL area covered */
		width *= this->tiles_width_spin->value();
		height *= this->tiles_height_spin->value();
	}
	const DistanceType::Unit distance_unit = Preferences::get_unit_distance();
	switch (distance_unit.u) {
	case DistanceType::Unit::E::Kilometres:
		label_text = tr("Total area: %1m x %2m (%3 sq. km)").arg((long) width).arg((long) height).arg((width * height / 1000000), 0, 'f', 3); /* "%.3f" */
		break;
	case DistanceType::Unit::E::Miles:
		label_text = tr("Total area: %1m x %2m (%3 sq. miles)").arg((long) width).arg((long) height).arg((width * height / 2589988.11), 0, 'f', 3); /* "%.3f" */
		break;
	case DistanceType::Unit::E::NauticalMiles:
		label_text = tr("Total area: %1m x %2m (%3 sq. NM)").arg((long) width).arg((long) height).arg((width * height / (1852.0 * 1852.0)), 0, 'f', 3); /* "%.3f" */
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected distance unit" << distance_unit;
		break;
	}

	this->total_area_label->setText(label_text);
}




/**
   @reviewed-on tbd
*/
ViewportSaveDialog::ViewportSaveDialog(QString const & title, GisViewport * new_gisview, QWidget * parent) : BasicDialog(parent)
{
	this->setWindowTitle(title);
	this->gisview = new_gisview;

	this->original_total_width = this->gisview->total_get_width();
	this->original_total_height = this->gisview->total_get_height();
	this->original_viking_scale = this->gisview->get_viking_scale();
	this->original_proportion = 1.0 * this->original_total_width / this->original_total_height;
}




/**
   @reviewed-on tbd
*/
ViewportSaveDialog::~ViewportSaveDialog()
{
}




/**
   @reviewed-on tbd
*/
void ViewportSaveDialog::accept_cb(void) /* Slot. */
{
	this->accept();
}




/**
   @reviewed-on tbd
*/
void ViewportSaveDialog::build_ui(ViewportToImage::SaveMode save_mode, ViewportToImage::FileFormat file_format)
{
	int row = 0;


	this->grid->addWidget(new QLabel(tr("Width (pixels):")), row, 0);

	this->width_spin = new QSpinBox();
	this->width_spin->setMinimum(1);
	this->width_spin->setMaximum(10 * 1024);
	this->width_spin->setSingleStep(1);
	this->width_spin->setToolTip(tr("Total width of saved image"));
	this->grid->addWidget(this->width_spin, row, 1);
	row++;



	this->grid->addWidget(new QLabel(tr("Height (pixels):")), row, 0);

	this->height_spin = new QSpinBox();
	this->height_spin->setMinimum(1);
	this->height_spin->setMaximum(10 * 1024);
	this->height_spin->setSingleStep(1);
	this->width_spin->setToolTip(tr("Total height of saved image"));
	this->grid->addWidget(this->height_spin, row, 1);
	row++;



	/* Right below width/height spinboxes. */
	QPushButton * use_current_area_button = new QPushButton(tr("Copy size from Viewport"));
	this->grid->addWidget(use_current_area_button, row, 1);
	connect(use_current_area_button, SIGNAL (clicked()), this, SLOT (get_size_from_viewport_cb()));
	row++;



	this->total_area_label = new QLabel(tr("Total Area"));
	this->grid->addWidget(this->total_area_label, row, 0, 1, 2);
	row++;



	WidgetIntEnumerationData file_format_items;
	if (save_mode == ViewportToImage::SaveMode::FileKMZ) {
		/* Only one file format. */
		file_format_items.values.push_back(SGLabelID(tr("Save as JPEG"), (int) ViewportToImage::FileFormat::JPEG));
	} else {
		file_format_items.values.push_back(SGLabelID(tr("Save as PNG"), (int) ViewportToImage::FileFormat::PNG));
		file_format_items.values.push_back(SGLabelID(tr("Save as JPEG"), (int) ViewportToImage::FileFormat::JPEG));
	}
	file_format_items.default_id = (int) file_format;
	this->output_format_radios = new RadioGroupWidget(tr("Output format"), file_format_items, this);


	this->grid->addWidget(this->output_format_radios, row, 0, 1, 2);
	row++;



	if (save_mode == ViewportToImage::SaveMode::Directory) {

		this->grid->addWidget(new QLabel(tr("East-west image tiles:")), row, 0);

		this->tiles_width_spin = new QSpinBox();
		this->tiles_width_spin->setRange(1, 10);
		this->tiles_width_spin->setSingleStep(1);
		this->tiles_width_spin->setValue(5);
		this->grid->addWidget(this->tiles_width_spin, row, 1);
		row++;



		this->grid->addWidget(new QLabel(tr("North-south image tiles:")), row, 0);

		this->tiles_height_spin = new QSpinBox();
		this->tiles_height_spin->setRange(1, 10);
		this->tiles_height_spin->setSingleStep(1);
		this->tiles_height_spin->setValue(5);
		this->grid->addWidget(this->tiles_height_spin, row, 1);
		row++;

		connect(this->tiles_width_spin, SIGNAL(valueChanged(int)), this, SLOT(calculate_total_area_cb()));
		connect(this->tiles_height_spin, SIGNAL(valueChanged(int)), this, SLOT(calculate_total_area_cb()));
	}


	connect(this->width_spin, SIGNAL (valueChanged(int)), this, SLOT (calculate_total_area_cb()));
	connect(this->height_spin, SIGNAL (valueChanged(int)), this, SLOT (calculate_total_area_cb()));
	connect(this->width_spin, SIGNAL (valueChanged(int)), this, SLOT (handle_changed_width_cb(int)));
	connect(this->height_spin, SIGNAL (valueChanged(int)), this, SLOT (handle_changed_height_cb(int)));
	connect(this->button_box, &QDialogButtonBox::accepted, this, &ViewportSaveDialog::accept_cb);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	/* Set initial size info now. */
	this->get_size_from_viewport_cb();
	this->calculate_total_area_cb();

	//this->accept();
}




/**
   @reviewed-on tbd
*/
void ViewportSaveDialog::get_scaled_parameters(int & width, int & height, VikingScale & viking_scale) const
{
	width = this->width_spin->value();
	height = this->height_spin->value();

	const double scale = 1.0 * width / this->original_total_width;
	viking_scale = this->original_viking_scale * scale;

	qDebug() << SG_PREFIX_I << "Returning width" << width << "height" << height << "viking scale" << viking_scale << "scale" << scale;
	return;
}




/**
   @reviewed-on tbd
*/
ViewportToImage::FileFormat ViewportSaveDialog::get_image_format(void) const
{
	return (ViewportToImage::FileFormat) this->output_format_radios->get_selected_id();
}




/**
   @reviewed-on tbd
*/
void ViewportSaveDialog::handle_changed_width_cb(int w)
{
	/* proportion = w/h */
	this->height_spin->blockSignals(true); /* Temporarily block signals sent by this object. */
	this->height_spin->setValue(w / this->original_proportion);
	this->height_spin->blockSignals(false);
	qDebug() << SG_PREFIX_I << "Set new height" << (w / this->original_proportion);
}




/**
   @reviewed-on tbd
*/
void ViewportSaveDialog::handle_changed_height_cb(int h)
{
	/* proportion = w/h */
	this->width_spin->blockSignals(true); /* Temporarily block signals sent by this object. */
	this->width_spin->setValue(h * this->original_proportion);
	this->width_spin->blockSignals(false);
	qDebug() << SG_PREFIX_I << "Set new width" << (h * this->original_proportion);
}




/**
   @reviewed-on tbd
*/
int ViewportSaveDialog::get_n_tiles_x(void) const
{
	return this->tiles_width_spin->value();
}




/**
   @reviewed-on tbd
*/
int ViewportSaveDialog::get_n_tiles_y(void) const
{
	return this->tiles_height_spin->value();
}




/**
   @reviewed-on tbd
*/
ViewportToImage::ViewportToImage(GisViewport * new_gisview, ViewportToImage::SaveMode new_save_mode, Window * new_window)
{
	this->gisview = new_gisview;
	this->save_mode = new_save_mode;
	this->window = new_window;
	this->original_viking_scale = this->gisview->get_viking_scale();
	this->scaled_viking_scale = this->gisview->get_viking_scale(); /* This value will be re-calculated by function called in ::run_config_dialog((), */

	if (!ApplicationState::get_integer(VIK_SETTINGS_VIEWPORT_SAVE_WIDTH, &this->scaled_total_width)) {
		this->scaled_total_width = VIEWPORT_SAVE_DEFAULT_WIDTH;
	}

	if (!ApplicationState::get_integer(VIK_SETTINGS_VIEWPORT_SAVE_HEIGHT, &this->scaled_total_height)) {
		this->scaled_total_height = VIEWPORT_SAVE_DEFAULT_HEIGHT;
	}

	if (!ApplicationState::get_integer(VIK_SETTINGS_VIEWPORT_SAVE_FORMAT, (int *) &this->file_format)) {
		this->file_format = VIEWPORT_SAVE_DEFAULT_FORMAT;
	}
}




/**
   @reviewed-on tbd
*/
ViewportToImage::~ViewportToImage()
{
	ApplicationState::set_integer(VIK_SETTINGS_VIEWPORT_SAVE_WIDTH, this->scaled_total_width);
	ApplicationState::set_integer(VIK_SETTINGS_VIEWPORT_SAVE_HEIGHT, this->scaled_total_height);
	ApplicationState::set_integer(VIK_SETTINGS_VIEWPORT_SAVE_FORMAT, (int) this->file_format);
}




/**
   @reviewed-on tbd
*/
bool ViewportToImage::run_config_dialog(const QString & title)
{
	ViewportSaveDialog dialog(title, this->gisview, NULL);

	dialog.build_ui(this->save_mode, this->file_format);
	if (QDialog::Accepted != dialog.exec()) {
		return false;
	}

	dialog.get_scaled_parameters(this->scaled_total_width, this->scaled_total_height, this->scaled_viking_scale);
	this->file_format = dialog.get_image_format();

	if (this->save_mode == ViewportToImage::SaveMode::Directory) {
		this->n_tiles_x = dialog.get_n_tiles_x();
		this->n_tiles_y = dialog.get_n_tiles_y();
	}

	return true;
}





/**
   @reviewed-on tbd
*/
bool ViewportToImage::run_save_dialog_and_save(void)
{
	const QString destination_full_path = this->get_destination_full_path();
	if (destination_full_path.isEmpty()) {
		return false;
	} else {
		return sg_ret::ok == this->save_to_destination(destination_full_path);
	}

}



/**
   @reviewed-on tbd
*/
sg_ret ViewportToImage::save_to_destination(const QString & full_path)
{
	switch (this->save_mode) {
	case ViewportToImage::SaveMode::File:
	case ViewportToImage::SaveMode::FileKMZ:
		return this->save_to_image(full_path);
	case ViewportToImage::SaveMode::Directory:
		return this->save_to_dir(full_path);
	default:
		qDebug() << SG_PREFIX_E << "Unexpected save mode" << (int) this->save_mode;
		return sg_ret::err;
	}
}




/**
   @reviewed-on tbd
*/
sg_ret ViewportToImage::save_to_image(const QString & file_full_path)
{
	if (this->save_mode == ViewportToImage::SaveMode::FileKMZ
	    && this->gisview->get_coord_mode() == CoordMode::UTM) {

		/* Caller of this module should have checked this earlier. */
		qDebug() << SG_PREFIX_E << "Called the function while in UTM mode";
		return sg_ret::err;
	}

	this->window->statusbar()->set_message(StatusBarField::Info, QObject::tr("Generating image file..."));



	/* This class provides width/height of target device that
	   gives the same proportions as the source viewport has. So
	   there is no need to call calculate_scaled_sizes() to
	   calculate correct sizes for scaled viewport. */
	qDebug() << SG_PREFIX_I << "Will create scaled viewport of total width ="
		 << this->scaled_total_width
		 << ", total height ="
		 << this->scaled_total_height
		 << ", viking scale ="
		 << this->scaled_viking_scale
		 << ", size scale factor ="
		 << (1.0 * this->scaled_total_width / this->gisview->total_get_width());
	GisViewport * scaled_viewport = this->gisview->copy(this->scaled_total_width,
							    this->scaled_total_height,
							    this->window);

	qDebug() << SG_PREFIX_I << "Created scaled viewport of size" << scaled_viewport->total_get_width() << scaled_viewport->total_get_height();

	if (this->save_mode == ViewportToImage::SaveMode::FileKMZ) {
		/* Remove some viewport overlays as these aren't useful in KMZ file. */
		if (scaled_viewport->get_center_mark_visibility()) {
			scaled_viewport->set_center_mark_visibility(false);
		}
		if (scaled_viewport->get_scale_visibility()) {
			scaled_viewport->set_scale_visibility(false);
		}
	}

	/* Redraw all layers at current position and zoom.
	   Since we are saving viewport as it is, we allow existing highlights to be drawn to image. */
	ThisApp::layers_panel()->draw_tree_items(scaled_viewport, true, false);

	/* Save buffer as file. */
	const QPixmap pixmap = scaled_viewport->get_pixmap();
	delete scaled_viewport;

	if (pixmap.isNull()) {
		qDebug() << SG_PREFIX_E << "Failed to get viewport pixmap";

		this->window->statusbar()->set_message(StatusBarField::Info, "");
		Dialog::error(QObject::tr("Failed to generate internal image.\n\nTry creating a smaller image."), this->window);

		return sg_ret::err;
	}
	qDebug() << SG_PREFIX_I << "Generated pixmap from scaled viewport, pixmap size =" << pixmap.width() << pixmap.height();


	bool success = true;

	if (this->save_mode == ViewportToImage::SaveMode::FileKMZ) {

		/* For saving to KMZ the file format must always be
		   ViewportToImage::FileFormat::JPEG. */
		if (this->file_format != ViewportToImage::FileFormat::JPEG) {
			qDebug() << SG_PREFIX_E << "Unexpected non-JPEG file mode for KMZ save mode:" << (int) this->file_format;
			this->file_format = ViewportToImage::FileFormat::JPEG;
		}

		const LatLonBBox bbox = this->gisview->get_bbox();
		/* TODO_LATER: should we use here bound_value() or unbound_value() for longitudes? */
		const int ans = kmz_save_file(pixmap, file_full_path, bbox.north.value(), bbox.east.unbound_value(), bbox.south.value(), bbox.west.unbound_value());
		if (0 != ans) {
			qDebug() << SG_PREFIX_E << "Saving to kmz file failed with code" << ans;
			success = false;
		}

	} else {
		qDebug() << SG_PREFIX_I << "Saving pixmap to file" << file_full_path;
		if (!pixmap.save(file_full_path, this->file_format == ViewportToImage::FileFormat::PNG ? "png" : "jpeg")) {
			qDebug() << SG_PREFIX_E << "Unable to write to file" << file_full_path;
			success = false;
		}
	}



	this->window->statusbar()->set_message(StatusBarField::Info, "");
	if (success) {
		Dialog::info(QObject::tr("Image file generated."), this->window);
		return sg_ret::ok;
	} else {
		Dialog::info(QObject::tr("Failed to generate image file."), this->window);
		return sg_ret::err;
	}
}




/**
   @reviewed-on tbd
*/
sg_ret ViewportToImage::save_to_dir(const QString & dir_full_path)
{
	if (this->gisview->get_coord_mode() != CoordMode::UTM) {
		/* Caller of this module should have checked this earlier. */
		qDebug() << SG_PREFIX_E << "Called the function while not in UTM mode";
		return sg_ret::err;
	}


	QDir dir(dir_full_path);
	if (!dir.exists()) {
		if (!dir.mkpath(dir_full_path)) {
			qDebug() << SG_PREFIX_E << "Failed to create directory" << dir_full_path;
			return sg_ret::err;
		}
	}


	const char * extension = this->file_format == ViewportToImage::FileFormat::PNG ? "png" : "jpg";
	const UTM center_utm_orig = this->gisview->get_center_coord().get_utm();
	GisViewport * viewport = this->gisview->copy(this->scaled_total_width,
						     this->scaled_total_height,
						     1.0,
						     this->window);


	const double xmpp = viewport->get_viking_scale().get_x();
	const double ympp = viewport->get_viking_scale().get_y();


	for (int y = 1; y <= this->n_tiles_y; y++) {
		for (int x = 1; x <= this->n_tiles_x; x++) {
			const QString file_full_path = QString("%1%2y%3-x%4.%5").arg(dir_full_path).arg(QDir::separator()).arg(y).arg(x).arg(extension);
			UTM center_utm = center_utm_orig;
			if (this->n_tiles_x & 0x1) {
				center_utm.m_easting += ((double) x - ceil(((double) this->n_tiles_x)/2)) * (this->scaled_total_width * xmpp);
			} else {
				center_utm.m_easting += ((double) x - (((double) this->n_tiles_x)+1)/2) * (this->scaled_total_width * xmpp);
			}

			if (this->n_tiles_y & 0x1) { /* odd */
				center_utm.m_northing -= ((double) y - ceil(((double) this->n_tiles_y)/2)) * (this->scaled_total_height * ympp);
			} else { /* even */
				center_utm.m_northing -= ((double) y - (((double) this->n_tiles_y)+1)/2) * (this->scaled_total_height * ympp);
			}

			viewport->set_center_coord(center_utm, false);

			/*
			  Paint all layers at current position and
			  zoom to viewport's pixmap.

			  We could call viewport->request_redraw(), but:
			  1. we didn't connect GisViewport::center_coord_or_zoom_changed() signal.
			  2. we want to draw immediately, without waiting for handling of signal.
			*/
			this->window->draw_tree_items(viewport);

			/* Save viewport's pixmap to file. */
			const QPixmap pixmap = viewport->get_pixmap();
			if (pixmap.isNull()) {
				qDebug() << SG_PREFIX_E << "Unable to get viewport pixmap" << file_full_path;
				this->window->statusbar()->set_message(StatusBarField::Info, QObject::tr("Unable to create viewport's image"));
				continue;
			}

			if (!pixmap.save(file_full_path, extension)) {
				qDebug() << SG_PREFIX_E << "Unable to write to file" << file_full_path;
				this->window->statusbar()->set_message(StatusBarField::Info, QObject::tr("Unable to write to file %1").arg(file_full_path));
			} else {
				; /* Pixmap is valid and has been saved. */
			}
		}
	}

	delete viewport;

	return sg_ret::ok;
}




/**
   Get full path to either single file or to directory, to which to save a viewport image(s).

   @reviewed-on tbd
*/
QString ViewportToImage::get_destination_full_path(void)
{
	QString result;
	QStringList mime;



	QFileDialog file_selector(this->window);
	file_selector.setOption(QFileDialog::DontUseNativeDialog, true); /* Otherwise QFileDialog::ShowDirsOnly won't work. */
	file_selector.setAcceptMode(QFileDialog::AcceptSave);
	if (g_last_folder_images_url.isValid()) {
		file_selector.setDirectoryUrl(g_last_folder_images_url);
	}

	switch (this->save_mode) {
	case ViewportToImage::SaveMode::Directory:
		file_selector.setWindowTitle(QObject::tr("Select directory to save Viewport to"));
		file_selector.setFileMode(QFileDialog::Directory);
		file_selector.setOption(QFileDialog::ShowDirsOnly);
		break;

	case ViewportToImage::SaveMode::FileKMZ:
	case ViewportToImage::SaveMode::File: /* png or jpeg. */
		file_selector.setWindowTitle(QObject::tr("Select file to save Viewport to"));
		file_selector.setFileMode(QFileDialog::AnyFile); /* Specify new or select existing file. */

		mime << "application/octet-stream"; /* "All files (*)" */
		if (this->save_mode == ViewportToImage::SaveMode::FileKMZ) {
			mime << "vnd.google-earth.kmz"; /* "KMZ" / "*.kmz"; */
		} else {
			switch (this->file_format) {
			case ViewportToImage::FileFormat::PNG:
				mime << "image/png";
				break;
			case ViewportToImage::FileFormat::JPEG:
				mime << "image/jpeg";
				break;
			default:
				qDebug() << SG_PREFIX_E << "Unhandled viewport save format" << (int) this->file_format;
				break;
			};
		}
		file_selector.setMimeTypeFilters(mime);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unsupported mode" << (int) this->save_mode;
		return result; /* Empty string. */
	}


	if (QDialog::Accepted == file_selector.exec()) {
		g_last_folder_images_url = file_selector.directoryUrl();
		qDebug() << SG_PREFIX_I << "Last directory saved as:" << g_last_folder_images_url;

		result = file_selector.selectedFiles().at(0);
		qDebug() << SG_PREFIX_I << "Target file:" << result;

		if (0 == access(result.toUtf8().constData(), F_OK)) {
			if (!Dialog::yes_or_no(QObject::tr("The file \"%1\" exists, do you want to overwrite it?").arg(file_base_name(result)), this->window, "")) {
				result.resize(0);
			}
		}
	}


	qDebug() << SG_PREFIX_D << "Obtained file full path =" << result;
	if (result.isEmpty()) {
		qDebug() << SG_PREFIX_N << "Obtained file full path is empty";
	}

	return result;
}
