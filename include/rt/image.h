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
#ifndef RT_IMAGE_H_
#define RT_IMAGE_H_

#include <vector>

#include "rt/graphics.h"
#include "rt/restrict.h"

namespace rt {

namespace image {

// Helper functions to convert 2D to 1D flat array co-ordinates, and
// vice versa.

inline size_t index(const size_t x, const size_t y, const size_t width) {
        return y * width + x;
}

inline size_t x(const size_t index, const size_t width) {
        return index % width;
}

inline size_t y(const size_t index, const size_t width) {
        return index / width;
}

}  // namespace image

// A rendered image.
class Image {
 public:
        std::vector<Pixel> data;
        const size_t width;
        const size_t height;
        const size_t size;
        const Scalar saturation;
        const Colour gamma;
        const bool inverted;

        Image(const size_t width, const size_t height,
              const Scalar saturation = 1,
              const Colour gamma = Colour(1, 1, 1),
              const bool inverted = true);

        ~Image();

        // [x,y] = value
        auto inline set(const size_t x,
                        const size_t y,
                        const Colour &value) {
                // Apply Y axis inversion if needed.
                const size_t row = inverted ? height - 1 - y : y;
                // Convert 2D coordinates to flat array index.
                _set(image::index(x, row, width), value);
        }

        // [index] = value
        auto inline set(const size_t index,
                        const Colour &value) {
                const size_t x = image::x(index, width);
                const size_t y = image::y(index, width);

                set(x, y, value);
        }

        auto index(const size_t x, const size_t y) {
                return image::index(x, y, width);
        }

        auto x(const size_t index) {
                return image::x(index, width);
        }

        auto y(const size_t index) {
                return image::y(index, width);
        }

        friend auto& operator<<(std::ostream& out, const Image &image) {
                // Print PPM header.

                // Magic number:
                out << "P3" << std::endl;
                // Image dimensions:
                out << image.width << " " << image.height << std::endl;
                // Max colour value:
                out << unsigned(Pixel::ComponentMax) << std::endl;

                // Iterate over each point in the image, writing pixel data.
                for (size_t i = 0; i < image.size; i++) {
                        const Pixel pixel = image.data[i];
                        out << pixel << " ";

                        // Add newline at the end of each row:
                        if (!i % image.width)
                                out << std::endl;
                }

                return out;
        }

 private:
#pragma GCC diagnostic push  // Ignore unused "_pad" variable.
#pragma GCC diagnostic ignored "-Wunused-private-field"
        // Padding bytes since the "inverted" member bool is only a
        // single byte.
        char _pad[7];
#pragma GCC diagnostic pop  // Ignore unused "_pad" variable.

        void _set(const size_t i,
                  const Colour &value);
};

}  // namespace rt

#endif  // RT_IMAGE_H_
