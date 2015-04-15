// -*- c-basic-offset: 8; -*-

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <math.h>

#include "tbb/parallel_for.h"

#include "rt.h"

//////////////////////////
// Configurable Options //
//////////////////////////
#define SEXY 0

// For each pixel at location x,y, we sample N extra points at
// locations randomly distributed about x,y. The sample count
// determines the number of extra rays to trace, and the offset
// determines the maximum distance about the origin.
//
// For softlights, we emit rays at points randomly distributed about
// the light's position. The number of rays emitted is equal to: N =
// (base + radius * factor) ^ 3.
#if SEXY
static const unsigned int MAX_DEPTH           = 100;
static const size_t ANTIALIASING_SAMPLE_COUNT = 8;
static const Scalar ANTIALIASING_OFFSET       = .6;
static const Scalar SOFTLIGHT_FACTOR          = .075;
static const Scalar SOFTLIGHT_BASE            = 3;
static const float RENDER_SCALE               = 3;
#else
static const unsigned int MAX_DEPTH           = 5;
static const size_t ANTIALIASING_SAMPLE_COUNT = 0;
static const Scalar ANTIALIASING_OFFSET       = .6;
static const Scalar SOFTLIGHT_FACTOR          = .01;
static const Scalar SOFTLIGHT_BASE            = 3;
static const float RENDER_SCALE               = 1;
#endif

// Dimensions of "camera" image.
static const int IMG_WIDTH = 750;
static const int IMG_HEIGHT = 422;

// Dimensions of rendered image (output pixels).
static const int RENDER_WIDTH = IMG_WIDTH * RENDER_SCALE;
static const int RENDER_HEIGHT = IMG_HEIGHT * RENDER_SCALE;

////////////////////
// Implementation //
////////////////////

// A profiling counter that keeps track of how many times we've called
// Renderer::trace().
static std::atomic<long long> traceCounter;

// A profiling counter that keeps track of how many times we've
// contributed light to a ray.
static std::atomic<long long> rayCounter;

long long samplesPerRay;

// The random distribution sampler for calculating the offsets of
// stochastic anti-aliasing.
static UniformDistribution sampler = UniformDistribution(-ANTIALIASING_OFFSET,
                                                         ANTIALIASING_OFFSET);

static const unsigned long long rngMax = 4294967295ULL;
const unsigned long long UniformDistribution::longMax = rngMax;
const Scalar UniformDistribution::scalarMax = rngMax;
const unsigned long long UniformDistribution::mult = 62089911ULL;

UniformDistribution::UniformDistribution(const Scalar min, const Scalar max,
                                         const unsigned long long seed)
                : divisor(scalarMax / (max - min)), min(min), seed(seed) {}

Scalar inline UniformDistribution::operator()() {
        seed *= mult;

        // Generate a new random value in the range [0,max - min].
        const double r = seed % longMax / divisor;
        // Apply "-min" offset to value.
        return r + min;
}

Colour::Colour(const int hex)
                : r((hex >> 16) / 255.),
                  g(((hex >> 8) & 0xff) / 255.),
                  b((hex & 0xff) / 255.) {}

Colour::Colour(const float r, const float g, const float b)
                : r(r), g(g), b(b) {}

void Colour::operator+=(const Colour &c) {
        r += c.r;
        g += c.g;
        b += c.b;
}

void Colour::operator/=(const Scalar x) {
        r /= x;
        g /= x;
        b /= x;
}

Colour Colour::operator*(const Scalar x) const {
        return Colour(r * x, g * x, b * x);
}

Colour Colour::operator/(const Scalar x) const {
        return Colour(r / x, g / x, b / x);
}

Colour Colour::operator*(const Colour &rhs) const {
        return Colour(r * rhs.r, g * rhs.g, b * rhs.b);
}

Colour::operator Pixel() const {
        return {scale(clamp(r)), scale(clamp(g)), scale(clamp(b))};
}

