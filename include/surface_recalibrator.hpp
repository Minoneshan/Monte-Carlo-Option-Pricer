#pragma once

#include "black_scholes.hpp"
#include "monte_carlo_pricer.hpp"

#include <vector>

namespace mc {

struct SurfacePoint {
    OptionInput option;
    MonteCarloResult mc;
    double black_scholes;     // analytic reference price (NaN if unavailable)
    double elapsed_seconds;   // wall-clock time spent pricing this option
};

struct SurfaceSummary {
    std::vector<SurfacePoint> points;
    double total_elapsed_seconds;
    double mean_abs_diff;
    double max_abs_diff;
};

SurfaceSummary recalibrate_surface(const MonteCarloPricer &pricer,
                                   const std::vector<OptionInput> &surface);

} // namespace mc
