// capacity_scaling.cpp
// Implementazione di CapacityScaling (Gabow 1985 - O(m² log U))

#include "capacity_scaling.hpp"

#include <cmath>
#include <deque>
#include <limits>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Costruttore
// ---------------------------------------------------------------------------

CapacityScaling::CapacityScaling(const std::map<std::pair<int,int>, Capacity>& graph)
    : _graph(graph)
{
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
        int u         = _index.at(edge.first);
        int v         = _index.at(edge.second);
        int u_edge_idx = static_cast<int>(_adj[u].size());
        int v_edge_idx = static_cast<int>(_adj[v].size());

        _adj[u].push_back({v, v_edge_idx, cap});                       // forward
        _adj[v].push_back({u, u_edge_idx, Capacity{int64_t(0)}});      // reverse

        _edge_location[{edge.first, edge.second}] = {u, u_edge_idx};
    }
}

// ---------------------------------------------------------------------------
// max_flow
// ---------------------------------------------------------------------------

Capacity CapacityScaling::max_flow(int source, int sink) {
    int s = _index.at(source);
    int t = _index.at(sink);

    if (s == t) return int64_t(0);

    // U = max capacità forward nel grafo originale
    double U = 0.0;
    for (auto& [edge, cap] : _graph)
        U = std::max(U, cap_to_double(cap));

    if (U == 0.0) return int64_t(0);

    // delta iniziale = 2^floor(log2(U))
    double delta = std::pow(2.0, std::floor(std::log2(U)));

    // eps = 1 per interi/frazioni, piccolo epsilon per floating-point
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

// ---------------------------------------------------------------------------
// flow_on_edge
// ---------------------------------------------------------------------------

Capacity CapacityScaling::flow_on_edge(int u, int v) const {
    auto it = _edge_location.find({u, v});
    if (it == _edge_location.end())
        throw std::runtime_error("Arco non trovato nel grafo: (" +
                                 std::to_string(u) + ", " + std::to_string(v) + ")");
    auto [u_idx, edge_idx] = it->second;
    Capacity original = _graph.at({u, v});
    Capacity residual = _adj[u_idx][edge_idx].cap;
    return cap_sub(original, residual);
}

// ---------------------------------------------------------------------------
// _bfs  (delta-residual graph)
// ---------------------------------------------------------------------------

std::optional<CapacityScaling::Parent>
CapacityScaling::_bfs(int s, int t, double delta) {
    int n = static_cast<int>(_adj.size());
    Parent parent(n, {-1, -1});
    std::vector<bool> visited(n, false);

    parent[s]  = {s, -1};
    visited[s] = true;

    std::deque<int> queue;
    queue.push_back(s);

    while (!queue.empty()) {
        int u = queue.front(); queue.pop_front();
        if (u == t) return parent;

        for (int idx = 0; idx < static_cast<int>(_adj[u].size()); ++idx) {
            const Edge& e = _adj[u][idx];
            if (!visited[e.to] && cap_ge(e.cap, delta)) {
                visited[e.to]   = true;
                parent[e.to]    = {u, idx};
                queue.push_back(e.to);
            }
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// _augment
// ---------------------------------------------------------------------------

Capacity CapacityScaling::_augment(int s, int t, const Parent& parent) {
    // Step 1: bottleneck lungo il cammino s→t
    Capacity bottleneck{std::numeric_limits<double>::infinity()};
    for (int v = t; v != s; ) {
        auto [u, edge_idx] = parent[v];
        bottleneck = cap_min(bottleneck, _adj[u][edge_idx].cap);
        v = u;
    }

    // Step 2: aggiorna capacità residue
    for (int v = t; v != s; ) {
        auto [u, edge_idx] = parent[v];
        int rev_idx = _adj[u][edge_idx].rev;
        _adj[u][edge_idx].cap = cap_sub(_adj[u][edge_idx].cap, bottleneck);     // forward: riduci
        _adj[v][rev_idx].cap  = capacity_add(_adj[v][rev_idx].cap, bottleneck); // reverse: aumenta
        v = u;
    }

    return bottleneck;
}

// ---------------------------------------------------------------------------
// _is_rational
// ---------------------------------------------------------------------------

bool CapacityScaling::_is_rational() const {
    if (_graph.empty()) return true;
    const Capacity& first = _graph.begin()->second;
    return std::holds_alternative<int>(first) ||
           std::holds_alternative<Fraction>(first);
}
