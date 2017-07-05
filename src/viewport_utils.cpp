/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 *
 * Lat/Lon plotting functions calcxy* are from GPSDrive
 * GPSDrive Copyright (C) 2001-2004 Fritz Ganter <ganter@ganter.at>
 *
 * Multiple UTM zone patch by Kit Transue <notlostyet@didactek.com>
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

#include <QDebug>

#include "viewport_internal.h"
#include "viewport_utils.h"
#include "dialog.h"
#include "preferences.h"
#include "util.h"




using namespace SlavGPS;




/* The last used directories. */
static char * last_folder_images_uri = NULL; /* kamilFIXME: this should be freed somewhere. */




void ViewportToImageDialog::get_size_from_viewport_cb(void) /* Slot */
{
	int active = this->zoom_combo->currentIndex();
	double zoom = pow(2, active - 2);

	double width_min = this->width_spin->minimum();
	double width_max = this->width_spin->maximum();
	double height_min = this->height_spin->minimum();
	double height_max = this->height_spin->maximum();

	/* TODO: support for xzoom and yzoom values */
	int width_size = this->viewport->get_width() * this->viewport->get_xmpp() / zoom;
	int height_size = this->viewport->get_height() * this->viewport->get_xmpp() / zoom;

	if (width_size > width_max || width_size < width_min || height_size > height_max || height_size < height_min) {
		dialog_info("Viewable region outside allowable pixel size bounds for image. Clipping width/height values.", NULL);
	}

	qDebug() << "DD: Viewport: Save: current viewport size:" << this->viewport->get_width() << "/" << this->viewport->get_height() << ", zoom:" << zoom << ", xmpp:" << this->viewport->get_xmpp();
	this->width_spin->setValue(width_size);
	this->height_spin->setValue(height_size);

	return;
}




void ViewportToImageDialog::calculate_total_area_cb(void)
{
	int active = this->zoom_combo->currentIndex();
	double zoom = pow(2, active - 2);

	char * label_text = NULL;
	double w = this->width_spin->value() * zoom;
	double h = this->height_spin->value() * zoom;
	if (this->tiles_width_spin) { /* save many images; find TOTAL area covered */
		w *= this->tiles_width_spin->value();
		h *= this->tiles_height_spin->value();
	}
	DistanceUnit distance_unit = Preferences::get_unit_distance();
	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		label_text = g_strdup_printf(_("Total area: %ldm x %ldm (%.3f sq. km)"), (long) w, (long) h, (w * h / 1000000));
		break;
	case DistanceUnit::MILES:
		label_text = g_strdup_printf(_("Total area: %ldm x %ldm (%.3f sq. miles)"), (long) w, (long) h, (w * h / 2589988.11));
		break;
	case DistanceUnit::NAUTICAL_MILES:
		label_text = g_strdup_printf(_("Total area: %ldm x %ldm (%.3f sq. NM)"), (long) w, (long) h, (w * h / (1852.0 * 1852.0)));
		break;
	default:
		label_text = g_strdup_printf("Just to keep the compiler happy");
		qDebug() << "EE: Viewport: Save: wrong distance unit:" << (int) distance_unit;
	}

	this->total_area_label.setText(QString(label_text));
	free(label_text);
}




ViewportToImageDialog::ViewportToImageDialog(QString const & title, Viewport * vp, QWidget * parent_widget) : QDialog(NULL)
{
	this->setWindowTitle(title);
	this->parent = parent;
	this->viewport = vp;
}




ViewportToImageDialog::~ViewportToImageDialog()
{
	delete this->button_box;
	delete this->vbox;
	delete this->zoom_combo;
}




void ViewportToImageDialog::accept_cb(void) /* Slot. */
{
	this->accept();
}




