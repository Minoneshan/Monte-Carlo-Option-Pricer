#include "monte_carlo_pricer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <stdexcept>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace mc {

namespace {

double payoff(double spot, double strike, OptionType type) {
    if (type == OptionType::Call) {
        return std::max(spot - strike, 0.0);
    }
    return std::max(strike - spot, 0.0);
}

double evolve_spot(double spot,
                   double drift,
                   double diffusion,
                   double z) {
    return spot * std::exp(drift + diffusion * z);
}

} // namespace

MonteCarloPricer::MonteCarloPricer(MonteCarloConfig config) : config_{config} {}

MonteCarloResult MonteCarloPricer::price(const OptionInput &option) const {
    if (option.maturity <= 0.0) {
        throw std::invalid_argument("Option maturity must be positive");
    }
    if (config_.paths == 0) {
        throw std::invalid_argument("Monte Carlo path count must be positive");
    }
    if (option.volatility < 0.0) {
        throw std::invalid_argument("Volatility must be non-negative");
    }

    const double maturity = option.maturity;
    const double variance = option.volatility * option.volatility;
    const double drift = (option.rate - option.dividend - 0.5 * variance) * maturity;
    const double diffusion = option.volatility * std::sqrt(maturity);
    const double discount = std::exp(-option.rate * maturity);

    const bool use_antithetic = config_.antithetic && diffusion != 0.0;
    const std::size_t paths = config_.paths;

    double sum = 0.0;
    double sum_sq = 0.0;

#ifdef _OPENMP
#pragma omp parallel
#endif
    {
#ifdef _OPENMP
        const int thread_id = omp_get_thread_num();
        const int total_threads = omp_get_num_threads();
#else
        const int thread_id = 0;
        const int total_threads = 1;
#endif
        const std::size_t thread_seed = static_cast<std::size_t>(config_.seed) +
                                        0x9E3779B97F4A7C15ull * static_cast<std::size_t>(thread_id + 1);
        std::mt19937_64 rng(thread_seed);
        std::normal_distribution<double> standard_normal(0.0, 1.0);

        double local_sum = 0.0;
        double local_sum_sq = 0.0;

#ifdef _OPENMP
#pragma omp for schedule(static)
#endif
        for (std::size_t i = 0; i < paths; ++i) {
            const double z = standard_normal(rng);
            const double spot_plus = evolve_spot(option.spot, drift, diffusion, z);
            const double payoff_plus = payoff(spot_plus, option.strike, option.type);

            if (use_antithetic) {
                const double spot_minus = evolve_spot(option.spot, drift, diffusion, -z);
                const double payoff_minus = payoff(spot_minus, option.strike, option.type);
                const double averaged = 0.5 * (payoff_plus + payoff_minus);
                local_sum += averaged;
                local_sum_sq += averaged * averaged;
            } else {
                local_sum += payoff_plus;
                local_sum_sq += payoff_plus * payoff_plus;
            }
        }

#ifdef _OPENMP
#pragma omp atomic
#endif
        sum += local_sum;

#ifdef _OPENMP
#pragma omp atomic
#endif
        sum_sq += local_sum_sq;
    }

    const double mean = sum / static_cast<double>(paths);
    const double second_moment = sum_sq / static_cast<double>(paths);
    const double variance_estimate = std::max(0.0, second_moment - mean * mean);
    const double std_error = std::sqrt(variance_estimate / static_cast<double>(paths));

    MonteCarloResult result{};
    result.price = discount * mean;
    result.standard_error = discount * std_error;
    result.samples = use_antithetic ? paths * 2 : paths;
    result.used_antithetic = use_antithetic;
    return result;
}

} // namespace mc
