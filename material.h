#ifndef MATERIAL_H
#define MATERIAL_H

#include "hittable.h"
#include "texture.h"
#include "point_light.h"

// --- 1. Base Material ---
class material {
  public:
    virtual ~material() = default;

    virtual color emitted(double u, double v, const point3& p) const {
        return color(0,0,0);
    }

    virtual bool scatter(
        const ray& r_in, const hit_record& rec, const hittable& world, 
        const point_light& light, color& direct_light, color& attenuation, ray& scattered
    ) const = 0;
};

// --- 2. Lambertian ---
class lambertian : public material {
  public:
    color albedo;

    lambertian(const color& a) : albedo(a) {}

    static double power_heuristic(double p_f, double p_g) {
        double f2 = p_f * p_f;
        double g2 = p_g * p_g;
        return f2 / (f2 + g2);
    }

    double scattering_pdf(const hit_record& rec, const vec3& direction) const {
        double cosine = dot(rec.normal, unit_vector(direction));
        return cosine < 0.0 ? 0.0 : cosine / M_PI; 
    }

    bool scatter(
        const ray& r_in, const hit_record& rec, const hittable& world, 
        const point_light& light, color& direct_light, color& attenuation, ray& scattered
    ) const override {
        
        direct_light = color(0, 0, 0);
        vec3 offset_p = rec.p + rec.normal * 1e-4;

        // ==========================================
        // STRATEGY 1: DIRECT LIGHT SAMPLING (MIS)
        // ==========================================
        vec3 to_light = light.position - rec.p;
        double dist_to_light_sq = to_light.length_squared();
        vec3 light_dir = unit_vector(to_light);

        if (dot(rec.normal, light_dir) > 0) {
            ray shadow_ray(offset_p, light_dir);
            hit_record shadow_rec;

            double dist_to_light = std::sqrt(dist_to_light_sq);
            if (!world.hit(shadow_ray, interval(0.001, dist_to_light - 1e-4), shadow_rec)) {
                // Point light is a delta distribution — BSDF sampling can never
                // hit it, so MIS weight for the light-sampling strategy is 1.
                double w_light = 1.0;

                color f_r = albedo / M_PI;
                direct_light = w_light * f_r * light.intensity
                             * dot(rec.normal, light_dir) / dist_to_light_sq;
            }
        }

        // ==========================================
        // STRATEGY 2: BSDF SAMPLING
        // ==========================================
        vec3 scatter_direction = rec.normal + random_unit_vector();
        if (scatter_direction.near_zero()) {
            scatter_direction = rec.normal;
        }

        scattered = ray(offset_p, unit_vector(scatter_direction));

        // BSDF-strategy MIS weight: p_light_for_bsdf = 0 because BSDF sampling
        // cannot hit a delta point light, so w_bsdf = 1. Attenuation for
        // cosine-weighted hemisphere sampling simplifies to albedo:
        //   f_r * cosθ / pdf  =  (albedo/π) * cosθ / (cosθ/π)  =  albedo
        attenuation = albedo;
        return true;
    }
};

// --- 3. Dielectric
class dielectric : public material {
  public:
    double ir;

    dielectric(double index_of_refraction) : ir(index_of_refraction) {}

    static double reflectance(double cosine, double ref_idx) {
        auto r0 = (1 - ref_idx) / (1 + ref_idx);
        r0 = r0 * r0;
        return r0 + (1 - r0) * std::pow((1 - cosine), 5);
    }

    bool scatter(
        const ray& r_in, const hit_record& rec, const hittable& world,
        const point_light& light, color& direct_light, color& attenuation, ray& scattered
    ) const override {

        // Dielectric is a delta BSDF. MIS with a delta point light is
        // degenerate on both strategies, so the direct-light contribution is 0.
        // The light is gathered when the refracted/reflected path eventually
        // lands on a diffuse surface.
        direct_light = color(0,0,0);
        attenuation  = color(1.0, 1.0, 1.0);

        // ==========================================
        // SPECULAR / REFRACTIVE SCATTERING
        // ==========================================
        double refraction_ratio = rec.front_face ? (1.0 / ir) : ir;
        vec3 unit_direction = unit_vector(r_in.direction());

        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

        // Total internal reflection condition (sin²θ_t > 1):
        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        vec3 direction;

        if (cannot_refract) {
            // Pure TIR — do not attempt to compute a refraction direction.
            direction = reflect(unit_direction, rec.normal);
        } else if (reflectance(cos_theta, refraction_ratio) > random_double()) {
            direction = reflect(unit_direction, rec.normal);
        } else {
            direction = refract(unit_direction, rec.normal, refraction_ratio);
        }

        // Offset along the geometric normal in the direction of travel to avoid
        // self-intersection (ε = 1e-4 per spec §4.1).
        vec3 offset_normal = dot(direction, rec.normal) > 0 ? rec.normal : -rec.normal;
        scattered = ray(rec.p + offset_normal * 1e-4, direction);
        return true;
    }
};
#endif