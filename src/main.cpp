/**
 * @file main.cpp
 * @brief Entry point e orchestrazione del benchmark di algoritmi di flusso massimo.
 *
 * Traduzione C++ di `main.py`. Supporta tre algoritmi di flusso massimo
 * su grafi in formato DIMACS `.max`:
 *
 * | Flag CLI   | Algoritmo             |
 * |------------|-----------------------|
 * | `-pr`      | Push-Relabel          |
 * | `-cs`      | Capacity Scaling      |
 * | `-alt`     | Almost Linear Time    |
 *
 * ### Modalità di esecuzione
 * ```
 * ./maxflow -pr  <dataset|file.max>   # singolo file o dataset reale
 * ./maxflow -cs  <dataset|file.max>
 * ./maxflow -alt <dataset|file.max>
 * ./maxflow -fullsuite                # tutti gli algoritmi, tutti i dataset sintetici
 * ```
 *
 * ### Configurazione
 * Legge `configs/configmain.toml` dalla stessa directory dell'eseguibile
 * tramite un parser TOML minimale (solo sezioni piatte e annidate con
 * valori stringa; nessuna dipendenza esterna).
 *
 * ### Gestione delle capacità
 * Le capacità degli archi possono essere intere, razionali (`Fraction`) o
 * double. La funzione scala_grafo() normalizza il grafo prima di passarlo
 * al solver appropriato, usando scale_rationals() per i grafi razionali.
 *
 * ### Formato CSV di output
 * Identico al Python originale, compresi i tempi formattati con 17 cifre
 * decimali (`"%.17f"`).
 *
 * @see push_relabel.hpp, capacity_scaling.hpp, almost_linear_time.hpp
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
#include <set>

#include "src/util/parse_dimacs.hpp"
#include "src/util/capacity.hpp"
#include "src/util/fraction.hpp"
#include "src/util/scale_rationals.hpp"

#include "src/almost_linear/almost_linear_time.hpp"
#include "src/capacity_scaling/capacity_scaling.hpp"
#include "src/push_relabel/push_relabel.hpp"

namespace fs = std::filesystem;

/// Forward declaration: implementazione in `src/util/parse_dimacs.cpp`.
DimacsResult parse_dimacs(const std::string& path);

/// Alias per il grafo con capacità intere (usato da PushRelabel e AlmostLinearTime).
using IntGraph = std::unordered_map<std::pair<int,int>, long long, PairHash>;

/// Alias per il grafo con capacità double (usato da PushRelabel e CapacityScaling in modalità double).
using DblGraph = std::unordered_map<std::pair<int,int>, double, PairHash>;


// ============================================================================
// Parser TOML minimale
// ============================================================================

/**
 * @brief Mappa chiave→valore per una singola sezione TOML.
 *
 * Tutti i valori sono trattati come stringhe (le virgolette vengono rimosse).
 */
using TomlSection = std::map<std::string, std::string>;

/**
 * @brief Documento TOML: mappa nome-sezione → TomlSection.
 *
 * Supporta sezioni piatte (`[algs]`) e sezioni annidate simulate con punti
 * (`[dataset_reali.-BVZ]`). Non supporta array TOML né valori numerici nativi.
 */
using TomlDoc = std::map<std::string, TomlSection>;

/**
 * @brief Rimuove spazi, tab e newline all'inizio e alla fine di una stringa.
 *
 * @param s  Stringa da trimmare.
 * @return   Copia trimmata di @p s. Stringa vuota se @p s contiene solo
 *           caratteri di spaziatura.
 */
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

/**
 * @brief Rimuove le virgolette doppie che racchiudono una stringa TOML.
 *
 * Se @p s inizia e finisce con `"`, restituisce il contenuto interno;
 * altrimenti restituisce @p s invariata.
 *
 * @param s  Valore TOML grezzo (con o senza virgolette).
 * @return   Valore senza virgolette.
 */
static std::string strip_quotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

