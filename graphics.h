// -*- c-basic-offset: 8; -*-
#ifndef GRAPHICS_H_
#define GRAPHICS_H_

#include "./math.h"

/*
 * Pixels and colour types.
 */

// The output type of a single R,G,B colour component.
typedef uint8_t PixelColourType;

// The maximum value of a single R,G,B colour component.
static const uint8_t PixelColourMax = 255;

// Format string to be passed to fprintf().
#define PixelFormatString "%u"

// Transform a scalar from the range [0,1] to [0,PixelColourMax]. Note
// that this transformation may be non-linear.
PixelColourType inline scale(const Scalar x) {
        return static_cast<PixelColourType>(
            x * static_cast<Scalar>(PixelColourMax));
}

// A pixel is a trio of R,G,B components.
struct Pixel { PixelColourType r, g, b; };

// A colour is represented by R,G,B scalars, and are mutable through
// the += and /= operators. They behave identically to Vectors.
class Colour {
 public:
        Scalar r, g, b;

        // Constructor for specifying colours as 32 bit hex
        // string. E.g. 0xff00aa.
        explicit inline Colour(const int hex)
                        : r((hex >> 16) / 255.),
                        g(((hex >> 8) & 0xff) / 255.),
                        b((hex & 0xff) / 255.) {}

        // Contructor: C = (r,g,b)
        explicit inline Colour(const float r = 0,
                               const float g = 0,
                               const float b = 0)
                        : r(r), g(g), b(b) {}

        // Colour addition.
        void operator+=(const Colour &c) {
                r += c.r;
                g += c.g;
                b += c.b;
        }

        // Scalar division.
        void operator/=(const Scalar x) {
                r /= x;
                g /= x;
                b /= x;
        }

        // Scalar colour multiplication.
        Colour operator*(const Scalar x) const {
                return Colour(r * x, g * x, b * x);
        }

        // Scalar colour divison.
        Colour operator/(const Scalar x) const {
                return Colour(r / x, g / x, b / x);
        }

        // Combination of two colours: A' = (Ar * Br, Ag * Bg, Ab * Bb)
        Colour operator*(const Colour &rhs) const {
                return Colour(r * rhs.r, g * rhs.g, b * rhs.b);
        }

        // Explicit cast operation from Colour -> Pixel.
        explicit operator Pixel() const {
                return {scale(clamp(r)), scale(clamp(g)), scale(clamp(b))};
        }
};

#endif  // GRAPHICS_H_