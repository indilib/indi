/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *  
 *  Copyright (C) 2000 - 2005 Petr Kubanek
 */

#ifndef _LN_PARALLAX_H
#define _LN_PARALLAX_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \fn void ln_get_parallax (struct ln_equ_posn * object, double au_distance, struct ln_lnlat_posn * observer, double height, double JD, struct ln_equ_posn * parallax);
* \ingroup parallax
* \brief Calculate parallax in RA and DEC for given geographic location
*/
void ln_get_parallax (struct ln_equ_posn * object, double au_distance, struct ln_lnlat_posn * observer, double height, double JD, struct ln_equ_posn * parallax);

/*! \fn void ln_get_parallax_ha (struct ln_equ_posn * object, double au_distance, struct ln_lnlat_posn * observer, double height, double H, struct ln_equ_posn * parallax);
* \ingroup parallax
* \brief Calculate parallax in RA and DEC for given geographic location
*/
void ln_get_parallax_ha (struct ln_equ_posn * object, double au_distance, struct ln_lnlat_posn * observer, double height, double H, struct ln_equ_posn * parallax);

#ifdef __cplusplus
};
#endif

#endif
