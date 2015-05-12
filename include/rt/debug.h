/* -*- c-basic-offset: 8; -*-
 *
 * Copyright (C) 2015 Chris Cummins.
 *
 * This file is part of rt.
 *
 * rt is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * rt is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rt.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef RT_DEBUG_H_
#define RT_DEBUG_H_

#include "./math.h"

namespace debug {

bool SHOW_SUPERSAMPLE_PIXELS           = false;
bool SHOW_RECURSIVE_SUPERSAMPLE_PIXELS = false;
int  PIXEL_HIGHLIGHT_COLOUR            = 0xffffff;

}  // namespace debug

#endif  // RT_DEBUG_H_
