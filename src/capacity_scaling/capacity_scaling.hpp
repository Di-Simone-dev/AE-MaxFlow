#pragma once

// capacity_scaling.hpp
// Gabow (1985) - O(m² log U)

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <cassert>
#include <deque>
#include <limits>
#include <map>
#include <numeric>   // std::lcm
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <cstdint>

#include "capacity.hpp"

// ---------------------------------------------------------------------------
// Helpers su Capacity  (tutti inline: usati sia in .hpp che in .cpp)
// ---------------------------------------------------------------------------

/// Converte qualsiasi Capacity in double
inline double cap_to_double(const Capacity& c) {
    return std::visit([](auto&& v) -> double {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, Fraction>) return v.to_double();
        else return static_cast<double>(v);
    }, c);
}

/// Capacity > 0
inline bool cap_positive(const Capacity& c) {
    return cap_to_double(c) > 0.0;
}

/// cap >= delta  (delta è sempre double in questo algo)
inline bool cap_ge(const Capacity& cap, double delta) {
    return cap_to_double(cap) >= delta;
}

/// min tra due Capacity (degrada a double se i tipi differiscono)
inline Capacity cap_min(const Capacity& a, const Capacity& b) {
    // caso int + int
    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
        int ia = std::get<int>(a);
        int ib = std::get<int>(b);
        return Capacity{ std::min(ia, ib) };
    }

    // caso Fraction + Fraction
    if (std::holds_alternative<Fraction>(a) && std::holds_alternative<Fraction>(b)) {
        const auto& fa = std::get<Fraction>(a);
        const auto& fb = std::get<Fraction>(b);
        return (fa.to_double() <= fb.to_double()) ? a : b;
    }

    // tipi misti → confronto in double
    return Capacity{ std::min(cap_to_double(a), cap_to_double(b)) };
}

/// Capacity - Capacity  (mantiene il tipo se entrambi uguali)
inline Capacity cap_sub(const Capacity& a, const Capacity& b) {
    // caso int - int
    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
        int ia = std::get<int>(a);
        int ib = std::get<int>(b);
        return Capacity{ ia - ib };
    }

    // caso Fraction - Fraction (con lcm come avevi già fatto)
    if (std::holds_alternative<Fraction>(a) && std::holds_alternative<Fraction>(b)) {
        const auto& fa = std::get<Fraction>(a);
        const auto& fb = std::get<Fraction>(b);
        int64_t lcm = std::lcm(fa.den, fb.den);
        return Fraction(
            fa.num * (lcm / fa.den) - fb.num * (lcm / fb.den),
            lcm
        );
    }

    // tipi misti → degrado a double
    return Capacity{ cap_to_double(a) - cap_to_double(b) };
}

/// Capacity + Capacity  (simmetrico a cap_sub, mantiene il tipo se entrambi uguali)
inline Capacity capacity_add(const Capacity& a, const Capacity& b) {
    // caso int + int
    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
        int ia = std::get<int>(a);
        int ib = std::get<int>(b);
        return Capacity{ ia + ib };
    }

    // caso Fraction + Fraction (con lcm come avevi già fatto)
    if (std::holds_alternative<Fraction>(a) && std::holds_alternative<Fraction>(b)) {
        const auto& fa = std::get<Fraction>(a);
        const auto& fb = std::get<Fraction>(b);
        int64_t lcm = std::lcm(fa.den, fb.den);
        return Fraction(
            fa.num * (lcm / fa.den) + fb.num * (lcm / fb.den),
            lcm
        );
    }

    // tipi misti → degrado a double
    return Capacity{ cap_to_double(a) + cap_to_double(b) };
}

// ---------------------------------------------------------------------------
// Struttura dell'arco nel grafo residuo
// ---------------------------------------------------------------------------

struct Edge {
    int      to;   // nodo destinazione
    int      rev;  // indice dell'arco inverso in _adj[to]
    Capacity cap;  // capacità residua
};

// ---------------------------------------------------------------------------
// CapacityScaling
// ---------------------------------------------------------------------------

class CapacityScaling {
public:
    explicit CapacityScaling(const std::map<std::pair<int,int>, Capacity>& graph);

    /// Calcola il flusso massimo da source a sink.
    Capacity max_flow(int source, int sink);

    /// Flusso sull'arco (u, v) dopo aver chiamato max_flow().
    Capacity flow_on_edge(int u, int v) const;

private:
    using Parent = std::vector<std::pair<int,int>>;  // parent[v] = {u, edge_idx}

    // BFS nel delta-residual graph; ritorna nullopt se t non è raggiungibile
    std::optional<Parent> _bfs(int s, int t, double delta);

    // Augment sul cammino trovato dalla BFS; ritorna il bottleneck inviato
    Capacity _augment(int s, int t, const Parent& parent);

    // true se tutte le capacità sono int64 o Fraction (→ eps = 1)
    bool _is_rational() const;

    // ---- dati ----
    const std::map<std::pair<int,int>, Capacity>&              _graph;
    std::map<int, int>                                          _index;
    std::vector<int>                                            _label;
    std::vector<std::vector<Edge>>                              _adj;
    std::map<std::pair<int,int>, std::pair<int,int>>            _edge_location;
};