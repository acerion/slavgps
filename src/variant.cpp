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


#include <cassert>

#include <QDebug>

#include "variant.h"
#include "vikutils.h"
#include "measurements.h"




using namespace SlavGPS;




#define PREFIX   " SGVariant"




SGVariant::SGVariant(const SGVariant & val)
{
	*this = val;

	if (val.type_id == SGVariantType::EMPTY) {
		qDebug() << "EE:" PREFIX << __FUNCTION__ << __LINE__ << ": passed value with empty type to copy constructor";
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
	case SGVariantType::TIMESTAMP:
		this->val_timestamp = (time_t) strtoul(str, NULL, 10);
		break;
	case SGVariantType::LATITUDE:
	case SGVariantType::LONGITUDE:
	case SGVariantType::ALTITUDE:
		this->val_lat_lon_alt = strtod(str, NULL);
		break;
	default:
		qDebug() << "EE:" PREFIX << __FUNCTION__ << __LINE__ << ": unsupported variant type id" << (int) this->type_id;
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
	case SGVariantType::TIMESTAMP:
		this->val_timestamp = (time_t) str.toULong();
		break;
	case SGVariantType::LATITUDE:
	case SGVariantType::LONGITUDE:
	case SGVariantType::ALTITUDE:
		this->val_lat_lon_alt = str.toDouble();
		break;
	default:
		qDebug() << "EE:" PREFIX << __FUNCTION__ << __LINE__ << ": unsupported variant type id" << (int) this->type_id;
		break;
	}
}




SGVariant::SGVariant(SGVariantType type_id_, time_t timestamp)
{
	assert (type_id_ == SGVariantType::TIMESTAMP);
	this->type_id = type_id_;
	this->val_timestamp = timestamp;
}




SGVariant::SGVariant(SGVariantType type_id_, double d)
{
	assert (type_id_ == SGVariantType::LATITUDE || type_id_ == SGVariantType::LONGITUDE || type_id_ == SGVariantType::ALTITUDE);
	this->type_id = type_id_;
	this->val_lat_lon_alt = d;
}




SGVariant::~SGVariant()
{
	if (this->type_id == SGVariantType::EMPTY) {
		qDebug() << "EE:" PREFIX << __FUNCTION__ << __LINE__ << ": passed value with type id empty to destructor";
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
	case SGVariantType::TIMESTAMP:
		debug << value.get_timestamp();
		break;
	case SGVariantType::LATITUDE:
		/* This is for debug, so we don't apply any format specifiers. */
		debug << value.get_latitude();
		break;
	case SGVariantType::LONGITUDE:
		/* This is for debug, so we don't apply any format specifiers. */
		debug << value.get_longitude();
		break;
	case SGVariantType::ALTITUDE:
		/* This is for debug, so we don't apply any format specifiers. */
		debug << value.get_altitude();
		break;
	default:
		debug << "EE:" PREFIX << __FUNCTION__ << __LINE__ << ": unsupported variant type id" << (int) value.type_id;
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
	case SGVariantType::TIMESTAMP:
		debug << "timestamp";
		break;
	case SGVariantType::LATITUDE:
		debug << "latitude";
		break;
	case SGVariantType::LONGITUDE:
		debug << "longitude";
		break;
	case SGVariantType::ALTITUDE:
		debug << "altitude";
		break;
	default:
		debug << "EE:" PREFIX << __FUNCTION__ << __LINE__ << ": unsupported variant type id" << (int) type_id;
		break;
	};

	return debug;
}




time_t SGVariant::get_timestamp() const
{
	return this->val_timestamp;
}




double SGVariant::get_latitude() const
{
	assert (this->type_id == SGVariantType::LATITUDE);

	return this->val_lat_lon_alt;
}




double SGVariant::get_longitude() const
{
	assert (this->type_id == SGVariantType::LONGITUDE);
	return this->val_lat_lon_alt;
}




double SGVariant::get_altitude() const
{
	assert (this->type_id == SGVariantType::ALTITUDE);
	return this->val_lat_lon_alt;
}




QString SGVariant::to_string() const
{
	static QLocale c_locale = QLocale::c();

	switch (this->type_id) {
	case SGVariantType::EMPTY:
		return QString("<empty value>");

	case SGVariantType::DOUBLE:
		return QString("%1").arg(this->val_double, 0, 'f', 20);

	case SGVariantType::UINT:
		return QString("%1").arg(this->val_uint);

	case SGVariantType::INT:
		return QString("%1").arg(this->val_int);

	case SGVariantType::STRING:
		return this->val_string;

	case SGVariantType::BOOLEAN:
		return QString("%1").arg(this->val_bool);

	case SGVariantType::COLOR:
		return QString("%1 %2 %3 %4").arg(this->val_color.red()).arg(this->val_color.green()).arg(this->val_color.blue()).arg(this->val_color.alpha());

	case SGVariantType::STRING_LIST:
		return this->val_string_list.join(" / ");

	case SGVariantType::PTR:
		return QString("0x%1").arg((qintptr) this->val_pointer);

	case SGVariantType::TIMESTAMP:
		return QString("%1").arg(this->get_timestamp());

	case SGVariantType::LATITUDE:
		return c_locale.toString(this->get_latitude(), 'f', SG_PRECISION_LATITUDE);

	case SGVariantType::LONGITUDE:
		return c_locale.toString(this->get_longitude(), 'f', SG_PRECISION_LONGITUDE);

	case SGVariantType::ALTITUDE:
		return c_locale.toString(this->get_altitude(), 'f', SG_PRECISION_ALTITUDE);

	default:
		qDebug() << "EE:" PREFIX << __FUNCTION__ << __LINE__ << ": unsupported variant type id" << (int) this->type_id;
		return QString("");
	};
}
