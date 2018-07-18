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
#include "clipboard.h"




using namespace SlavGPS;




#define PREFIX ": Variant:" << __FUNCTION__ << __LINE__ << ">"




SGVariant::SGVariant(const SGVariant & val)
{
	*this = val;

	if (val.type_id == SGVariantType::Empty) {
		qDebug() << "EE" PREFIX << "passed value with empty type to copy constructor";
	}
}




SGVariant::SGVariant(SGVariantType type_id_, const char * str)
{
	this->type_id = type_id_;

	switch (type_id_) {
	case SGVariantType::Double:
		this->u.val_double = (double) strtod(str, NULL);
		break;
	case SGVariantType::Uint:
		this->u.val_uint = strtoul(str, NULL, 10);
		break;
	case SGVariantType::Int:
		this->u.val_int = strtol(str, NULL, 10);
		break;
	case SGVariantType::Boolean:
		this->u.val_bool = TEST_BOOLEAN(str);
		break;
	case SGVariantType::Color:
		this->val_color = QColor(str);
		break;
	case SGVariantType::String:
		this->val_string = str;
		break;
	case SGVariantType::StringList:
		this->val_string = str; /* TODO: improve this assignment of string list. */
		break;
	case SGVariantType::Timestamp:
		this->val_timestamp = (time_t) strtoul(str, NULL, 10);
		break;
	case SGVariantType::Latitude:
	case SGVariantType::Longitude:
	case SGVariantType::Altitude:
		this->val_lat_lon_alt = strtod(str, NULL);
		break;
	default:
		qDebug() << "EE" PREFIX << "unsupported variant type id" << (int) this->type_id;
		break;
	}
}




SGVariant::SGVariant(SGVariantType type_id_, const QString & str)
{
	this->type_id = type_id_;

	switch (type_id_) {
	case SGVariantType::Double:
		this->u.val_double = str.toDouble();
		break;
	case SGVariantType::Uint:
		this->u.val_uint = str.toULong();
		break;
	case SGVariantType::Int:
		this->u.val_int = str.toLong();
		break;
	case SGVariantType::Boolean:
		this->u.val_bool = TEST_BOOLEAN(str.toUtf8().constData());
		break;
	case SGVariantType::Color:
		this->val_color = QColor(str);
		break;
	case SGVariantType::String:
		this->val_string = str;
		break;
	case SGVariantType::StringList:
		this->val_string = str; /* TODO: improve this assignment of string list. */
		break;
	case SGVariantType::Timestamp:
		this->val_timestamp = (time_t) str.toULong();
		break;
	case SGVariantType::Latitude:
	case SGVariantType::Longitude:
	case SGVariantType::Altitude:
		this->val_lat_lon_alt = str.toDouble();
		break;
	default:
		qDebug() << "EE" PREFIX "unsupported variant type id" << (int) this->type_id;
		break;
	}
}




SGVariant::SGVariant(double d, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::Double
		|| type_id_ == SGVariantType::Latitude
		|| type_id_ == SGVariantType::Longitude
		|| type_id_ == SGVariantType::Altitude);

	this->type_id = type_id_;

	switch (type_id_) {
	case SGVariantType::Double:
		this->u.val_double = d;
		break;
	case SGVariantType::Latitude:
	case SGVariantType::Longitude:
	case SGVariantType::Altitude:
		this->val_lat_lon_alt = d;
		break;
	default:
		assert (0);
		break;
	}
}




SGVariant::SGVariant(int32_t i, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::Int);
	this->type_id = type_id_;
	this->u.val_int = i;
}




SGVariant::SGVariant(uint32_t new_u, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::Uint);
	this->type_id = type_id_;
	this->u.val_uint = new_u;
}




SGVariant::SGVariant(const char * str, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::String);
	this->type_id = type_id_;
	this->val_string = QString(str);
}




SGVariant::SGVariant(const QString & str, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::String);
	this->type_id = type_id_;
	this->val_string = str;
}




SGVariant::SGVariant(bool b, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::Boolean);
	this->type_id = type_id_;
	this->u.val_bool = b;
}




