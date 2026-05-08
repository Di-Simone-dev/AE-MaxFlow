/*
 * push_relabel.cpp
 * ----------------
 * Implementazione di PushRelabel con supporto nativo per capacità
 * long long (interi/razionali scalati) e double (irrazionali).
 *
 * La struttura è duplicata intenzionalmente: _max_flow_int e _max_flow_dbl
 * sono identici nella logica ma differiscono nel tipo di capacità e nei
 * confronti (esatti per long long, con soglia EPS per double).
 */

#include "push_relabel.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <stdexcept>
#include <vector>


// ============================================================================
// Costruttore modalità intera
// ============================================================================

PushRelabel::PushRelabel(const IntGraph& graph)
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

PushRelabel::PushRelabel(const DblGraph& graph)
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

std::variant<long long, double> PushRelabel::max_flow(int source, int sink) {
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
// _bfs_heights_int — BFS inversa per modalità intera
// ============================================================================

std::vector<int> PushRelabel::_bfs_heights_int(int t) const {
    const int n = _n;
    std::vector<int> h(n, 2 * n);
    h[t] = 0;

    std::vector<std::vector<int>> radj(n);
    for (int u = 0; u < n; ++u)
        for (auto& e : _adj_int[u])
            if (e.cap > 0)
                radj[e.to].push_back(u);

    std::deque<int> queue;
    queue.push_back(t);
    while (!queue.empty()) {
        int v = queue.front(); queue.pop_front();
        for (int u : radj[v]) {
            if (h[u] == 2 * n) { h[u] = h[v] + 1; queue.push_back(u); }
        }
    }
    return h;
}


// ============================================================================
// _bfs_heights_dbl — BFS inversa per modalità double
// ============================================================================

std::vector<int> PushRelabel::_bfs_heights_dbl(int t) const {
    const int n = _n;
    std::vector<int> h(n, 2 * n);
    h[t] = 0;

    std::vector<std::vector<int>> radj(n);
    for (int u = 0; u < n; ++u)
        for (auto& e : _adj_dbl[u])
            if (e.cap > EPS)           // soglia epsilon per double
                radj[e.to].push_back(u);

    std::deque<int> queue;
    queue.push_back(t);
    while (!queue.empty()) {
        int v = queue.front(); queue.pop_front();
        for (int u : radj[v]) {
            if (h[u] == 2 * n) { h[u] = h[v] + 1; queue.push_back(u); }
        }
    }
    return h;
}


// ============================================================================
// _max_flow_int — algoritmo Push-Relabel su long long
// ============================================================================

long long PushRelabel::_max_flow_int(int s, int t) {
    const int n = _n;
    auto& adj   = _adj_int;

    std::vector<int> h = _bfs_heights_int(t);
    h[s] = n;
    for (int v = 0; v < n; ++v)
        if (v != s && h[v] >= n) h[v] = n + 1;

    std::vector<int> cnt(2 * n + 2, 0);
    for (int v = 0; v < n; ++v) cnt[h[v]]++;

    std::vector<long long> excess(n, 0LL);
    std::vector<int>       cur(n, 0);

    // Pre-saturazione archi uscenti da s
    for (auto& edge : adj[s]) {
        if (edge.cap > 0) {
            long long cap = edge.cap;
            edge.cap = 0;
            adj[edge.to][edge.rev].cap += cap;
            excess[edge.to] += cap;
            excess[s]       -= cap;
        }
    }

    std::deque<int>   queue;
    std::vector<bool> in_queue(n, false);
    for (int v = 0; v < n; ++v)
        if (v != s && v != t && excess[v] > 0) { queue.push_back(v); in_queue[v] = true; }

    while (!queue.empty()) {
        int u = queue.front(); queue.pop_front(); in_queue[u] = false;

        while (excess[u] > 0) {
            if (cur[u] == static_cast<int>(adj[u].size())) {
                // RELABEL
                int old_h = h[u];
                cnt[old_h]--;

                // Gap heuristic
                if (cnt[old_h] == 0 && old_h > 0 && old_h < n) {
                    for (int v = 0; v < n; ++v) {
                        if (v != s && h[v] > old_h && h[v] < n) {
                            cnt[h[v]]--;
                            h[v] = n + 1;
                            cnt[h[v]]++;
                            cur[v] = 0;
                        }
                    }
                }

                int min_h = 2 * n;
                for (auto& edge : adj[u])
                    if (edge.cap > 0)
                        min_h = std::min(min_h, h[edge.to]);

                h[u] = min_h + 1;
                cnt[h[u]]++;
                cur[u] = 0;
                if (h[u] > 2 * n) break;

            } else {
                // PUSH
                ResidualEdge<long long>& edge = adj[u][cur[u]];
                int       v   = edge.to;
                long long res = edge.cap;

                if (res > 0 && h[u] == h[v] + 1) {
                    long long delta = std::min(excess[u], res);
                    edge.cap                -= delta;
                    adj[v][edge.rev].cap    += delta;
                    excess[u]               -= delta;
                    excess[v]               += delta;
                    if (v != s && v != t && !in_queue[v] && excess[v] > 0) {
                        queue.push_back(v); in_queue[v] = true;
                    }
                } else {
                    cur[u]++;
                }
            }
        }
    }

    return excess[t];
}


// ============================================================================
// _max_flow_dbl — algoritmo Push-Relabel su double
//
// Identico a _max_flow_int tranne che:
//   - i confronti "cap > 0" diventano "cap > EPS"
//   - i confronti "excess > 0" diventano "excess > EPS"
//   - std::min su double invece di long long
// ============================================================================

double PushRelabel::_max_flow_dbl(int s, int t) {
    const int n = _n;
    auto& adj   = _adj_dbl;

    std::vector<int> h = _bfs_heights_dbl(t);
    h[s] = n;
    for (int v = 0; v < n; ++v)
        if (v != s && h[v] >= n) h[v] = n + 1;

    std::vector<int> cnt(2 * n + 2, 0);
    for (int v = 0; v < n; ++v) cnt[h[v]]++;

    std::vector<double> excess(n, 0.0);
    std::vector<int>    cur(n, 0);

    // Pre-saturazione archi uscenti da s
    for (auto& edge : adj[s]) {
        if (edge.cap > EPS) {
            double cap = edge.cap;
            edge.cap = 0.0;
            adj[edge.to][edge.rev].cap += cap;
            excess[edge.to] += cap;
            excess[s]       -= cap;
        }
    }

    std::deque<int>   queue;
    std::vector<bool> in_queue(n, false);
    for (int v = 0; v < n; ++v)
        if (v != s && v != t && excess[v] > EPS) { queue.push_back(v); in_queue[v] = true; }

    while (!queue.empty()) {
        int u = queue.front(); queue.pop_front(); in_queue[u] = false;

        while (excess[u] > EPS) {
            if (cur[u] == static_cast<int>(adj[u].size())) {
                // RELABEL
                int old_h = h[u];
                cnt[old_h]--;

                // Gap heuristic
                if (cnt[old_h] == 0 && old_h > 0 && old_h < n) {
                    for (int v = 0; v < n; ++v) {
                        if (v != s && h[v] > old_h && h[v] < n) {
                            cnt[h[v]]--;
                            h[v] = n + 1;
                            cnt[h[v]]++;
                            cur[v] = 0;
                        }
                    }
                }

                int min_h = 2 * n;
                for (auto& edge : adj[u])
                    if (edge.cap > EPS)
                        min_h = std::min(min_h, h[edge.to]);

                h[u] = min_h + 1;
                cnt[h[u]]++;
                cur[u] = 0;
                if (h[u] > 2 * n) break;

            } else {
                // PUSH
                ResidualEdge<double>& edge = adj[u][cur[u]];
                int    v   = edge.to;
                double res = edge.cap;

                if (res > EPS && h[u] == h[v] + 1) {
                    double delta = std::min(excess[u], res);
                    edge.cap                -= delta;
                    adj[v][edge.rev].cap    += delta;
                    excess[u]               -= delta;
                    excess[v]               += delta;
                    if (v != s && v != t && !in_queue[v] && excess[v] > EPS) {
                        queue.push_back(v); in_queue[v] = true;
                    }
                } else {
                    cur[u]++;
                }
            }
        }
    }

    return excess[t];
}