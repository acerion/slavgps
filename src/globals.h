/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2018, Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_GLOBALS_H_
#define _SG_GLOBALS_H_




#include <QString>




namespace SlavGPS {




	typedef int param_id_t; /* This shall be a signed type. */



	/* Function return value. */
	enum class sg_ret : int {
		err_algo,  /* Function algorithm error. */
		err_arg,   /* Function arguments error. */
		err,       /* General error. */
		ok = 0,
	};




#ifndef MSECS_PER_SEC
#define MSECS_PER_SEC 1000
#endif




#define SG_NAMESPACE_PREFIX "SlavGPS::"

#define SG_PREFIX_D      "DD  " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_I      "II  " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_N      "NN  " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_W      "WW  " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_E      "EE  " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_SLOT   "SLT " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_SIGNAL "SIG " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GLOBALS_H_ */
