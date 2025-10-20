#include "black_scholes.hpp"
#include "monte_carlo_pricer.hpp"
#include "surface_recalibrator.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <exception>
#include <fstream>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace mc;

struct CliOptions {
    OptionInput option{100.0, 100.0, 0.01, 0.0, 0.2, 1.0, OptionType::Call};
    MonteCarloConfig config{};
    std::string surface_csv;
};

void print_usage(const char *exe) {
    std::cout << "Usage: " << exe << " [--spot value] [--strike value] [--rate value] [--dividend value]\n"
              << "             [--vol value] [--maturity years] [--type call|put] [--paths N]\n"
              << "             [--seed value] [--antithetic 0|1]\n"
              << "             [--surface path/to/grid.csv]\n";
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

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

OptionType parse_option_type(const std::string &raw) {
    const std::string lowered = to_lower_copy(trim(raw));
    if (lowered == "call") {
        return OptionType::Call;
    }
    if (lowered == "put") {
        return OptionType::Put;
    }
    std::ostringstream oss;
    oss << "Option type must be 'call' or 'put' (received '" << raw << "')";
    throw std::invalid_argument(oss.str());
}

std::vector<OptionInput> load_surface_csv(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        std::ostringstream oss;
        oss << "Unable to open surface CSV: " << path;
        throw std::runtime_error(oss.str());
    }

    std::vector<OptionInput> surface;
    std::string line;
    bool header_skipped = false;
    std::size_t line_no = 0;

    while (std::getline(file, line)) {
        ++line_no;
        std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        if (!trimmed.empty() && trimmed.front() == '#') {
            continue;
        }

        std::vector<std::string> tokens;
        std::string token;
        std::istringstream iss(trimmed);
        while (std::getline(iss, token, ',')) {
            tokens.push_back(trim(token));
        }

        if (tokens.empty()) {
            continue;
        }

        if (!header_skipped) {
            const std::string first_lower = to_lower_copy(tokens.front());
            if (first_lower == "type" || first_lower == "option_type") {
                header_skipped = true;
                continue;
            }
        }

        if (tokens.size() != 7) {
            std::ostringstream oss;
            oss << "Expected 7 comma-separated fields (type, spot, strike, rate, dividend, vol, maturity) on line "
                << line_no << " but found " << tokens.size();
            throw std::invalid_argument(oss.str());
        }

        OptionInput option{};
        option.type = parse_option_type(tokens[0]);
        option.spot = parse_double(tokens[1], "spot");
        option.strike = parse_double(tokens[2], "strike");
        option.rate = parse_double(tokens[3], "rate");
        option.dividend = parse_double(tokens[4], "dividend");
        option.volatility = parse_double(tokens[5], "volatility");
        option.maturity = parse_double(tokens[6], "maturity");

        surface.push_back(option);
    }

    if (surface.empty()) {
        throw std::invalid_argument("Surface CSV is empty after parsing");
    }

    return surface;
}

void print_surface_summary(const MonteCarloConfig &config, const SurfaceSummary &summary) {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Volatility surface recalibration" << '\n';
    std::cout << "--------------------------------" << '\n';
    std::cout << "Options processed: " << summary.points.size() << '\n';
    std::cout << "Paths per option:  " << config.paths
              << (config.antithetic ? " (antithetic variates enabled)" : "") << '\n';
    std::cout << "Total runtime (s): " << summary.total_elapsed_seconds << '\n';
    std::cout << "Mean |MC-BS|:     " << summary.mean_abs_diff << '\n';
    std::cout << "Max |MC-BS|:      " << summary.max_abs_diff << '\n';

    std::cout << '\n';
    constexpr int column_width = 12;
    std::cout << std::left << std::setw(6) << "Type" << " | "
              << std::right << std::setw(column_width) << "Spot" << " | "
              << std::right << std::setw(column_width) << "Strike" << " | "
              << std::right << std::setw(column_width) << "Rate" << " | "
              << std::right << std::setw(column_width) << "Dividend" << " | "
              << std::right << std::setw(column_width) << "Vol" << " | "
              << std::right << std::setw(column_width) << "Maturity" << " | "
              << std::right << std::setw(column_width + 2) << "MC Price" << " | "
              << std::right << std::setw(column_width + 2) << "Std Err" << " | "
              << std::right << std::setw(column_width + 2) << "Black-Sch" << " | "
              << std::right << std::setw(column_width + 2) << "Diff" << " | "
              << std::right << std::setw(column_width) << "Time(s)"
              << '\n';

    for (const auto &point : summary.points) {
        const std::string type = point.option.type == OptionType::Call ? "CALL" : "PUT";
        const double diff = std::isnan(point.black_scholes)
                                ? std::numeric_limits<double>::quiet_NaN()
                                : point.mc.price - point.black_scholes;

        std::cout << std::left << std::setw(6) << type << " | "
                  << std::right << std::setw(column_width) << point.option.spot << " | "
                  << std::right << std::setw(column_width) << point.option.strike << " | "
                  << std::right << std::setw(column_width) << point.option.rate << " | "
                  << std::right << std::setw(column_width) << point.option.dividend << " | "
                  << std::right << std::setw(column_width) << point.option.volatility << " | "
                  << std::right << std::setw(column_width) << point.option.maturity << " | "
                  << std::right << std::setw(column_width + 2) << point.mc.price << " | "
                  << std::right << std::setw(column_width + 2) << point.mc.standard_error << " | "
                  << std::right << std::setw(column_width + 2) << point.black_scholes << " | "
                  << std::right << std::setw(column_width + 2) << diff << " | "
                  << std::right << std::setw(column_width) << point.elapsed_seconds
                  << '\n';
    }
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
        } else if (arg == "--surface") {
            options.surface_csv = value;
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

        if (!options.surface_csv.empty()) {
            const auto surface = load_surface_csv(options.surface_csv);
            const auto summary = recalibrate_surface(pricer, surface);
            print_surface_summary(options.config, summary);
        } else {
            const auto start = std::chrono::high_resolution_clock::now();
            const MonteCarloResult result = pricer.price(options.option);
            const auto end = std::chrono::high_resolution_clock::now();

            const std::chrono::duration<double> elapsed = end - start;
            print_summary(options.option, options.config, result, elapsed.count());
        }
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