/**
 * @brief Legge e analizza un file TOML minimale.
 *
 * Il parser riconosce:
 * - Righe vuote e commenti (iniziano con `#`): ignorati.
 * - Intestazioni di sezione: `[nome_sezione]`
 * - Coppie chiave-valore: `chiave = "valore"` (virgolette opzionali).
 *
 * Non supporta array, tabelle inline, valori multiriga o tipi nativi TOML
 * (interi, booleani, date). Sufficiente per `configmain.toml`.
 *
 * @param path  Percorso del file TOML da leggere.
 * @return      Documento TOML come TomlDoc.
 * @throws std::runtime_error  Se il file non può essere aperto.
 */
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
            // Intestazione di sezione: "[nome]" o "[sezione.sottosezione]"
            cur_section = trim(l.substr(1, l.rfind(']') - 1));
            doc[cur_section]; // assicura che la sezione esista anche se vuota
            continue;
        }

        auto eq = l.find('=');
        if (eq == std::string::npos) continue;
        std::string key = strip_quotes(trim(l.substr(0, eq)));
        std::string val = strip_quotes(trim(l.substr(eq + 1)));
        doc[cur_section][key] = val;
    }
    return doc;
}


// ============================================================================
// Configurazione
// ============================================================================

/**
 * @brief Configurazione del programma, caricata da `configmain.toml`.
 *
 * Ogni campo corrisponde a una sezione del file TOML. Le sezioni annidate
 * `[dataset_reali.<flag>]` vengono raccolte in dataset_reali.
 */
struct Config {
    std::map<std::string,std::string> algs;            ///< `[algs]`: flag CLI → chiave algoritmo.
    std::map<std::string,std::string> out_files_single;///< `[out_files_single]`: flag → percorso CSV.
    std::map<std::string,std::string> out_files_bvz;   ///< `[out_files_bvz]`: flag → CSV dataset BVZ.
    std::map<std::string,std::string> out_files_kz2;   ///< `[out_files_kz2]`: flag → CSV dataset KZ2.
    std::map<std::string,std::string> instances_dir;   ///< `[instances_dir]`: algoritmo → directory.

    /**
     * @brief Metadati di un dataset reale (BVZ, KZ2, ...).
     */
    struct DatasetInfo {
        std::string prefix;       ///< Prefisso del nome file (es. `"BVZ-tsukuba"`).
        std::string directory;    ///< Directory contenente i file `.max`.
        std::string output_dir;   ///< Directory di output per i CSV.
        std::string out_files_key;///< Chiave per selezionare `out_files_bvz` o `out_files_kz2`.
        int         count;        ///< Numero di istanze nel dataset.
    };

    std::map<std::string, DatasetInfo> dataset_reali; ///< Dataset reali indicizzati per flag CLI.
};

/**
 * @brief Carica la configurazione da un file TOML.
 *
 * Legge le sezioni piatte (`algs`, `out_files_single`, ecc.) e le sezioni
 * annidate `[dataset_reali.<flag>]` convertendole in Config::DatasetInfo.
 *
 * @param path  Percorso del file `configmain.toml`.
 * @return      Oggetto Config popolato.
 * @throws std::runtime_error  Se il file non può essere aperto.
 */
static Config load_config(const std::string& path) {
    TomlDoc doc = parse_toml(path);
    Config cfg;

    for (auto& [k, v] : doc["algs"])            cfg.algs[k]             = v;
    for (auto& [k, v] : doc["out_files_single"]) cfg.out_files_single[k] = v;
    for (auto& [k, v] : doc["out_files_bvz"])   cfg.out_files_bvz[k]    = v;
    for (auto& [k, v] : doc["out_files_kz2"])   cfg.out_files_kz2[k]    = v;
    for (auto& [k, v] : doc["instances_dir"])   cfg.instances_dir[k]     = v;

    // Sezioni annidate: "dataset_reali.<flag>" → DatasetInfo
    // "dataset_reali." ha lunghezza 14; il flag inizia dal carattere 14.
    static constexpr size_t PREFIX_LEN = 14; // lunghezza di "dataset_reali."
    for (auto& [sec, kvmap] : doc) {
        if (sec.substr(0, PREFIX_LEN) != "dataset_reali.") continue;
        std::string flag = sec.substr(PREFIX_LEN);
        Config::DatasetInfo di;
        di.prefix        = kvmap.count("prefix")        ? kvmap.at("prefix")        : "";
        di.directory     = kvmap.count("directory")     ? kvmap.at("directory")     : "";
        di.count         = kvmap.count("count")         ? std::stoi(kvmap.at("count")) : 0;
        di.output_dir    = kvmap.count("output_dir")    ? kvmap.at("output_dir")    : "";
        di.out_files_key = kvmap.count("out_files_key") ? kvmap.at("out_files_key") : "";
        cfg.dataset_reali[flag] = di;
    }
    return cfg;
}


