#pragma once

#include <cstddef>

namespace mc {

enum class OptionType {
    Call,
    Put
};

struct OptionInput {
    double spot;        // current underlying price
    double strike;      // option strike price
    double rate;        // continuous risk-free rate
    double dividend;    // continuous dividend yield
    double volatility;  // annualized volatility
    double maturity;    // time to maturity in years
    OptionType type;    // option type
};

struct MonteCarloResult {
    double price;            // discounted Monte Carlo estimate
    double standard_error;   // standard error of the estimate
    std::size_t samples;     // number of Monte Carlo samples used
    bool used_antithetic;    // flag indicating antithetic variates usage
};

struct MonteCarloConfig {
    std::size_t paths = 1'000'000;    // number of Monte Carlo paths
    unsigned int seed = 5489u;        // default seed value (Mersenne Twister default)
    bool antithetic = true;           // enable antithetic variates
};

class MonteCarloPricer {
  public:
    MonteCarloPricer() = default;
    explicit MonteCarloPricer(MonteCarloConfig config);

    const MonteCarloConfig &config() const noexcept { return config_; }
    void set_config(MonteCarloConfig config) noexcept { config_ = config; }

    MonteCarloResult price(const OptionInput &option) const;

  private:
    MonteCarloConfig config_{ };
};

} // namespace mc
