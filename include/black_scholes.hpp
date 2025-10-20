#pragma once

#include "monte_carlo_pricer.hpp"

namespace mc {

double black_scholes_price(const OptionInput &option);

double cumulative_normal(double x);

} // namespace mc
