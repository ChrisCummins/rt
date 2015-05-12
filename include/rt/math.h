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
#ifndef RT_MATH_H_
#define RT_MATH_H_

#include <cmath>

namespace rt {

/*
 * Maths. can't live with it, can't live without it. For ray tracing,
 * we're interested in vector and matrix manipulation. Both vector and
 * matrix types are immutable, and entirely inline. This means there's
 * no implementation file.
 */

// Changing between different floating point sizes for scalar values
// will affect the system's performance.
typedef double Scalar;

// The "rounding error" to accomodate for when approximate infinite
// precision real numbers.
static const Scalar ScalarPrecision = 1e-6;

namespace radians {
        // Conversion from radians to degrees.
        Scalar inline toDegrees(Scalar radians) {
                return radians * M_PI / 180.0;
        }
}  // namespace radians

namespace deg {
        // Trigonometric functions accepting theta angles in degrees
        // rather than radians:

        Scalar inline sin(Scalar theta) {
                return sin(radians::toDegrees(theta));
        }

        Scalar inline cos(Scalar theta) {
                return cos(radians::toDegrees(theta));
        }
}  // namespace deg

// Clamp a Scalar value to within the range [0,1].
Scalar inline clamp(Scalar x) {
        if (x > 1)
                return 1;
        if (x < 0)
                return 0;
        else
                return x;
}

// A vector consists of three coordinates and a translation
// scalar. Vectors are immutable.
class Vector {
 public:
        Scalar x;
        Scalar y;
        Scalar z;
        Scalar w;

        // Contructor: V = (x,y,z,w)
        inline Vector(Scalar _x, Scalar _y, Scalar _z,
                      Scalar _w = 0) : x(_x), y(_y), z(_z), w(_w) {}

        // Addition: A' = A + B
        Vector inline operator+(Vector b) {
                return Vector(x + b.x, y + b.y, z + b.z);
        }

        // Subtraction: A' = A - B
        Vector inline operator-(Vector b) {
                return Vector(x - b.x, y - b.y, z - b.z);
        }

        // Multiplication: A' = aA
        Vector inline operator*(Scalar a) {
                return Vector(a * x, a * y, a * z);
        }

        // Division: A' = A / a
        Vector inline operator/(Scalar a) {
                return Vector(x / a, y / a, z / a);
        }

        // Product: A' = (Ax * Bx, Ay * By, Az * Bz)
        Vector inline operator*(Vector b) {
                return Vector(x * b.x, y * b.y, z * b.z);
        }

        // Dot product: x = A . B
        Scalar inline operator^(Vector b) {
                // Dot product uses the forth component.
                return x * b.x + y * b.y + z * b.z + w * b.w;
        }

        // Cross product: A' = A x B
        Vector inline operator|(Vector b) {
                return Vector(y * b.z - z * b.y,
                              z * b.x - x * b.z,
                              x * b.y - y * b.z);
        }

        // Equality: A == B
        bool inline operator==(Vector b) {
                return x == b.x && y == b.y && z == b.z;
        }

        // Inequality: A != B
        bool inline operator!=(Vector b) {
                return !(*this == b);
        }

        // Length of vector: |A| = sqrt(x^2 + y^2 + z^2)
        Scalar inline size() {
                return sqrt(x * x + y * y + z * z);
        }

        // Product of components: x * y * z
        Scalar inline product() {
                return x * y * z;
        }

        // Sum of components: x + y + z
        Scalar inline sum() {
                return x + y + z;
        }

        // Normalise: A' = A / |A|
        Vector inline normalise() {
                return *this / size();
        }
};

// A 4x4 matrix. Matrices are immutable, and while declared row-wise,
// they store both row-wise and column-wise vectors internally for
// quick row or column-wise indexing.
class Matrix {
 public:
        // Row-wise vectors.
        Vector r[4];
        // Column-wise vectors.
        Vector c[4];

        inline Matrix(Vector r1,
                      Vector r2,
                      Vector r3,
                      Vector r4)
               : r({r1, r2, r3, r4}),
                 c({Vector(r1.x, r2.x, r3.x, r4.x),
                    Vector(r1.y, r2.y, r3.y, r4.y),
                    Vector(r1.z, r2.z, r3.z, r4.z),
                    Vector(r1.w, r2.w, r3.w, r4.w)}) {}