Material::Material(const Colour &colour,
                   const Scalar ambient,
                   const Scalar diffuse,
                   const Scalar specular,
                   const Scalar shininess,
                   const Scalar reflectivity)
                : colour(colour),
                  ambient(ambient),
                  diffuse(diffuse),
                  specular(specular),
                  shininess(shininess),
                  reflectivity(reflectivity) {}

Vector::Vector(const Scalar x, const Scalar y, const Scalar z, const Scalar w)
                : x(x), y(y), z(z), w(w) {}

Vector inline Vector::operator+(const Vector &b) const {
        return Vector(x + b.x, y + b.y, z + b.z);
}

Vector inline Vector::operator-(const Vector &b) const {
        return Vector(x - b.x, y - b.y, z - b.z);
}

Vector inline Vector::operator*(const Scalar a) const {
        return Vector(a * x, a * y, a * z);
}

Vector inline Vector::operator/(const Scalar a) const {
        return Vector(x / a, y / a, z / a);
}

Vector inline Vector::operator*(const Vector &b) const {
        return Vector(x * b.x, y * b.y, z * b.z);
}

bool inline Vector::operator==(const Vector &b) const {
        return x == b.x && y == b.y && z == b.z;
}

bool inline Vector::operator!=(const Vector &b) const {
        return !(*this == b);
}

Scalar inline Vector::size() const {
        return sqrt(x * x + y * y + z * z);
}

Scalar inline Vector::product() const {
        return x * y * z;
}

Scalar inline Vector::sum() const {
        return x + y + z;
}

Vector inline Vector::normalise() const {
        return *this / size();
}

Scalar inline Vector::operator^(const Vector &b) const {
        // Dot product uses the forth component.
        return x * b.x + y * b.y + z * b.z + w * b.w;
}

Vector inline Vector::operator|(const Vector &b) const {
        return Vector(y * b.z - z * b.y,
                      z * b.x - x * b.z,
                      x * b.y - y * b.z);
}

Transformation::Transformation(const Vector transformX,
                               const Vector transformY,
                               const Vector transformZ,
                               const Vector translate)
                : transformX(transformX), transformY(transformY),
                  transformZ(transformZ), translate(translate) {}

Transformation Transformation::operator*(const Transformation &b) const {
        const Vector A1 = Vector(transformX.x, transformY.x,
                                 transformZ.x, translate.x);
        const Vector A2 = Vector(transformX.y, transformY.y,
                                 transformZ.y, translate.y);
        const Vector A3 = Vector(transformX.z, transformY.z,
                                 transformZ.z, translate.z);
        const Vector A4 = Vector(transformX.w, transformY.w,
                                 transformZ.w, translate.w);

        return Transformation(
            Vector(A1 ^ b.transformX, A2 ^ b.transformX,
                   A3 ^ b.transformX, A4 ^ b.transformX),
            Vector(A1 ^ b.transformY, A2 ^ b.transformY,
                   A3 ^ b.transformY, A4 ^ b.transformY),
            Vector(A1 ^ b.transformZ, A2 ^ b.transformZ,
                   A3 ^ b.transformZ, A4 ^ b.transformZ),
            Vector(A1 ^ b.translate,  A2 ^ b.translate,
                   A3 ^ b.translate,  A4 ^ b.translate));
}

Vector Transformation::operator*(const Vector &b) const {
        const Vector b1 = Vector(b.x, b.y, b.z, 1);
        const Vector A1 = Vector(transformX.x, transformY.x,
                                 transformZ.x, translate.x);
        const Vector A2 = Vector(transformX.y, transformY.y,
                                 transformZ.y, translate.y);
        const Vector A3 = Vector(transformX.z, transformY.z,
                                 transformZ.z, translate.z);
        const Vector A4 = Vector(transformX.w, transformY.w,
                                 transformZ.w, translate.w);

        return Vector(A1 ^ b1, A2 ^ b1, A3 ^ b1, A4 ^ b1);
}

