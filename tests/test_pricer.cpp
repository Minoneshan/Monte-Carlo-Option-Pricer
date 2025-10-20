#include "black_scholes.hpp"
#include "monte_carlo_pricer.hpp"

#include <cmath>
#include <iostream>

int main() {
    using namespace mc;

    MonteCarloConfig config;
    config.paths = 200'000;
    config.seed = 1337u;
    config.antithetic = true;

    MonteCarloPricer call_pricer(config);

    OptionInput call_option{100.0, 100.0, 0.01, 0.0, 0.2, 1.0, OptionType::Call};
    const MonteCarloResult call_result = call_pricer.price(call_option);
    const double call_bs = black_scholes_price(call_option);

    if (!call_result.used_antithetic) {
        std::cerr << "Expected antithetic variates to be enabled for call option test\n";
        return 1;
    }

    const double call_diff = std::fabs(call_result.price - call_bs);
    if (call_diff > 0.35) {
        std::cerr << "Call price differs from Black-Scholes by " << call_diff << " (expected <= 0.35)\n";
        return 1;
    }

    MonteCarloPricer put_pricer(config);

    OptionInput put_option{95.0, 100.0, 0.015, 0.005, 0.25, 0.5, OptionType::Put};
    const MonteCarloResult put_result = put_pricer.price(put_option);
    const double put_bs = black_scholes_price(put_option);

    const double put_diff = std::fabs(put_result.price - put_bs);
    if (put_diff > 0.35) {
        std::cerr << "Put price differs from Black-Scholes by " << put_diff << " (expected <= 0.35)\n";
        return 1;
    }

    std::cout << "Monte Carlo pricer tests passed." << std::endl;
    return 0;
}