// ============================================================================
// Utilità CSV
// ============================================================================

/**
 * @brief Crea il file CSV con intestazione se non esiste già.
 *
 * Crea le directory intermedie se necessario. Se il file esiste già,
 * non lo sovrascrive (comportamento append-safe).
 *
 * @param csv_path  Percorso del file CSV.
 * @param header    Vettore dei nomi di colonna.
 */
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

/**
 * @brief Aggiunge una riga di dati a un file CSV esistente.
 *
 * Apre il file in modalità append e scrive i valori separati da virgola.
 *
 * @param csv_path  Percorso del file CSV (deve esistere con intestazione).
 * @param row       Valori della riga, già convertiti in stringa.
 */
static void scrivi_riga_csv(const std::string& csv_path,
                             const std::vector<std::string>& row) {
    std::ofstream f(csv_path, std::ios::app);
    for (size_t i = 0; i < row.size(); ++i)
        f << (i ? "," : "") << row[i];
    f << "\n";
}


// ============================================================================
// Lettura soluzione di riferimento
// ============================================================================

/**
 * @brief Legge il valore di flusso atteso da un file `.sol` DIMACS.
 *
 * Cerca la prima riga con tag `s` e restituisce il valore associato.
 * Il formato atteso è: `s <valore>`.
 *
 * @param path  Percorso del file `.sol`.
 * @return      Valore del flusso atteso come `long long`.
 * @throws std::runtime_error  Se il file non esiste o non contiene la riga `s`.
 */
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


// ============================================================================
// Normalizzazione del grafo
// ============================================================================

/**
 * @brief Grafo normalizzato pronto per un solver di flusso massimo.
 *
 * Uno dei due campi (`graph` o `graph_dbl`) è popolato a seconda del
 * tipo di capacità rilevato nel grafo originale.
 */
struct GraphScaled {
    IntGraph  graph;          ///< Grafo con capacità `long long` (interi o razionali scalati).
    DblGraph  graph_dbl;      ///< Grafo con capacità `double` (irrazionali).
    long long fattore;        ///< Fattore di scala applicato (1 se nessuna scalatura).
    bool      is_double = false; ///< `true` se il grafo usa `graph_dbl`.
};

/**
 * @brief Normalizza le capacità del grafo per il solver appropriato.
 *
 * Seleziona una delle tre strategie in base al tipo di capacità presente:
 *
 * | Caso                    | Strategia                                    |
 * |-------------------------|----------------------------------------------|
 * | Solo interi             | Copia diretta in `IntGraph` (`fattore = 1`). |
 * | Almeno un `double`      | Conversione tutto-double in `DblGraph`.      |
 * | Solo `Fraction`         | Scalatura via MCM dei denominatori (scale_rationals()). |
 *
 * Per il caso razionale, il `fattore` restituito permette di riconvertire
 * il flusso intero calcolato nel valore razionale corretto.
 *
 * @param graph_in  Grafo DIMACS con capacità di tipo `Capacity` (variant).
 * @return          Struttura GraphScaled con grafo normalizzato e fattore di scala.
 */
static GraphScaled scala_grafo(const DimacsResult& graph_in) {

    bool has_fraction = false;
    bool has_double   = false;
    for (auto& [edge, cap] : graph_in.graph) {
        (void)edge; // edge non usato nel controllo del tipo
        if (std::holds_alternative<Fraction>(cap)) has_fraction = true;
        if (std::holds_alternative<double>(cap))   has_double   = true;
    }

    // Caso 1: solo interi — nessuna trasformazione necessaria
    if (!has_fraction && !has_double) {
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

    // Caso 2: almeno una capacità double (irrazionale) — il solver lavora in double con eps
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

    IntGraph ig;
    for (size_t i = 0; i < keys.size(); ++i)
        ig[keys[i]] = scaled[i];
    return {std::move(ig), {}, static_cast<long long>(k), false};
}


// ============================================================================
// Formattazione
// ============================================================================

/**
 * @brief Converte un valore di flusso `Capacity` in stringa leggibile.
 *
 * | Tipo sottostante | Formato output             |
 * |------------------|----------------------------|
 * | `int`            | Decimale intero.           |
 * | `Fraction`       | `"num/den"`.               |
 * | `double`         | Notazione decimale standard.|
 *
 * @param flow  Valore di flusso come `Capacity` (variant).
 * @return      Rappresentazione stringa del flusso.
 */
static std::string format_flow(const Capacity& flow) {
    return std::visit([](auto&& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, int>)
            return std::to_string(v);
        else if constexpr (std::is_same_v<T, Fraction>)
            return std::to_string(v.num) + "/" + std::to_string(v.den);
        else // double
            return std::to_string(v);
    }, flow);
}

