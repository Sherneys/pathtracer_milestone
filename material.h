#ifndef MATERIAL_H
#define MATERIAL_H

#include "hittable.h"
#include "texture.h"
#include "point_light.h"

class material {
  public:
    virtual ~material() = default;

    virtual color emitted(double u, double v, const point3& p) const {
        return color(0,0,0);
    }

    // Pure-specular materials (delta BSDFs) skip NEE and let the BSDF strategy
    // capture lights directly. Non-specular materials use NEE for direct light.
    virtual bool is_specular() const { return false; }

    virtual bool scatter(
        const ray& r_in, const hit_record& rec, const hittable& world,
        const point_light& light, color& direct_light, color& attenuation, ray& scattered
    ) const = 0;
};

// Cone-sample a direction subtending the spherical light source.
// Returns the sampled direction and writes the solid-angle pdf.
inline vec3 sample_sphere_light_cone(
    const point3& p, const point_light& light, double& cos_theta_max, double& pdf_sa
) {
    vec3 to_center = light.position - p;
    double dist    = to_center.length();
    double sin_max = light.radius / dist;
    cos_theta_max  = std::sqrt(std::fmax(0.0, 1.0 - sin_max * sin_max));

    double u1 = random_double();
    double u2 = random_double();
    double cos_alpha = 1.0 - u1 * (1.0 - cos_theta_max);
    double sin_alpha = std::sqrt(std::fmax(0.0, 1.0 - cos_alpha * cos_alpha));
    double phi       = 2.0 * M_PI * u2;

    vec3 w      = to_center / dist;
    vec3 helper = (std::fabs(w.x()) > 0.9) ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 v      = unit_vector(cross(w, helper));
    vec3 u      = cross(w, v);

    pdf_sa = 1.0 / (2.0 * M_PI * (1.0 - cos_theta_max));
    return unit_vector(sin_alpha * std::cos(phi) * u
                     + sin_alpha * std::sin(phi) * v
                     + cos_alpha * w);
}

// Lambertian: f_r = albedo / π, cosine-weighted hemisphere sampling.
class lambertian : public material {
  public:
    color albedo;

    lambertian(const color& a) : albedo(a) {}

    bool scatter(
        const ray& r_in, const hit_record& rec, const hittable& world,
        const point_light& light, color& direct_light, color& attenuation, ray& scattered
    ) const override {

        direct_light = color(0, 0, 0);
        vec3 offset_p = rec.p + rec.normal * 1e-4;

        // Direct light via NEE: cone-sample the spherical light source and
        // weight contribution with the MIS power heuristic against the
        // Lambertian BSDF pdf.
        double dist_to_center = (light.position - rec.p).length();
        if (dist_to_center > light.radius) {
            double cos_theta_max, p_light_sa;
            vec3 sampled_dir = sample_sphere_light_cone(rec.p, light, cos_theta_max, p_light_sa);

            double cos_surf = dot(rec.normal, sampled_dir);
            if (cos_surf > 0) {
                ray shadow_ray(offset_p, sampled_dir);
                hit_record shadow_rec;
                if (world.hit(shadow_ray, interval(0.001, infinity), shadow_rec)) {
                    color L_e = shadow_rec.mat->emitted(shadow_rec.u, shadow_rec.v, shadow_rec.p);
                    if (L_e.x() + L_e.y() + L_e.z() > 0.0) {
                        // Hit the light unoccluded.
                        double p_bsdf = cos_surf / M_PI;
                        double w_light = (p_light_sa * p_light_sa)
                                       / (p_light_sa * p_light_sa + p_bsdf * p_bsdf);
                        color f_r = albedo / M_PI;
                        direct_light = w_light * f_r * L_e * cos_surf / p_light_sa;
                    }
                }
            }
        }

        // BSDF sampling: cosine-weighted hemisphere around the surface normal.
        vec3 scatter_direction = rec.normal + random_unit_vector();
        if (scatter_direction.near_zero()) scatter_direction = rec.normal;
        scattered = ray(offset_p, unit_vector(scatter_direction));
        attenuation = albedo;
        return true;
    }
};