Translation::Translation(const Scalar x, const Scalar y, const Scalar z)
                : Transformation(Vector(1, 0, 0, 0),
                                 Vector(0, 1, 0, 0),
                                 Vector(0, 0, 1, 0),
                                 Vector(x, y, z, 1)) {}

Translation::Translation(const Vector &t)
                : Transformation(Vector(1, 0, 0, 0),
                                 Vector(0, 1, 0, 0),
                                 Vector(0, 0, 1, 0),
                                 Vector(t.x, t.y, t.z, 1)) {}

Scale::Scale(const Scalar x, const Scalar y, const Scalar z)
                : Transformation(Vector(x, 0, 0, 0),
                                 Vector(0, y, 0, 0),
                                 Vector(0, 0, z, 0),
                                 Vector(0, 0, 0, 1)) {}

Scale::Scale(const Vector &s)
                : Transformation(Vector(s.x, 0, 0, 0),
                                 Vector(0, s.y, 0, 0),
                                 Vector(0, 0, s.z, 0),
                                 Vector(0, 0, 0, 1)) {}

RotationX::RotationX(const Scalar theta)
                : Transformation(Vector(1, 0, 0, 0),
                                 Vector(0, dcos(theta), dsin(theta), 0),
                                 Vector(0, -dsin(theta), dcos(theta), 0),
                                 Vector(0, 0, 0, 1)) {}

RotationY::RotationY(const Scalar theta)
                : Transformation(Vector(dcos(theta), 0, -dsin(theta), 0),
                                 Vector(0, 1, 0, 0),
                                 Vector(dsin(theta), 0, dcos(theta), 0),
                                 Vector(0, 0, 0, 1)) {}

RotationZ::RotationZ(const Scalar theta)
                : Transformation(Vector(dcos(theta), dsin(theta), 0, 0),
                                 Vector(-dsin(theta), dcos(theta), 0, 0),
                                 Vector(0, 0, 1, 0),
                                 Vector(0, 0, 0, 1)) {}

Scalar inline dsin(const Scalar theta) {
        return sin(theta * M_PI / 180.0);
}

Scalar inline dcos(const Scalar theta) {
        return cos(theta * M_PI / 180.0);
}

Scalar inline datan(const Scalar theta) {
        return atan(theta) * 180.0 / M_PI;
}

Object::Object(const Vector &position)
                : position(position) {}

Plane::Plane(const Vector &origin,
             const Vector &direction,
             const Material *const material)
                : Object(origin), direction(direction), material(material) {}

Vector Plane::normal(const Vector &p) const {
        return direction;
}

Scalar Plane::intersect(const Ray &ray) const {
        // Calculate intersection of line and plane.
        const Scalar f = (position - ray.position) ^ direction;
        const Scalar g = ray.direction ^ direction;
        const Scalar t = f / g;

        // Accommodate for precision errors.
        const Scalar t0 = t - ScalarPrecision / 2;
        const Scalar t1 = t + ScalarPrecision / 2;

        if (t0 > ScalarPrecision)
                return t0;
        else if (t1 > ScalarPrecision)
                return t1;
        else
                return 0;
}

const Material *Plane::surface(const Vector &point) const {
        return material;
}

// Checkerboard material types.
static const Material CBLACK = Material(Colour(0x888888), 0, .3, 1, 10, 0.7);
static const Material CWHITE = Material(Colour(0x888888), 0, .3, 1, 10, 0.7);

CheckerBoard::CheckerBoard(const Vector &origin,
                           const Vector &direction,
                           const Scalar checkerWidth)
                : Plane(origin, direction, NULL), black(&CBLACK), white(&CWHITE),
                  checkerWidth(checkerWidth) {}

CheckerBoard::~CheckerBoard() {}

const Material *CheckerBoard::surface(const Vector &point) const {
        // TODO: translate point to a relative position on plane.
        const Vector relative = point;

        const int x = relative.x;
        const int y = relative.y;
        const int half = static_cast<int>(checkerWidth * 2);
        const int mod = half * 2;

        if (x % mod < half)
                return y % mod < half ? black : white;
        else
                return y % mod < half ? white : black;
}

