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
#ifndef RT_RENDERER_H_
#define RT_RENDERER_H_

#include <cstdint>

#include "./camera.h"
#include "./image.h"
#include "./random.h"
#include "./ray.h"
#include "./scene.h"

namespace rt {

class Renderer {
 public:
        Renderer(Scene *restrict scene,
                 rt::Camera *restrict camera,
                 size_t numDofSamples = 1,
                 size_t maxRayDepth   = 5000);

        ~Renderer();

        Scene *restrict scene;
        rt::Camera *restrict camera;

        // Renderer configuration:

        // The maximum depth to trace reflected rays to:
        size_t maxRayDepth;

        // TODO: Super sampling anti-aliasing configuration:

        // Number of samples to make for depth of field:
        size_t numDofSamples;

        // The heart of the raytracing engine.
        void render(Image *restrict image) ;

 private:
        // Recursively supersample a region.
        Colour renderRegion(Scalar x,
                            Scalar y,
                            Scalar regionSize,
                            Matrix &transform,
                            size_t depth = 0) ;

        // Get the colour value at a single point.
        Colour renderPoint(Scalar x,
                           Scalar y,
                           Matrix &transform) ;

        // Trace a ray trough a given scene and return the final
        // colour.
        Colour trace(Ray &ray,
                     unsigned int depth = 0) ;

        // Perform supersample interpolation.
        Colour interpolate(size_t image_x,
                           size_t image_y,
                           size_t dataWidth,
                           Colour *restrict data) ;
};

}  // namespace rt

#endif  // RT_RENDERER_H_