/**
 * @brief Formatta un tempo in secondi con 17 cifre decimali.
 *
 * Equivalente a `f"{elapsed:.17f}"` in Python, garantendo compatibilità
 * esatta del formato CSV tra le due implementazioni.
 *
 * @param t  Tempo in secondi.
 * @return   Stringa con 17 cifre decimali.
 */
static std::string fmt_time(double t) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(17) << t;
    return ss.str();
}


// ============================================================================
// Parsing DIMACS sicuro
// ============================================================================

/**
 * @brief Versione sicura di parse_dimacs() che termina con messaggio d'errore.
 *
 * Distingue tra file non trovato e formato errato (analogamente a
 * `FileNotFoundError` vs `ValueError` in Python) e stampa un messaggio
 * appropriato su stderr prima di terminare il programma.
 *
 * @param path  Percorso del file DIMACS `.max`.
 * @return      DimacsResult con il grafo analizzato.
 * @note        Non ritorna mai in caso di errore (chiama `std::exit(1)`).
 */
[[nodiscard]] static DimacsResult parse_dimacs_safe(const std::string& path) {
    try {
        return parse_dimacs(path);
    } catch (const std::runtime_error& e) {
        std::string what = e.what();
        if (what.find("not found")   != std::string::npos ||
            what.find("non trovato") != std::string::npos ||
            what.find("open")        != std::string::npos) {
            std::cerr << "Errore: file non trovato: \"" << path << "\"\n";
        } else {
            std::cerr << "Errore nel parsing di \"" << path << "\": " << what << "\n";
        }
        std::exit(1);
    }
}


// ============================================================================
// Esecuzione del solver
// ============================================================================

/**
 * @brief Esegue il solver di flusso massimo selezionato e misura il tempo.
 *
 * Smista la chiamata all'algoritmo giusto in base al parametro @p algorithm,
 * passando il tipo di grafo corretto (intero o double) a seconda di
 * GraphScaled::is_double.
 *
 * Per i grafi razionali scalati, applica la funzione `descala` per
 * ricondurre il flusso intero calcolato al valore Capacity originale
 * (int, Fraction o double), semplificando la frazione tramite MCD.
 *
 * @param algorithm  Nome dell'algoritmo: `"push_relabel"`, `"capacity_scaling"`
 *                   o `"almost_linear_time"`.
 * @param gs         Grafo normalizzato con fattore di scala.
 * @param source     Nodo sorgente (identificatore originale).
 * @param sink       Nodo pozzo (identificatore originale).
 * @return           Coppia `(flusso massimo, tempo_elapsed_in_secondi)`.
 *
 * @note Se source è isolato (nessun arco uscente), il solver lancia
 *       `std::out_of_range`; in questo caso il flusso viene impostato a 0.
 *
 * @throws std::runtime_error  Se @p algorithm non è riconosciuto.
 * @throws std::runtime_error  Se `almost_linear_time` viene usato con
 *                             capacità non intere.
 */
