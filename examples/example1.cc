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

// Include ray tracer header.
#include "rt/rt.h"

int main() {
        // Create colours.
        rt::Colour red   = rt::Colour(0xff0000);
        rt::Colour green = rt::Colour(0x00ff00);
        rt::Colour blue  = rt::Colour(0x0000ff);

        // Create objects.
        rt::Object *_objects[] = {
                new rt::Sphere(rt::Vector(0,    50, 0), 50,
                               new rt::Material(red, 0, 1, .2, 10, 0)),
                new rt::Sphere(rt::Vector(50,  -50, 0), 50,
                               new rt::Material(green, 0, 1, .2, 10, 0)),
                new rt::Sphere(rt::Vector(-50, -50, 0), 50,
                               new rt::Material(blue, 0, 1, .2, 10, 0))
        };

        // Create lights.
        rt::Light *_lights[] = {
                new rt::SoftLight(rt::Vector(-300,  400, -400),
                                  rt::Colour(0xffffff)),
                new rt::SoftLight(rt::Vector( 300, -200,  100),
                                  rt::Colour(0x505050))
        };

        // Create camera.
        rt::Camera *restrict camera =
                        new rt::Camera(rt::Vector(0, 0, -250),  // position
                                       rt::Vector(0, 0, 0),     // look at
                                       50, 50,         // film width & height
                                       rt::Lens(50));  // focal length

        // Create collections.
        rt::Objects objects(_objects, _objects + 3);
        rt::Lights  lights( _lights,  _lights  + 2);

        // Create scene and renderer.
        rt::Scene *restrict scene
                        = new rt::Scene(objects, lights);
        rt::Renderer *restrict renderer
                        = new rt::Renderer(scene, camera);

        // Run ray tracer.
        rt::render(renderer, new rt::Image(512, 512), "render1.ppm");

        return 0;
}
