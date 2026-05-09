#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
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
static constexpr uint32_t NIL = std::numeric_limits<uint32_t>::max();

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

class PointerBST {
public:
    struct Node {
        int32_t key;
        Node* left = nullptr;
        Node* right = nullptr;
    };

    void build(const std::vector<int32_t>& sorted_values) {
        nodes.clear();
        nodes.reserve(sorted_values.size());
        root = build_rec(sorted_values, 0, sorted_values.size());
    }

    bool contains(int32_t key) const {
        Node* cur = root;
        while (cur != nullptr) {
            if (key == cur->key) {
                return true;
            }
            cur = (key < cur->key) ? cur->left : cur->right;
        }
        return false;
    }

private:
    Node* root = nullptr;
    std::vector<std::unique_ptr<Node>> nodes;

    Node* make_node(int32_t key) {
        nodes.push_back(std::make_unique<Node>());
        nodes.back()->key = key;
        return nodes.back().get();
    }

    Node* build_rec(const std::vector<int32_t>& values, std::size_t lo, std::size_t hi) {
        if (lo >= hi) {
            return nullptr;
        }
        std::size_t mid = lo + (hi - lo) / 2;
        Node* node = make_node(values[mid]);
        node->left = build_rec(values, lo, mid);
        node->right = build_rec(values, mid + 1, hi);
        return node;
    }
};

class StaticTree {
public:
    struct Node {
        int32_t key = 0;
        uint32_t left = NIL;
        uint32_t right = NIL;
    };

    void build(const std::vector<int32_t>& sorted_values) {
        logical.clear();
        logical.reserve(sorted_values.size());
        int root = build_logical(sorted_values, 0, sorted_values.size());

        std::vector<int> order;
        order.reserve(logical.size());
        layout_veb(root, tree_height(root), order);
        if (order.size() != logical.size()) {
            throw std::runtime_error("fallo construyendo el layout del static tree");
        }

        std::vector<uint32_t> pos(logical.size(), NIL);
        nodes.assign(order.size(), Node{});
        for (std::size_t i = 0; i < order.size(); ++i) {
            pos[order[i]] = static_cast<uint32_t>(i);
            nodes[i].key = logical[order[i]].key;
        }
        for (std::size_t i = 0; i < order.size(); ++i) {
            const LogicalNode& old = logical[order[i]];
            nodes[i].left = (old.left < 0) ? NIL : pos[old.left];
            nodes[i].right = (old.right < 0) ? NIL : pos[old.right];
        }
    }

    bool contains(int32_t key) const {
        uint32_t cur = nodes.empty() ? NIL : 0;
        while (cur != NIL) {
            const Node& node = nodes[cur];
            if (key == node.key) {
                return true;
            }
            cur = (key < node.key) ? node.left : node.right;
        }
        return false;
    }

    std::size_t bytes_used() const {
        return nodes.size() * sizeof(Node);
    }

private:
    struct LogicalNode {
        int32_t key = 0;
        int left = -1;
        int right = -1;
    };

    std::vector<LogicalNode> logical;
    std::vector<Node> nodes;

    int build_logical(const std::vector<int32_t>& values, std::size_t lo, std::size_t hi) {
        if (lo >= hi) {
            return -1;
        }
        std::size_t mid = lo + (hi - lo) / 2;
        int id = static_cast<int>(logical.size());
        logical.push_back(LogicalNode{values[mid], -1, -1});
        logical[id].left = build_logical(values, lo, mid);
        logical[id].right = build_logical(values, mid + 1, hi);
        return id;
    }

    int tree_height(int id) const {
        if (id < 0) {
            return 0;
        }
        int lh = tree_height(logical[id].left);
        int rh = tree_height(logical[id].right);
        return 1 + std::max(lh, rh);
    }

    void collect_depth(int id, int depth, std::vector<int>& out) const {
        if (id < 0) {
            return;
        }
        if (depth == 0) {
            out.push_back(id);
            return;
        }
        collect_depth(logical[id].left, depth - 1, out);
        collect_depth(logical[id].right, depth - 1, out);
    }

    void layout_veb(int id, int height, std::vector<int>& out) const {
        if (id < 0 || height <= 0) {
            return;
        }
        if (height == 1) {
            out.push_back(id);
            return;
        }

        int top_height = (height + 1) / 2;
        layout_veb(id, top_height, out);

        std::vector<int> bottom_roots;
        bottom_roots.reserve(1u << std::min(top_height, 20));
        collect_depth(id, top_height, bottom_roots);
        for (int child_root : bottom_roots) {
            layout_veb(child_root, height - top_height, out);
        }
    }
};

