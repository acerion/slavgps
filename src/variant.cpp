/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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

#include "variant.h"
#include "vikutils.h"




using namespace SlavGPS;




SGVariant::SGVariant(const SGVariant & val)
{
	*this = val;

	if (val.type_id == SGVariantType::EMPTY) {
		qDebug() << "EE: SG Variant: passed value with empty type to copy constructor";
	}
}




SGVariant::SGVariant(const char * str, SGVariantType type_id_)
{
	this->type_id = type_id_;

	switch (type_id_) {
	case SGVariantType::DOUBLE:
		this->d = strtod(str, NULL);
		break;
	case SGVariantType::UINT:
		this->u = strtoul(str, NULL, 10);
		break;
	case SGVariantType::INT:
		this->i = strtol(str, NULL, 10);
		break;
	case SGVariantType::BOOLEAN:
		this->b = TEST_BOOLEAN(str);
		break;
	case SGVariantType::COLOR:
		{
			/* TODO: maybe we could use *this = SGVariant(color); ? */
			const QColor color(str);
			this->c.r = color.red();
			this->c.g = color.green();
			this->c.b = color.blue();
			this->c.a = color.alpha();
			break;
		}
	/* STRING or STRING_LIST -- if STRING_LIST, just set param to add a STRING. */
	default:
		this->s = str;
		break;
	}
}




SGVariant::SGVariant(const QColor & color)
{
	this->c.r = color.red();
	this->c.g = color.green();
	this->c.b = color.blue();
	this->c.a = color.alpha();
	this->type_id = SGVariantType::COLOR;
}




SGVariant::~SGVariant()
{
	if (this->type_id == SGVariantType::EMPTY) {
		qDebug() << "EE: SG Variant: passed value with type id empty to destructor";
	}
}




SGVariant SlavGPS::sg_variant_true(void)
{
	return SGVariant((bool) true);
}




SGVariant SlavGPS::sg_variant_false(void)
{
	return SGVariant((bool) false);
}




void SGVariant::to_qcolor(QColor & color) const
{
	if (this->type_id != SGVariantType::COLOR) {
		qDebug() << "EE: Variant: To QColor: type of variant is not COLOR";
	}

	color = QColor(this->c.r, this->c.g, this->c.b, this->c.a);
}




QDebug SlavGPS::operator<<(QDebug debug, const SGVariant & value)
{
	switch ((int) value.type_id) {
	case (int) SGVariantType::EMPTY:
		debug << "<empty type>";
		break;
	case (int) SGVariantType::DOUBLE:
		debug << "double" << value.d;
		break;
	case (int) SGVariantType::UINT:
		debug << "uint" << value.u;
		break;
	case (int) SGVariantType::INT:
		debug << "int" << value.i;
		break;
	case (int) SGVariantType::STRING:
		debug << "string" << value.s;
		break;
	case (int) SGVariantType::BOOLEAN:
		debug << "bool" << value.b;
		break;
	case (int) SGVariantType::COLOR:
		debug << "color" << value.c.r << value.c.g << value.c.b << value.c.a;
		break;
	case (int) SGVariantType::STRING_LIST:
		debug << "slist" << value.sl;
		break;
	case (int) SGVariantType::PTR:
		debug << "ptr" << QString("0x%1").arg((unsigned long) value.ptr);
		break;
	};

	return debug;
}