        // Matrix multiplication.
        Matrix inline operator*(Matrix b) {
                return Matrix(
                    Vector(r[0] ^ b.c[0], r[0] ^ b.c[1],
                           r[0] ^ b.c[2], r[0] ^ b.c[3]),
                    Vector(r[1] ^ b.c[0], r[1] ^ b.c[1],
                           r[1] ^ b.c[2], r[1] ^ b.c[3]),
                    Vector(r[2] ^ b.c[0], r[2] ^ b.c[1],
                           r[2] ^ b.c[2], r[2] ^ b.c[3]),
                    Vector(r[3] ^ b.c[0], r[3] ^ b.c[1],
                           r[3] ^ b.c[2], r[3] ^ b.c[3]));
        }

        // Matrix by vector multiplication.
        Vector inline operator*(Vector b) {
                // Pad the "w" component.
                Vector v = Vector(b.x, b.y, b.z, 1);
                return Vector(r[0] ^ v, r[1] ^ v, r[2] ^ v, r[3] ^ v);
        }

        // Scalar multiplication.
        Matrix inline operator*(Scalar a) {
                return Matrix(r[0] * a, r[1] * a, r[2] * a, r[3] * a);
        }
};

// A translation matrix.
class Translation : public Matrix {
 public:
        inline Translation(Scalar x, Scalar y, Scalar z)
                        : Matrix(Vector(1, 0, 0, x),
                                 Vector(0, 1, 0, y),
                                 Vector(0, 0, 1, z),
                                 Vector(0, 0, 0, 1)) {}
        explicit inline Translation(Vector t)
                        : Matrix(Vector(1, 0, 0, t.x),
                                 Vector(0, 1, 0, t.y),
                                 Vector(0, 0, 1, t.z),
                                 Vector(0, 0, 0, 1)) {}
};

// A scale matrix.
class Scale : public Matrix {
 public:
        inline Scale(Scalar x, Scalar y, Scalar z)
                        : Matrix(Vector(x, 0, 0, 0),
                                 Vector(0, y, 0, 0),
                                 Vector(0, 0, z, 0),
                                 Vector(0, 0, 0, 1)) {}

        explicit inline Scale(Vector w)
                        : Matrix(Vector(w.x, 0, 0, 0),
                                 Vector(0, w.y, 0, 0),
                                 Vector(0, 0, w.z, 0),
                                 Vector(0, 0, 0, 1)) {}
};

// A rotation matrix about the X axis.
class RotationX : public Matrix {
 public:
        explicit inline RotationX(Scalar theta)
                       : Matrix(Vector(1, 0, 0, 0),
                                Vector(0, deg::cos(theta), -deg::sin(theta), 0),
                                Vector(0, deg::sin(theta), deg::cos(theta), 0),
                                Vector(0, 0, 0, 1)) {}
};

// A rotation matrix about the Y axis.
class RotationY : public Matrix {
 public:
        explicit inline RotationY(Scalar theta)
                       : Matrix(Vector(deg::cos(theta), 0, deg::sin(theta), 0),
                                Vector(0, 1, 0, 0),
                                Vector(-deg::sin(theta), 0, deg::cos(theta), 0),
                                Vector(0, 0, 0, 1)) {}
};

// A rotation matrix about the Z axis.
class RotationZ : public Matrix {
 public:
        explicit inline RotationZ(Scalar theta)
                       : Matrix(Vector(deg::cos(theta), -deg::sin(theta), 0, 0),
                                Vector(deg::sin(theta), deg::cos(theta), 0, 0),
                                Vector(0, 0, 1, 0),
                                Vector(0, 0, 0, 1)) {}
};

// Yaw, pitch, roll rotation.
Matrix inline rotation(Scalar x, Scalar y, Scalar z) {
        Matrix Z = RotationZ(z);
        Matrix Y = RotationY(y);
        Matrix X = RotationX(x);
        return Z * Y * X;
}

}  // namespace rt

#endif  // RT_MATH_H_
