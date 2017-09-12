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

	switch (val.type_id) {
	case SGVariantType::STRING:
		this->s = g_strdup(val.s);
		break;

		/* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
		   the internals call get_param -- i.e. it should be managed w/in the layer.
		   The value passed by the internals into set_param should also be managed
		   by the layer -- i.e. free'd by the layer. */
	case SGVariantType::STRING_LIST:
		qDebug() << "EE: Variant Copy: copying 'string list' not implemented";
		break;
	default:
		break;
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
		this->s = strdup(str);
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
	switch (this->type_id) {
	case SGVariantType::STRING:
		if (this->s) {
#ifdef K
			/* TODO: restore this line. */
			free((void *) this->s);
#endif
			this->s = NULL;
		}
		break;
		/* TODO: APPLICABLE TO US? NOTE: string layer works auniquely: data.sl should NOT be free'd when
		 * the internals call get_param -- i.e. it should be managed w/in the layer.
		 * The value passed by the internals into set_param should also be managed
		 * by the layer -- i.e. free'd by the layer.
		 */
	case SGVariantType::STRING_LIST:
		fprintf(stderr, "WARNING: Param strings not implemented\n"); //fake it
		break;
	default:
		break;
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
