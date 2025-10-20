#include "surface_recalibrator.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <limits>

namespace mc {

SurfaceSummary recalibrate_surface(const MonteCarloPricer &pricer,
                                   const std::vector<OptionInput> &surface) {
    SurfaceSummary summary{};
    summary.points.reserve(surface.size());

    const auto start = std::chrono::high_resolution_clock::now();

    double total_abs_diff = 0.0;
    double max_abs_diff = 0.0;

    for (const auto &option : surface) {
        const auto option_start = std::chrono::high_resolution_clock::now();
        const MonteCarloResult result = pricer.price(option);
        const auto option_end = std::chrono::high_resolution_clock::now();

        SurfacePoint point{option, result, std::numeric_limits<double>::quiet_NaN(),
                           std::chrono::duration<double>(option_end - option_start).count()};

        try {
            point.black_scholes = black_scholes_price(option);
        } catch (const std::exception &) {
            point.black_scholes = std::numeric_limits<double>::quiet_NaN();
        }

        const double diff = std::isnan(point.black_scholes)
                                 ? 0.0
                                 : std::abs(point.mc.price - point.black_scholes);
        total_abs_diff += diff;
        max_abs_diff = std::max(max_abs_diff, diff);

        summary.points.push_back(point);
    }

    const auto end = std::chrono::high_resolution_clock::now();
    summary.total_elapsed_seconds = std::chrono::duration<double>(end - start).count();
    summary.mean_abs_diff = summary.points.empty()
                                ? 0.0
                                : total_abs_diff / static_cast<double>(summary.points.size());
    summary.max_abs_diff = max_abs_diff;

    return summary;
}

} // namespace mc
