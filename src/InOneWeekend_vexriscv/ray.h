#ifndef RAY_H
#define RAY_H
//==============================================================================================
// VexRiscV / openfpgaOS port — float version
//
// Changes vs. original:
//   • at(double t) → at(float t)
//==============================================================================================

#include "vec3.h"


class ray {
  public:
    ray() {}

    ray(const point3& origin, const vec3& direction) : orig(origin), dir(direction) {}

    const point3& origin()    const  { return orig; }
    const vec3&   direction() const  { return dir; }

    point3 at(float t) const {
        return orig + t*dir;
    }

  private:
    point3 orig;
    vec3   dir;
};


#endif