SGVariant::SGVariant(const QColor & color, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::Color);
	this->type_id = type_id_;
	this->val_color = color;
}




SGVariant::SGVariant(int r, int g, int b, int a, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::Color);
	this->type_id = type_id_;
	this->val_color = QColor(r, g, b, a);
}




SGVariant::SGVariant(const QStringList & string_list, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::StringList);
	this->type_id = type_id_;
	this->val_string_list = string_list;
}




SGVariant::SGVariant(time_t timestamp, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::Timestamp);
	this->type_id = type_id_;
	this->val_timestamp = timestamp;
}




SGVariant::~SGVariant()
{
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
	case SGVariantType::Empty:
		break;
	case SGVariantType::Double:
		debug << QString("%1").arg(value.u.val_double, 0, 'f', 12);
		break;
	case SGVariantType::Uint:
		debug << value.u.val_uint;
		break;
	case SGVariantType::Int:
		debug << value.u.val_int;
		break;
	case SGVariantType::String:
		debug << value.val_string;
		break;
	case SGVariantType::Boolean:
		debug << value.u.val_bool;
		break;
	case SGVariantType::Color:
		debug << value.val_color.red() << value.val_color.green() << value.val_color.blue() << value.val_color.alpha();
		break;
	case SGVariantType::StringList:
		debug << value.val_string_list;
		break;
	case SGVariantType::Pointer:
		debug << QString("0x%1").arg((qintptr) value.u.val_pointer);
		break;
	case SGVariantType::Timestamp:
		debug << value.get_timestamp();
		break;
	case SGVariantType::Latitude:
		/* This is for debug, so we don't apply any format specifiers. */
		debug << value.get_latitude();
		break;
	case SGVariantType::Longitude:
		/* This is for debug, so we don't apply any format specifiers. */
		debug << value.get_longitude();
		break;
	case SGVariantType::Altitude:
		/* This is for debug, so we don't apply any format specifiers. */
		debug << value.get_altitude();
		break;
	default:
		debug << "EE" PREFIX << "unsupported variant type id" << (int) value.type_id;
		break;
	};

	return debug;
}




QDebug SlavGPS::operator<<(QDebug debug, const SGVariantType type_id)
{
	switch (type_id) {
	case SGVariantType::Empty:
		debug << "<Empty type>";
		break;
	case SGVariantType::Double:
		debug << "Double";
		break;
	case SGVariantType::Uint:
		debug << "Uint";
		break;
	case SGVariantType::Int:
		debug << "Int";
		break;
	case SGVariantType::String:
		debug << "String";
		break;
	case SGVariantType::Boolean:
		debug << "Bool";
		break;
	case SGVariantType::Color:
		debug << "Color";
		break;
	case SGVariantType::StringList:
		debug << "String List";
		break;
	case SGVariantType::Pointer:
		debug << "Pointer";
		break;
	case SGVariantType::Timestamp:
		debug << "Timestamp";
		break;
	case SGVariantType::Latitude:
		debug << "Latitude";
		break;
	case SGVariantType::Longitude:
		debug << "Longitude";
		break;
	case SGVariantType::Altitude:
		debug << "Altitude";
		break;
	default:
		debug << "EE" PREFIX << "unsupported variant type id" << (int) type_id;
		break;
	};

	return debug;
}




SGVariant & SGVariant::operator=(const SGVariant & other)
{
	if (&other == this) {
		return *this;
	}

	this->type_id = other.type_id;

	switch (other.type_id) {
	case SGVariantType::Empty:
		break;
	case SGVariantType::Double:
		this->u.val_double = other.u.val_double;
		break;
	case SGVariantType::Uint:
		this->u.val_uint = other.u.val_uint;
		break;
	case SGVariantType::Int:
		this->u.val_int = other.u.val_int;
		break;
	case SGVariantType::String:
		this->val_string = other.val_string;
		break;
	case SGVariantType::Boolean:
		this->u.val_bool = other.u.val_bool;
		break;
	case SGVariantType::Color:
		this->val_color = other.val_color;
		break;
	case SGVariantType::StringList:
		this->val_string_list = other.val_string_list;
		break;
	case SGVariantType::Pointer:
		this->u.val_pointer = other.u.val_pointer;
		break;
	case SGVariantType::Timestamp:
		this->val_timestamp = other.val_timestamp;
		break;
	case SGVariantType::Latitude:
	case SGVariantType::Longitude:
	case SGVariantType::Altitude:
		/* This is for debug, so we don't apply any format specifiers. */
		this->val_lat_lon_alt = other.val_lat_lon_alt;
		break;
	default:
		qDebug() << "EE" PREFIX << "unsupported variant type id" << (int) other.type_id;
		break;
	};

	return *this;
}




