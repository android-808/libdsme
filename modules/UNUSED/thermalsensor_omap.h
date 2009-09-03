/**
   @file thermalsensor_omap.h

   DSME internal OMAP temperature i/f
   <p>
   Copyright (C) 2009 Nokia Corporation.

   @author Semi Malinen <semi.malinen@nokia.com>

   This file is part of Dsme.

   Dsme is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License
   version 2.1 as published by the Free Software Foundation.

   Dsme is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Dsme.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef THERMALSENSOR_OMAP_H
#define THERMALSENSOR_OMAP_H

#include <stdbool.h>

extern bool dsme_omap_get_temperature(int* temperature);
extern bool dsme_omap_is_blacklisted(void);

#endif
