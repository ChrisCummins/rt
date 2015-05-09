# Ray Tracer

A fast parallelised ray tracer written in pure C++11, with support for
soft lighting, adaptive anti-aliasing, perspective and depth of field.

## Installation

Build with `make`. Requires a C++11 capable compiler
(e.g. [g++](http://www.cprogramming.com/g++.html).

## Usage

Include the `rt/rt.h` header and link against the compiled
`src/librt.so` library. For example programs, see
`examples/example1.cc` and `examples/example2.cc`.

## Features

* Diffuse (Lambert) and specular (Phong) shading, and recursive
reflections.
* Fast anti-aliasing using adaptive supersampling.
* Camera abstraction providing focal lengths and aperture.
* Automatic scene code generation using
  [mkscene](https://github.com/ChrisCummins/rt/blob/master/scripts/mkscene.py).

## License

Copyright 2015 Chris Cummins.

Released under the terms of the
[GNU General Public License, Version 3](http://www.gnu.org/copyleft/gpl.html).

This is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your
option) any later version.

This software is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.
