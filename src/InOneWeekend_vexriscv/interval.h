#ifndef INTERVAL_H
#define INTERVAL_H
//==============================================================================================
// VexRiscV / openfpgaOS port — float version
//
// Changes vs. original:
//   • All double members/parameters/return types → float
//   • Floating-point literals: added 'f' suffix
//
// NOTE: infinity is defined in rtweekend.h and must be in scope before this
// header is processed.  interval.h is included from rtweekend.h after the
// infinity constant is defined.
//==============================================================================================

class interval {
  public:
    float min, max;

    interval() : min(+infinity), max(-infinity) {} // Default interval is empty

    interval(float min, float max) : min(min), max(max) {}

    float size() const {
        return max - min;
    }

    bool contains(float x) const {
        return min <= x && x <= max;
    }

    bool surrounds(float x) const {
        return min < x && x < max;
    }

    float clamp(float x) const {
        if (x < min) return min;
        if (x > max) return max;
        return x;
    }

    static const interval empty, universe;
};

const interval interval::empty    = interval(+infinity, -infinity);
const interval interval::universe = interval(-infinity, +infinity);


#endif
