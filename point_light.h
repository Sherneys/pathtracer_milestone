#ifndef POINT_LIGHT_H
#define POINT_LIGHT_H

#include "vec3.h"
#include "color.h"

struct point_light {
    point3 position;
    color intensity;

    point_light() {}
    point_light(point3 p, color i) : position(p), intensity(i) {}
};

#endif