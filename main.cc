#include "rt.h"

#include <memory>

// The maximum depth to trace rays for.
static const unsigned int MAX_DEPTH = 1;


// Clamp a value to within the range [0,255].
uint8_t inline clamp(const double x) {
        const double min = 0;
        const double max = 255;

        return static_cast<uint8_t>(std::max(std::min(x, max), min));
}


// COLOUR

Colour::Colour(const int hex)
                : r(hex >> 16), g((hex >> 8) & 0xff), b(hex & 0xff) {}

Colour::Colour(const float r, const float g, const float b)
                : r(r), g(g), b(b) {}

void Colour::operator+=(const Colour &c) {
        r += c.r;
        g += c.g;
        b += c.b;
}

Colour Colour::operator*(const double x) const {
        return Colour(r * x, g * x, b * x);
}

Colour Colour::operator*(const Colour c) const {
        return Colour(r * (c.r / 255), g * (c.g / 255), b * (c.b / 255));
}

Colour::operator Pixel() const {
        return {clamp(r), clamp(g), clamp(b)};
}



// MATERIAL

Material::Material(const Colour &colour,
                   const double ambient,
                   const double diffuse,
                   const double specular)
                : colour(colour),
                  ambient(ambient),
                  diffuse(diffuse),
                  specular(specular) {}



// VECTOR

Vector::Vector(const double x, const double y, const double z)
                : x(x), y(y), z(z) {}

Vector Vector::operator+(const Vector &b) const {
        return Vector(x + b.x, y + b.y, z + b.z);
}

Vector Vector::operator-(const Vector &b) const {
        return Vector(x - b.x, y - b.y, z - b.z);
}

Vector Vector::operator*(const double a) const {
        return Vector(a * x, a * y, a * z);
}

Vector Vector::operator/(const double a) const {
        return Vector(x / a, y / a, z / a);
}

Vector Vector::operator*(const Vector &b) const {
        return Vector(x * b.x, y * b.y, z * b.z);
}

bool Vector::operator==(const Vector &b) const {
        return x == b.x && y == b.y && z == b.z;
}

bool Vector::operator!=(const Vector &b) const {
        return !(*this == b);
}

double Vector::magnitude() const {
        return sqrt(x * x + y * y + z * z);
}

double Vector::product() const {
        return x * y * z;
}

double Vector::sum() const {
        return x + y + z;
}

Vector Vector::normalise() const {
        return *this / magnitude();
}

double Vector::operator^(const Vector &b) const {
        return x * b.x + y * b.y + z * b.z;
}

Vector Vector::operator|(const Vector &b) const {
        return Vector(y * b.z - z * b.y,
                      z * b.x - x * b.z,
                      x * b.y - y * b.z);
}


// Starting depth of rays.
static const double RAY_START_Z = -1000;
// We use this value to accomodate for rounding errors in the
// intersect() code.
static const double ROUNDING_ERROR = 1e-6;

Object::Object(const Vector &position)
                : position(position) {}

Sphere::Sphere(const Vector &position,
               const double radius,
               const Material *const material)
                : Object(position), radius(radius), material(material) {}

Vector Sphere::normal(const Vector &p) const {
        return (p - position).normalise();
}

double Sphere::intersect(const Ray &ray) const {
        const Vector distance = position - ray.position;
        const double B = ray.direction ^ distance;
        const double D = B * B + radius * radius - (distance ^ distance);

        // No solution.
        if (D < 0)
                return 0;

        const double t0 = B - sqrt(D);
        const double t1 = B + sqrt(D);

        if (t0 > ROUNDING_ERROR)
                return t0;
        else if (t1 > ROUNDING_ERROR)
                return t1;
        else
                return 0;
}

const Material *Sphere::surface(const Vector &point) const {
        return material;
}

// LIGHT

PointLight::PointLight(const Vector &position, const Colour &colour)
                : position(position), colour(colour) {};

Colour PointLight::shade(const Material *const material,
                         const Vector &intersect,
                         const Vector &normal,
                         const Ray &ray,
                         const std::vector<const Object *> objects) const {
        // Shading is additive, starting with black.
        Colour shade = Colour();

        // Direction vector from intersection to light.
        const Vector toLight = (position - intersect).normalise();
        const Ray shadowRay = Ray(intersect, toLight);
        const bool blocked = intersects(shadowRay, objects);

        // Don't apply shading if the light is blocked.
        if (blocked)
                return shade;

        //const Material &material = object->material;

        // Product of material and light colour.
        const Colour illumination = colour * material->colour;

        // Lambert shading.
        const double lambert = std::max(normal ^ toLight,
                                        static_cast<double>(0));
        shade += illumination * material->diffuse * lambert;

        // Blinn-Phong Specular shading.
        const Vector toRay = (ray.position - intersect).normalise();
        const Vector bisector = (toRay + toLight).normalise();
        const double phong = pow(std::max(normal ^ bisector,
                                          static_cast<double>(0)), 40);
        shade += illumination * material->specular * phong;

        return shade;
}

// SCENE


Scene::Scene(const std::vector<const Object *> &objects,
             const std::vector<const Light *> &lights)
                : objects(objects), lights(lights) {}



static const Colour WHITE(255, 255, 255);
static const int WIDTH = 512;
static const int HEIGHT = 512;


Ray::Ray(const double x, const double y)
                : position(Vector(x, y, RAY_START_Z)),
                  direction(Vector(0, 0, 1)) {}

