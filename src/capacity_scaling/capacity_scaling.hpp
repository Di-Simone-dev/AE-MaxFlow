#pragma once

// capacity_scaling.hpp
// Traduzione di src/capacity_scaling/capacity_scaling.py
// Gabow (1985) - O(m² log U)

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <cassert>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "parsing.hpp"   // Capacity, Fraction

// ---------------------------------------------------------------------------
// Helpers su Capacity
// ---------------------------------------------------------------------------

// Converte qualsiasi Capacity in double
inline double cap_to_double(const Capacity& c) {
    return std::visit([](auto&& v) -> double {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, Fraction>) return v.to_double();
        else return static_cast<double>(v);
    }, c);
}

// Capacity > 0
inline bool cap_positive(const Capacity& c) {
    return cap_to_double(c) > 0.0;
}

// min tra due Capacity (degrada a double se i tipi differiscono)
inline Capacity cap_min(const Capacity& a, const Capacity& b) {
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b))
        return std::min(std::get<int64_t>(a), std::get<int64_t>(b));
    if (std::holds_alternative<Fraction>(a) && std::holds_alternative<Fraction>(b)) {
        const auto& fa = std::get<Fraction>(a);
        const auto& fb = std::get<Fraction>(b);
        return fa.to_double() <= fb.to_double() ? a : b;
    }
    return std::min(cap_to_double(a), cap_to_double(b));
}

// Capacity - Capacity
inline Capacity cap_sub(const Capacity& a, const Capacity& b) {
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b))
        return std::get<int64_t>(a) - std::get<int64_t>(b);
    if (std::holds_alternative<Fraction>(a) && std::holds_alternative<Fraction>(b)) {
        const auto& fa = std::get<Fraction>(a);
        const auto& fb = std::get<Fraction>(b);
        int64_t lcm = std::lcm(fa.den, fb.den);
        return Fraction(fa.num * (lcm / fa.den) - fb.num * (lcm / fb.den), lcm);
    }
    return cap_to_double(a) - cap_to_double(b);
}

// cap >= delta  (delta è sempre double in questo algo)
inline bool cap_ge(const Capacity& cap, double delta) {
    return cap_to_double(cap) >= delta;
}

// ---------------------------------------------------------------------------
// Struttura dell'arco nel grafo residuo
// ---------------------------------------------------------------------------

struct Edge {
    int    to;       // nodo destinazione
    int    rev;      // indice dell'arco inverso in _adj[to]
    Capacity cap;   // capacità residua
};

// ---------------------------------------------------------------------------
// CapacityScaling
// ---------------------------------------------------------------------------

class CapacityScaling {
public:
    // graph: mappa (u, v) -> capacità, con nodi etichettati come interi
    explicit CapacityScaling(const std::map<std::pair<int,int>, Capacity>& graph)
        : _graph(graph)
    {
        // Costruisce ordinamento stabile dei nodi
        for (auto& [edge, _] : graph) {
            for (int node : {edge.first, edge.second}) {
                if (_index.find(node) == _index.end()) {
                    _index[node] = static_cast<int>(_label.size());
                    _label.push_back(node);
                }
            }
        }

        int n = static_cast<int>(_label.size());
        _adj.resize(n);

        for (auto& [edge, cap] : graph) {
            int u = _index.at(edge.first);
            int v = _index.at(edge.second);
            int u_edge_idx = static_cast<int>(_adj[u].size());
            int v_edge_idx = static_cast<int>(_adj[v].size());
            _adj[u].push_back({v, v_edge_idx, cap});  // forward
            _adj[v].push_back({u, u_edge_idx, Capacity{int64_t(0)}});  // reverse
            _edge_location[{edge.first, edge.second}] = {u, u_edge_idx};
        }
    }

    // ------------------------------------------------------------------
    // Interfaccia pubblica
    // ------------------------------------------------------------------