Sphere::Sphere(const Vector &position,
               const Scalar radius,
               const Material *const material)
                : Object(position), radius(radius), material(material) {}

Vector Sphere::normal(const Vector &p) const {
        return (p - position).normalise();
}

Scalar Sphere::intersect(const Ray &ray) const {
        // Calculate intersection of line and sphere.
        const Vector distance = position - ray.position;
        const Scalar b = ray.direction ^ distance;
        const Scalar d = b * b + radius * radius - (distance ^ distance);

        if (d < 0)
                return 0;

        const Scalar t0 = b - sqrt(d);
        const Scalar t1 = b + sqrt(d);

        if (t0 > ScalarPrecision)
                return t0;
        else if (t1 > ScalarPrecision)
                return t1;
        else
                return 0;
}

const Material *Sphere::surface(const Vector &point) const {
        return material;
}

PointLight::PointLight(const Vector &position, const Colour &colour)
                : position(position), colour(colour) {};

Colour PointLight::shade(const Vector &point,
                         const Vector &normal,
                         const Vector &toRay,
                         const Material *const material,
                         const std::vector<const Object *> objects) const {
        // Shading is additive, starting with black.
        Colour shade = Colour();

        // Direction vector from point to light.
        const Vector toLight = (position - point).normalise();
        // Determine with light is blocked.
        const bool blocked = intersects(Ray(point, toLight), objects);
        // Do nothing without line of sight.
        if (blocked)
                return shade;

        // Bump the profiling counter.
        rayCounter++;

        // Product of material and light colour.
        const Colour illumination = colour * material->colour;

        // Apply Lambert (diffuse) shading.
        const Scalar lambert = std::max(normal ^ toLight,
                                        static_cast<Scalar>(0));
        shade += illumination * material->diffuse * lambert;

        // Apply Blinn-Phong (specular) shading.
        const Vector bisector = (toRay + toLight).normalise();
        const Scalar phong = pow(std::max(normal ^ bisector,
                                          static_cast<Scalar>(0)),
                                 material->shininess);
        shade += illumination * material->specular * phong;

        return shade;
}

static UniformDistribution softSampler = UniformDistribution(-1, 1);

SoftLight::SoftLight(const Vector &position, const Scalar radius,
                     const Colour &colour)
                : position(position), radius(radius), colour(colour),
                  samples(SOFTLIGHT_BASE +
                          std::pow(radius * SOFTLIGHT_FACTOR, 3)) {
        samplesPerRay += samples;
};

Colour SoftLight::shade(const Vector &point,
                        const Vector &normal,
                        const Vector &toRay,
                        const Material *const material,
                        const std::vector<const Object *> objects) const {
        // Shading is additive, starting with black.
        Colour shade = Colour();

        // Product of material and light colour.
        const Colour illumination = (colour * material->colour) / samples;

        // Cast multiple light rays, nomrally distributed about the
        // light's centre.
        for (size_t i = 0; i < samples; i++) {
                const Vector origin = Vector(position.x + softSampler() * radius,
                                             position.y + softSampler() * radius,
                                             position.z + softSampler() * radius);

                // Direction vector from point to light.
                const Vector toLight = (origin - point).normalise();
                // Determine whether the light is blocked.
                const bool blocked = intersects(Ray(point, toLight), objects);
                // Do nothing without line of sight.
                if (blocked)
                        continue;

                // Bump the profiling counter.
                rayCounter++;

                // Apply Lambert (diffuse) shading.
                const Scalar lambert = std::max(normal ^ toLight,
                                                static_cast<Scalar>(0));
                shade += illumination * material->diffuse * lambert;

                // Apply Blinn-Phong (specular) shading.
                const Vector bisector = (toRay + toLight).normalise();
                const Scalar phong = pow(std::max(normal ^ bisector,
                                                  static_cast<Scalar>(0)),
                                         material->shininess);
                shade += illumination * material->specular * phong;
        }

        return shade;
}

