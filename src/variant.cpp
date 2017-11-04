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




SGVariant::SGVariant(SGVariantType type_id_, const char * str)
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
		qDebug() << "EE: Variant: from string: unsupported type id" << (int) this->type_id;
		break;
	}
}




SGVariant::SGVariant(SGVariantType type_id_, const QString & str)
{
	this->type_id = type_id_;

	switch (type_id_) {
	case SGVariantType::DOUBLE:
		this->val_double = str.toDouble();
		break;
	case SGVariantType::UINT:
		this->val_uint = str.toULong();
		break;
	case SGVariantType::INT:
		this->val_int = str.toLong();
		break;
	case SGVariantType::BOOLEAN:
		this->val_bool = TEST_BOOLEAN(str.toUtf8().constData());
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
		qDebug() << "EE: Variant: from string: unsupported type id" << (int) this->type_id;
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
	debug << value.type_id;

	switch (value.type_id) {
	case SGVariantType::EMPTY:
		break;
	case SGVariantType::DOUBLE:
		debug << value.val_double;
		break;
	case SGVariantType::UINT:
		debug << value.val_uint;
		break;
	case SGVariantType::INT:
		debug << value.val_int;
		break;
	case SGVariantType::STRING:
		debug << value.val_string;
		break;
	case SGVariantType::BOOLEAN:
		debug << value.val_bool;
		break;
	case SGVariantType::COLOR:
		debug << value.val_color.red() << value.val_color.green() << value.val_color.blue() << value.val_color.alpha();
		break;
	case SGVariantType::STRING_LIST:
		debug << value.val_string_list;
		break;
	case SGVariantType::PTR:
		debug << QString("0x%1").arg((qintptr) value.val_pointer);
		break;
	default:
		debug << "EE: SGVariant<<: unsupported type" << (int) value.type_id;
		break;
	};

	return debug;
}




QDebug SlavGPS::operator<<(QDebug debug, const SGVariantType type_id)
{
	switch (type_id) {
	case SGVariantType::EMPTY:
		debug << "<empty type>";
		break;
	case SGVariantType::DOUBLE:
		debug << "double";
		break;
	case SGVariantType::UINT:
		debug << "uint";
		break;
	case SGVariantType::INT:
		debug << "int";
		break;
	case SGVariantType::STRING:
		debug << "string";
		break;
	case SGVariantType::BOOLEAN:
		debug << "bool";
		break;
	case SGVariantType::COLOR:
		debug << "color";
		break;
	case SGVariantType::STRING_LIST:
		debug << "string list";
		break;
	case SGVariantType::PTR:
		debug << "pointer";
		break;
	default:
		debug << "EE: SGVariantType<<: unsupported type" << (int) type_id;
		break;
	};

	return debug;
}
