#ifndef HITTABLE_LIST_H
#define HITTABLE_LIST_H
//==============================================================================================
// VexRiscV / openfpgaOS port — fixed-capacity array replaces std::vector
//
// Changes vs. original:
//   • std::vector<shared_ptr<hittable>> replaced by a fixed-size array
//     (std::vector requires libstdc++ which is unavailable in the SDK)
//   • Maximum capacity controlled by HITTABLE_LIST_MAX (default 512)
//     The InOneWeekend final scene uses at most ~490 spheres.
//   • Range-based for replaced by index-based loop
//==============================================================================================

#include "hittable.h"

/* Maximum number of hittable objects.  Override at compile time if needed:
 *   -DHITTABLE_LIST_MAX=256 */
#ifndef HITTABLE_LIST_MAX
#define HITTABLE_LIST_MAX 512
#endif


class hittable_list : public hittable {
  public:
    shared_ptr<hittable> objects[HITTABLE_LIST_MAX];
    int count;

    hittable_list() : count(0) {}
    hittable_list(shared_ptr<hittable> object) : count(0) { add(object); }

    void clear() {
        for (int i = 0; i < count; ++i)
            objects[i] = shared_ptr<hittable>();
        count = 0;
    }

    void add(shared_ptr<hittable> object) {
        if (count < HITTABLE_LIST_MAX) {
            objects[count++] = object;
        } else {
            std::cerr << "hittable_list: capacity exceeded (HITTABLE_LIST_MAX="
                      << HITTABLE_LIST_MAX << "); object dropped\n";
        }
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        hit_record temp_rec;
        bool hit_anything   = false;
        auto closest_so_far = ray_t.max;

        for (int i = 0; i < count; ++i) {
            if (objects[i]->hit(r, interval(ray_t.min, closest_so_far), temp_rec)) {
                hit_anything    = true;
                closest_so_far  = temp_rec.t;
                rec             = temp_rec;
            }
        }

        return hit_anything;
    }
};


#endif
