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
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Variant"




SGVariant::SGVariant(const SGVariant & val)
{
	*this = val;

	if (val.type_id == SGVariantType::Empty) {
		qDebug() << SG_PREFIX_E << "Passed value with empty type to copy constructor";
	}
}




SGVariant::SGVariant(SGVariantType type_id_, const char * str)
{
	this->type_id = type_id_;

	switch (type_id_) {
	case SGVariantType::Double:
		this->u.val_double = (double) strtod(str, NULL);
		break;
	case SGVariantType::Int: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		this->u.val_int = strtol(str, NULL, 10);
		break;
	case SGVariantType::Enumeration: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		this->u.val_enumeration = (int) strtol(str, NULL, 10);
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
		this->val_string = str; /* TODO_LATER: improve this assignment of string list. */
		break;
	case SGVariantType::Timestamp:
		this->val_timestamp.set_timestamp_from_string(str);
		break;
	case SGVariantType::DurationType:
		this->val_duration.set_duration_from_string(str);
		break;
	case SGVariantType::Latitude:
		this->lat = Latitude(str);
		break;
	case SGVariantType::Longitude:
		this->lon = Longitude(str);
		break;
	case SGVariantType::AltitudeType:
		this->altitude = Altitude(strtod(str, NULL), HeightUnit::Metres);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unsupported variant type id" << (int) this->type_id;
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
	case SGVariantType::Int: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		this->u.val_int = str.toLong();
		break;
	case SGVariantType::Enumeration: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		this->u.val_enumeration = (int) str.toLong();
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
		this->val_string = str; /* TODO_LATER: improve this assignment of string list. */
		break;
	case SGVariantType::Timestamp:
		this->val_timestamp.set_timestamp_from_string(str);
		break;
	case SGVariantType::DurationType:
		this->val_duration.set_duration_from_string(str);
		break;
	case SGVariantType::Latitude:
		this->lat = Latitude(str);
		break;
	case SGVariantType::Longitude:
		this->lon = Longitude(str);
		break;
	case SGVariantType::AltitudeType:
		this->altitude = Altitude(str.toDouble(), HeightUnit::Metres);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unsupported variant type id" << (int) this->type_id;
		break;
	}
}




SGVariant::SGVariant(double d, SGVariantType type_id_)
{
	this->type_id = type_id_;

	switch (type_id_) {
	case SGVariantType::Double:
		this->u.val_double = d;
		break;
	case SGVariantType::Latitude:
		this->lat.set_value(d);
		break;
	case SGVariantType::Longitude:
		this->lon.set_value(d);
		break;
	case SGVariantType::AltitudeType:
		this->altitude = Altitude(d, HeightUnit::Metres);
		break;
	default:
		assert (0);
		break;
	}
}




SGVariant::SGVariant(int32_t i, SGVariantType type_id_)
{
	this->type_id = type_id_;

	switch (type_id_) {
	case SGVariantType::Int:
		this->u.val_int = i;
		break;
	case SGVariantType::Enumeration:
		this->u.val_enumeration = i;
		break;
	default:
		assert (type_id_ == SGVariantType::Int || type_id_ == SGVariantType::Enumeration);
		break;
	}
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




SGVariant::SGVariant(const Latitude & new_lat, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::Latitude);
	this->type_id = type_id_;
	this->lat = new_lat;
}




SGVariant::SGVariant(const Longitude & new_lon, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::Longitude);
	this->type_id = type_id_;
	this->lon = new_lon;
}




SGVariant::SGVariant(const Altitude & a, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::AltitudeType);
	this->type_id = type_id_;
	this->altitude = a;
}




SGVariant::SGVariant(const Time & timestamp, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::Timestamp);
	this->type_id = type_id_;
	this->val_timestamp = timestamp;
}




SGVariant::SGVariant(const Duration & duration, SGVariantType type_id_)
{
	assert (type_id_ == SGVariantType::DurationType);
	this->type_id = type_id_;
	this->val_duration = duration;
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
	case SGVariantType::Int: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		debug << value.u.val_int;
		break;
	case SGVariantType::Enumeration: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		debug << value.u.val_enumeration;
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
	case SGVariantType::DurationType:
		debug << value.get_duration();
		break;
	case SGVariantType::Latitude:
		/* This is for debug, so we don't apply any format specifiers. */
		debug << value.get_latitude().to_string();
		break;
	case SGVariantType::Longitude:
		/* This is for debug, so we don't apply any format specifiers. */
		debug << value.get_longitude().to_string();
		break;
	case SGVariantType::AltitudeType:
		/* This is for debug, so we don't apply any format specifiers. */
		debug << value.get_altitude().to_string();
		break;
	default:
		debug << SG_PREFIX_E << "Unsupported variant type id" << (int) value.type_id;
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
	case SGVariantType::Int:
		debug << "Int";
		break;
	case SGVariantType::Enumeration:
		debug << "Enumeration";
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
	case SGVariantType::DurationType:
		debug << "Duration";
		break;
	case SGVariantType::Latitude:
		debug << "Latitude";
		break;
	case SGVariantType::Longitude:
		debug << "Longitude";
		break;
	case SGVariantType::AltitudeType:
		debug << "Altitude";
		break;
	default:
		debug << SG_PREFIX_E << "Unsupported variant type id" << (int) type_id;
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
	case SGVariantType::Int: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		this->u.val_int = other.u.val_int;
		break;
	case SGVariantType::Enumeration: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		this->u.val_enumeration = other.u.val_enumeration;
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
	case SGVariantType::DurationType:
		this->val_duration = other.val_duration;
		break;
	case SGVariantType::Latitude:
		this->lat = other.lat;
		break;
	case SGVariantType::Longitude:
		this->lon = other.lon;
		break;
	case SGVariantType::AltitudeType:
		this->altitude = other.altitude;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unsupported variant type id" << (int) other.type_id;
		break;
	};

	return *this;
}




