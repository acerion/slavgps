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

#include <cmath>

#include <QDebug>

#include "window.h"
#include "viewport_internal.h"
#include "viewport_utils.h"
#include "dialog.h"
#include "preferences.h"
#include "util.h"




using namespace SlavGPS;




extern Tree * g_tree;




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
		Dialog::info(tr("Viewable region outside allowable pixel size bounds for image. Clipping width/height values."), NULL);
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

	QString label_text;
	double w = this->width_spin->value() * zoom;
	double h = this->height_spin->value() * zoom;
	if (this->tiles_width_spin) { /* save many images; find TOTAL area covered */
		w *= this->tiles_width_spin->value();
		h *= this->tiles_height_spin->value();
	}
	DistanceUnit distance_unit = Preferences::get_unit_distance();
	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		label_text = tr("Total area: %1m x %2m (%3 sq. km)").arg((long) w).arg((long) h).arg((w * h / 1000000), 0, 'f', 3); /* "%.3f" */
		break;
	case DistanceUnit::MILES:
		label_text = tr("Total area: %1m x %2m (%3 sq. miles)").arg((long) w).arg((long) h).arg((w * h / 2589988.11), 0, 'f', 3); /* "%.3f" */
		break;
	case DistanceUnit::NAUTICAL_MILES:
		label_text = tr("Total area: %1m x %2m (%3 sq. NM)").arg((long) w).arg((long) h).arg((w * h / (1852.0 * 1852.0), 0, 'f', 3)); /* "%.3f" */
		break;
	default:
		qDebug() << "EE: Viewport: Save: wrong distance unit:" << (int) distance_unit;
	}

	this->total_area_label.setText(label_text);
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




void ViewportToImageDialog::build_ui(ViewportSaveMode mode)
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




	if (mode == ViewportSaveMode::FILE_KMZ) {
		/* Don't show image type selection if creating a KMZ (always JPG internally).
		   Start with viewable area by default. */
		this->get_size_from_viewport_cb();
	} else {
		std::vector<SGLabelID> items;
		items.push_back(SGLabelID(tr("Save as PNG"), 0));
		items.push_back(SGLabelID(tr("Save as JPEG"), 1));
		this->output_format_radios = new SGRadioGroup(tr("Output format"), &items, this);
		this->vbox->addWidget(this->output_format_radios);

		if (!g_tree->tree_get_main_window()->save_viewport_as_png) {
			this->output_format_radios->set_id_of_selected(1); /* '1' corresponds to '1' in code preparing items above. */
		}
	}

	if (mode == ViewportSaveMode::DIRECTORY) {

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


	this->button_box = new QDialogButtonBox();
	this->button_box->addButton("&Ok", QDialogButtonBox::AcceptRole);
	this->button_box->addButton("&Cancel", QDialogButtonBox::RejectRole);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &ViewportToImageDialog::accept_cb);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	this->vbox->addWidget(this->button_box);

	this->button_box->button(QDialogButtonBox::Ok)->setDefault(true);

	this->accept();
}
