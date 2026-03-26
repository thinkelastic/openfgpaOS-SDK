#ifndef CAMERA_H
#define CAMERA_H
//==============================================================================================
// VexRiscV / openfpgaOS port — float version
//
// Changes vs. original:
//   • All double members/parameters/return types → float
//   • Floating-point literals: added 'f' suffix
//   • std::clog replaced by std::cerr
//     (std::clog is not defined in the SDK's minimal iostream)
//   • Default image_width reduced to 160 (suitable for embedded target)
//   • Default samples_per_pixel reduced to 4
//   • Default max_depth reduced to 8
//   • std::tan, std::sqrt, std::fabs all use float overloads from compat/cmath
//
// Output:
//   Renders to stdout as PPM (plain text).  On VexRiscV this goes to fd 1
//   (usually a UART or debug channel).  For on-screen display, integrate
//   with of_video.h (see PORTING_VEXRISCV.md for details).
//==============================================================================================

#include "hittable.h"
#include "material.h"


class camera {
  public:
    float aspect_ratio      = 1.0f;  // Ratio of image width over height
    int   image_width       = 160;   // Rendered image width (pixels) — reduced for embedded
    int   samples_per_pixel = 4;     // Samples per pixel — reduced for embedded
    int   max_depth         = 8;     // Max ray bounces — reduced for embedded

    float vfov     = 90.0f;                // Vertical field of view
    point3 lookfrom = point3(0,0,0);       // Point camera is looking from
    point3 lookat   = point3(0,0,-1);      // Point camera is looking at
    vec3   vup      = vec3(0,1,0);         // Camera-relative "up" direction

    float defocus_angle = 0.0f;  // Variation angle of rays through each pixel
    float focus_dist    = 10.0f; // Distance from lookfrom to focus plane

    void render(const hittable& world) {
        uint16_t *fb16;
        initialize();

        //std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";
        fb16 = of_video_surface16();
        for (int j = 0; j < image_height; j++) {
            //std::cerr << "\rScanlines remaining: " << (image_height - j) << ' ';
            for (int i = 0; i < image_width; i++) {
                color pixel_color(0,0,0);
                for (int sample = 0; sample < samples_per_pixel; sample++) {
                    ray r = get_ray(i, j);
                    pixel_color += ray_color(r, max_depth, world);
                }
                //write_color(std::cout, pixel_samples_scale * pixel_color);
                *fb16 = write_color_val(pixel_samples_scale * pixel_color);
                fb16++;
            }

            
        }
        of_video_flip();
        //std::cerr << "\rDone.                 \n";
    }

  private:
    int    image_height;
    float  pixel_samples_scale;
    point3 center;
    point3 pixel00_loc;
    vec3   pixel_delta_u;
    vec3   pixel_delta_v;
    vec3   u, v, w;
    vec3   defocus_disk_u;
    vec3   defocus_disk_v;

    void initialize() {
        image_height = int((float)image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        pixel_samples_scale = 1.0f / (float)samples_per_pixel;

        center = lookfrom;

        // Determine viewport dimensions.
        auto theta          = degrees_to_radians(vfov);
        auto h              = std::tan(theta / 2.0f);
        auto viewport_height = 2.0f * h * focus_dist;
        auto viewport_width  = viewport_height * ((float)image_width / (float)image_height);

        // Calculate the u,v,w unit basis vectors for the camera coordinate frame.
        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        // Calculate the vectors across the horizontal and down the vertical viewport edges.
        vec3 viewport_u =  viewport_width  * u;
        vec3 viewport_v = -viewport_height * v;

        // Calculate the horizontal and vertical delta vectors from pixel to pixel.
        pixel_delta_u = viewport_u / (float)image_width;
        pixel_delta_v = viewport_v / (float)image_height;

        // Calculate the location of the upper left pixel.
        auto viewport_upper_left =
            center - (focus_dist * w) - viewport_u / 2.0f - viewport_v / 2.0f;
        pixel00_loc = viewport_upper_left + 0.5f * (pixel_delta_u + pixel_delta_v);

        // Calculate the camera defocus disk basis vectors.
        auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2.0f));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    ray get_ray(int i, int j) const {
        auto offset       = sample_square();
        auto pixel_sample = pixel00_loc
                          + ((float)(i) + offset.x()) * pixel_delta_u
                          + ((float)(j) + offset.y()) * pixel_delta_v;

        auto ray_origin    = (defocus_angle <= 0.0f) ? center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;

        return ray(ray_origin, ray_direction);
    }

    vec3 sample_square() const {
        return vec3(random_double() - 0.5f, random_double() - 0.5f, 0.0f);
    }

    vec3 sample_disk(float radius) const {
        return radius * random_in_unit_disk();
    }

    point3 defocus_disk_sample() const {
        auto p = random_in_unit_disk();
        return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }

    color ray_color(const ray& r, int depth, const hittable& world) const {
        if (depth <= 0)
            return color(0,0,0);

        hit_record rec;

        if (world.hit(r, interval(0.001f, infinity), rec)) {
            ray   scattered;
            color attenuation;
            if (rec.mat->scatter(r, rec, attenuation, scattered))
                return attenuation * ray_color(scattered, depth - 1, world);
            return color(0,0,0);
        }

        vec3 unit_direction = unit_vector(r.direction());
        auto a = 0.5f * (unit_direction.y() + 1.0f);
        return (1.0f - a) * color(1.0f, 1.0f, 1.0f) + a * color(0.5f, 0.7f, 1.0f);
    }
};


#endif
