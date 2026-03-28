#ifndef HITTABLE_H
#define HITTABLE_H
//==============================================================================================
// VexRiscV / openfpgaOS port — float version
//
// Changes vs. original:
//   • double t → float t  in hit_record
//==============================================================================================

class material;


class hit_record {
  public:
    point3 p;
    vec3   normal;
    shared_ptr<material> mat;
    float  t;
    bool   front_face;

    void set_face_normal(const ray& r, const vec3& outward_normal) {
        // Sets the hit record normal vector.
        // NOTE: the parameter `outward_normal` is assumed to have unit length.

        front_face = dot(r.direction(), outward_normal) < 0.0f;
        normal = front_face ? outward_normal : -outward_normal;
    }
};


class hittable {
  public:
    virtual ~hittable() = default;

    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const = 0;
};


#endif