template <class Tree>
static std::size_t run_queries(const Tree& tree, const std::vector<int32_t>& queries) {
    std::size_t found = 0;
    for (int32_t key : queries) {
        found += tree.contains(key) ? 1u : 0u;
    }
    return found;
}

struct Measurement {
    double build_ms = 0.0;
    double query_ms = 0.0;
    std::size_t found = 0;
};

static void ensure_parent_dir(const std::string& file) {
    std::filesystem::path path(file);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
}

static void write_row(
    std::ofstream& csv,
    const std::string& trial,
    const Config& cfg,
    const std::string& structure,
    const Measurement& m
) {
    double ns_per_query = (m.query_ms * 1000000.0) / static_cast<double>(cfg.queries);
    csv << trial << ','
        << cfg.n << ','
        << cfg.queries << ','
        << cfg.block_size << ','
        << structure << ','
        << std::fixed << std::setprecision(3) << m.build_ms << ','
        << m.query_ms << ','
        << ns_per_query << ','
        << m.found << ','
        << std::thread::hardware_concurrency() << ','
        << (8 * sizeof(void*)) << ','
        << '"' << __VERSION__ << '"'
        << '\n';
}

int main(int argc, char** argv) {
    try {
        Config cfg = parse_args(argc, argv);
        std::vector<int32_t> values = make_elements(cfg.n);
        ensure_parent_dir(cfg.out);

        std::ofstream csv(cfg.out);
        if (!csv) {
            throw std::runtime_error("no se pudo abrir el csv de salida");
        }
        csv << "trial,n,queries,block_size,structure,build_ms,query_ms,"
            << "ns_per_query,found,hardware_threads,pointer_bits,compiler\n";

        std::cout << "n=" << cfg.n
                  << " queries=" << cfg.queries
                  << " trials=" << cfg.trials
                  << " B=" << cfg.block_size << "\n";
        std::cout << "hardware_threads=" << std::thread::hardware_concurrency()
                  << " int_bits=32 pointer_bits=" << (8 * sizeof(void*)) << "\n";

        Measurement static_sum;
        Measurement bst_sum;

        for (int trial = 1; trial <= cfg.trials; ++trial) {
            std::vector<int32_t> queries = make_queries(values, cfg.queries, cfg.seed + trial);

            StaticTree static_tree;
            auto begin = Clock::now();
            static_tree.build(values);
            auto end = Clock::now();
            Measurement static_m;
            static_m.build_ms = elapsed_ms(begin, end);

            begin = Clock::now();
            static_m.found = run_queries(static_tree, queries);
            end = Clock::now();
            static_m.query_ms = elapsed_ms(begin, end);

            PointerBST bst;
            begin = Clock::now();
            bst.build(values);
            end = Clock::now();
            Measurement bst_m;
            bst_m.build_ms = elapsed_ms(begin, end);

            begin = Clock::now();
            bst_m.found = run_queries(bst, queries);
            end = Clock::now();
            bst_m.query_ms = elapsed_ms(begin, end);

            if (static_m.found != bst_m.found) {
                throw std::runtime_error("los arboles no devolvieron las mismas respuestas");
            }

            static_sum.build_ms += static_m.build_ms;
            static_sum.query_ms += static_m.query_ms;
            static_sum.found += static_m.found;
            bst_sum.build_ms += bst_m.build_ms;
            bst_sum.query_ms += bst_m.query_ms;
            bst_sum.found += bst_m.found;

            write_row(csv, std::to_string(trial), cfg, "cache_oblivious_static_tree", static_m);
            write_row(csv, std::to_string(trial), cfg, "pointer_bst", bst_m);

            std::cout << "trial " << trial
                      << " static=" << std::fixed << std::setprecision(2) << static_m.query_ms
                      << " ms bst=" << bst_m.query_ms
                      << " ms found=" << static_m.found
                      << " static_bytes=" << static_tree.bytes_used()
                      << "\n";
        }

        static_sum.build_ms /= cfg.trials;
        static_sum.query_ms /= cfg.trials;
        static_sum.found /= static_cast<std::size_t>(cfg.trials);
        bst_sum.build_ms /= cfg.trials;
        bst_sum.query_ms /= cfg.trials;
        bst_sum.found /= static_cast<std::size_t>(cfg.trials);

        write_row(csv, "avg", cfg, "cache_oblivious_static_tree", static_sum);
        write_row(csv, "avg", cfg, "pointer_bst", bst_sum);

        std::cout << "resultados guardados en " << cfg.out << "\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        usage(argv[0]);
        return 1;
    }
    return 0;
}