time_t SGVariant::get_timestamp() const
{
	return this->val_timestamp;
}




double SGVariant::get_latitude() const
{
	assert (this->type_id == SGVariantType::Latitude);

	return this->val_lat_lon_alt;
}




double SGVariant::get_longitude() const
{
	assert (this->type_id == SGVariantType::Longitude);
	return this->val_lat_lon_alt;
}




double SGVariant::get_altitude() const
{
	assert (this->type_id == SGVariantType::Altitude);
	return this->val_lat_lon_alt;
}




QString SGVariant::to_string() const
{
	static QLocale c_locale = QLocale::c();

	switch (this->type_id) {
	case SGVariantType::Empty:
		return QString("<empty value>");

	case SGVariantType::Double:
		return QString("%1").arg(this->u.val_double, 0, 'f', 20);

	case SGVariantType::Uint:
		return QString("%1").arg(this->u.val_uint);

	case SGVariantType::Int:
		return QString("%1").arg(this->u.val_int);

	case SGVariantType::String:
		return this->val_string;

	case SGVariantType::Boolean:
		return QString("%1").arg(this->u.val_bool);

	case SGVariantType::Color:
		return QString("%1 %2 %3 %4").arg(this->val_color.red()).arg(this->val_color.green()).arg(this->val_color.blue()).arg(this->val_color.alpha());

	case SGVariantType::StringList:
		return this->val_string_list.join(" / ");

	case SGVariantType::Pointer:
		return QString("0x%1").arg((qintptr) this->u.val_pointer);

	case SGVariantType::Timestamp:
		return QString("%1").arg(this->get_timestamp());

	case SGVariantType::Latitude:
		return c_locale.toString(this->get_latitude(), 'f', SG_PRECISION_LATITUDE);

	case SGVariantType::Longitude:
		return c_locale.toString(this->get_longitude(), 'f', SG_PRECISION_LONGITUDE);

	case SGVariantType::Altitude:
		return c_locale.toString(this->get_altitude(), 'f', SG_PRECISION_ALTITUDE);

	default:
		qDebug() << "EE" PREFIX << "unsupported variant type id" << (int) this->type_id;
		return QString("");
	};
}




void SGVariant::marshall(Pickle & pickle, SGVariantType new_type_id) const
{
	pickle.put_pickle_tag("pickle.variant");
	pickle.put_raw_int((int) this->type_id);

	/* We can just do memcpy() on union with POD fields, but
	   non-trivial data types need to be handled separately. */

	switch (new_type_id) {
	case SGVariantType::Color: {
		pickle.put_raw_int(0); /* Dummy value. */

		int r = this->val_color.red();
		int g = this->val_color.green();
		int b = this->val_color.blue();
		int a = this->val_color.alpha();

		pickle.put_raw_object((char *) &r, sizeof (r));
		pickle.put_raw_object((char *) &g, sizeof (g));
		pickle.put_raw_object((char *) &b, sizeof (b));
		pickle.put_raw_object((char *) &a, sizeof (a));

		break;
	}
	case SGVariantType::String:
		pickle.put_raw_int(0); /* Dummy value. */

		if (!this->val_string.isEmpty()) {
			pickle.put_string(this->val_string);
		} else {
			/* Need to insert empty string otherwise the unmarshall will get confused. */
			pickle.put_string("");
		}
		break;
		/* Print out the string list in the array. */
	case SGVariantType::StringList:

		/* Write length of list (# of strings). */
		pickle.put_raw_int(this->val_string_list.size());

		/* Write each string. */
		for (auto i = this->val_string_list.constBegin(); i != this->val_string_list.constEnd(); i++) {
			pickle.put_string(*i);
		}

		break;
	default:
		/* Plain Old Datatype. */
		pickle.put_raw_int(sizeof (this->u));
		pickle.put_raw_object((char *) &this->u, sizeof (this->u));
		break;
	}
}




