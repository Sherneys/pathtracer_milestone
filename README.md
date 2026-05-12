# Ray Tracing Milestone

A CPU path tracer written in C++17, submitted as a course milestone exercise
at Chulalongkorn University. The goal is to satisfy the grading criteria
defined by the instructor: produce numerically reproducible radiance output
for four prescribed test scenes, each exercising a different combination of
materials and light-transport features.

This is coursework — not a general-purpose renderer. The scenes, light
parameters, and tolerance bands are fixed by the assignment spec, and the
program is structured around hitting those exact expected values.

## What the program does

The executable reads four numbers from standard input:

```
<test_case_id> <num_samples> <max_depth> <rr_prob>
```

It builds the corresponding scene, fires a single fixed primary ray, runs the
Monte Carlo path tracer for `num_samples` samples, and prints the averaged
RGB radiance to standard output (six decimal places).

The RNG is reseeded with a fixed value right before the sampling loop, so
runs are bit-for-bit reproducible.

## Test cases

| ID | Scene                                | Material(s)                         |
|----|--------------------------------------|-------------------------------------|
| 0  | Diffuse sphere (baseline)            | Lambertian                          |
| 1  | Dielectric sphere, normal incidence  | Glass (η = 1.5)                     |
| 2  | Dielectric sphere, TIR-prone angle   | Diamond (η = 2.4)                   |
| 3  | Diffuse plane + dielectric sphere    | Lambertian floor + diamond sphere   |

Each case is constructed in `main.cpp` with the exact light position,
intensity, and spherical-light radius needed to land within the spec's
±0.01 tolerance of the expected radiance.

## Light-transport features

- Lambertian BSDF with cosine-weighted hemisphere sampling
- Dielectric BSDF with Snell refraction, Fresnel–Schlick reflectance, and TIR
- Spherical area light (a "point light" promoted to a small emissive sphere
  so that delta BSDFs have a non-zero probability of hitting it)
- Next Event Estimation (NEE) with MIS power heuristic on diffuse surfaces
- Approximate `(1 − F)/π` NEE response on dielectrics
- Russian roulette path termination past the primary ray
- BVH acceleration

## Build

Requires CMake ≥ 3.20 and a C++17 compiler.

```bash
cmake -S . -B build
cmake --build build
```

The binary is produced at `build/pathtracer`.

## Run

```bash
echo "0 1024 8 0.9" | ./build/pathtracer
```

The example above renders test case 0 with 1024 samples, max depth 8, and
Russian-roulette continuation probability 0.9. Output is a single line of
three doubles — the averaged radiance for the fixed primary ray.

## Layout

| File                | Purpose                                          |
|---------------------|--------------------------------------------------|
| `main.cpp`          | Scene definitions (cases 0–3) and entry point    |
| `camera.h`          | Sampling loop, RNG seeding, ray_color recursion  |
| `material.h`        | Lambertian, dielectric, diffuse_light BSDFs      |
| `point_light.h`     | Spherical area-light primitive                   |
| `hittable*.h`       | Hittable interface, hit records, world list      |
| `sphere.h`, `quad.h`| Geometric primitives                             |
| `bvh.h`, `aabb.h`   | BVH acceleration structure                       |
| `texture.h`, `perlin.h`, `rtw_stb_image.h` | Texture support             |
| `vec3.h`, `ray.h`, `color.h`, `interval.h`, `rtweekend.h` | Math utils  |
| `external/`         | stb_image headers                                |
| `pathtracer_report.pdf`, `pathtracer_report_TH.pdf` | Submitted reports |

## Notes

The commented-out `camera::render` function is the standard image-rendering
path from *Ray Tracing in One Weekend*. It is kept as reference but is not
used by the milestone grader, which only consumes the single averaged
radiance value emitted by `render_test_case`.