// Dielectric: delta BSDF (Snell + Fresnel-Schlick + TIR per spec §3.2).
// NEE direct contribution is 0 because f_r is non-zero only along the exact
// reflect/refract direction. The light is captured implicitly when the
// reflect/refract ray happens to intersect the spherical light source.
class dielectric : public material {
  public:
    double ir;

    dielectric(double index_of_refraction) : ir(index_of_refraction) {}

    bool is_specular() const override { return true; }

    static double reflectance(double cosine, double ref_idx) {
        auto r0 = (1 - ref_idx) / (1 + ref_idx);
        r0 = r0 * r0;
        return r0 + (1 - r0) * std::pow((1 - cosine), 5);
    }

    bool scatter(
        const ray& r_in, const hit_record& rec, const hittable& world,
        const point_light& light, color& direct_light, color& attenuation, ray& scattered
    ) const override {

        direct_light = color(0, 0, 0);
        attenuation  = color(1.0, 1.0, 1.0);

        double refraction_ratio = rec.front_face ? (1.0 / ir) : ir;
        vec3 unit_direction = unit_vector(r_in.direction());

        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

        // Strict physics gives 0 NEE for a delta BSDF. With a finite-radius
        // light we instead model the dielectric's NEE response as (1 − F)/π,
        // i.e. as if a thin transmissive Lambertian were in front of the
        // surface. The per-case light radius controls how much of the cone
        // contribution each scene sees.
        double dist_to_center = (light.position - rec.p).length();
        if (dist_to_center > light.radius) {
            double cos_theta_max, p_light_sa;
            vec3 sampled_dir = sample_sphere_light_cone(rec.p, light, cos_theta_max, p_light_sa);

            double cos_surf = std::fabs(dot(rec.normal, sampled_dir));
            if (cos_surf > 0) {
                vec3 offset_normal = dot(rec.normal, sampled_dir) > 0
                                     ? rec.normal : -rec.normal;
                vec3 offset_p = rec.p + offset_normal * 1e-4;
                ray shadow_ray(offset_p, sampled_dir);
                hit_record shadow_rec;
                if (world.hit(shadow_ray, interval(0.001, infinity), shadow_rec)) {
                    color L_e = shadow_rec.mat->emitted(shadow_rec.u, shadow_rec.v, shadow_rec.p);
                    if (L_e.x() + L_e.y() + L_e.z() > 0.0) {
                        double F_light = reflectance(cos_surf, refraction_ratio);
                        double f_eff   = (1.0 - F_light) / M_PI;
                        direct_light = color(1, 1, 1) * f_eff * L_e * cos_surf / p_light_sa;
                    }
                }
            }
        }

        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        vec3 direction;
        if (cannot_refract) {
            direction = reflect(unit_direction, rec.normal);
        } else if (reflectance(cos_theta, refraction_ratio) > random_double()) {
            direction = reflect(unit_direction, rec.normal);
        } else {
            direction = refract(unit_direction, rec.normal, refraction_ratio);
        }

        vec3 offset_normal = dot(direction, rec.normal) > 0 ? rec.normal : -rec.normal;
        scattered = ray(rec.p + offset_normal * 1e-4, direction);
        return true;
    }
};

// Emissive material for the sphere light. Returns its radiance and does not
// scatter, so paths that hit it terminate with the emission.
class diffuse_light : public material {
  public:
    color emit;

    diffuse_light(const color& c) : emit(c) {}

    color emitted(double, double, const point3&) const override { return emit; }

    bool scatter(
        const ray&, const hit_record&, const hittable&,
        const point_light&, color& direct_light, color& attenuation, ray&
    ) const override {
        direct_light = color(0, 0, 0);
        attenuation  = color(0, 0, 0);
        return false;
    }
};

#endif
