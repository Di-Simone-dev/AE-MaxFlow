// capacity_scaling.cpp
// Implementazione di CapacityScaling (Gabow 1985 - O(m² log U))
//
// Struttura identica a PushRelabel: due implementazioni separate
// _max_flow_int (long long, aritmetica esatta) e _max_flow_dbl
// (double, confronti con soglia EPS).

#include "capacity_scaling.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <stdexcept>


// ============================================================================
// Costruttore modalità intera
// ============================================================================

CapacityScaling::CapacityScaling(const IntGraph& graph)
    : _is_double(false)
{
    for (auto& [edge, _cap] : graph) {
        for (int node : {edge.first, edge.second}) {
            if (_index.find(node) == _index.end()) {
                _index[node] = static_cast<int>(_label.size());
                _label.push_back(node);
            }
        }
    }
    _n = static_cast<int>(_label.size());
    _adj_int.resize(_n);

    for (auto& [edge, cap] : graph) {
        int u  = _index.at(edge.first);
        int v  = _index.at(edge.second);
        int eu = static_cast<int>(_adj_int[u].size());
        int ev = static_cast<int>(_adj_int[v].size());
        _adj_int[u].push_back({v, ev, cap});
        _adj_int[v].push_back({u, eu, 0LL});
    }
}


// ============================================================================
// Costruttore modalità double
// ============================================================================

CapacityScaling::CapacityScaling(const DblGraph& graph)
    : _is_double(true)
{
    for (auto& [edge, _cap] : graph) {
        for (int node : {edge.first, edge.second}) {
            if (_index.find(node) == _index.end()) {
                _index[node] = static_cast<int>(_label.size());
                _label.push_back(node);
            }
        }
    }
    _n = static_cast<int>(_label.size());
    _adj_dbl.resize(_n);

    for (auto& [edge, cap] : graph) {
        int u  = _index.at(edge.first);
        int v  = _index.at(edge.second);
        int eu = static_cast<int>(_adj_dbl[u].size());
        int ev = static_cast<int>(_adj_dbl[v].size());
        _adj_dbl[u].push_back({v, ev, cap});
        _adj_dbl[v].push_back({u, eu, 0.0});
    }
}


// ============================================================================
// max_flow — dispatcher
// ============================================================================

std::variant<long long, double> CapacityScaling::max_flow(int source, int sink) {
    auto it_s = _index.find(source);
    auto it_t = _index.find(sink);
    if (it_s == _index.end()) throw std::out_of_range("source non trovato nel grafo");
    if (it_t == _index.end()) throw std::out_of_range("sink non trovato nel grafo");

    int s = it_s->second;
    int t = it_t->second;

    if (s == t) return _is_double ? std::variant<long long, double>(0.0)
                                  : std::variant<long long, double>(0LL);

    if (_is_double)
        return _max_flow_dbl(s, t);
    else
        return _max_flow_int(s, t);
}


// ============================================================================
// _bfs_int — BFS nel delta-residual graph (long long)
// ============================================================================

std::optional<CapacityScaling::Parent>
CapacityScaling::_bfs_int(int s, int t, long long delta) const {
    Parent parent(_n, {-1, -1});
    std::vector<bool> visited(_n, false);
    parent[s]  = {s, -1};
    visited[s] = true;

    std::deque<int> queue;
    queue.push_back(s);

    while (!queue.empty()) {
        int u = queue.front(); queue.pop_front();
        if (u == t) return parent;
        for (int idx = 0; idx < static_cast<int>(_adj_int[u].size()); ++idx) {
            const auto& e = _adj_int[u][idx];
            if (!visited[e.to] && e.cap >= delta) {
                visited[e.to] = true;
                parent[e.to]  = {u, idx};
                queue.push_back(e.to);
            }
        }
    }
    return std::nullopt;
}


// ============================================================================
// _bfs_dbl — BFS nel delta-residual graph (double)
// ============================================================================

std::optional<CapacityScaling::Parent>
CapacityScaling::_bfs_dbl(int s, int t, double delta) const {
    Parent parent(_n, {-1, -1});
    std::vector<bool> visited(_n, false);
    parent[s]  = {s, -1};
    visited[s] = true;

    std::deque<int> queue;
    queue.push_back(s);

    while (!queue.empty()) {
        int u = queue.front(); queue.pop_front();
        if (u == t) return parent;
        for (int idx = 0; idx < static_cast<int>(_adj_dbl[u].size()); ++idx) {
            const auto& e = _adj_dbl[u][idx];
            if (!visited[e.to] && e.cap >= delta - EPS) {
                visited[e.to] = true;
                parent[e.to]  = {u, idx};
                queue.push_back(e.to);
            }
        }
    }
    return std::nullopt;
}


// ============================================================================
// _augment_int — augment su long long
// ============================================================================

long long CapacityScaling::_augment_int(int s, int t, const Parent& parent) {
    // Bottleneck
    long long bn = std::numeric_limits<long long>::max();
    for (int v = t; v != s; ) {
        auto [u, idx] = parent[v];
        bn = std::min(bn, _adj_int[u][idx].cap);
        v  = u;
    }
    // Aggiorna residui
    for (int v = t; v != s; ) {
        auto [u, idx] = parent[v];
        int rev = _adj_int[u][idx].rev;
        _adj_int[u][idx].cap      -= bn;
        _adj_int[v][rev].cap      += bn;
        v = u;
    }
    return bn;
}


// ============================================================================
// _augment_dbl — augment su double
// ============================================================================

double CapacityScaling::_augment_dbl(int s, int t, const Parent& parent) {
    // Bottleneck
    double bn = std::numeric_limits<double>::infinity();
    for (int v = t; v != s; ) {
        auto [u, idx] = parent[v];
        bn = std::min(bn, _adj_dbl[u][idx].cap);
        v  = u;
    }
    // Aggiorna residui
    for (int v = t; v != s; ) {
        auto [u, idx] = parent[v];
        int rev = _adj_dbl[u][idx].rev;
        _adj_dbl[u][idx].cap -= bn;
        _adj_dbl[v][rev].cap += bn;
        v = u;
    }
    return bn;
}


// ============================================================================
// _max_flow_int — algoritmo Capacity Scaling su long long
// ============================================================================

long long CapacityScaling::_max_flow_int(int s, int t) {
    // U = max capacità forward
    long long U = 0;
    for (auto& edges : _adj_int)
        for (auto& e : edges)
            U = std::max(U, e.cap);

    if (U == 0) return 0LL;

    // delta iniziale = più grande potenza di 2 <= U
    long long delta = 1LL;
    while (delta * 2 <= U) delta *= 2;

    long long total = 0LL;
    while (delta >= 1) {
        while (true) {
            auto path = _bfs_int(s, t, delta);
            if (!path.has_value()) break;
            total += _augment_int(s, t, *path);
        }
        delta /= 2;
    }
    return total;
}


// ============================================================================
// _max_flow_dbl — algoritmo Capacity Scaling su double
// ============================================================================

double CapacityScaling::_max_flow_dbl(int s, int t) {
    // U = max capacità forward
    double U = 0.0;
    for (auto& edges : _adj_dbl)
        for (auto& e : edges)
            U = std::max(U, e.cap);

    if (U < EPS) return 0.0;

    // delta iniziale = 2^floor(log2(U))
    double delta = std::pow(2.0, std::floor(std::log2(U)));

    double total = 0.0;
    while (delta >= EPS) {
        while (true) {
            auto path = _bfs_dbl(s, t, delta);
            if (!path.has_value()) break;
            total += _augment_dbl(s, t, *path);
        }
        delta /= 2.0;
    }
    return total;
}