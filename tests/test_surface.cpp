#include "surface_recalibrator.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    using namespace mc;

    MonteCarloConfig config;
    config.paths = 200'000;
    config.antithetic = true;
    config.seed = 1337u;

    MonteCarloPricer pricer(config);

    std::vector<OptionInput> surface{
        {100.0, 100.0, 0.01, 0.0, 0.20, 0.5, OptionType::Call},
        {95.0, 105.0, 0.005, 0.0, 0.25, 1.0, OptionType::Put},
        {120.0, 110.0, 0.015, 0.01, 0.30, 1.5, OptionType::Call}
    };

    const SurfaceSummary summary = recalibrate_surface(pricer, surface);

    assert(summary.points.size() == surface.size());
    assert(summary.total_elapsed_seconds > 0.0);

    double worst_abs = 0.0;
    for (const SurfacePoint &point : summary.points) {
        assert(point.mc.samples == (config.antithetic ? config.paths * 2 : config.paths));
        assert(point.elapsed_seconds > 0.0);
        if (!std::isnan(point.black_scholes)) {
            const double diff = std::abs(point.mc.price - point.black_scholes);
            worst_abs = std::max(worst_abs, diff);
        }
    }

    // Within a loose band because Monte Carlo noise shrinks with antithetic variates.
    assert(worst_abs < 0.40);
    assert(summary.max_abs_diff >= 0.0);
    assert(summary.mean_abs_diff >= 0.0);

    std::cout << "surface_recalibrator smoke test passed\n";
    return 0;
}
