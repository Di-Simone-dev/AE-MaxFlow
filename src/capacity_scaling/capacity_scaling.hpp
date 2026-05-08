#pragma once

// capacity_scaling.hpp
// Gabow (1985) - O(m² log U)
//
// Supporta due modalità selezionate dal costruttore:
//   - IntGraph (long long): per grafi interi o razionali già scalati.
//                           max_flow restituisce variant<long long, double>
//                           con long long attivo.
//   - DblGraph (double):    per grafi irrazionali/floating-point.
//                           max_flow restituisce variant con double attivo.

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <deque>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <vector>
#include <cstdint>

#include "pair_hash.hpp"

// PairHash — condiviso con PushRelabel, guard contro doppia definizione
/*
#ifndef PAIR_HASH_DEFINED
#define PAIR_HASH_DEFINED
struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const noexcept {
        std::size_t h1 = std::hash<int>{}(p.first);
        std::size_t h2 = std::hash<int>{}(p.second);
        return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};
#endif
*/


// Arco nel grafo residuo, templatizzato sul tipo di capacità
template<typename Cap>
struct CSEdge {
    int to;
    int rev;
    Cap cap;
};

class CapacityScaling {
public:

    using IntGraph = std::unordered_map<std::pair<int,int>, long long, PairHash>;
    using DblGraph = std::unordered_map<std::pair<int,int>, double,    PairHash>;

    // Modalità intera: max_flow restituisce variant con long long attivo
    explicit CapacityScaling(const IntGraph& graph);

    // Modalità double: max_flow restituisce variant con double attivo
    explicit CapacityScaling(const DblGraph& graph);

    // Calcola il flusso massimo.
    // Usare std::get<long long> o std::get<double> in base al costruttore usato.
    std::variant<long long, double> max_flow(int source, int sink);

private:

    int  _n;
    bool _is_double = false;

    // Soglia epsilon per confronti in modalità double
    static constexpr double EPS = 1e-9;

    std::unordered_map<int, int> _index;
    std::vector<int>             _label;

    // Uno solo dei due è popolato in base al costruttore
    std::vector<std::vector<CSEdge<long long>>> _adj_int;
    std::vector<std::vector<CSEdge<double>>>    _adj_dbl;

    using Parent = std::vector<std::pair<int,int>>;  // parent[v] = {u, edge_idx}

    std::optional<Parent> _bfs_int(int s, int t, long long delta) const;
    std::optional<Parent> _bfs_dbl(int s, int t, double   delta) const;

    long long _augment_int(int s, int t, const Parent& parent);
    double    _augment_dbl(int s, int t, const Parent& parent);

    long long _max_flow_int(int s, int t);
    double    _max_flow_dbl(int s, int t);
};