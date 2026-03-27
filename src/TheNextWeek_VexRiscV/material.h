#ifndef MATERIAL_H
#define MATERIAL_H
//==============================================================================================
// VexRiscV / openfpgaOS port — float version
//
// Changes vs. original:
//   • All double members/parameters/return types → float
//   • Floating-point literals: added 'f' suffix
//   • std::pow((1 - cosine), 5) → std::pow((1.0f - cosine), 5.0f)
//     (avoids int overload ambiguity; std::pow(float,float) from compat/cmath)
//   • std::fmin → std::fmin with float args (from compat/cmath)
//   • std::sqrt  → float overload (from compat/cmath)
//==============================================================================================

#include "hittable.h"


class material {
  public:
    virtual ~material() = default;

    virtual bool scatter(
        const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered
    ) const {
        return false;
    }
};


class lambertian : public material {
  public:
    lambertian(const color& albedo) : albedo(albedo) {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered)
    const override {
        auto scatter_direction = rec.normal + random_unit_vector();

        // Catch degenerate scatter direction
        if (scatter_direction.near_zero())
            scatter_direction = rec.normal;

        scattered  = ray(rec.p, scatter_direction);
        attenuation = albedo;
        return true;
    }

  private:
    color albedo;
};


class metal : public material {
  public:
    metal(const color& albedo, float fuzz) : albedo(albedo), fuzz(fuzz < 1.0f ? fuzz : 1.0f) {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered)
    const override {
        vec3 reflected = reflect(r_in.direction(), rec.normal);
        reflected  = unit_vector(reflected) + (fuzz * random_unit_vector());
        scattered  = ray(rec.p, reflected);
        attenuation = albedo;
        return (dot(scattered.direction(), rec.normal) > 0.0f);
    }

  private:
    color albedo;
    float fuzz;
};


class dielectric : public material {
  public:
    dielectric(float refraction_index) : refraction_index(refraction_index) {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered)
    const override {
        attenuation = color(1.0f, 1.0f, 1.0f);
        float ri = rec.front_face ? (1.0f / refraction_index) : refraction_index;

        vec3  unit_direction = unit_vector(r_in.direction());
        float cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0f);
        float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

        bool  cannot_refract = ri * sin_theta > 1.0f;
        vec3  direction;

        if (cannot_refract || reflectance(cos_theta, ri) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, ri);

        scattered = ray(rec.p, direction);
        return true;
    }

  private:
    float refraction_index;

    static float reflectance(float cosine, float refraction_index) {
        // Schlick approximation for reflectance.
        auto r0 = (1.0f - refraction_index) / (1.0f + refraction_index);
        r0 = r0 * r0;
        return r0 + (1.0f - r0) * std::pow((1.0f - cosine), 5.0f);
    }
};


#endif
