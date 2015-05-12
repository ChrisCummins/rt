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
#include "rt/lights.h"

#include "rt/profiling.h"

namespace rt {

namespace {

// Return whether a given ray intersects any of the objects within a
// given distance.
bool intersects(Ray ray,
                Objects objects,
                Scalar distance) {
    // Determine any object intersects ray within distance:
    for (size_t i = 0; i < objects.size(); i++) {
        Scalar t = objects[i]->intersect(ray);
        if (t > 0 && t < distance)
            return true;
    }

    // No intersect.
    return false;
}

}  // namespace

Colour SoftLight::shade(Vector point,
                        Vector normal,
                        Vector toRay,
                        Material *restrict material,
                        Objects objects) {
        // Shading is additive, starting with black.
        Colour output = Colour();

        // Product of material and light colour.
        Colour illumination = (colour * material->colour) / samples;

        // Cast multiple light rays, nomrally distributed about the
        // light's centre.
        for (size_t i = 0; i < samples; i++) {
                // Create a new point origin randomly offset from centre.
                Vector origin = Vector(position.x + sampler(),
                                             position.y + sampler(),
                                             position.z + sampler());
                // Vector from point to light.
                Vector toLight = origin - point;
                // Distance from point to light.
                Scalar distance = toLight.size();
                // Direction from point to light.
                Vector direction = toLight / distance;

                // Determine whether light is blocked.
                Ray ray = Ray(point, direction);
                bool blocked = intersects(ray, objects, distance);
                // Do nothing without line of sight.
                if (blocked)
                        continue;

                // Bump the profiling counter.
                profiling::counters::incRayCount();

                // Apply Lambert (diffuse) shading.
                Scalar lambert = std::max(normal ^ direction,
                                                static_cast<Scalar>(0));
                output += illumination * material->diffuse * lambert;

                // Apply Blinn-Phong (specular) shading.
                Vector bisector = (toRay + direction).normalise();
                Scalar phong = pow(std::max(normal ^ bisector,
                                                  static_cast<Scalar>(0)),
                                         material->shininess);
                output += illumination * material->specular * phong;
        }

        return output;
}


}  // namespace rt
