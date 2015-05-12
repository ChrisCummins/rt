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
#ifndef OBJECTS_H_
#define OBJECTS_H_

#include <vector>

#include "./graphics.h"
#include "./math.h"
#include "./profiling.h"
#include "./ray.h"
#include "./restrict.h"

namespace rt {

// Properties that describe a material.
class Material {
 public:
        Colour colour;
        Scalar ambient;       // 0 <= ambient <= 1
        Scalar diffuse;       // 0 <= diffuse <= 1
        Scalar specular;      // 0 <= specular <= 1
        Scalar shininess;     // shininess >= 0
        Scalar reflectivity;  // 0 <= reflectivity < 1

        // Constructor.
        inline Material(Colour _colour,
                        Scalar _ambient,
                        Scalar _diffuse,
                        Scalar _specular,
                        Scalar _shininess,
                        Scalar _reflectivity)
                : colour(_colour),
                  ambient(_ambient),
                  diffuse(_diffuse),
                  specular(_specular),
                  shininess(_shininess),
                  reflectivity(_reflectivity) {}
};

// A physical object that light interacts with.
class Object {
 public:
        Vector position;

        // Constructor.
        explicit inline Object(Vector _position)
                : position(_position) {
                // Register object with profiling counter.
                profiling::counters::incObjectsCount();
        }

        // Virtual destructor.
        virtual ~Object() {}

        // Return surface normal at point p.
        virtual Vector normal(Vector p) = 0;
        // Return whether ray intersects object, and if so, at what
        // distance (0 if no intersect).
        virtual Scalar intersect(Ray ray) = 0;
        // Return material at point on surface.
        virtual Material *surface(Vector point) = 0;
};

typedef std::vector<Object *> Objects;

// A plane.
class Plane : public Object {
 public:
        Vector direction;
        Material *restrict material;

        // Constructor.
        inline Plane(Vector _origin,
                     Vector _direction,
                     Material *restrict _material)
                : Object(_origin),
                  direction(_direction.normalise()),
                  material(_material) {}

        virtual inline Vector normal(Vector p) {
                return direction;
        }

        virtual inline Scalar intersect(Ray ray) {
                // Calculate intersection of line and plane.
                Scalar f = (position - ray.position) ^ direction;
                Scalar g = ray.direction ^ direction;
                Scalar t = f / g;

                // Accommodate for precision errors.
                Scalar t0 = t - ScalarPrecision / 2;
                Scalar t1 = t + ScalarPrecision / 2;

                if (t0 > ScalarPrecision)
                        return t0;
                else if (t1 > ScalarPrecision)
                        return t1;
                else
                        return 0;
        }

        virtual inline Material *surface(Vector point) {
                return material;
        }
};

class CheckerBoard : public Plane {
public:
        Material *restrict material1;
        Material *restrict material2;
        Scalar checkerWidth;

        inline CheckerBoard(Vector _origin,
                            Vector _direction,
                            Scalar _checkerWidth,
                            Material *restrict _material1,
                            Material *restrict _material2)
                : Plane(_origin, _direction, nullptr),
                        material1(_material1), material2(_material2),
                        checkerWidth(_checkerWidth) {}

        inline ~CheckerBoard() {}

        virtual inline Material *surface(Vector point) {
                // TODO: translate point to a relative position on plane.
                Vector relative = Vector(point.x + gridOffset,
                                               point.z + gridOffset, 0);

                int x = relative.x;
                int y = relative.y;
                int half = static_cast<int>(checkerWidth * 2);
                int mod = half * 2;

                if (x % mod < half)
                        return y % mod < half ? material1 : material2;
                else
                        return y % mod < half ? material2 : material1;
        }

private:
        static Scalar gridOffset;
};

// A sphere consits of a position and a radius.
class Sphere : public Object {
public:
        Scalar radius;
        Material *restrict material;

        // Constructor.
        inline Sphere(Vector _position,
                      Scalar _radius,
                      Material *restrict _material)
                : Object(_position), radius(_radius), material(_material) {}

        virtual inline Vector normal(Vector p) {
                return (p - position).normalise();
        }

        virtual inline Scalar intersect(Ray ray) {
                // Calculate intersection of line and sphere.
                Vector distance = position - ray.position;
                Scalar b = ray.direction ^ distance;
                Scalar d = b * b + radius * radius - (distance ^ distance);

                if (d < 0)
                        return 0;

                Scalar t0 = b - sqrt(d);
                Scalar t1 = b + sqrt(d);

                if (t0 > ScalarPrecision)
                        return t0;
                else if (t1 > ScalarPrecision)
                        return t1;
                else
                        return 0;
        }

        virtual inline Material *surface(Vector point) {
                return material;
        }
};

}  // namespace rt

#endif  // OBJECTS_H_