static std::pair<Capacity, double>
esegui_max_flow(const std::string& algorithm,
                const GraphScaled& gs,
                int source, int sink) {
    auto t0 = std::chrono::high_resolution_clock::now();
    Capacity flow = 0;

    // Converti long long + fattore di scala → Capacity (int, Fraction o double)
    auto descala = [&](long long f) -> Capacity {
        if (gs.fattore == 1) {
            return static_cast<int>(f);
        }
        long long g   = std::gcd(f, gs.fattore);
        long long num = f          / g;
        long long den = gs.fattore / g;
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
                throw std::runtime_error(
                    "almost_linear_time supporta solo capacità intere.");
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
        // Source isolato (nessun arco uscente): flusso massimo = 0 per definizione
        std::cout << "Source isolato → flusso = 0\n";
        flow = 0;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    return {flow, elapsed};
}


// ============================================================================
// Benchmark su dataset reali
// ============================================================================

/**
 * @brief Esegue il benchmark su un dataset reale (BVZ o KZ2).
 *
 * Per ogni istanza `i` in `[0, count)`:
 * 1. Legge `<directory>/<prefix><i>.max`.
 * 2. Esegue il solver e misura il tempo.
 * 3. Se esiste `<directory>/<prefix><i>.sol`, verifica la correttezza
 *    confrontando il flusso calcolato con il valore atteso.
 * 4. Scrive una riga nel CSV di output.
 *
 * @param algorithm   Nome dell'algoritmo da usare.
 * @param prefix      Prefisso dei file del dataset (es. `"BVZ-tsukuba"`).
 * @param directory   Directory contenente i file `.max` e `.sol`.
 * @param count       Numero di istanze nel dataset (indici 0-based).
 * @param output_dir  Directory di output per il CSV.
 * @param out_files   Mappa algoritmo → percorso CSV (da `Config::out_files_bvz`
 *                    o `Config::out_files_kz2`).
 */
static void run_benchmark_reale(
    const std::string& algorithm,
    const std::string& prefix,
    const std::string& directory,
    int count,
    const std::string& output_dir,
    const std::map<std::string,std::string>& out_files)
{
    fs::path csv_path = fs::path(output_dir) /
                        fs::path(out_files.at(algorithm)).filename();

    init_csv(csv_path.string(),
             {"n", "m", "time_seconds", "flow", "graph_file", "correctness"});

    for (int i = 0; i < count; ++i) {
        std::string base       = directory + "/" + prefix + std::to_string(i);
        std::string graph_file = prefix + std::to_string(i);

        DimacsResult pg = parse_dimacs_safe(base + ".max");

        auto gs = scala_grafo(pg);
        auto [flow, elapsed] = esegui_max_flow(algorithm, gs, pg.source, pg.sink);

        std::string correctness = "N/A";
        std::string sol_path    = base + ".sol";
        std::string flow_str    = format_flow(flow);

        if (fs::exists(sol_path)) {
            long long sol = estrai_valore_sol(sol_path);
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


// ============================================================================
// Benchmark sintetico
// ============================================================================

/**
 * @brief Esegue il benchmark su istanze sintetiche (layered, grid ed erdag).
 *
 * Visita ricorsivamente @p instances_dir cercando file `.max` con nomi
 * conformi ai pattern:
 * - `layered_n<N>_d<D>_seed<S>.max`
 * - `grid_n<N>_d<D>_seed<S>.max`
 * - `erdag_n<N>_p<P>_<DENSITY>_seed<S>.max`
 *
 * Per ogni istanza esegue @p runs_per_instance misurazioni, scarta la prima
 * (warmup) e riporta la mediana delle restanti.
 *
 * Le istanze vengono ordinate per `graph_type → cap_type → d → n → seed`
 * prima dell'esecuzione, garantendo output riproducibile.
 * Per i grafi erdag il campo @c d nel CSV corrisponde al parametro
 * di densità numerico (es. @c 3000, @c 0040), funzionalmente analogo
 * al parametro @c d di layered e grid.
 *
 * @param algorithm          Nome dell'algoritmo da usare.
 * @param instances_dir      Directory radice delle istanze sintetiche.
 *                           Deve contenere sottocartelle per cap_type
 *                           (es. @c int, @c unit, @c rational, @c irrational),
 *                           ciascuna con sottocartelle per gruppo di istanze
 *                           (es. @c n1000_d4, @c grid1000_d5, @c erdag_n1000_p0_0040).
 * @param out_dir            Directory di output per il CSV.
 *                           Il file generato sarà @c <algorithm>_results.csv.
 * @param runs_per_instance  Numero di ripetizioni per istanza (default: 4).
 *                           La prima viene sempre scartata come warmup;
 *                           la mediana viene calcolata sulle restanti @c (runs_per_instance - 1).
 */
static void run_benchmark_sintetico(
    const std::string& algorithm,
    const std::string& instances_dir,
    const std::string& out_dir,
    int runs_per_instance = 4)
{
    std::string csv_path = out_dir + "/" + algorithm + "_results.csv";
    init_csv(csv_path, {"graph_type","cap_type","n","d","hi","seed","m",
                         "median_time","flow","graph_file"});
    std::cout << "Location risultati: " << csv_path << "\n";

    std::regex pattern_layered(
        R"(layered_n(\d+)_d(\d+)(?:_hi(\d+))?_seed(\d+)\.max$)");
    std::regex pattern_grid(
        R"(grid_n(\d+)_rows(\d+)(?:_d(\d+)_hi(\d+))?_seed(\d+)\.max$)");
    std::regex pattern_erdag(
        R"(erdag_n(\d+)_p(\d+)_(\d+)(?:_d(\d+)_hi(\d+))?_seed(\d+)\.max$)");

    const std::set<std::string> known_cap_types = {"int", "unit", "rational", "irrational"};

    struct Entry {
        std::string graph_type, cap_type, path, filename;
        int n, d, hi, seed;
        std::string d_display;
    };
    std::vector<Entry> entries;

    auto visit_cap_dir = [&](const fs::path& cap_path, const std::string& cap_type) {
        for (auto& nd_entry : fs::directory_iterator(cap_path)) {
            if (!nd_entry.is_directory()) continue;
            std::string nd_group = nd_entry.path().filename().string();

            std::string graph_type;
            if      (nd_group.rfind("grid",  0) == 0)  graph_type = "grid";
            else if (nd_group.rfind("erdag", 0) == 0)  graph_type = "erdag";
            else                                        graph_type = "layered";

            const std::regex& pat =
                (graph_type == "grid")  ? pattern_grid  :
                (graph_type == "erdag") ? pattern_erdag :
                                          pattern_layered;

            for (auto& fentry : fs::directory_iterator(nd_entry.path())) {
                if (fentry.path().extension() != ".max") continue;
                std::string filename = fentry.path().filename().string();
                std::smatch m;
                if (!std::regex_search(filename, m, pat)) {
                    std::cout << "Ignoro file non conforme: " << filename << "\n";
                    continue;
                }

                int n_val, d_val, hi_val = -1, seed_val;
                std::string d_display_val;

                if (graph_type == "layered") {
                    // [1]=n  [2]=d  [3]=hi(opt)  [4]=seed
                    n_val    = std::stoi(m[1]);
                    d_val    = std::stoi(m[2]);
                    if (m[3].matched) hi_val = std::stoi(m[3]);
                    seed_val = std::stoi(m[4]);
                    d_display_val = std::to_string(d_val);

                } else if (graph_type == "grid") {
                    // [1]=n  [2]=rows  [3]=d(opt)  [4]=hi(opt)  [5]=seed
                    n_val    = std::stoi(m[1]);
                    d_val    = m[3].matched ? std::stoi(m[3]) : std::stoi(m[2]);
                    if (m[4].matched) hi_val = std::stoi(m[4]);
                    seed_val = std::stoi(m[5]);
                    d_display_val = std::to_string(d_val);

                } else {
                    // erdag: [1]=n  [2]=p_int  [3]=p_frac  [4]=d(opt)  [5]=hi(opt)  [6]=seed
                    n_val    = std::stoi(m[1]);
                    if (m[4].matched) {
                        d_val         = std::stoi(m[4]);
                        d_display_val = std::to_string(d_val);
                    } else {
                        d_val         = -1;
                        d_display_val = m[2].str() + "." + m[3].str();
                    }
                    if (m[5].matched) hi_val = std::stoi(m[5]);
                    seed_val = std::stoi(m[6]);
                }

                entries.push_back({
                    graph_type, cap_type,
                    fentry.path().string(), filename,
                    n_val, d_val, hi_val, seed_val,
                    d_display_val
                });
            }
        }
    };

    for (auto& first : fs::directory_iterator(instances_dir)) {
        if (!first.is_directory()) continue;
        std::string first_name = first.path().filename().string();

        if (known_cap_types.count(first_name)) {
            visit_cap_dir(first.path(), first_name);
        } else {
            std::cout << "Ignoro directory inattesa: " << first.path() << "\n";
        }
    }

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.graph_type != b.graph_type) return a.graph_type < b.graph_type;
        if (a.cap_type   != b.cap_type)   return a.cap_type   < b.cap_type;
        if (a.d  != b.d)  return a.d  < b.d;
        if (a.hi != b.hi) return a.hi < b.hi;
        if (a.n  != b.n)  return a.n  < b.n;
        return a.seed < b.seed;
    });

    for (auto& e : entries) {
        std::cout << "Processing (type=" << e.graph_type
                  << ", cap=" << e.cap_type
                  << ", d=" << e.d_display
                  << ", hi=" << e.hi
                  << ", n=" << e.n
                  << ", seed=" << e.seed << "): " << e.path << "\n";

        DimacsResult pg;
        try { pg = parse_dimacs(e.path); }
        catch (const std::exception& ex) {
            std::cerr << "Errore nel parsing di " << e.path << ": " << ex.what() << "\n";
            continue;
        }

        auto gs_original = scala_grafo(pg);

        std::vector<double> times;
        Capacity flow_value = 0;

        for (int r = 0; r < runs_per_instance; ++r) {
            auto gs = gs_original;
            auto [flow, elapsed] = esegui_max_flow(algorithm, gs, pg.source, pg.sink);
            if (r == 0) flow_value = flow;
            times.push_back(elapsed);
        }

        std::vector<double> rest(times.begin() + 1, times.end());
        std::sort(rest.begin(), rest.end());

        double median_time = rest[rest.size() / 2];
        if (rest.size() % 2 == 0)
            median_time = (rest[rest.size()/2 - 1] + rest[rest.size()/2]) / 2.0;

        std::cout << "median_time = " << std::fixed << std::setprecision(6)
                  << median_time << "s\n";

        scrivi_riga_csv(csv_path, {
            e.graph_type, e.cap_type,
            std::to_string(e.n),
            e.d_display,
            std::to_string(e.hi),
            std::to_string(e.seed),
            std::to_string(pg.m_actual),
            fmt_time(median_time),
            format_flow(flow_value),
            e.filename
        });
    }
}


// ============================================================================
// Esperimento su singolo file
// ============================================================================

/**
 * @brief Esegue il solver su un singolo file `.max` e registra i risultati.
 *
 * Stampa su stdout le informazioni del grafo, il flusso calcolato e il tempo
 * impiegato. Scrive una riga nel CSV specificato.
 *
 * @param algorithm   Nome dell'algoritmo da usare.
 * @param graph_file  Percorso del file DIMACS `.max`.
 * @param csv_file    Percorso del file CSV di output (creato se non esiste).
 */
static void run_esperimento_singolo(
    const std::string& algorithm,
    const std::string& graph_file,
    const std::string& csv_file)
{
    init_csv(csv_file, {"n","m","time_seconds","flow","graph_file"});

    DimacsResult pg = parse_dimacs_safe(graph_file);
    auto gs = scala_grafo(pg);

    std::cout << "File   : " << graph_file   << "\n";
    std::cout << "Source : " << pg.source << "   Sink : " << pg.sink << "\n";
    std::cout << "Archi  : " << pg.graph.size() << "\n\n";

    auto [flow, elapsed] = esegui_max_flow(algorithm, gs, pg.source, pg.sink);

    std::string flow_str = format_flow(flow);

    double flow_d = std::visit([](auto&& v) -> double {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, Fraction>) return v.to_double();
        else return static_cast<double>(v);
    }, flow);

    std::cout << "Flow = " << flow_str
              << "  (reale = " << std::fixed << std::setprecision(6)
              << flow_d << ")\n";
    std::cout << "Tempo = " << std::fixed << std::setprecision(6) << elapsed << " s\n";

    scrivi_riga_csv(csv_file, {
        std::to_string(pg.n), std::to_string(pg.m_actual),
        fmt_time(elapsed), flow_str, graph_file
    });
}


