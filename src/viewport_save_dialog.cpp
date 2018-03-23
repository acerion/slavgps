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




#include <QDebug>




#include "window.h"
#include "viewport_internal.h"
#include "viewport_save_dialog.h"
#include "preferences.h"
#include "util.h"




using namespace SlavGPS;




#define PREFIX ": Viewport Save:" << __FUNCTION__ << __LINE__ << ">"




extern Tree * g_tree;




void ViewportSaveDialog::get_size_from_viewport_cb(void) /* Slot */
{
	this->width_spin->setValue(this->viewport->get_width());
	this->height_spin->setValue(this->viewport->get_height());

	return;
}




void ViewportSaveDialog::calculate_total_area_cb(void)
{
	QString label_text;
	double w = this->width_spin->value() * this->viewport->get_xmpp();
	double h = this->height_spin->value() * this->viewport->get_xmpp();
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
		qDebug() << "EE" PREFIX "wrong distance unit:" << (int) distance_unit;
	}

	this->total_area_label->setText(label_text);
}




ViewportSaveDialog::ViewportSaveDialog(QString const & title, Viewport * vp, QWidget * parent_widget) : BasicDialog(NULL)
{
	this->setWindowTitle(title);
	this->parent = parent;
	this->viewport = vp;
}




ViewportSaveDialog::~ViewportSaveDialog()
{
}




void ViewportSaveDialog::accept_cb(void) /* Slot. */
{
	this->accept();
}




void ViewportSaveDialog::build_ui(ViewportSaveMode mode)
{
	int row = 0;


	this->grid->addWidget(new QLabel(tr("Width (pixels):")), row, 0);

	this->width_spin = new QSpinBox();
	this->width_spin->setMinimum(0);
	this->width_spin->setMaximum(10 * 1024);
	this->width_spin->setSingleStep(1);
	this->grid->addWidget(this->width_spin, row, 1);
	row++;



	this->grid->addWidget(new QLabel(tr("Height (pixels):")), row, 0);

	this->height_spin = new QSpinBox();
	this->height_spin->setMinimum(0);
	this->height_spin->setMaximum(10 * 1024);
	this->height_spin->setSingleStep(1);
	this->grid->addWidget(this->height_spin, row, 1);
	row++;



	/* Right below width/height spinboxes. */
	QPushButton * use_current_area_button = new QPushButton("Copy size from Viewport");
	this->grid->addWidget(use_current_area_button, row, 1);
	connect(use_current_area_button, SIGNAL(clicked()), this, SLOT(get_size_from_viewport_cb()));
	row++;



	this->total_area_label = new QLabel(tr("Total Area"));
	this->grid->addWidget(this->total_area_label, row, 0, 1, 2);
	row++;



	if (mode == ViewportSaveMode::FileKMZ) {
		/* Don't show image type selection if creating a KMZ (always JPG internally).
		   Start with viewable area by default. */
		this->get_size_from_viewport_cb();
	} else {
		std::vector<SGLabelID> items;
		items.push_back(SGLabelID(tr("Save as PNG"), (int) ViewportSaveFormat::PNG));
		items.push_back(SGLabelID(tr("Save as JPEG"), (int) ViewportSaveFormat::JPEG));

		this->output_format_radios = new SGRadioGroup(tr("Output format"), &items, this);
		this->output_format_radios->set_id_of_selected((int) g_tree->tree_get_main_window()->viewport_save_format);

		this->grid->addWidget(this->output_format_radios, row, 0, 1, 2);
		row++;
	}

	if (mode == ViewportSaveMode::Directory) {

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


	connect(this->width_spin, SIGNAL(valueChanged(int)), this, SLOT(calculate_total_area_cb()));
	connect(this->height_spin, SIGNAL(valueChanged(int)), this, SLOT(calculate_total_area_cb()));
	connect(this->button_box, &QDialogButtonBox::accepted, this, &ViewportSaveDialog::accept_cb);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	/* Set initial size info now. */
	this->get_size_from_viewport_cb();
	this->calculate_total_area_cb();

	this->accept();
}




int ViewportSaveDialog::get_width(void) const
{
	return this->width_spin->value();
}




int ViewportSaveDialog::get_height(void) const
{
	return this->height_spin->value();
}




ViewportSaveFormat ViewportSaveDialog::get_image_format(void) const
{
	return (ViewportSaveFormat) this->output_format_radios->get_id_of_selected();
}
