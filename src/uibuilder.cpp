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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>

#include "uibuilder.h"
#include "globals.h"




using namespace SlavGPS;




ParameterValue vik_lpd_true_default(void)
{
	return VIK_LPD_BOOLEAN (true);
}




ParameterValue vik_lpd_false_default(void)
{
	return VIK_LPD_BOOLEAN (false);
}




void uibuilder_run_setparam(ParameterValue * paramdatas, uint16_t i, ParameterValue data, Parameter * params)
{
	/* Could have to copy it if it's a string! */
	switch (params[i].type) {
	case ParameterType::STRING:
		paramdatas[i].s = g_strdup(data.s);
		break;
	default:
		paramdatas[i] = data; /* Dtring list will have to be freed by layer. anything else not freed. */
	}
}




ParameterValue uibuilder_run_getparam(ParameterValue * params_defaults, uint16_t i)
{
	return params_defaults[i];
}




static void a_uibuilder_free_paramdatas_sub(ParameterValue * paramdatas, int i)
{
        /* Should make a util function out of this. */
	std::list<char *> * a_list = paramdatas[i].sl;
	for (auto iter = a_list->begin(); iter != a_list->end(); iter++) {
		free(*iter);
        }
        delete paramdatas[i].sl;
	paramdatas[i].sl = NULL;
}




/* Frees data from last (if necessary). */
void a_uibuilder_free_paramdatas(ParameterValue *paramdatas, Parameter *params, uint16_t params_count)
{
	int i;
	/* May have to free strings, etc. */
	for (i = 0; i < params_count; i++) {
		switch (params[i].type) {
		case ParameterType::STRING:
			free((char *) paramdatas[i].s);
			break;
		case ParameterType::STRING_LIST: {
			a_uibuilder_free_paramdatas_sub(paramdatas, i);
			break;
			default:
				break;
		}
		}
	}
	free(paramdatas);
}




bool SlavGPS::operator==(unsigned int event_button, MouseButton button)
{
	return event_button == static_cast<unsigned int>(button);
}




bool SlavGPS::operator!=(unsigned int event_button, MouseButton button)
{
	return event_button != static_cast<unsigned int>(button);
}




bool SlavGPS::operator==(MouseButton button, unsigned int event_button)
{
	return event_button == static_cast<unsigned int>(button);
}




bool SlavGPS::operator!=(MouseButton button, unsigned int event_button)
{
	return event_button != static_cast<unsigned int>(button);
}
