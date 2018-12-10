/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2006-2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#include <cstring>
#include <cstdlib>




#include <glib.h>




#include "degrees_converters.h"
#include "measurements.h"




static char * convert_dec_to_ddd(double dec, char pos_c, char neg_c);
static char * convert_dec_to_dmm(double dec, char pos_c, char neg_c);
static char * convert_dec_to_dms(double dec, char pos_c, char neg_c);




/**
 * @param pos_c char for positive value
 * @param neg_c char for negative value
 */
static char * convert_dec_to_ddd(double dec, char pos_c, char neg_c)
{
	char sign_c = ' ';

	if (dec > 0) {
		sign_c = pos_c;
	} else if (dec < 0) {
		sign_c = neg_c;
	} else { /* Nul value */
		sign_c = ' ';
	}

	/* Degree. */
	double val_d = fabs(dec);

	/* Format. */
	char * result = g_strdup_printf("%c%f" DEGREE_SYMBOL, sign_c, val_d);
	return result;
}




QString SlavGPS::convert_lat_dec_to_ddd(double lat)
{
	char * str = convert_dec_to_ddd(lat, 'N', 'S');
	const QString result(str);
	free(str);

	return result;
}




QString SlavGPS::convert_lon_dec_to_ddd(double lon)
{
	char * str = convert_dec_to_ddd(lon, 'E', 'W');
	const QString result(str);
	free(str);

	return result;
}




/**
 * @param pos_c char for positive value
 * @param neg_c char for negative value
 */
static char * convert_dec_to_dmm(double dec, char pos_c, char neg_c)
{
	char sign_c = ' ';

	if (dec > 0) {
		sign_c = pos_c;
	} else if (dec < 0) {
		sign_c = neg_c;
	} else { /* Nul value. */
		sign_c = ' ';
	}

	/* Degree. */
	double tmp = fabs(dec);
	int val_d = (int) tmp;

	/* Minutes. */
	double val_m = (tmp - val_d) * 60;

	/* Format. */
	char * result = g_strdup_printf("%c%d" DEGREE_SYMBOL "%f'", sign_c, val_d, val_m);
	return result;
}




QString SlavGPS::convert_lat_dec_to_dmm(double lat)
{
	char * str = convert_dec_to_dmm(lat, 'N', 'S');
	const QString result(str);
	free(str);

	return result;
}




QString SlavGPS::convert_lon_dec_to_dmm(double lon)
{
	char * str = convert_dec_to_dmm(lon, 'E', 'W');
	const QString result(str);
	free(str);

	return result;
}




/**
 * @param pos_c char for positive value
 * @param neg_c char for negative value
 */
static char * convert_dec_to_dms(double dec, char pos_c, char neg_c)
{
	char sign_c = ' ';


	if (dec > 0) {
		sign_c = pos_c;
	} else if (dec < 0) {
		sign_c = neg_c;
	} else { /* Nul value */
		sign_c = ' ';
	}

	/* Degree. */
	double tmp = fabs(dec);
	int val_d = (int) tmp;

	/* Minutes. */
	tmp = (tmp - val_d) * 60;
	int val_m = (int)tmp;

	/* Seconds. */
	double val_s = (tmp - val_m) * 60;

	/* Format. */
	char * result = g_strdup_printf("%c%d" DEGREE_SYMBOL "%d'%.4f\"", sign_c, val_d, val_m, val_s);
	return result;
}




QString SlavGPS::convert_lat_dec_to_dms(double lat)
{
	char * str = convert_dec_to_dms(lat, 'N', 'S');
	const QString result(str);
	free(str);

	return result;
}




QString SlavGPS::convert_lon_dec_to_dms(double lon)
{
	char * str = convert_dec_to_dms(lon, 'E', 'W');
	const QString result(str);
	free(str);

	return result;
}




double SlavGPS::convert_dms_to_dec(char const * dms)
{
	double d = 0.0; /* Degree. */
	double m = 0.0; /* Minutes. */
	double s = 0.0; /* Seconds. */
	int neg = false;

	if (dms != NULL) {
		int nbFloat = 0;
		const char * ptr = NULL;
		const char * endptr = NULL;

		/* Compute the sign.
		   It is negative if:
		   - the '-' sign occurs
		   - it is a west longitude or south latitude. */
		if (strpbrk(dms, "-wWsS") != NULL) {
			neg = true;
		}

		/* Peek the different components. */
		endptr = dms;
		do {
			double value;
			ptr = strpbrk(endptr, "0123456789,.");
			if (ptr != NULL) {
				const char * tmpptr = endptr;
				value = strtod((const char *) ptr, (char **) &endptr); /* TODO_LATER: remove casting. */
				/* Detect when endptr hasn't changed (which may occur if no conversion took place),
				   particularly if the last character is a ',' or there are multiple '.'s like '5.5.'. */
				if (endptr == tmpptr) {
					break;
				}
				nbFloat++;
				switch (nbFloat) {
				case 1:
					d = value;
					break;
				case 2:
					m = value;
					break;
				case 3:
					s = value;
					break;
				default: break;
				}
			}
		} while (ptr != NULL && endptr != NULL);
	}

	double result = d + m / 60 + s / 3600;
	if (neg) {
		result = -result;
	}

	return result;
}
