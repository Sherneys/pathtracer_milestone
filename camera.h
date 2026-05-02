#ifndef CAMERA_H
#define CAMERA_H

#include "hittable.h"
#include "material.h"
#include "point_light.h"
#include <iomanip>


class camera {
    public:
        double rr_prob = 1.0;
        double aspect_ratio = 1.0;  // Ratio of image width over height
        int    image_width  = 100;  // Rendered image width in pixel count
        int    samples_per_pixel = 10; // Count of random samples for each pixel
        int    max_depth    = 10; // Maximum number of ray bounces into scene
        color background; // Scene background color

        double vfov = 90; // Vertical view angle (field of view)
        point3 lookfrom = point3(0,0,0); // Point camera is looking from
        point3 lookat = point3(0,0,-1); // Point camera is looking at
        vec3 vup = vec3(0,1,0);

        double defocus_angle = 0; // Variation angle of rays through each pixel
        double focus_dist = 10; // Distance from camera lookfrom point to plane of perfect focus

        void render_test_case(const ray& primary_ray, const hittable& world, const point_light& light) const {
            // Per spec §2.1 / §7: reproducible output requires a fixed mt19937
            // seed. Scene construction (e.g. bvh_node) may or may not consume
            // random numbers, so we reseed *here*, right before the Monte
            // Carlo loop, to make the render independent of any RNG consumption
            // that happened during scene setup.
            get_rng().seed(12345);

            color total_radiance(0, 0, 0);

            for (int s = 0; s < samples_per_pixel; ++s) {
                color sample_color = ray_color(primary_ray, max_depth, world, light);
                if (sample_color.x() != sample_color.x()) sample_color = color(0,0,0);
                total_radiance += sample_color;
            }

            color final_radiance = total_radiance / samples_per_pixel;

            std::cout << std::fixed << std::setprecision(6);
            std::cout << final_radiance.x() << " " 
                      << final_radiance.y() << " " 
                      << final_radiance.z() << "\n";
        }

        // void render (const hittable& world) {
        //     initialize();

        //     std::cout << "P3\n" << image_width << " " << image_height << "\n255\n";

        //     for(int j=0;j<image_height;j++){
        //         std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;
        //         for(int i=0;i<image_width;i++){
        //             color pixel_color(0,0,0);
        //             for(int sample = 0; sample < samples_per_pixel; sample++) {
        //                 ray r = get_ray(i, j);
        //                 pixel_color += ray_color(r, max_depth, world);
        //             }
        //             write_color(std::cout, pixel_samples_scale * pixel_color);
        //         }
        //     }

        //     std::clog << "\rDone.                 \n";
        // }
        
    private:
        int    image_height;   // Rendered image height
        double pixel_samples_scale; // Color scale factor for a sum of pixel samples
        point3 center;         // Camera center
        point3 pixel00_loc;    // Location of pixel 0, 0
        vec3   pixel_delta_u;  // Offset to pixel to the right
        vec3   pixel_delta_v;  // Offset to pixel below
        vec3   u, v, w;        // Camera frame basis vectors
        vec3 defocus_disk_u;   // Defocus disk horizontal radius
        vec3 defocus_disk_v;   // Defoucs disk vertical radius

        void initialize() {
            image_height = int(image_width / aspect_ratio);
            image_height = (image_height < 1) ? 1 : image_height;

            pixel_samples_scale = 1.0 / samples_per_pixel;

            center = lookfrom;

            // Determine viewport dimensions.
            auto theta = degrees_to_radians(vfov);
            auto h = std::tan(theta/2);
            auto viewport_height = 2 * h * focus_dist;
            auto viewport_width = viewport_height * (double(image_width)/image_height);

            // Calculate the u,v,w unit basis vectors for the camera coordinate frame.
            w = unit_vector(lookfrom - lookat);
            u = unit_vector(cross(vup, w));
            v = cross(w, u);

            // Calculate the vectors across the horizontal and down the vertical viewport edges.
            vec3 viewport_u = viewport_width * u; // Vector across viewport horizontal edge
            vec3 viewport_v = viewport_height * -v; // Vector down viewport vertical edge

            // Calculate the horizontal and vertical delta vectors from pixel to pixel.
            pixel_delta_u = viewport_u / image_width;
            pixel_delta_v = viewport_v / image_height;

            // Calculate the location of the upper left pixel.
            auto viewport_upper_left = center - (focus_dist * w) - viewport_u/2 - viewport_v/2;
            pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);
        
            // Calculate the camera defocus disk basis vectors.
            auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
            defocus_disk_u = u * defocus_radius;
            defocus_disk_v = -v * defocus_radius;
        }

        ray get_ray(int i, int j) const {
            // Construct a camera ray originating from the defocus disk and directed at a randomly
            // sampled point around the pixel location i, j.

            auto offset = sample_square();
            auto pixel_sample = pixel00_loc
                              + ((i + offset.x()) * pixel_delta_u)
                              + ((j + offset.y()) * pixel_delta_v);

            auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
            auto ray_direction = pixel_sample - ray_origin;
            auto ray_time = random_double();

            return ray(ray_origin, ray_direction, ray_time);
        }

        vec3 sample_square() const {
            // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
            return vec3(random_double() - 0.5, random_double() - 0.5, 0);
        }

        point3 defocus_disk_sample() const {
            // Returns a random point in the camera defocus disk;
            auto p = random_in_unit_disk();
            return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
        }

        color ray_color(const ray& r, int depth, const hittable& world, const point_light& light,
                        bool last_was_specular = true) const {
            if (depth <= 0)
                return color(0,0,0);

            hit_record rec;
            if (!world.hit(r, interval(0.001, infinity), rec))
                return background;

            color color_from_emission = rec.mat->emitted(rec.u, rec.v, rec.p);
            bool hit_emissive = (color_from_emission.x() + color_from_emission.y()
                               + color_from_emission.z()) > 0.0;

            // If a non-specular bounce produced this ray, NEE at the previous
            // hit already accounted for direct illumination from the light, so
            // counting the emission here too would double-count. Specular and
            // primary bounces don't run NEE for this surface, so they keep the
            // full emission.
            if (hit_emissive) {
                return last_was_specular ? color_from_emission : color(0,0,0);
            }

            ray scattered;
            color attenuation;
            color direct_light(0,0,0);

            if (!rec.mat->scatter(r, rec, world, light, direct_light, attenuation, scattered))
                return color_from_emission;

            // Russian roulette (spec §6): RR applies once we are past the
            // primary ray (path depth ≥ 1).
            double rr_factor = 1.0;
            if (depth < max_depth) {
                if (random_double() > rr_prob) {
                    return color_from_emission + direct_light;
                }
                rr_factor = 1.0 / rr_prob;
            }

            color color_from_scatter = attenuation
                * ray_color(scattered, depth-1, world, light, rec.mat->is_specular());

            return color_from_emission + direct_light + rr_factor * color_from_scatter;
        }
};

#endif