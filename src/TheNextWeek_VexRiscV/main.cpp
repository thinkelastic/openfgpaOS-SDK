//==============================================================================================
// Ray Tracing: The Next Week — VexRiscV / openfpgaOS port
//
// Target: VexRiscV RV32IMF core running openfpgaOS (Analogue Pocket)
// Build:  See Makefile in this directory
//
// Based on "Ray Tracing: The Next Week" by Peter Shirley
//   https://raytracing.github.io/books/RayTracingTheNextWeek.html
//
// Ported to:
//   • Use float throughout (hardware-accelerated on RV32F)
//   • Freestanding C++ environment (no exceptions, no RTTI)
//   • openfpgaOS framebuffer API (of_video_*)
//==============================================================================================
#include "of.h"
#include "rtweekend.h"

#include "camera.h"
#include "hittable_list.h"
#include "material.h"
#include "sphere.h"

#define W 320
#define H 240

int main() {
    hittable_list world;

    of_video_init();
    /* Clear framebuffer */
    uint8_t *fb = of_video_surface();
    for (int i = 0; i < W * H * 2; i++) fb[i] = 0;
    of_video_set_color_mode(OF_VIDEO_MODE_RGB565);

    // Ground
    auto ground_material = make_shared<lambertian>(color(0.5f, 0.5f, 0.5f));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000.0f, ground_material));

    // Center sphere
    auto center_material = make_shared<lambertian>(color(0.1f, 0.2f, 0.5f));
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0f, center_material));

    camera cam;
    cam.aspect_ratio      = 4.0f / 3.0f;
    cam.image_width       = W;
    cam.samples_per_pixel = 4;
    cam.max_depth         = 8;

    cam.vfov     = 20.0f;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat   = point3(0, 0, 0);
    cam.vup      = vec3(0, 1, 0);

    cam.defocus_angle = 0.6f;
    cam.focus_dist    = 10.0f;

    cam.render(world);

    while (1) {
        of_input_poll();
        if (of_btn_pressed(OF_BTN_A)) {
            break;
        }
        of_delay_ms(16);
    }

    return 0;
}