Scene::Scene(const std::vector<const Object *> &objects,
             const std::vector<const Light *> &lights)
                : objects(objects), lights(lights) {}

Ray::Ray(const Vector &position, const Vector &direction)
                : position(position), direction(direction) {}

Camera::Camera(const Vector &position,
               const Vector &lookAt,
               const size_t width,
               const size_t height)
                : position(position), lookAt(lookAt),
                  direction((lookAt - position).normalise()),
                  width(width), height(height) {}

Renderer::Renderer(const Scene &scene,
                   const Camera &camera)
                : scene(scene), camera(camera) {}

Colour Renderer::supersample(const Ray &ray) const {
        Colour sample = Colour();

        // Trace the origin ray.
        sample += trace(ray);

// For fast builds, we disable antialiasing. This sets
// ANTIALIASING_SAMPLE_COUNT to 0, which causes the compiler to kick
// up a fuss about comparing 0 < 0. Let's disable that warning for
// such builds.
#if !SEXY
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
        // Accumulate extra samples, randomly distributed around x,y.
        for (size_t i = 0; i < ANTIALIASING_SAMPLE_COUNT; i++) {
                const Vector origin = Vector(ray.position.x + sampler(),
                                             ray.position.y + sampler(),
                                             ray.position.z);
                sample += trace(Ray(origin, ray.direction));
        }
#if !SEXY
#pragma GCC diagnostic pop
#endif

        // Average the accumulated samples.
        sample /= ANTIALIASING_SAMPLE_COUNT + 1;

        return sample;
}

void Renderer::render(const Image &image) const {
        const Transformation imageToWorld = imageToGlobalSpace(image, camera);

        // For each pixel in the image:
        tbb::parallel_for(
            static_cast<size_t>(0), image.height * image.width, [&](size_t i) {
                    // Image space coordinates.
                    const size_t y = i / image.width;
                    const size_t x = i % image.width;

                    // Translate image space to global space.
                    const Vector position = imageToWorld * Vector(x, y, 0);

                    // Create a ray.
                    const Ray ray = Ray(position, camera.direction);

                    // Sample the ray.
                    image.set(x, y, supersample(ray));
            });
}

Colour Renderer::trace(const Ray &ray, Colour colour,
                       const unsigned int depth) const {
        // Bump the profiling counter.
        traceCounter++;

        // Determine the closet ray-object intersection.
        Scalar t;
        int index = closestIntersect(ray, scene.objects, t);

        // If the ray doesn't intersect any object, return.
        if (index == -1)
            return colour;

        // Object with closest intersection.
        const Object *object = scene.objects[index];
        // Point of intersection.
        const Vector intersect = ray.position + ray.direction * t;
        // Surface normal at point of intersection.
        const Vector normal = object->normal(intersect);
        // Direction between intersection and source ray.
        const Vector toRay = (ray.position - intersect).normalise();
        // Material at point of intersection.
        const Material *material = object->surface(intersect);

        // Apply ambient lighting.
        colour += material->colour * material->ambient;

        // Apply shading from each light source.
        for (size_t i = 0; i < scene.lights.size(); i++)
                colour += scene.lights[i]->shade(intersect, normal, toRay,
                                                 material, scene.objects);

        // Create reflection ray and recursive evaluate.
        const Scalar reflectivity = material->reflectivity;
        if (depth < MAX_DEPTH && reflectivity > 0) {
                // Direction of reflected ray.
                const Vector reflectionDirection = (normal * 2*(normal ^ toRay) - toRay).normalise();
                // Create a reflection.
                const Ray reflection(intersect, reflectionDirection);
                // Add reflection light.
                colour += trace(reflection, colour, depth + 1) * reflectivity;
        }

        return colour;
}