Ray::Ray(const Vector &position, const Vector &direction)
                : position(position), direction(direction) {}

// RENDERER

Renderer::Renderer(const Scene scene)
                : scene(scene), width(WIDTH), height(HEIGHT) {}

void Renderer::render(FILE *const out) const {
        printf("Rendering scene size [%lu x %lu] ...\n", width, height);

        // Image data.
        Pixel image[height][width];

        // For each pixel in the screen:
        for (size_t y = 0; y < height; y++) {
                for (size_t x = 0; x < width; x++) {
                        // Emit a ray.
                        const Ray ray(x, y);
                        // Trace the ray.
                        image[y][x] = static_cast<Pixel>(trace(ray));
                }
        }

        // Once rendering is complete, write data to file.
        fprintf(out, "P3\n"); // PPM Magic number
        fprintf(out, "%lu %lu\n", width, height); // Header line 2
        fprintf(out, "255\n"); // Header line 3: max colour value

        // Iterate over each point in the image, generating and writing
        // pixel data.
        for (size_t y = 0; y < height; y++) {
                for (size_t x = 0; x < width; x++) {
                        const Pixel pixel = image[y][x];
                        fprintf(out, "%u %u %u ", pixel.r, pixel.g, pixel.b);
                }
                fprintf(out, "\n");
        }
}

Colour Renderer::trace(const Ray &ray, Colour colour,
                       const unsigned int depth) const {
        // Do nothing if we have reached the maximum depth.
        if (depth > MAX_DEPTH)
                return colour;

        // Determine the closet ray-object intersection.
        double t;
        int index = closestIntersect(ray, scene.objects, t);

        // Test whether there's an intersect.
        if (index == -1) {
                if (depth == 0)
                        // The ray doesn't intersect anything, so
                        // apply a background gradient.
                        colour += WHITE * (ray.position.y / HEIGHT) * .4;
                return colour;
        }

        // Object with closest intersection.
        const Object *object = scene.objects[index];
        // Point of intersection.
        const Vector intersect = ray.position + ray.direction * t;
        // Surface normal at point of intersection.
        const Vector normal = object->normal(intersect);
        // Material at point of intersection.
        const Material *material = object->surface(intersect);

        // Ambient lighting.
        colour += material->colour * material->ambient;

        // Accumulate each light in turn:
        for (std::vector<const Light *>::const_iterator l = scene.lights.begin();
             l != scene.lights.end(); l++) {
                const Light *light = *l;
                colour += light->shade(material, intersect, normal,
                                       ray, scene.objects);
        }

        return colour;
}

int closestIntersect(const Ray &ray, const std::vector<const Object *> &objects, double &t) {
        int index = -1;
        t = INFINITY; // Distance to closest intersect.

        // For each object:
        for (size_t i = 0; i < objects.size(); i++) {
                const Object *object = objects[i];
                double currentT = object->intersect(ray);

                // Check if intersects, and if so, whether the intersection is
                // closer than the current best.
                if (currentT > 0 && currentT < t) {
                        // New closest intersection.
                        t = currentT;
                        index = static_cast<int>(i);
                }
        }
        return index;
}

bool intersects(const Ray &ray, const std::vector<const Object *> &objects) {
        for (size_t i = 0; i < objects.size(); i++) {
                const Object *const object = objects[i];
                double t = object->intersect(ray);
                if (t > 0)
                        return true;
        }
        return false;
}


// Return the length of array.
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
// Return the end of an array.
#define ARRAY_END(x) (x + ARRAY_LENGTH(x))

// Program entry point.
int main() {
        // Material parameters: colour, ambient, diffuse, specular, shininess
        const Material *const green = new Material(Colour(0x00c805),
                                                   0,  1,  0);
        const Material *const red   = new Material(Colour(0x641905),
                                                   0,  1,  1);
        const Material *const white = new Material(Colour(0xffffff),
                                                   0,  1,  1);
        const Material *const blue  = new Material(Colour(0x0064c8),
                                                   0, .7, .3);

        // The scene:
        const Object *_objects[] = {
                new Sphere(Vector( 95, 250,  300), 75, green), // Green ball
                new Sphere(Vector(150, 250,    0), 75, red),   // Red ball
                new Sphere(Vector(250, 275,  -85), 50, white), // White ball
                new Sphere(Vector(400, 275,    0), 50, blue)   // Blue ball
        };

        const Light *_lights[] = {
                new PointLight(Vector( 800, -200, -300), Colour(0xffffff)),
                new PointLight(Vector(-300, -200, -700), Colour(0x505050))
        };

        // Create the scene and renderer.
        const std::vector<const Object *> objects(_objects, ARRAY_END(_objects));
        const std::vector<const Light *> lights(_lights, ARRAY_END(_lights));
        const Scene scene(objects, lights);
        const Renderer renderer(scene);

        // Output file to write to.
        const char *path = "render.ppm";

        // Open the output file.
        printf("Opening file '%s'...\n", path);
        FILE *const out = fopen(path, "w");

        // Render the scene to the output file.
        renderer.render(out);

        // Close the output file.
        printf("Closing file '%s'...\n", path);
        fclose(out);

        // Free heap memory.
        for (size_t i = 0; i < ARRAY_LENGTH(_objects); i++)
                delete _objects[i];
        for (size_t i = 0; i < ARRAY_LENGTH(_lights); i++)
                delete _lights[i];

        return 0;
}