    Capacity max_flow(int source, int sink) {
        int s = _index.at(source);
        int t = _index.at(sink);

        if (s == t) return int64_t(0);

        // U = max capacità forward -> delta iniziale = 2^floor(log2(U))
        double U = 0.0;
        for (auto& adj : _adj)
            for (auto& e : adj)
                U = std::max(U, cap_to_double(e.cap));

        if (U == 0.0) return int64_t(0);

        double delta = std::pow(2.0, std::floor(std::log2(U)));

        // EPS dipende dal tipo: 1 per int/Fraction, 1e-12 per float
        double eps = _is_rational() ? 1.0 : 1e-12;

        Capacity total{int64_t(0)};

        while (delta >= eps) {
            while (true) {
                auto path = _bfs(s, t, delta);
                if (!path.has_value()) break;
                total = capacity_add(total, _augment(s, t, *path));
            }
            if (eps == 1.0)
                delta = std::floor(delta / 2.0);  // equivalente a >>= 1 su interi
            else
                delta /= 2.0;
        }

        return total;
    }

    // Flusso sull'arco (u, v) dopo aver chiamato max_flow()
    Capacity flow_on_edge(int u, int v) const {
        auto it = _edge_location.find({u, v});
        if (it == _edge_location.end())
            throw std::runtime_error("Arco non trovato nel grafo");
        auto [u_idx, edge_idx] = it->second;
        // flusso = capacità originale - residua
        Capacity original = _graph.at({u, v});
        Capacity residual = _adj[u_idx][edge_idx].cap;
        return cap_sub(original, residual);
    }

private:
    // ------------------------------------------------------------------
    // Stato interno
    // ------------------------------------------------------------------

    const std::map<std::pair<int,int>, Capacity>& _graph;

    std::map<int, int>          _index;         // label -> indice interno
    std::vector<int>            _label;         // indice interno -> label
    std::vector<std::vector<Edge>> _adj;        // grafo residuo
    std::map<std::pair<int,int>, std::pair<int,int>> _edge_location; // (u,v) -> (u_idx, edge_idx)

    // ------------------------------------------------------------------
    // BFS nel delta-residual graph
    // Ritorna parent table se t è raggiungibile, altrimenti nullopt
    // ------------------------------------------------------------------
    using Parent = std::vector<std::pair<int,int>>;  // parent[v] = {u, edge_idx}

    std::optional<Parent> _bfs(int s, int t, double delta) {
        int n = static_cast<int>(_adj.size());
        Parent parent(n, {-1, -1});
        std::vector<bool> visited(n, false);

        parent[s] = {s, -1};
        visited[s] = true;
        std::deque<int> queue;
        queue.push_back(s);

        while (!queue.empty()) {
            int u = queue.front(); queue.pop_front();
            if (u == t) return parent;
            for (int idx = 0; idx < static_cast<int>(_adj[u].size()); ++idx) {
                const Edge& e = _adj[u][idx];
                if (!visited[e.to] && cap_ge(e.cap, delta)) {
                    visited[e.to] = true;
                    parent[e.to] = {u, idx};
                    queue.push_back(e.to);
                }
            }
        }
        return std::nullopt;
    }

    // ------------------------------------------------------------------
    // Augment: trova il bottleneck e aggiorna le capacità residue
    // ------------------------------------------------------------------
    Capacity _augment(int s, int t, const Parent& parent) {
        // Step 1: bottleneck
        Capacity bottleneck{std::numeric_limits<double>::infinity()};
        for (int v = t; v != s; ) {
            auto [u, edge_idx] = parent[v];
            bottleneck = cap_min(bottleneck, _adj[u][edge_idx].cap);
            v = u;
        }

        // Step 2: aggiorna residui
        for (int v = t; v != s; ) {
            auto [u, edge_idx] = parent[v];
            int rev_idx = _adj[u][edge_idx].rev;
            _adj[u][edge_idx].cap = cap_sub(_adj[u][edge_idx].cap, bottleneck);  // forward: riduci
            _adj[v][rev_idx].cap  = capacity_add(_adj[v][rev_idx].cap, bottleneck); // reverse: aumenta
            v = u;
        }

        return bottleneck;
    }

    // Determina se le capacità sono tutte int o Fraction (→ EPS = 1)
    bool _is_rational() const {
        if (_graph.empty()) return true;
        const Capacity& first = _graph.begin()->second;
        return std::holds_alternative<int64_t>(first) ||
               std::holds_alternative<Fraction>(first);
    }
};