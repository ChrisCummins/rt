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
#include "rt/renderer.h"

#include "rt/debug.h"
#include "rt/profiling.h"

#include "tbb/parallel_for.h"

namespace {

// We're using an anonymous namespace so we're allowed to import rt::
using namespace rt;  // NOLINT(build/namespaces)

// Anti-aliasing tunable knobs.
Scalar maxPixelDiff     = 0.040;
Scalar maxSubpixelDiff  = 0.008;
size_t maxSubpixelDepth = 3;

// Return the object with the closest intersection to ray, and set the
// distance to the intersection `t'. If no intersection, returns a
// nullptr.
static inline Object *closestIntersect(Ray &ray,
                                             Objects &objects,
                                             Scalar *restrict t) {
        // Index of, and distance to closest intersect:
        Object *closest = nullptr;
        *t = INFINITY;

        // For each object:
        for (size_t i = 0; i < objects.size(); i++) {
                // Get intersect distance.
                Scalar currentT = objects[i]->intersect(ray);

                // Check if intersects, and if so, whether the
                // intersection is closer than the current best.
                if (currentT != 0 && currentT < *t) {
                        // New closest intersection.
                        *t = currentT;
                        closest = objects[i];
                }
        }

        return closest;
}

// Create a transformation matrix to scale from image space
// coordinates (i.e. [x,y] coordinates with reference to the image
// size) to camera space coordinates (i.e. [x,y] coordinates with
// reference to the camera's film size).
Matrix cameraImageTransform(Camera *restrict camera,
                            Image *restrict image) {
        // Scale image coordinates to camera coordinates.
        Scale scale(camera->width / image->width,
                          camera->height / image->height, 1);
        // Offset from image coordinates to camera coordinates.
        Translation offset(-(image->width * .5),
                                 -(image->height * .5), 0);
        // Combine the transformation matrices.
        return scale * offset;
}

}  // namespace

