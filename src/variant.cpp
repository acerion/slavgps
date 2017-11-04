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
		this->val_double = (double) strtod(str, NULL);
		break;
	case SGVariantType::UINT:
		this->val_uint = strtoul(str, NULL, 10);
		break;
	case SGVariantType::INT:
		this->val_int = strtol(str, NULL, 10);
		break;
	case SGVariantType::BOOLEAN:
		this->val_bool = TEST_BOOLEAN(str);
		break;
	case SGVariantType::COLOR:
		this->val_color = QColor(str);
		break;
	case SGVariantType::STRING:
		this->val_string = str;
		break;
	case SGVariantType::STRING_LIST:
		this->val_string = str; /* TODO: improve this assignment of string list. */
		break;
	default:
		qDebug() << "EE: Variant: from string: unsupported type id" << (int) type_id;
		break;
	}
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




QDebug SlavGPS::operator<<(QDebug debug, const SGVariant & value)
{
	switch ((int) value.type_id) {
	case (int) SGVariantType::EMPTY:
		debug << "<empty type>";
		break;
	case (int) SGVariantType::DOUBLE:
		debug << "double" << value.val_double;
		break;
	case (int) SGVariantType::UINT:
		debug << "uint" << value.val_uint;
		break;
	case (int) SGVariantType::INT:
		debug << "int" << value.val_int;
		break;
	case (int) SGVariantType::STRING:
		debug << "string" << value.val_string;
		break;
	case (int) SGVariantType::BOOLEAN:
		debug << "bool" << value.val_bool;
		break;
	case (int) SGVariantType::COLOR:
		debug << "color" << value.val_color.red() << value.val_color.green() << value.val_color.blue() << value.val_color.alpha();
		break;
	case (int) SGVariantType::STRING_LIST:
		debug << "string list" << value.val_string_list;
		break;
	case (int) SGVariantType::PTR:
		debug << "pointer" << QString("0x%1").arg((qintptr) value.val_pointer);
		break;
	};

	return debug;
}
