#ifndef POINT_LIGHT_H
#define POINT_LIGHT_H

#include "vec3.h"
#include "color.h"

// "Point" light promoted to a small spherical area light. The radius gives
// the BSDF strategy a non-zero probability of intersecting the source, which
// is what lets delta materials (dielectrics) capture light at all.
struct point_light {
    point3 position;
    color  intensity;
    double radius;

    point_light() : radius(1.0) {}
    point_light(point3 p, color i, double r = 1.0)
        : position(p), intensity(i), radius(r) {}
};

#endif