// ============================================================================
// main
// ============================================================================

/**
 * @brief Punto di ingresso del programma.
 *
 * Legge la configurazione da `configs/configmain.toml` (relativo alla
 * directory dell'eseguibile), analizza gli argomenti CLI e smista verso
 * la modalità di esecuzione appropriata:
 *
 * | Argomento 2 (`argv[2]`) | Modalità                       |
 * |-------------------------|--------------------------------|
 * | `-BVZ` / `-KZ2`         | Benchmark su dataset reale.    |
 * | `-SINTH`                | Benchmark su istanze sintetiche.|
 * | `<file.max>`            | Singola istanza.               |
 *
 * Se il TOML non contiene i dataset reali, vengono applicati valori
 * hardcoded di fallback per BVZ-tsukuba e KZ2-venus.
 *
 * @param argc  Numero di argomenti CLI (minimo 2, o 3 per la maggior parte delle modalità).
 * @param argv  Array di argomenti: `argv[1]` = flag algoritmo, `argv[2]` = dataset/file.
 * @return      0 in caso di successo, 1 in caso di errore.
 */
int main(int argc, char* argv[]) {

    // Risolve il percorso del config rispetto alla directory dell'eseguibile
    fs::path exe_dir  = fs::path(argv[0]).parent_path();
    fs::path cfg_path = exe_dir / "configs" / "configmain.toml";

    Config cfg;
    try {
        cfg = load_config(cfg_path.string());
    } catch (const std::exception& e) {
        std::cerr << "Errore nel caricamento della configurazione: " << e.what() << "\n";
        std::cerr << "(percorso cercato: " << cfg_path << ")\n";
        return 1;
    }

    // Fallback hardcoded per i dataset reali se non presenti nel TOML
    if (!cfg.dataset_reali.count("-BVZ")) {
        Config::DatasetInfo bvz;
        bvz.prefix        = "BVZ-tsukuba";
        bvz.directory     = "benchmarkreali/BVZ-tsukuba";
        bvz.count         = 16;
        bvz.output_dir    = "benchmarkBVZ";
        bvz.out_files_key = "out_files_bvz";
        cfg.dataset_reali["-BVZ"] = bvz;
    }
    if (!cfg.dataset_reali.count("-KZ2")) {
        Config::DatasetInfo kz2;
        kz2.prefix        = "KZ2-venus";
        kz2.directory     = "benchmarkreali/KZ2-venus";
        kz2.count         = 22;
        kz2.output_dir    = "benchmarkKZ2";
        kz2.out_files_key = "out_files_kz2";
        cfg.dataset_reali["-KZ2"] = kz2;
    }

    if (argc == 1) {
        std::cerr << "Utilizzo: " << argv[0] << " <flag> [dataset|file]\n";
        return 1;
    }

    std::string alg_flag = argv[1];

    // ── -fullsuite: esegue tutti gli algoritmi su tutti i dataset sintetici ──
    if (alg_flag == "-fullsuite") {
        for (auto& [alg_name, inst_dir] : cfg.instances_dir)
            run_benchmark_sintetico(alg_name, inst_dir, "benchmarksintetici");
        return 0;
    }

    if (cfg.algs.find(alg_flag) == cfg.algs.end()) {
        std::cerr << "Argomento sconosciuto: " << alg_flag << ". Usa uno tra:";
        for (auto& [k, v] : cfg.algs) {
            (void)v;
            std::cerr << " " << k;
        }
        std::cerr << "\n";
        return 1;
    }

    if (argc < 3) {
        std::cerr << "Utilizzo: " << argv[0] << " <flag> <dataset|file>\n";
        return 1;
    }

    std::string algorithm = cfg.algs.at(alg_flag);
    std::string dataset   = argv[2];

    // ── Dataset reale (es. -BVZ, -KZ2) ──────────────────────────────────────
    if (cfg.dataset_reali.count(dataset)) {
        auto& di = cfg.dataset_reali.at(dataset);
        const auto& out_files = (di.out_files_key == "out_files_bvz")
                                ? cfg.out_files_bvz
                                : cfg.out_files_kz2;
        run_benchmark_reale(algorithm, di.prefix, di.directory,
                            di.count, di.output_dir, out_files);

    // ── Benchmark sintetico ──────────────────────────────────────────────────
    } else if (dataset == "-SINTH") {
        if (cfg.instances_dir.find(algorithm) == cfg.instances_dir.end()) {
            std::cerr << "Nessuna directory di istanze configurata per: " << algorithm << "\n";
            return 1;
        }
        run_benchmark_sintetico(algorithm, cfg.instances_dir.at(algorithm),
                                "benchmarksintetici");

    // ── Singolo file .max ────────────────────────────────────────────────────
    } else {
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