int closestIntersect(const Ray &ray,
                     const std::vector<const Object *> &objects,
                     Scalar &t) {
        // Index of, and distance to closest intersect:
        int index = -1;
        t = INFINITY;

        // For each object:
        for (size_t i = 0; i < objects.size(); i++) {
                // Get intersect distance.
                Scalar currentT = objects[i]->intersect(ray);

                // Check if intersects, and if so, whether the
                // intersection is closer than the current best.
                if (currentT != 0 && currentT < t) {
                        // New closest intersection.
                        t = currentT;
                        index = static_cast<int>(i);
                }
        }

        return index;
}

bool intersects(const Ray &ray, const std::vector<const Object *> &objects) {
        // Iterate over all objects:
        for (size_t i = 0; i < objects.size(); i++)
                // If the ray intersects object, return true.
                if (objects[i]->intersect(ray) != 0)
                        return true;

        // No intersect.
        return false;
}

Transformation imageToGlobalSpace(const Image &image, const Camera &camera) {
        // Create Scale matrix from image space to local (camera) space.
        const Scalar dX = camera.width / static_cast<Scalar>(image.width);
        const Scalar dY = camera.height / static_cast<Scalar>(image.height);
        const Scale scale(dX, dY, 1);

        // Create rotation matrix from local (camera) space to world space.
        const Scalar oY = camera.lookAt.y - camera.position.y;
        const Scalar aY = camera.lookAt.z - camera.position.z;
        const RotationX rotation(-datan(oY / aY));

        // Determine image space [0,0] position.
        const Vector imageOffset = Vector(image.width / 2, image.height / 2, 0);
        // Create translation matrix from image space to global world space.
        const Translation offset(camera.position -
                                 rotation * scale * imageOffset);

        // Combine the transformations.
        return offset * rotation * scale;
}

Scalar inline clamp(const Scalar x) {
        if (x > 1)
                return 1;
        if (x < 0)
                return 0;
        else
                return x;
}

PixelColourType inline scale(const Scalar x) {
        // Scale value.
        const Scalar scaled = x * static_cast<Scalar>(PixelColourMax);

        // Cast to output type.
        return static_cast<PixelColourType>(scaled);
}

// Return the length of array.
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
// Return the end of an array.
#define ARRAY_END(x) (x + ARRAY_LENGTH(x))

