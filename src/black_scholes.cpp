#include "black_scholes.hpp"

#include <cmath>
#include <stdexcept>

namespace mc {

namespace {
constexpr double INV_SQRT_2PI = 0.39894228040143267793994605993438;

// Abramowitz-Stegun approximation for Phi(x)
double norm_cdf(double x) {
    const double abs_x = std::fabs(x);
    const double k = 1.0 / (1.0 + 0.2316419 * abs_x);
    const double k_sum = k * (0.319381530 + k * (-0.356563782 + k * (1.781477937 + k * (-1.821255978 + 1.330274429 * k))));
    const double cnd = 1.0 - INV_SQRT_2PI * std::exp(-0.5 * abs_x * abs_x) * k_sum;
    return x < 0 ? 1.0 - cnd : cnd;
}
} // namespace

double cumulative_normal(double x) {
    return norm_cdf(x);
}

double black_scholes_price(const OptionInput &option) {
    if (option.maturity <= 0.0) {
        throw std::invalid_argument("Maturity must be positive");
    }
    if (option.volatility <= 0.0) {
        throw std::invalid_argument("Volatility must be positive for Black-Scholes pricing");
    }

    const double sqrt_t = std::sqrt(option.maturity);
    const double sigma = option.volatility;
    const double d1 = (std::log(option.spot / option.strike) + (option.rate - option.dividend + 0.5 * sigma * sigma) * option.maturity) / (sigma * sqrt_t);
    const double d2 = d1 - sigma * sqrt_t;

    const double discounted_strike = option.strike * std::exp(-option.rate * option.maturity);
    const double discounted_spot = option.spot * std::exp(-option.dividend * option.maturity);

    if (option.type == OptionType::Call) {
        return discounted_spot * cumulative_normal(d1) - discounted_strike * cumulative_normal(d2);
    } else {
        return discounted_strike * cumulative_normal(-d2) - discounted_spot * cumulative_normal(-d1);
    }
}

} // namespace mc
