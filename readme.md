# Transformer (CPU Mesh Skinning)
A console app for CPU skinning of an OBJ mesh based on:
- bone indices and weights per vertex (up to 4 influences);
- inverse bind pose matrices;
- new pose matrices.

## Source structure
- `src/app` — app flow orchestration.
- `src/io` — input parsing and output writing (OBJ/JSON/stats).
- `src/skinning` — CPU skinning.
- `src/core` — basic types, math, profiler.

## Build
The project uses `CMakePresets.json`.

### Linux/macOS (Ninja)
```bash
cmake --preset release
cmake --build --preset build
```

For other build types:
- Debug: `cmake --preset debug && cmake --build --preset build-debug`
- RelWithDebInfo: `cmake --preset relwithdeb && cmake --build --preset build-relwithdeb`

### Windows (new machine / MSVC + Ninja)
```powershell
cmake --preset windows-msvc-release
cmake --build --preset build-win-release
```

For debugging on Windows, use `windows-msvc-debug` + `build-win-debug`.

## Run
```bash
./out/build/release/transformer --mesh <meshFile.obj> --bones-weights <boneWeightFile.json> --inverse-bind-pose <inverseBindPoseFile.json> --new-pose <newPoseFile.json> --output <resultFile.obj> --stats <statsFile.json> [--bench <N>]
```

Typical run:
```bash
--mesh "assets/test_mesh.obj" --bones-weights "assets/bone_weight.json" --inverse-bind-pose "assets/inverse_bind_pose.json" --new-pose "assets/new_pose.json" --output "result_mesh.obj" --stats stats.json
```

## `boneWeightFile.json` format
```json
{
  "vertices": [
    {
      "bone_indices": [0, 4, 9, 15],
      "weights": [0.6, 0.2, 0.2, 0.0]
    }
  ]
}
```

## Pose JSON format (`inverseBindPoseFile.json` and `newPoseFile.json`)
```json
{
  "bones": [
    {
      "matrix": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    }
  ]
}
```
The matrix is in **column-major** order.

## Profiling
`statsFile.json` stores time in microseconds (with 3 decimal places) for:
- each module (mesh/weights/poses read, skinning, mesh write);
- total time `total` (top-level).

If `--bench <N>` is provided, `cpu_skinning` is executed `N` times. In this case, `statsFile.json` also contains: `min/max/mean/median/stddev` for those `N` runs.

# Environment
The project uses CMake/CMakePresets/clang-format to keep code style consistent and make build/run steps simple across platforms. It also helps keep the toolchain consistent to avoid issues between compiler/linker versions.

# Stress testing
A separate test setup is available: CMake target `stress`.
Now it is **one** utility run with `--bench 100` (the number is controlled by `STRESS_RUNS`), without an external CMake loop.

Aggregated stats come from the app `statsFile.json` and include:
- `mean`, `median`, `stddev`, `min`, `max` (in microseconds) for `cpu_skinning` over `N` runs.

## CLI errors
- For unknown arguments, missing flag values, or missing required flags, the app exits with code `1` and prints usage.
- `--bench` accepts only a positive integer.

# Optimization
Current test setup specifics to keep in mind when reading optimization results:
- Low-poly mesh and a small number of bones: the dataset is small and can fit cache on many CPUs. Real models usually have much larger datasets and cache behavior can differ a lot. It is better to test several model types.
- Tests and results are for specific hardware. To evaluate algorithms well, you need a wider hardware sample.
- Work is done with one model, which is cache-friendly. In real games many objects are animated, so cache context switches are more frequent.
- We ignore small precision errors from parsing floating-point values.
- Threading and SIMD optimizations are not made cross-platform on purpose, to keep things simple.
- At the final stage, after SIMD integration, there was an attempt to move computation to multithreading, but results became worse. Overhead was higher than the gain. With bigger datasets it could be different.

Optimization iterations and stress test results:
- 1.0 Base version with straightforward sequential computation:

```json
{
  "bench": {
    "runs": 10000,
    "min_microseconds": 78.700,
    "max_microseconds": 413.900,
    "mean_microseconds": 137.157,
    "median_microseconds": 138.500,
    "stddev_microseconds": 14.750
  }
}
```

- 1.1 Version with optimized data layouts (combined data for the hot loop, SOA -> AOS). Result stayed almost the same, but it may help on other architectures:

```json
{
  "bench": {
    "runs": 10000,
    "min_microseconds": 135.100,
    "max_microseconds": 294.400,
    "mean_microseconds": 137.542,
    "median_microseconds": 135.700,
    "stddev_microseconds": 7.487
  }
}
```

- 1.2 Version with precomputed skinning matrices. Matrix multiplications were reduced from 4400 to 33 (number of bones), and extra operations were removed. Previous layout optimizations should also become more useful:

```json
{
  "bench": {
    "runs": 10000,
    "min_microseconds": 47.400,
    "max_microseconds": 637.400,
    "mean_microseconds": 49.582,
    "median_microseconds": 47.800,
    "stddev_microseconds": 12.631
  }
}
```

- 1.3 During loading, we verified bone weights always sum to 1. Based on this, extra work was removed (no W component work, no normalization or coordinate division). Another key point: no branching in the hot loop. Data is prepared so every vertex always has all 4 bone slots, and skinning always processes all four matrices (fake ones are zeros). It is often easier for CPU to process zero data in sequence than to branch and pay branch misprediction costs:

```json
{
  "bench": {
    "runs": 10000,
    "min_microseconds": 27.100,
    "max_microseconds": 113.400,
    "mean_microseconds": 29.923,
    "median_microseconds": 27.700,
    "stddev_microseconds": 4.475
  }
}
```

- 1.4 SIMD was used for vector-coordinate multiplication by precomputed skinning matrices. At previous stage, operations were already decomposed component-wise; here they are executed with SIMD instructions using vector registers:

```json
{
  "bench": {
    "runs": 10000,
    "min_microseconds": 13.800,
    "max_microseconds": 493.100,
    "mean_microseconds": 17.044,
    "median_microseconds": 14.200,
    "stddev_microseconds": 8.457
  }
}
```