namespace rt {

Renderer::Renderer(Scene *restrict _scene,
                   rt::Camera *restrict _camera,
                   size_t _numDofSamples,
                   size_t _maxRayDepth)
                : scene(_scene), camera(_camera),
                  maxRayDepth(_maxRayDepth),
                  numDofSamples(_numDofSamples) {}

Renderer::~Renderer() {
        delete scene;
        delete camera;
}

void Renderer::render(Image *restrict image) {
        // Create image to camera transformation matrix.
        Matrix transform = cameraImageTransform(camera, image);

        // First, we collect a single sample for every pixel in the
        // image, plus an additional border of 1 pixel on all sides.
        size_t borderedWidth = image->width + 2;
        size_t borderedHeight = image->height + 2;
        size_t borderedSize = borderedWidth * borderedHeight;
        Colour *restrict sampled = new Colour[borderedSize];

        // Collect pixel samples:
        tbb::parallel_for(
            static_cast<size_t>(0), borderedSize, [&](size_t index) {
                    // Get the pixel coordinates.
                    size_t x = image::x(index, borderedWidth);
                    size_t y = image::y(index, borderedWidth);

                    // Sample a point in the centre of the pixel.
                    sampled[index] = renderPoint(x + .5, y + .5,
                                                 transform);
            });

        Colour *restrict superSampled = new Colour[image->size];

        // For each pixel in the image:
        for (size_t index = 0; index < image->size; index++) {
                // Get the pixel coordinates.
                size_t x = image::x(index, image->width);
                size_t y = image::y(index, image->width);

                // Get the previously sampled pixel value.
                Colour pixel = sampled[image::index(x + 1, y + 1,
                                                    borderedWidth)];

                // Create a list of all neighbouring elements.
                size_t neighbours[] = {
                        image::index(x - 1, y - 1, borderedWidth),
                        image::index(x,     y - 1, borderedWidth),
                        image::index(x + 1, y - 1, borderedWidth),
                        image::index(x - 1, y,     borderedWidth),
                        image::index(x + 1, y,     borderedWidth),
                        image::index(x - 1, y + 1, borderedWidth),
                        image::index(x,     y + 1, borderedWidth),
                        image::index(x + 1, y + 1, borderedWidth)
                };

                // Iterate over each of the neighbouring elements.
                for (size_t i = 0; i < 8; i++) {
                        // Calculate the difference between the centre
                        // pixel and the neighbour.
                        Scalar diff = pixel.diff(sampled[neighbours[i]]);

                        // If the difference is above a given
                        // threshold, then recursively supersample the
                        // pixel.
                        if (diff > maxPixelDiff) {
                                if (debug::SHOW_SUPERSAMPLE_PIXELS)
                                        pixel = Colour(
                                            debug::PIXEL_HIGHLIGHT_COLOUR);
                                else
                                        pixel = renderRegion(x, y, 1,
                                                             transform);
                                break;
                        }
                }

                // Set new value.
                superSampled[index] = pixel;
        }

        delete[] sampled;

        // Write pixel information to image.
        for (size_t index = 0; index < image->size; index++)
                image->set(index, superSampled[index]);

        delete[] superSampled;
}

Colour Renderer::renderRegion(Scalar regionX,
                              Scalar regionY,
                              Scalar regionSize,
                              Matrix &transform,
                              size_t depth) {
        Colour samples[4];
        Colour supersamples[4];
        Scalar subregion_x[4];
        Scalar subregion_y[4];
        Colour *sample;

        // Determine the size of a sample.
        Scalar subregionSize = regionSize / 2;
        // Determine the offset to centre of a sample.
        Scalar subregionOffset = subregionSize / 2;

        // Iterate over each subregion.
        sample = &samples[0];
        for (size_t index = 0; index < 4; index ++) {
                size_t i = image::x(index, 2);
                size_t j = image::y(index, 2);

                // Determine subregion origin.
                Scalar x = regionX + i * subregionSize;
                Scalar y = regionY + j * subregionSize;

                // Save X,Y coordinates for later.
                subregion_x[index] = x;
                subregion_y[index] = y;

                // Take a sample at the centre of the subregion.
                *sample++ = renderPoint(x + subregionOffset,
                                        y + subregionOffset,
                                        transform);
        }

        // Determine the average region colour.
        Colour mean;
        mean += samples[0];
        mean += samples[1];
        mean += samples[2];
        mean += samples[3];
        mean /= 4;

        // If we've already recursed as far as we can do, there's
        // nothing more to do.
        if (depth >= maxSubpixelDepth)
                return mean;

        // Iterate over each sub-region.
        for (size_t i = 0; i < 4; i++) {
                sample = &samples[i];

                // Determine the difference between the average region
                // colour and the subregion sample.
                Scalar diff = mean.diff(*sample);

                // If the difference is above a threshold, recursively
                // supersample this region.
                if (diff > maxSubpixelDiff) {
                        Scalar x = subregion_x[i];
                        Scalar y = subregion_y[i];

                        if (debug::SHOW_RECURSIVE_SUPERSAMPLE_PIXELS)
                                return Colour(debug::PIXEL_HIGHLIGHT_COLOUR);

                        // Recursively evaluate sample.
                        *sample = renderRegion(x, y,
                                                 regionSize / 4,
                                                 transform,
                                                 depth + 1);
                }

                // Write updated value.
                supersamples[i] = *sample;
        }

        // Determine mean colour of supersampled subregions.
        Colour output;
        output += supersamples[0];
        output += supersamples[1];
        output += supersamples[2];
        output += supersamples[3];
        output /= 4;

        return output;
}

Colour Renderer::renderPoint(Scalar x,
                             Scalar y,
                             Matrix &transform) {
        Colour output;

        // Convert image to camera space coordinates.
        Vector imageOrigin = transform * Vector(x, y, 0);

        // Translate camera space to world space.
        Vector focalOrigin =
                        camera->right * imageOrigin.x +
                        camera->up * imageOrigin.y +
                        camera->position * 1;

        // Determine direction from point on lens to
        // exposure point.
        Vector focalDirection =
                        (focalOrigin - camera->filmBack)
                        .normalise();

        // Determine the focus point of the pixel.
        Vector focalPoint = camera->filmBack + focalDirection
                        * camera->focusDistance;

        // Accumulate numDofSamples samples.
        for (size_t i = 0; i < numDofSamples; i++) {
                // Convert image to camera space coordinates.
                Vector cameraSpace = imageOrigin +
                                camera->lens.aperture();

                // Translate camera space to world space.
                Vector worldSpace =
                                camera->right * cameraSpace.x +
                                camera->up * cameraSpace.y +
                                camera->position;

                // Determine direction from point on lens
                // to focus point.
                Vector direction =
                                (focalPoint - worldSpace)
                                .normalise();

                // Create a ray.
                Ray ray = Ray(worldSpace, direction);

                // Sample the ray.
                output += trace(ray) / numDofSamples;
        }

        return output;
}

Colour Renderer::trace(Ray &ray,
                       unsigned int depth) {
        Colour colour;

        // Bump profiling counter.
        profiling::counters::incTraceCount();

        // Determine the closet ray-object intersection (if any).
        Scalar t;
        Object *restrict object =
                        closestIntersect(ray, scene->objects, &t);
        // If the ray doesn't intersect any object, do nothing.
        if (object == nullptr)
                return colour;

        // Point of intersection.
        Vector intersect = ray.position + ray.direction * t;
        // Surface normal at point of intersection.
        Vector normal = object->normal(intersect);
        // Direction between intersection and source ray.
        Vector toRay = (ray.position - intersect).normalise();
        // Material at point of intersection.
        Material *material = object->surface(intersect);

        // Apply ambient lighting.
        colour += material->colour * material->ambient;

        // Apply shading from each light source.
        for (size_t i = 0; i < scene->lights.size(); i++)
                colour += scene->lights[i]->shade(intersect, normal, toRay,
                                                  material, scene->objects);

        // Create reflection ray and recursive evaluate.
        Scalar reflectivity = material->reflectivity;
        if (depth < maxRayDepth && reflectivity > 0) {
                // Direction of reflected ray.
                Vector reflectionDirection = (normal * 2*(normal ^ toRay)
                                                    - toRay).normalise();
                // Create a reflection.
                Ray reflection(intersect, reflectionDirection);
                // Add reflection light.
                colour += trace(reflection, depth + 1) * reflectivity;
        }

        return colour;
}

}  // namespace rt
