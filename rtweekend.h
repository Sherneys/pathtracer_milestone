#ifndef RTWEEKEND_H
#define RTWEEKEND_H

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <random>


// C++ Std Usings

using std::make_shared;
using std::shared_ptr;

// Constants

const double infinity = std::numeric_limits<double>::infinity();
const double pi = 3.1415926535897932385;

// Utility Functions

#include <random>

inline std::mt19937& get_rng() {
    static std::mt19937 rng(12345); // Seeded exactly as required [cite: 28]
    return rng;
}

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

inline double random_double() {
    // uniform_real_distribution ensures an unbiased spread
    static std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(get_rng());
}

inline double random_double(double min, double max) {
    return min + (max - min) * random_double();
}

inline int random_int(int min, int max) {
    // Returns a random integer in [min, max].
    return int(random_double(min, max+1));
}

// Common Headers

#include "color.h"
#include "interval.h"
#include "ray.h"
#include "vec3.h"

#endif