// Program entry point.
int main() {
        // Material parameters:
        //   colour, ambient, diffuse, specular, shininess, reflectivity
        const Material *const green  = new Material(Colour(0x00c805),
                                                    0, 1, .9, 75, 0);
        const Material *const red    = new Material(Colour(0x641905),
                                                    0, 1, .6, 150, 0.25);
        const Material *const mirror = new Material(Colour(0xffffff),
                                                    0, 0, 1, 200, .99999);
        const Material *const grey   = new Material(Colour(0xffffff),
                                                    0, .25, 1, 200, .05);
        const Material *const blue   = new Material(Colour(0x0064c8),
                                                    0, .7, .7, 90, 0);

        // The scene:
        const Object *_objects[] = {
                new CheckerBoard(Vector(0, 0, 0),
                                 Vector(0, 1, 0), 30), // Floor
                new Sphere(Vector(-220,  140, -385), 135, green),  // Green ball
                new Sphere(Vector(-155,   95,  -85), 105, red),    // Red ball
                new Sphere(Vector(  50,   92,    0), 75,  mirror), // Mirror ball
                new Sphere(Vector( 180,   90,   20), 50,  blue),   // Blue ball
                new Sphere(Vector( 290,  270,  -85), 50,  grey),   // Grey ball
                new Sphere(Vector( 290,  170,  -85), 50,  grey),   // Grey ball
                new Sphere(Vector( 290,   70,  -85), 50,  grey)    // Grey ball
        };
        const Light *_lights[] = {
                new SoftLight (Vector( 350, 480,  500), 120, Colour(0xffffff)), // White light
                new SoftLight (Vector(-650, 580,  700),  75, Colour(0x105010)), // Green light
                new SoftLight (Vector(-250, 580, -200),  25, Colour(0x501010)), // Red light
                new PointLight(Vector(-250, 280, -500),      Colour(0x303030))  // Fill light
        };

        // Create the scene.
        const std::vector<const Object *> objects(_objects, ARRAY_END(_objects));
        const std::vector<const Light *> lights(_lights, ARRAY_END(_lights));
        const Scene scene(objects, lights);

        // Setup the camera.
        const Camera camera(Vector(0, 400, 1000), // position
                            Vector(0, 170, 0), // look at
                            IMG_WIDTH, IMG_HEIGHT); // size

        // Create the renderer.
        const Renderer renderer(scene, camera);

        // Create the output image.
        const Image image = Image(RENDER_WIDTH, RENDER_HEIGHT);

        // Print start message.
        printf("Rendering %d pixels, with %lld samples per ray ...\n",
               RENDER_WIDTH * RENDER_HEIGHT, samplesPerRay);

        // Record start time.
        const std::chrono::high_resolution_clock::time_point startTime
                        = std::chrono::high_resolution_clock::now();

        // Render the scene to the output file.
        renderer.render(image);

        // Record end time.
        const std::chrono::high_resolution_clock::time_point endTime
                        = std::chrono::high_resolution_clock::now();

        // Open the output file.
        const char *path = "render.ppm";
        printf("Opening file '%s'...\n", path);
        FILE *const out = fopen(path, "w");

        // Write to output file.
        image.write(out);

        // Close the output file.
        printf("Closing file '%s'...\n\n", path);
        fclose(out);

        // Free heap memory.
        for (size_t i = 0; i < ARRAY_LENGTH(_objects); i++)
                delete _objects[i];
        for (size_t i = 0; i < ARRAY_LENGTH(_lights); i++)
                delete _lights[i];

        // Calculate performance information.
        Scalar elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime).count() / 1e6;
        long long traceCount = static_cast<long long>(traceCounter);
        long long rayCount = static_cast<long long>(rayCounter);
        long long traceRate = traceCount / elapsed;
        long long rayRate = rayCount / elapsed;
        long long pixelRate = RENDER_WIDTH * RENDER_HEIGHT / elapsed;
        Scalar tracePerPixel = static_cast<Scalar>(traceCount)
            / static_cast<Scalar>(RENDER_WIDTH * RENDER_HEIGHT);

        // Print performance summary.
        printf("Rendered %d pixels from %lld traces in %.3f seconds.\n\n",
               RENDER_WIDTH * RENDER_HEIGHT, traceCount, elapsed);
        printf("Render performance:\n");
        printf("\tRays per second:\t%lld\n", rayRate);
        printf("\tTraces per second:\t%lld\n", traceRate);
        printf("\tPixels per second:\t%lld\n", pixelRate);
        printf("\tTraces per pixel:\t%.2f\n", tracePerPixel);

        return 0;
}


Image::Image(const size_t width, const size_t height, const bool inverted)
                : image(new Pixel[width * height]),
                  width(width), height(height), inverted(inverted) {}

Image::~Image() {
        // Free pixel data.
        delete[] image;
}

void inline Image::set(const size_t x, const size_t y,
                       const Colour &value) const {
        // Apply Y axis inversion if needed.
        const size_t row = inverted ? height - 1 - y : y;

        // Explicitly cast colour to pixel data.
        image[row * width + x] = static_cast<Pixel>(value);
}

void Image::write(FILE *const out) const {
        // Print PPM header.
        fprintf(out, "P3\n"); // Magic number
        fprintf(out, "%lu %lu\n", width, height); // Image dimensions
        fprintf(out, "%d\n", PixelColourMax); // Max colour value

        // Iterate over each point in the image, writing pixel data.
        for (size_t i = 0; i < height * width; i++) {
                const Pixel pixel = image[i];
                fprintf(out,
                        PixelFormatString" "
                        PixelFormatString" "
                        PixelFormatString" ",
                        pixel.r, pixel.g, pixel.b);

                if (!i % width) // Add newline at the end of each row.
                        fprintf(out, "\n");
        }
}
