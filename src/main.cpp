#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

struct Config {
    std::size_t n = 1000000;
    std::size_t queries = 1000000;
    int trials = 3;
    std::size_t block_size = 64;
    uint32_t seed = 12345;
    std::string out = "results/results.csv";
};

static void usage(const char* program) {
    std::cerr
        << "Uso: " << program << " [opciones]\n"
        << "  --n NUM             cantidad de elementos (default 1000000)\n"
        << "  --queries NUM       cantidad de consultas (default 1000000)\n"
        << "  --trials NUM        repeticiones para promediar (default 3)\n"
        << "  --block-size NUM    B objetivo en bytes (default 64)\n"
        << "  --seed NUM          semilla base (default 12345)\n"
        << "  --out PATH          archivo csv de salida\n";
}

static std::size_t parse_size(const std::string& s) {
    unsigned long long value = std::stoull(s);
    return static_cast<std::size_t>(value);
}

static Config parse_args(int argc, char** argv) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto need_value = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("falta valor para " + name);
            }
            return argv[++i];
        };

        if (arg == "--n") {
            cfg.n = parse_size(need_value(arg));
        } else if (arg == "--queries" || arg == "--q") {
            cfg.queries = parse_size(need_value(arg));
        } else if (arg == "--trials" || arg == "--t") {
            cfg.trials = std::stoi(need_value(arg));
        } else if (arg == "--block-size" || arg == "--B") {
            cfg.block_size = parse_size(need_value(arg));
        } else if (arg == "--seed") {
            cfg.seed = static_cast<uint32_t>(std::stoul(need_value(arg)));
        } else if (arg == "--out") {
            cfg.out = need_value(arg);
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("opcion desconocida: " + arg);
        }
    }

    if (cfg.n == 0 || cfg.queries == 0 || cfg.trials <= 0 || cfg.block_size == 0) {
        throw std::runtime_error("los parametros numericos deben ser positivos");
    }
    if (cfg.n > static_cast<std::size_t>(std::numeric_limits<int32_t>::max() / 2 - 1)) {
        throw std::runtime_error("n demasiado grande para generar enteros int32 sin repetir");
    }
    return cfg;
}

static std::vector<int32_t> make_elements(std::size_t n) {
    std::vector<int32_t> values;
    values.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        values.push_back(static_cast<int32_t>(2 * i + 1));
    }
    return values;
}

static std::vector<int32_t> make_queries(
    const std::vector<int32_t>& values,
    std::size_t query_count,
    uint32_t seed
) {
    std::vector<int32_t> queries;
    queries.reserve(query_count);

    std::mt19937 rng(seed);
    std::uniform_int_distribution<std::size_t> hit_dist(0, values.size() - 1);
    std::uniform_int_distribution<std::size_t> miss_dist(0, values.size() - 1);

    for (std::size_t i = 0; i < query_count; ++i) {
        if (i % 2 == 0) {
            queries.push_back(values[hit_dist(rng)]);
        } else {
            queries.push_back(static_cast<int32_t>(2 * miss_dist(rng)));
        }
    }
    std::shuffle(queries.begin(), queries.end(), rng);
    return queries;
}

static double elapsed_ms(Clock::time_point begin, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

int main(int argc, char** argv) {
    try {
        Config cfg = parse_args(argc, argv);
        std::vector<int32_t> values = make_elements(cfg.n);

        std::cout << "n=" << cfg.n
                  << " queries=" << cfg.queries
                  << " trials=" << cfg.trials
                  << " B=" << cfg.block_size << "\n";
        std::cout << "hardware_threads=" << std::thread::hardware_concurrency()
                  << " int_bits=32 pointer_bits=" << (8 * sizeof(void*)) << "\n";

        for (int trial = 1; trial <= cfg.trials; ++trial) {
            auto t0 = Clock::now();
            std::vector<int32_t> queries = make_queries(values, cfg.queries, cfg.seed + trial);
            auto t1 = Clock::now();

            std::cout << "trial " << trial
                      << ": generated queries in " << std::fixed << std::setprecision(2)
                      << elapsed_ms(t0, t1) << " ms\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        usage(argv[0]);
        return 1;
    }
    return 0;
}