Time SGVariant::get_timestamp(void) const
{
	return this->val_timestamp;
}




Duration SGVariant::get_duration(void) const
{
	return this->val_duration;
}




Latitude SGVariant::get_latitude(void) const
{
	assert (this->type_id == SGVariantType::Latitude);
	return this->lat;
}




Longitude SGVariant::get_longitude(void) const
{
	assert (this->type_id == SGVariantType::Longitude);
	return this->lon;
}




Altitude SGVariant::get_altitude(void) const
{
	assert (this->type_id == SGVariantType::AltitudeType);
	return this->altitude;
}




QString SGVariant::to_string() const
{
	switch (this->type_id) {
	case SGVariantType::Empty:
		return QString("<empty value>");

	case SGVariantType::Double:
		return QString("%1").arg(this->u.val_double, 0, 'f', 20);

	case SGVariantType::Int: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		return QString("%1").arg(this->u.val_int);

	case SGVariantType::Enumeration: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
		return QString("%1").arg(this->u.val_enumeration);

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
		return QString("%1").arg(this->get_timestamp().to_timestamp_string());

	case SGVariantType::DurationType:
		return QString("%1").arg(this->get_duration().to_string());

	case SGVariantType::Latitude:
		return this->lat.to_string();

	case SGVariantType::Longitude:
		return this->lon.to_string();

	case SGVariantType::AltitudeType:
		return this->altitude.to_string();

	default:
		qDebug() << SG_PREFIX_E << "Unsupported variant type id" << (int) this->type_id;
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
		qDebug() << SG_PREFIX_E << "Wrong variant type id:" << type_id << "!=" << expected_type_id;
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
			qDebug() << SG_PREFIX_E << "Unexpected size of POD:" << expected_size << "!=" << sizeof (result.u);
			assert(0);
		}

		pickle.take_raw_object((char *) &result.u, sizeof (result.u));
		result.type_id = type_id; /* For non-trivial data types this is done by constructor. */
		break;
	}

	return result;
}




void SGVariant::write(FILE * file, const QString & param_name) const
{
	if (this->type_id == SGVariantType::StringList) {
		/*
		  String lists are handled differently. We get a
		  QStringList and if it is empty we shouldn't write
		  anything at all (otherwise we'd read in a list with
		  an empty string, not an empty string list).

		  For a list of files in DEM layer the result will look like this:

		  ~Layer DEM
		  name=DEM
		  files=/mnt/viking/test_data/srtm_hgt/version2_1/SRTM3/Australia/S11E119.hgt.zip
		  files=/mnt/viking/test_data/srtm_hgt/version2_1/SRTM3/South_America/S56W072.hgt.zip
		  files=/mnt/viking/test_data/srtm_hgt/version2_1/SRTM3/South_America/S04W042.hgt.zip
		*/
		for (auto iter = this->val_string_list.constBegin(); iter != this->val_string_list.constEnd(); iter++) {
			fprintf(file, "%s=", param_name.toUtf8().constData());
			fprintf(file, "%s\n", (*iter).toUtf8().constData());
		}
	} else {
		fprintf(file, "%s=", param_name.toUtf8().constData());
		switch (this->type_id) {
		case SGVariantType::Double: {
			// char buf[15]; /* Locale independent. */
			// fprintf(file, "%s\n", (char *) g_dtostr(this->u.val_double, buf, sizeof (buf))); break;
			fprintf(file, "%f\n", this->u.val_double);
			break;
		}

		case SGVariantType::Int: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
			fprintf(file, "%d\n", this->u.val_int);
			break;

		case SGVariantType::Enumeration: /* 'SGVariantType::Int' and 'SGVariantType::Enumeration' are distinct types, so keep them in separate cases. */
			fprintf(file, "%d\n", this->u.val_enumeration);
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

		case SGVariantType::DurationType:
			fprintf(file, "%s\n", this->get_duration().value_to_string_for_file().toUtf8().constData());
			break;

		case SGVariantType::Latitude:
			fprintf(file, "%s\n", this->lat.value_to_string_for_file().toUtf8().constData());
			break;

		case SGVariantType::Longitude:
			fprintf(file, "%s\n", this->lon.value_to_string_for_file().toUtf8().constData());
			break;

		case SGVariantType::AltitudeType:
			fprintf(file, "%s\n", this->altitude.value_to_string_for_file().toUtf8().constData());
			break;

		default:
			qDebug() << SG_PREFIX_E << "Unhandled variant type id" << (int) this->type_id;

			/* The newline is needed to prevent having two
			   consecutive lines "glued" into one. */
			fprintf(file, "\n");
			break;
		}
	}
}
