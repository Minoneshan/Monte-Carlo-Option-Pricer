#include "black_scholes.hpp"
#include "monte_carlo_pricer.hpp"

#include <chrono>
#include <cctype>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

using namespace mc;

struct CliOptions {
    OptionInput option{100.0, 100.0, 0.01, 0.0, 0.2, 1.0, OptionType::Call};
    MonteCarloConfig config{};
};

void print_usage(const char *exe) {
    std::cout << "Usage: " << exe << " [--spot value] [--strike value] [--rate value] [--dividend value]\n"
              << "             [--vol value] [--maturity years] [--type call|put] [--paths N]\n"
              << "             [--seed value] [--antithetic 0|1]\n";
}

double parse_double(const std::string &value, const std::string &name) {
    try {
        size_t idx = 0;
        double parsed = std::stod(value, &idx);
        if (idx != value.size()) {
            throw std::invalid_argument("Trailing characters");
        }
        return parsed;
    } catch (const std::exception &) {
        std::ostringstream oss;
        oss << "Invalid value for " << name << ": " << value;
        throw std::invalid_argument(oss.str());
    }
}

std::size_t parse_size(const std::string &value, const std::string &name) {
    try {
        size_t idx = 0;
        const unsigned long long parsed = std::stoull(value, &idx);
        if (idx != value.size()) {
            throw std::invalid_argument("Trailing characters");
        }
        return static_cast<std::size_t>(parsed);
    } catch (const std::exception &) {
        std::ostringstream oss;
        oss << "Invalid integer for " << name << ": " << value;
        throw std::invalid_argument(oss.str());
    }
}

bool parse_bool(const std::string &value, const std::string &name) {
    if (value == "1" || value == "true" || value == "TRUE") {
        return true;
    }
    if (value == "0" || value == "false" || value == "FALSE") {
        return false;
    }
    std::ostringstream oss;
    oss << "Invalid boolean for " << name << ": " << value;
    throw std::invalid_argument(oss.str());
}

CliOptions parse_arguments(int argc, char **argv) {
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (arg.rfind("--", 0) != 0) {
            std::ostringstream oss;
            oss << "Unknown argument: " << arg;
            throw std::invalid_argument(oss.str());
        }
        if (i + 1 >= argc) {
            std::ostringstream oss;
            oss << "Missing value for " << arg;
            throw std::invalid_argument(oss.str());
        }
        const std::string value = argv[++i];

        if (arg == "--spot") {
            options.option.spot = parse_double(value, "spot");
        } else if (arg == "--strike") {
            options.option.strike = parse_double(value, "strike");
        } else if (arg == "--rate") {
            options.option.rate = parse_double(value, "rate");
        } else if (arg == "--dividend") {
            options.option.dividend = parse_double(value, "dividend");
        } else if (arg == "--vol") {
            options.option.volatility = parse_double(value, "vol");
        } else if (arg == "--maturity") {
            options.option.maturity = parse_double(value, "maturity");
        } else if (arg == "--type") {
            std::string lowered = value;
            for (char &c : lowered) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (lowered == "call") {
                options.option.type = OptionType::Call;
            } else if (lowered == "put") {
                options.option.type = OptionType::Put;
            } else {
                throw std::invalid_argument("Option type must be 'call' or 'put'");
            }
        } else if (arg == "--paths") {
            options.config.paths = parse_size(value, "paths");
        } else if (arg == "--seed") {
            options.config.seed = static_cast<unsigned int>(parse_size(value, "seed"));
        } else if (arg == "--antithetic") {
            options.config.antithetic = parse_bool(value, "antithetic");
        } else {
            std::ostringstream oss;
            oss << "Unknown argument: " << arg;
            throw std::invalid_argument(oss.str());
        }
    }

    return options;
}

void print_summary(const OptionInput &option, const MonteCarloConfig &config, const MonteCarloResult &result, double wall_seconds) {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Monte Carlo option pricing summary\n";
    std::cout << "--------------------------------\n";
    std::cout << "Paths used:         " << config.paths << (result.used_antithetic ? " (antithetic variates enabled)" : "") << "\n";
    std::cout << "Effective samples:  " << result.samples << "\n";
    std::cout << "Elapsed time (s):   " << wall_seconds << "\n";
    std::cout << "Price:              " << result.price << "\n";
    std::cout << "Std. error:         " << result.standard_error << "\n";
    const double z95 = 1.959963984540054;
    std::cout << "95% CI:             [" << result.price - z95 * result.standard_error
              << ", " << result.price + z95 * result.standard_error << "]\n";

    if (option.volatility > 0.0) {
        try {
            const double bs = black_scholes_price(option);
            std::cout << "Black-Scholes:      " << bs << "\n";
            std::cout << "Difference:         " << (result.price - bs) << "\n";
        } catch (const std::exception &ex) {
            std::cout << "Black-Scholes:      unavailable (" << ex.what() << ")\n";
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    using namespace mc;

    try {
        const auto options = parse_arguments(argc, argv);
        MonteCarloPricer pricer(options.config);

        const auto start = std::chrono::high_resolution_clock::now();
        const MonteCarloResult result = pricer.price(options.option);
        const auto end = std::chrono::high_resolution_clock::now();

        const std::chrono::duration<double> elapsed = end - start;
        print_summary(options.option, options.config, result, elapsed.count());
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
