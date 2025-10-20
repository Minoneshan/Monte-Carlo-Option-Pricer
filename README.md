# Monte Carlo Option Pricer

High-performance C++17 engine for pricing European options via Monte Carlo simulation. The engine combines antithetic variates with OpenMP parallelisation to deliver intra-day ready turnaround times—measured at roughly 5–7× speedups (down to ~37 ms for ten million antithetic paths on Apple Silicon) while preserving pricing accuracy.

## Highlights

- ✅ **Antithetic variates** dramatically reduce estimator variance without extra memory.
- ⚙️ **OpenMP parallelism** scales pricing across CPU cores (configurable through `OMP_NUM_THREADS`).
- 📏 **Black–Scholes guardrail** keeps simulations honest with analytic benchmarks and convergence stats.
- 🧪 **Deterministic validation** executable exercises both call and put scenarios against Black–Scholes within tight tolerances.
- 📈 **Surface recalibration mode** ingests CSV grids and emits price/error summaries for intra-day volatility surface upkeep.

## Project Structure

```
├── CMakeLists.txt          # Build script (library + CLI + tests)
├── include/
│   ├── black_scholes.hpp
│   ├── monte_carlo_pricer.hpp
│   └── surface_recalibrator.hpp
├── examples/
│   └── sample_surface.csv
├── src/
│   ├── black_scholes.cpp
│   ├── monte_carlo_pricer.cpp
│   ├── surface_recalibrator.cpp
│   └── main.cpp            # CLI / benchmark + surface harness
└── tests/
    ├── test_pricer.cpp
    └── test_surface.cpp
```

## Prerequisites

- A C++17 toolchain (tested with Apple Clang 17 and GCC 12+).
- CMake ≥ 3.16 (for the recommended workflow).
- OpenMP runtime (`libomp`) for multi-threaded builds.
  - macOS: `brew install cmake libomp`
  - Linux: `sudo apt install cmake libomp-dev` (Ubuntu) or equivalent.

> ℹ️ Without OpenMP the code still builds and runs single-threaded; simply skip the `-fopenmp` flags.

## Build & Test

```bash
# Configure for Release
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build library, CLI, and tests
cmake --build build --config Release

# Run validation tests
./build/pricer_tests
./build/surface_tests
```

### Manual compile (fallback)

When `cmake` is unavailable, a quick single-threaded build is still possible:

```bash
clang++ -std=c++17 -O3 -Iinclude src/monte_carlo_pricer.cpp src/black_scholes.cpp src/surface_recalibrator.cpp src/main.cpp -o build/pricer
clang++ -std=c++17 -O3 -Iinclude src/monte_carlo_pricer.cpp src/black_scholes.cpp src/surface_recalibrator.cpp tests/test_pricer.cpp -o build/pricer_tests
clang++ -std=c++17 -O3 -Iinclude src/monte_carlo_pricer.cpp src/black_scholes.cpp src/surface_recalibrator.cpp tests/test_surface.cpp -o build/surface_tests
```

To enable OpenMP manually once `libomp` is installed:

```bash
LIBOMP_PREFIX="$(brew --prefix libomp)"
clang++ -std=c++17 -O3 \
  -Iinclude \
  -I"${LIBOMP_PREFIX}/include" \
  -Xpreprocessor -fopenmp \
  src/monte_carlo_pricer.cpp src/black_scholes.cpp src/surface_recalibrator.cpp src/main.cpp \
  -L"${LIBOMP_PREFIX}/lib" -lomp \
  -o build/pricer_omp
```

On Linux toolchains (GCC or Clang with `libgomp`), the shorter `-fopenmp` flag is typically sufficient.

## Usage

### Single instrument

```bash
./build/pricer \
  --spot 100 \
  --strike 100 \
  --rate 0.01 \
  --dividend 0.0 \
  --vol 0.20 \
  --maturity 1.0 \
  --type call \
  --paths 1000000 \
  --seed 42 \
  --antithetic 1
```

Key flags:

- `--paths N` – Monte Carlo paths (each path produces a pair of antithetic draws when enabled).
- `--antithetic 0|1` – toggle antithetic variates (defaults to enabled).
- `--seed` – deterministic results across runs.
- `--type call|put` – European option flavour.

Runtime output includes price, standard error, 95% CI, and the analytic Black–Scholes benchmark.

> Tip: swap `./build/pricer` for `./build/pricer_omp` to exercise the OpenMP build produced by the command above.

### Surface recalibration

Feed a CSV describing the strike/maturity grid to price an entire volatility surface in one pass:

```bash
./build/pricer --surface examples/sample_surface.csv --paths 1000000 --seed 42
```

CSV format (`type,spot,strike,rate,dividend,vol,maturity`) with optional comment lines starting with `#`. The engine runs each instrument through the antithetic Monte Carlo pricer, compares the results to Black–Scholes, and prints a table with timing, absolute differences, and aggregate error stats—ideal for intra-day surface recalibration loops.

## Performance

| Hardware | Build | Threads | Paths | Wall time |
|----------|-------|---------|-------|-----------|
| Apple M3 Pro (8 performance cores) | `clang++` + libomp | 8 | 1,000,000 (antithetic) | 0.0048 s |
| Apple M3 Pro | `clang++` (no OpenMP) | 1 | 1,000,000 (antithetic) | 0.0258 s |

> The OpenMP build provides a ~5.3× improvement at one million paths on Apple Silicon. Scaling the workload yields even larger gains; for ten million paths the eight-thread binary finishes in ~0.037 s versus 0.259 s serial (≈7× speedup). Adjust `OMP_NUM_THREADS` to match your CPU topology and experiment with larger path counts for closer-to-linear scaling.

| Paths | Serial (s) | OMP 8 threads (s) | Speedup |
|-------|------------|-------------------|---------|
| 1,000,000 | 0.0258 | 0.0048 | 5.3× |
| 5,000,000 | 0.1305 | 0.0194 | 6.7× |
| 10,000,000 | 0.2590 | 0.0370 | 7.0× |

To benchmark the two binaries created above:

```bash
# Serial reference (no OpenMP flags during compilation)
/usr/bin/time -p ./build/pricer --paths 5000000 --seed 42

# Parallel run (OpenMP binary + environment hint)
/usr/bin/time -p env OMP_NUM_THREADS=8 ./build/pricer_omp --paths 5000000 --seed 42
```

## Next Steps

- Extend to path-dependent payoffs (e.g., Asian, barrier options) by swapping the payoff functor.
- Integrate Sobol or Halton low-discrepancy sequences for quasi Monte Carlo.
- Wire into a Python binding (pybind11) for rapid scenario generation from notebooks.

Enjoy fast, variance-controlled option pricing! 🎯
