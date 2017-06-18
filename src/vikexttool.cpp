/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
 *
 */


#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "vikexttool.h"





using namespace SlavGPS;





External::External()
{
	this->label = strdup("<no-set>");
	this->id = 0;

	fprintf(stderr, "%s:%d, label = %s\n", __PRETTY_FUNCTION__, __LINE__, this->label);
}





External::~External()
{
	fprintf(stderr, "%s:%d, label = %s\n", __PRETTY_FUNCTION__, __LINE__, this->label);

	free(this->label);
	this->label = NULL;
}





char * External::get_label()
{
	return g_strdup(this->label);
}





void External::set_label(char const * new_label)
{
	if (new_label) {
		free(this->label);
		this->label = strdup(new_label);
	}
	return;
}





void External::set_id(int new_id)
{
	this->id = new_id;
	return;
}





int External::get_id()
{
	return this->id;
}
