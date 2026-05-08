/*
 * main.cpp
 * --------
 * Traduzione di main.py.
 *
 * Utilizzo (identico al Python):
 *   ./maxflow -pr <dataset|file.max>
 *   ./maxflow -cs <dataset|file.max>
 *   ./maxflow -fullsuite
 *
 * Note rispetto al Python:
 *   - La configurazione TOML viene letta da configs/configmain.toml tramite
 *     un parser minimale (nessuna dipendenza esterna: solo std).
 *   - Tutti e tre gli algoritmi sono supportati: push_relabel, capacity_scaling
 *     e almost_linear_time.
 *   - parse_dimacs_safe e format_flow sono funzioni dedicate (come in Python).
 *   - I Fraction di Python vengono gestiti tramite Fraction + scale_rationals.
 *   - Il formato del CSV è identico al Python (compreso "%.17f" per i tempi).
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "src/util/parse_dimacs.hpp"
#include "src/util/capacity.hpp"
#include "src/util/fraction.hpp"
#include "src/util/scale_rationals.hpp"

#include "src/almost_linear/almost_linear_time.hpp"
#include "src/capacity_scaling/capacity_scaling.hpp"
#include "src/push_relabel/push_relabel.hpp"

namespace fs = std::filesystem;

// Forward declaration: implementazione in src/util/parse_dimacs.cpp
DimacsResult parse_dimacs(const std::string& path);

// Type aliases
using IntGraph = std::unordered_map<std::pair<int,int>, long long, PairHash>;

// ──────────────────────────────────────────────────────────────────────────────
// Parser TOML minimale (solo sezioni semplici e sezioni annidate con chiavi stringa)
// Sufficiente per configmain.toml, che non usa array né valori numerici TOML.
// ──────────────────────────────────────────────────────────────────────────────

using TomlSection = std::map<std::string, std::string>;
using TomlDoc     = std::map<std::string, TomlSection>;

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static std::string strip_quotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

static TomlDoc parse_toml(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Impossibile aprire: " + path);

    TomlDoc doc;
    std::string cur_section;

    std::string line;
    while (std::getline(f, line)) {
        std::string l = trim(line);
        if (l.empty() || l[0] == '#') continue;

        if (l[0] == '[') {
            // Sezione: "[algs]" o "[dataset_reali.-BVZ]"
            cur_section = trim(l.substr(1, l.rfind(']') - 1));
            doc[cur_section];  // assicura che la sezione esista
            continue;
        }

        auto eq = l.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(l.substr(0, eq));
        std::string val = trim(l.substr(eq + 1));
        key = strip_quotes(key);
        val = strip_quotes(val);

        doc[cur_section][key] = val;
    }
    return doc;
}

// ──────────────────────────────────────────────────────────────────────────────
// Configurazione caricata dal TOML
// ──────────────────────────────────────────────────────────────────────────────

struct Config {
    // [algs]: flag CLI → chiave algoritmo
    std::map<std::string,std::string> algs;

    // [out_files_single]: flag → percorso CSV
    std::map<std::string,std::string> out_files_single;

    // [out_files_bvz] e [out_files_kz2]
    std::map<std::string,std::string> out_files_bvz;
    std::map<std::string,std::string> out_files_kz2;

    // [instances_dir]: chiave algoritmo → directory
    std::map<std::string,std::string> instances_dir;

    // [dataset_reali.*]: flag → {prefix, directory, count, output_dir, out_files_key}
    struct DatasetInfo {
        std::string prefix, directory, output_dir, out_files_key;
        int count;
    };
    std::map<std::string, DatasetInfo> dataset_reali;
};

static Config load_config(const std::string& path) {
    TomlDoc doc = parse_toml(path);
    Config cfg;

    // Sezioni flat
    for (auto& [k, v] : doc["algs"])           cfg.algs[k]            = v;
    for (auto& [k, v] : doc["out_files_single"]) cfg.out_files_single[k] = v;
    for (auto& [k, v] : doc["out_files_bvz"])  cfg.out_files_bvz[k]   = v;
    for (auto& [k, v] : doc["out_files_kz2"])  cfg.out_files_kz2[k]   = v;
    for (auto& [k, v] : doc["instances_dir"])  cfg.instances_dir[k]    = v;

    // Sezioni annidate: "dataset_reali.<flag>"
    for (auto& [sec, kvmap] : doc) {
        if (sec.substr(0, 15) != "dataset_reali.") continue;
        std::string flag = sec.substr(14);  // "dataset_reali." = 14 caratteri
        Config::DatasetInfo di;
        di.prefix       = kvmap.count("prefix")       ? kvmap.at("prefix")       : "";
        di.directory    = kvmap.count("directory")    ? kvmap.at("directory")    : "";
        di.count        = kvmap.count("count")        ? std::stoi(kvmap.at("count")) : 0;
        di.output_dir   = kvmap.count("output_dir")   ? kvmap.at("output_dir")   : "";
        di.out_files_key= kvmap.count("out_files_key")? kvmap.at("out_files_key")  : "";
        cfg.dataset_reali[flag] = di;
    }
    return cfg;
}

// ──────────────────────────────────────────────────────────────────────────────
// Utilità CSV — traduzione diretta delle funzioni Python omonime
// ──────────────────────────────────────────────────────────────────────────────

static void init_csv(const std::string& csv_path,
                     const std::vector<std::string>& header) {
    fs::path p(csv_path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path());

    if (!fs::exists(p)) {
        std::ofstream f(csv_path);
        for (size_t i = 0; i < header.size(); ++i)
            f << (i ? "," : "") << header[i];
        f << "\n";
    }
}

static void scrivi_riga_csv(const std::string& csv_path,
                             const std::vector<std::string>& row) {
    std::ofstream f(csv_path, std::ios::app);
    for (size_t i = 0; i < row.size(); ++i)
        f << (i ? "," : "") << row[i];
    f << "\n";
}

// ──────────────────────────────────────────────────────────────────────────────
// estrai_valore_sol — traduzione di main.py::estrai_valore_sol
// ──────────────────────────────────────────────────────────────────────────────

static long long estrai_valore_sol(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("File .sol non trovato: " + path);
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string tag; long long val;
        if (ss >> tag >> val && tag == "s") return val;
    }
    throw std::runtime_error("Nessun flusso trovato nel file: " + path);
}

// ──────────────────────────────────────────────────────────────────────────────
// scala_grafo — traduzione di main.py::scala_grafo_razionale
// Converte capacità double → long long via scale_rationals (se necessario).
// ──────────────────────────────────────────────────────────────────────────────

using DblGraph = std::unordered_map<std::pair<int,int>, double, PairHash>;

struct GraphScaled {
    IntGraph  graph;      // popolato se capacità intere/razionali
    DblGraph  graph_dbl;  // popolato se capacità double
    long long fattore;
    bool      is_double = false;
};

static GraphScaled scala_grafo(const DimacsResult& graph_in) {
    
    bool has_fraction = false;
    bool has_double   = false;
    for (auto& [_, cap] : graph_in.graph) {
        if (std::holds_alternative<Fraction>(cap)) has_fraction = true;
        if (std::holds_alternative<double>(cap))   has_double   = true;
    }

    // Caso 1: solo interi
    if (!has_fraction && !has_double) {
        //std::cout << "INTERI";
        IntGraph ig;
        for (auto& [edge, cap] : graph_in.graph)
            ig[edge] = std::visit([](auto&& v) -> long long {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, Fraction>)
                    return static_cast<long long>(v.num);
                else
                    return static_cast<long long>(v);
            }, cap);
        return {std::move(ig), {}, 1LL, false};
    }

    // Caso 2: ci sono double (irrazionali) — PushRelabel lavora in double con eps
    if (has_double) {
        DblGraph dg;
        for (auto& [edge, cap] : graph_in.graph) {
            double d = std::visit([](auto&& v) -> double {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, Fraction>) return v.to_double();
                else return static_cast<double>(v);
            }, cap);
            dg[edge] = d;
        }
        return {{}, std::move(dg), 1LL, true};
    }

    // Caso 3: solo Fraction — scala via MCM dei denominatori
    std::vector<std::pair<int,int>> keys;
    std::vector<Fraction> vals;
    keys.reserve(graph_in.graph.size());
    vals.reserve(graph_in.graph.size());
    for (auto& [edge, cap] : graph_in.graph) {
        keys.push_back(edge);
        vals.push_back(std::visit([](auto&& v) -> Fraction {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Fraction>) return v;
            else return Fraction(static_cast<int>(v), 1);
        }, cap));
    }

    auto [scaled, k] = scale_rationals(vals);
    //std::cout << "Scaling attivo: k = " << k << "  (flusso reale = flusso_intero / " << k << ")\n";

    IntGraph ig;
    for (size_t i = 0; i < keys.size(); ++i)
        ig[keys[i]] = scaled[i];
    return {std::move(ig), {}, static_cast<long long>(k), false};
}
// ──────────────────────────────────────────────────────────────────────────────
// esegui_max_flow — traduzione di main.py::esegui_max_flow
// Restituisce (flow, elapsed_seconds).
// ──────────────────────────────────────────────────────────────────────────────

//static std::pair<long long, double>
// esegui_max_flow accetta il DimacsResult originale (non scalato) e il fattore
// di scala. Ogni solver riceve il tipo di grafo che si aspetta:
//   PushRelabel      <- IntGraph (long long, gia' scalato)
//   CapacityScaling  <- map<pair<int,int>, Capacity> (grafo originale)
//   AlmostLinearTime <- map<pair<int,int>, int>      (interi, fattore applicato)
static std::pair<Capacity, double>
esegui_max_flow(const std::string& algorithm,
                const DimacsResult& pg_orig,
                const GraphScaled& gs,
                int source, int sink) {
    auto t0 = std::chrono::high_resolution_clock::now();
    Capacity flow = 0;

    // Helper lambda: converte long long + fattore → Capacity corretta
    auto descala = [&](long long f) -> Capacity {
        if (gs.fattore == 1) {
            return static_cast<int>(f);
        }
        long long g   = std::gcd(f, gs.fattore);
        long long num = f          / g;
        long long den = gs.fattore / g;
        //std::cout << "descala: f=" << f << " fattore=" << gs.fattore
        //        << " -> num=" << num << " den=" << den << "\n";
        if (num > std::numeric_limits<int>::max() || den > std::numeric_limits<int>::max())
            return static_cast<double>(num) / static_cast<double>(den);
        return Fraction(static_cast<int>(num), static_cast<int>(den));
    };

    try {
        if (algorithm == "push_relabel") {
            if (gs.is_double) {
                PushRelabel solver(gs.graph_dbl);
                auto raw = solver.max_flow(source, sink);
                flow = std::get<double>(raw);
            } else {
                PushRelabel solver(gs.graph);
                auto raw = solver.max_flow(source, sink);
                flow = descala(std::get<long long>(raw));
            }

        } else if (algorithm == "capacity_scaling") {
            if (gs.is_double) {
                CapacityScaling solver(gs.graph_dbl);
                auto raw = solver.max_flow(source, sink);
                flow = std::get<double>(raw);
            } else {
                CapacityScaling solver(gs.graph);
                auto raw = solver.max_flow(source, sink);
                flow = descala(std::get<long long>(raw));
            }
        } else if (algorithm == "almost_linear_time") {
            if (gs.is_double || gs.fattore != 1) {
                throw std::runtime_error("almost_linear_time supporta solo capacit\xA0 intere.");
            }
            std::map<std::pair<int,int>, long long> int_map;
            for (auto& [edge, cap] : gs.graph)
                int_map[edge] = static_cast<long long>(cap);
            AlmostLinearTime solver(int_map);
            flow = descala(static_cast<long long>(solver.max_flow(source, sink)));
        } else {
            throw std::runtime_error("Algoritmo sconosciuto: '" + algorithm + "'.");
        }
    } catch (const std::out_of_range&) {
        std::cout << "Source isolato → flusso = 0\n";
        flow = 0;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    return {flow, elapsed};
}

static std::string format_flow(const Capacity& flow) {
    return std::visit([](auto&& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, int>)
            return std::to_string(v);
        else if constexpr (std::is_same_v<T, Fraction>)
            return std::to_string(v.num) + "/" + std::to_string(v.den);
        else  // double
            return std::to_string(v);
    }, flow);
}

// Helper: formatta double con 17 cifre decimali (come f"{elapsed:.17f}" in Python)
static std::string fmt_time(double t) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(17) << t;
    return ss.str();
}

// ──────────────────────────────────────────────────────────────────────────────
// parse_dimacs_safe — traduzione di main.py::parse_dimacs_safe
// In caso di file mancante o formato errato termina il programma con messaggio.
// ──────────────────────────────────────────────────────────────────────────────

[[nodiscard]] static DimacsResult parse_dimacs_safe(const std::string& path) {
    try {
        return parse_dimacs(path);
    } catch (const std::runtime_error& e) {
        std::string what = e.what();
        // Distingue file mancante da formato errato (come FileNotFoundError vs ValueError in Python)
        if (what.find("not found") != std::string::npos ||
            what.find("non trovato") != std::string::npos ||
            what.find("open") != std::string::npos) {
            std::cerr << "Errore: file non trovato: \"" << path << "\"\n";
        } else {
            std::cerr << "Errore nel parsing di \"" << path << "\": " << what << "\n";
        }
        std::exit(1);
    }
}

// format_flow rimossa: sostituita dalla versione Capacity sopra

// ──────────────────────────────────────────────────────────────────────────────
// run_benchmark_reale — traduzione di main.py::run_benchmark_reale
// ──────────────────────────────────────────────────────────────────────────────

static void run_benchmark_reale(
    const std::string& algorithm,
    const std::string& prefix,
    const std::string& directory,
    int count,
    const std::string& output_dir,
    const std::map<std::string,std::string>& out_files)
{
    // Ricava il nome del file CSV dall'out_files dell'algoritmo
    fs::path csv_path = fs::path(output_dir) /
                        fs::path(out_files.at(algorithm)).filename();
    
    std::cout <<csv_path; //capire l'output nel debug
    
    init_csv(csv_path.string(),
             {"n", "m", "time_seconds", "flow", "graph_file", "correctness"});

    for (int i = 0; i < count; ++i) {
        std::string base       = directory + "/" + prefix + std::to_string(i);
        std::string graph_file = prefix + std::to_string(i);

        DimacsResult pg = parse_dimacs_safe(base + ".max");

        auto gs = scala_grafo(pg);
        auto [flow, elapsed] = esegui_max_flow(algorithm, pg, gs, pg.source, pg.sink);

        std::string correctness = "N/A";
        std::string sol_path    = base + ".sol";
        std::string flow_str    = format_flow(flow);
        if (fs::exists(sol_path)) {
            long long sol = estrai_valore_sol(sol_path);
            // Confronto: converte flow a double per supportare Fraction e double
            double flow_d = std::visit([](auto&& v) -> double {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, Fraction>) return v.to_double();
                else return static_cast<double>(v);
            }, flow);
            correctness = (sol == static_cast<long long>(std::round(flow_d)))
                          ? "RISULTATO CORRETTO" : "NON CORRETTO";
            std::cout << "Atteso: " << sol << "  Ottenuto: " << flow_str
                      << "  → " << correctness << "\n";
        } else {
            std::cout << "File soluzione non trovato: " << sol_path << "\n";
        }

        scrivi_riga_csv(csv_path.string(), {
            std::to_string(pg.n),
            std::to_string(pg.m_actual),
            fmt_time(elapsed),
            flow_str,
            graph_file,
            correctness
        });
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// run_benchmark_sintetico — traduzione di main.py::run_benchmark_sintetico
// ──────────────────────────────────────────────────────────────────────────────

static void run_benchmark_sintetico(
    const std::string& algorithm,
    const std::string& instances_dir,
    const std::string& out_dir,
    int runs_per_instance = 7)
{
    std::string csv_path = out_dir + "/" + algorithm + "_results.csv";
    init_csv(csv_path, {"graph_type","cap_type","n","d","seed","m",
                         "median_time","flow","graph_file"});
    std::cout << "Location risultati: " << csv_path << "\n";

    // Pattern nomi file: stessi regex del Python
    std::regex pattern_layered(R"(layered_n(\d+)_d(\d+)_seed(\d+)\.max$)");
    std::regex pattern_grid   (R"(grid_n(\d+)_d(\d+)_seed(\d+)\.max$)");

    // 1) Raccolta file
    struct Entry {
        std::string graph_type, cap_type, path, filename;
        int d, n, seed;
    };
    std::vector<Entry> entries;

    for (auto& cap_entry : fs::directory_iterator(instances_dir)) {
        if (!cap_entry.is_directory()) continue;
        std::string cap_type = cap_entry.path().filename().string();

        for (auto& nd_entry : fs::directory_iterator(cap_entry.path())) {
            if (!nd_entry.is_directory()) continue;
            std::string nd_group  = nd_entry.path().filename().string();
            std::string graph_type = (nd_group.rfind("grid", 0) == 0) ? "grid" : "layered";
            const std::regex& pat  = (graph_type == "grid") ? pattern_grid : pattern_layered;

            for (auto& fentry : fs::directory_iterator(nd_entry.path())) {
                if (fentry.path().extension() != ".max") continue;
                std::string filename = fentry.path().filename().string();
                std::smatch m;
                if (!std::regex_search(filename, m, pat)) {
                    std::cout << "Ignoro file non conforme: " << filename << "\n";
                    continue;
                }
                entries.push_back({
                    graph_type, cap_type,
                    fentry.path().string(), filename,
                    std::stoi(m[2]), std::stoi(m[1]), std::stoi(m[3])
                });
            }
        }
    }

    // 2) Ordinamento: graph_type → cap_type → d → n → seed
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.graph_type != b.graph_type) return a.graph_type < b.graph_type;
        if (a.cap_type   != b.cap_type)   return a.cap_type   < b.cap_type;
        if (a.d != b.d) return a.d < b.d;
        if (a.n != b.n) return a.n < b.n;
        return a.seed < b.seed;
    });

    // 3) Benchmark
    for (auto& e : entries) {
        std::cout << "Processing (type=" << e.graph_type
                  << ", cap=" << e.cap_type
                  << ", d=" << e.d << ", n=" << e.n
                  << ", seed=" << e.seed << "): " << e.path << "\n";

        DimacsResult pg;
        try { pg = parse_dimacs(e.path); }
        catch (const std::exception& ex) {
            std::cerr << "Errore nel parsing di " << e.path << ": " << ex.what() << "\n";
            continue;
        }

        auto gs = scala_grafo(pg);
        //std::cout << "use_scale = " << (gs.fattore != 1 ) << "\n";

        std::vector<double> times;
        Capacity flow_value = 0;

        for (int r = 0; r < runs_per_instance; ++r) {
            auto [flow, elapsed] = esegui_max_flow(algorithm, pg, gs, pg.source, pg.sink);
            if (r == 0) flow_value = flow;
            times.push_back(elapsed);
        }

        // Scarta il primo (warmup) e calcola la mediana del resto
        std::vector<double> rest(times.begin() + 1, times.end());
        std::sort(rest.begin(), rest.end());
        double median_time = rest[rest.size() / 2];
        if (rest.size() % 2 == 0)
            median_time = (rest[rest.size()/2 - 1] + rest[rest.size()/2]) / 2.0;

        std::cout << "median_time = " << std::fixed << std::setprecision(6)
                  << median_time << "s\n";

        // Flusso reale (come format_flow in Python)
        std::string flow_str = format_flow(flow_value);

        scrivi_riga_csv(csv_path, {
            e.graph_type, e.cap_type,
            std::to_string(e.n), std::to_string(e.d), std::to_string(e.seed),
            std::to_string(pg.m_actual),
            fmt_time(median_time),
            flow_str,
            e.filename
        });
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// run_esperimento_singolo — traduzione di main.py::run_esperimento_singolo
// ──────────────────────────────────────────────────────────────────────────────

static void run_esperimento_singolo(
    const std::string& algorithm,
    const std::string& graph_file,
    const std::string& csv_file)
{
    init_csv(csv_file, {"n","m","time_seconds","flow","graph_file"});

    DimacsResult pg = parse_dimacs_safe(graph_file);

    for (const auto& [edge, cap] : pg.graph) {
        double c = std::visit([](auto&& x) -> double {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, double>)
                return static_cast<double>(x);
            else
                return static_cast<double>(x.num) / static_cast<double>(x.den);
        }, cap);
    }
    
    auto gs = scala_grafo(pg);

    std::cout << "File   : " << graph_file   << "\n";
    std::cout << "Source : " << pg.source << "   Sink : " << pg.sink << "\n";
    std::cout << "Archi  : " << pg.graph.size() << "\n\n";

    auto [flow, elapsed] = esegui_max_flow(algorithm, pg, gs, pg.source, pg.sink);

    std::string flow_str = format_flow(flow);

    std::cout << "flow = " << flow_str << "\n";
    scrivi_riga_csv(csv_file, {
        std::to_string(pg.n), std::to_string(pg.m_actual),
        fmt_time(elapsed), flow_str, graph_file
    });

    double flow_d = std::visit([](auto&& v) -> double {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, Fraction>) return v.to_double();
        else return static_cast<double>(v);
    }, flow);
    std::cout << "Flow = " << flow_str
              << "  (reale = " << std::fixed << std::setprecision(6)
              << flow_d << ")\n";
    std::cout << "Tempo = " << std::fixed << std::setprecision(6) << elapsed << " s\n";
}

// ──────────────────────────────────────────────────────────────────────────────
// main — traduzione di main.py __main__
// ──────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    



    // Trova la directory dell'eseguibile per risolvere il percorso del config
    fs::path exe_dir = fs::path(argv[0]).parent_path();
    fs::path cfg_path = exe_dir / "configs" / "configmain.toml";

    Config cfg;
    try {
        cfg = load_config(cfg_path.string());
    } catch (const std::exception& e) {
        std::cerr << "Errore nel caricamento della configurazione: " << e.what() << "\n";
        std::cerr << "(percorso cercato: " << cfg_path << ")\n";
        return 1;
    }

        // HARDCODED FALLBACK — dataset_reali
    {
        Config::DatasetInfo bvz;
        bvz.prefix       = "BVZ-tsukuba";
        bvz.directory    = "benchmarkreali/BVZ-tsukuba";
        bvz.count        = 16;
        bvz.output_dir   = "benchmarkBVZ";
        bvz.out_files_key = "out_files_bvz";
        cfg.dataset_reali["-BVZ"] = bvz;

        Config::DatasetInfo kz2;
        kz2.prefix       = "KZ2-venus";
        kz2.directory    = "benchmarkreali/KZ2-venus";
        kz2.count        = 22;
        kz2.output_dir   = "benchmarkKZ2";
        kz2.out_files_key = "out_files_kz2";
        cfg.dataset_reali["-KZ2"] = kz2;
    }




    //debug
    /*    std::cout << "=== dataset_reali caricati ===\n";
    for (auto& [k, v] : cfg.dataset_reali)
        std::cout << "  [" << k << "] prefix=" << v.prefix << "\n";
    std::cout << "==============================\n";*/


    if (argc == 1) {
        std::cerr << "Utilizzo: " << argv[0] << " <flag> [dataset|file]\n";
        return 1;
    }

    std::string alg_flag = argv[1];

    // ── -fullsuite ─────────────────────────────────────────────────────────
    if (alg_flag == "-fullsuite") {
        for (auto& [alg_name, inst_dir] : cfg.instances_dir) {
            run_benchmark_sintetico(alg_name, inst_dir, "benchmarksintetici");
        }
        return 0;
    }

    if (cfg.algs.find(alg_flag) == cfg.algs.end()) {
        std::cerr << "Argomento sconosciuto: " << alg_flag << ". Usa uno tra:";
        for (auto& [k, _] : cfg.algs) std::cerr << " " << k;
        std::cerr << "\n";
        return 1;
    }

    if (argc < 3) {
        std::cerr << "Utilizzo: " << argv[0] << " <flag> <dataset|file>\n";
        return 1;
    }

    std::string algorithm = cfg.algs.at(alg_flag);
    std::string dataset   = argv[2];
    // ── Dataset reali ──────────────────────────────────────────────────────
    if (cfg.dataset_reali.count(dataset)) {
        
        auto& di = cfg.dataset_reali.at(dataset);
        const auto& out_files = (di.out_files_key == "out_files_bvz")
                                ? cfg.out_files_bvz
                                : cfg.out_files_kz2;
        run_benchmark_reale(algorithm, di.prefix, di.directory,
                            di.count, di.output_dir, out_files);
    }
    // ── Benchmark sintetico ────────────────────────────────────────────────
    else if (dataset == "-SINTH") {
        if (cfg.instances_dir.find(algorithm) == cfg.instances_dir.end()) {
            std::cerr << "Nessuna directory di istanze configurata per: " << algorithm << "\n";
            return 1;
        }
        run_benchmark_sintetico(algorithm, cfg.instances_dir.at(algorithm),
                                "benchmarksintetici");
    }
    // ── Singolo file .max ──────────────────────────────────────────────────
    else {
        if (cfg.out_files_single.find(alg_flag) == cfg.out_files_single.end()) {
            std::cerr << "Nessun file CSV configurato per: " << alg_flag << "\n";
            return 1;
        }
        std::string csv_file = cfg.out_files_single.at(alg_flag);
        std::cout << "graph file = " << dataset << "\n";
        run_esperimento_singolo(algorithm, dataset, csv_file);
    }

    return 0;
}