void ViewportToImageDialog::build_ui(img_generation_t img_gen)
{
	qDebug() << "II: Viewport To Image Dialog: building dialog UI";

	this->vbox = new QVBoxLayout;
	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);



	QLabel * label = new QLabel(tr("Width (pixels):"));
	this->vbox->addWidget(label);


	this->width_spin = new QSpinBox();
	this->width_spin->setMinimum(0);
	this->width_spin->setMaximum(10 * 1024);
	this->width_spin->setSingleStep(1);
	this->vbox->addWidget(this->width_spin);
	//connect(this->width_spin, SIGNAL (valueChanged(int)), this, SLOT (sync_timestamp_to_tp_cb(void)));


	label = new QLabel(tr("Height (pixels):"));
	this->vbox->addWidget(label);


	this->height_spin = new QSpinBox();
	this->height_spin->setMinimum(0);
	this->height_spin->setMaximum(10 * 1024);
	this->height_spin->setSingleStep(1);
	this->vbox->addWidget(this->height_spin);
	//connect(&this->height_spin, SIGNAL (valueChanged(int)), this, SLOT (sync_timestamp_to_tp_cb(void)));



	label = new QLabel(tr("Zoom (meters per pixel):"));
	this->vbox->addWidget(label);


	this->zoom_combo = SlavGPS::create_zoom_combo_all_levels(NULL);

	double mpp = this->viewport->get_xmpp();
	int active = 2 + round(log(mpp) / log(2));

	/* Can we not hard code size here? */
	if (active > 17) {
		active = 17;
	}
	if (active < 0) {
		active = 0;
	}
	this->zoom_combo->setCurrentIndex(active);
	this->vbox->addWidget(this->zoom_combo);


	this->total_area_label.setText(tr("Total Area"));
	this->vbox->addWidget(&this->total_area_label);


	this->use_current_area_button.setText("Area in current viewport");
	this->vbox->addWidget(&this->use_current_area_button);
	connect(&this->use_current_area_button, SIGNAL(clicked()), this, SLOT(get_size_from_viewport_cb()));




	if (img_gen == VW_GEN_KMZ_FILE) {
		// Don't show image type selection if creating a KMZ (always JPG internally)
		// Start with viewable area by default
		this->get_size_from_viewport_cb();
	} else {
		QStringList labels;
		labels << tr("Save as PNG");
		labels << tr("Save as JPEG");
		//labels.push_back(label);
		QString title(tr("Output format"));
		this->output_format_radios = new SGRadioGroup(title, labels, this);
		this->vbox->addWidget(this->output_format_radios);

#ifdef K
		if (!this->draw_image_save_as_png) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(jpeg_radio), true);
		}
#endif
	}

	if (img_gen == VW_GEN_DIRECTORY_OF_IMAGES) {

		label = new QLabel(tr("East-west image tiles:"));
		this->vbox->addWidget(label);

		this->tiles_width_spin = new QSpinBox();
		this->tiles_width_spin->setRange(1, 10);
		this->tiles_width_spin->setSingleStep(1);
		this->tiles_width_spin->setValue(5);
		this->vbox->addWidget(this->tiles_width_spin);

		label = new QLabel(tr("North-south image tiles:"));
		this->vbox->addWidget(label);

		this->tiles_height_spin = new QSpinBox();
		this->tiles_height_spin->setRange(1, 10);
		this->tiles_height_spin->setSingleStep(1);
		this->tiles_height_spin->setValue(5);
		this->vbox->addWidget(this->tiles_height_spin);

		connect(this->tiles_width_spin, SIGNAL(valueChanged(int)), this, SLOT(calculate_total_area_cb()));
		connect(this->tiles_height_spin, SIGNAL(valueChanged(int)), this, SLOT(calculate_total_area_cb()));
	}

	connect(this->width_spin, SIGNAL(valueChanged(int)), this, SLOT(calculate_total_area_cb()));
	connect(this->height_spin, SIGNAL(valueChanged(int)), this, SLOT(calculate_total_area_cb()));

	connect(this->zoom_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(calculate_total_area_cb()));

	this->calculate_total_area_cb(); /* Set correct size info now. */

#ifdef K
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
#endif


	this->button_box = new QDialogButtonBox();
	this->button_box->addButton("&Ok", QDialogButtonBox::AcceptRole);
	this->button_box->addButton("&Cancel", QDialogButtonBox::RejectRole);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &ViewportToImageDialog::accept_cb);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	this->vbox->addWidget(this->button_box);

	this->accept();
}
