#pragma once

/*
 * ============================================================================
 * Push-Relabel Max Flow  —  Goldberg & Tarjan (1988)
 * ============================================================================
 *
 * Supporta due modalità di capacità, selezionate dal costruttore usato:
 *
 *   - Modalità intera  (long long): per grafi con capacità intere o razionali
 *     già scalate. Aritmetica esatta, nessuna soglia epsilon.
 *
 *   - Modalità double: per grafi con capacità irrazionali (es. sqrt(2), pi).
 *     Usa una soglia epsilon per i confronti > 0, evitando push infiniti
 *     causati da rumore floating-point.
 *
 * Le tre ottimizzazioni (FIFO, gap heuristic, current-arc) sono presenti
 * in entrambe le modalità.
 *
 * Riferimento
 * -----------
 *   Goldberg, A. V., & Tarjan, R. E. (1988).
 *   "A new approach to the maximum-flow problem."
 *   Journal of the ACM, 35(4), 921–940.
 *   https://doi.org/10.1145/48014.61051
 * ============================================================================
 */

#include <deque>
#include <unordered_map>
#include <variant>
#include <vector>
#include <limits>
#include <stdexcept>


// ============================================================================
// PairHash — hasher per std::pair<int,int>
// ============================================================================
#include "pair_hash.hpp"
/*
struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const noexcept {
        std::size_t h1 = std::hash<int>{}(p.first);
        std::size_t h2 = std::hash<int>{}(p.second);
        return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};
*/



// ============================================================================
// ResidualEdge — arco nel grafo residuo, templatizzato sul tipo di capacità
// ============================================================================

template<typename Cap>
struct ResidualEdge {
    int to;   // nodo di destinazione (indice interno, 0-based)
    int rev;  // posizione dell'arco inverso in _adj[to]
    Cap cap;  // capacità residua corrente
};


// ============================================================================
// PushRelabel — classe principale
// ============================================================================

class PushRelabel {
public:

    using IntGraph = std::unordered_map<std::pair<int,int>, long long, PairHash>;
    using DblGraph = std::unordered_map<std::pair<int,int>, double,    PairHash>;

    /**
     * Costruttore modalità intera.
     * Per grafi con capacità long long (interi o razionali già scalati).
     * max_flow restituisce std::variant contenente long long.
     */
    explicit PushRelabel(const IntGraph& graph);

    /**
     * Costruttore modalità double.
     * Per grafi con capacità irrazionali o floating-point.
     * max_flow restituisce std::variant contenente double.
     */
    explicit PushRelabel(const DblGraph& graph);

    /**
     * Calcola il flusso massimo da source a sink.
     *
     * Il tipo restituito dipende dal costruttore usato:
     *   - IntGraph → std::get<long long>(result)
     *   - DblGraph → std::get<double>(result)
     *
     * @throws std::out_of_range  Se source o sink non sono nel grafo.
     */
    std::variant<long long, double> max_flow(int source, int sink);

private:

    int _n;
    std::unordered_map<int, int> _index;
    std::vector<int>             _label;

    // Uno solo dei due vettori è popolato, in base al costruttore chiamato
    std::vector<std::vector<ResidualEdge<long long>>> _adj_int;
    std::vector<std::vector<ResidualEdge<double>>>    _adj_dbl;

    bool _is_double = false;

    // Soglia epsilon per la modalità double:
    // ignora residui < EPS per evitare push infiniti da rumore floating-point.
    static constexpr double EPS = 1e-9;

    std::vector<int> _bfs_heights_int(int t) const;
    std::vector<int> _bfs_heights_dbl(int t) const;

    long long _max_flow_int(int s, int t);
    double    _max_flow_dbl(int s, int t);
};