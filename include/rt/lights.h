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
#ifndef RT_LIGHTS_H_
#define RT_LIGHTS_H_

#include <cstdint>
#include <cstddef>
#include <vector>

#include "./graphics.h"
#include "./math.h"
#include "./objects.h"
#include "./random.h"
#include "./restrict.h"

namespace rt {

// Base class light source.
class Light {
 public:
    // Virtual destructor.
    virtual ~Light() {}

    // Calculate the shading colour at `point' for a given surface
    // material, surface normal, and direction to the ray.
    virtual Colour shade(Vector point,
                         Vector normal,
                         Vector toRay,
                         Material *restrict material,
                         Objects objects) = 0;
};

typedef std::vector<Light *> Lights;

// A round light source.
class SoftLight : public Light {
 public:
        Vector position;
        Colour colour;
        size_t samples;
        mutable UniformDistribution sampler;

        // Constructor.
        inline SoftLight(Vector _position,
                         Colour _colour,
                         Scalar _radius = 0,
                         size_t _samples = 1)
                : position(_position), colour(_colour), samples(_samples),
                           sampler(UniformDistribution(-_radius, _radius)) {
                // Register lights with profiling counter.
                profiling::counters::incLightsCount(_samples);
        }

        virtual Colour shade(Vector point,
                             Vector normal,
                             Vector toRay,
                             Material *restrict material,
                             Objects objects) ;
};

}  // namespace rt

#endif  // RT_LIGHTS_H_