SGVariant SGVariant::unmarshall(Pickle & pickle, SGVariantType expected_type_id)
{
	SGVariant result;

	const char * tag = pickle.take_pickle_tag("pickle.variant");
	const SGVariantType type_id = (SGVariantType) pickle.take_raw_int();


	if (type_id != expected_type_id) {
		qDebug() << "EE" PREFIX << "wrong variant type id:" << type_id << "!=" << expected_type_id;
		assert(0);
	}


	/* We can just do memcpy() on union with POD fields, but
	   non-trivial data types need to be handled separately using
	   their constructors. */
	switch (expected_type_id) {
	case SGVariantType::Color: {
		const int dummy = pickle.take_raw_int();

		int r = 0;
		int g = 0;
		int b = 0;
		int a = 0;

		pickle.take_raw_object((char *) &r, sizeof (r));
		pickle.take_raw_object((char *) &g, sizeof (g));
		pickle.take_raw_object((char *) &b, sizeof (b));
		pickle.take_raw_object((char *) &a, sizeof (a));

		result = SGVariant(r, g, b, a);

		break;
	}
	case SGVariantType::String: {
		const int dummy = pickle.take_raw_int();
		result = SGVariant(pickle.take_string());
		break;
	}
	case SGVariantType::StringList: {
		const int n_strings = pickle.take_raw_int();
		for (int i = 0; i < n_strings; i++) {
			result.val_string_list.push_back(pickle.take_string());
		}

		break;
	}
	default:
		/* Plain Old Datatype. */
		const int expected_size = pickle.take_raw_int();

		/* Test that we are reading correct data. */
		if (expected_size != sizeof (result.u)) {
			qDebug() << "EE" PREFIX << "unexpected size of POD:" << expected_size << "!=" << sizeof (result.u);
			assert(0);
		}

		pickle.take_raw_object((char *) &result.u, sizeof (result.u));
		result.type_id = type_id; /* For non-trivial data types this is done by constructor. */
		break;
	}

	return result;
}




void SGVariant::write(FILE * file, char const * param_name) const
{
	/* String lists are handled differently. We get a QStringList (that shouldn't
	   be freed) back for get_param and if it is empty we shouldn't write
	   anything at all (otherwise we'd read in a list with an empty string,
	   not an empty string list). */
	if (this->type_id == SGVariantType::StringList) {
		for (auto iter = this->val_string_list.constBegin(); iter != this->val_string_list.constEnd(); iter++) {
			fprintf(file, "%s=", param_name);
			fprintf(file, "%s\n", (*iter).toUtf8().constData());
		}
	} else {
		fprintf(file, "%s=", param_name);
		switch (this->type_id) {
		case SGVariantType::Double: {
			// char buf[15]; /* Locale independent. */
			// fprintf(file, "%s\n", (char *) g_dtostr(this->u.val_double, buf, sizeof (buf))); break;
			fprintf(file, "%f\n", this->u.val_double);
			break;
		}
		case SGVariantType::Uint:
			fprintf(file, "%u\n", this->u.val_uint); /* kamilkamil: in viking code the format specifier was incorrect. */
			break;

		case SGVariantType::Int:
			fprintf(file, "%d\n", this->u.val_int);
			break;

		case SGVariantType::Boolean:
			fprintf(file, "%c\n", this->u.val_bool ? 't' : 'f');
			break;

		case SGVariantType::String:
			fprintf(file, "%s\n", this->val_string.isEmpty() ? "" : this->val_string.toUtf8().constData());
			break;

		case SGVariantType::Color:
			fprintf(file, "#%.2x%.2x%.2x\n", this->val_color.red(), this->val_color.green(), this->val_color.blue());
			break;

		default:
			qDebug() << "EE" PREFIX << "Unhandled variant type id" << (int) this->type_id;
			break;
		}
	}
}
