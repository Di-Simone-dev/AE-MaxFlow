#include "howard.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
static constexpr double INF = std::numeric_limits<double>::infinity();

// ---------------------------------------------------------------------------
// Costruttore
// ---------------------------------------------------------------------------
Howard::Howard(const MinCostFlow& graph,
               const Eigen::VectorXd& gradients,
               const Eigen::VectorXd& lengths)
    : g(graph),
      V(graph.n),
      distances(Eigen::VectorXd::Zero(graph.n)),
      policy(graph.n, -1),
      bad_vertices(graph.n, false),
      in_edges_list(graph.n),
      sink(-1),
      best_ratio(0.0),
      gradients_vec(gradients),
      lengths_vec(lengths)
{
    // Costruzione della _edge_cache.
    // ⚠️  BUG originale Python replicato: segno invertito per il nodo sorgente u.
    for (int edge_id = 0; edge_id < static_cast<int>(g.edges.size()); ++edge_id) {
        auto [u, w] = g.edges[edge_id];
        double grad = gradients_vec[edge_id];
        _edge_cache[{u, edge_id}] = {w, -grad};  // ← BUG fedele all'originale
        //_edge_cache[{u, edge_id}] = {w, grad};  // ← modificato
        _edge_cache[{w, edge_id}] = {u,  grad};
    }

    bound      = _compute_bound();
    best_ratio = bound;
}

// ---------------------------------------------------------------------------
// _compute_bound
// ---------------------------------------------------------------------------
double Howard::_compute_bound() const {
    double sum_weights = gradients_vec.cwiseAbs().sum();
    double min_len = INF;
    for (int i = 0; i < lengths_vec.size(); ++i) {
        double al = std::abs(lengths_vec[i]);
        if (al > 1e-10 && al < min_len) min_len = al;
    }
    if (min_len == INF) return INF;
    return sum_weights / min_len;
}

// ---------------------------------------------------------------------------
// Accesso alla cache
// ---------------------------------------------------------------------------
double Howard::_get_gradient(int start, int edge_id) const {
    return _edge_cache.at({start, edge_id}).second;
}

int Howard::_get_edge_target(int start, int edge_id) const {
    return _edge_cache.at({start, edge_id}).first;
}

// ---------------------------------------------------------------------------
// _construct_policy_graph
// ---------------------------------------------------------------------------
void Howard::_construct_policy_graph() {
    for (int v = 0; v < V; ++v) {
        int    best_edge   = -1;
        double best_weight = -INF;

        for (int edge_id : g.adj[v]) {
            double grad = _get_gradient(v, edge_id);
            if (grad > best_weight) {
                best_weight = grad;
                best_edge   = edge_id;
            }
        }

        if (best_edge == -1) {
            if (sink == -1) sink = v;
            bad_vertices[v] = true;
            in_edges_list[sink].insert(v);
        } else {
            int target = _get_edge_target(v, best_edge);
            in_edges_list[target].insert(v);
            policy[v] = best_edge;
        }
    }
}

// ---------------------------------------------------------------------------
// _find_cycle_vertex
// ---------------------------------------------------------------------------
int Howard::_find_cycle_vertex(int start) const {
    int current = start;
    std::unordered_set<int> visited;

    while (visited.find(current) == visited.end()) {
        visited.insert(current);
        if (!bad_vertices[current])
            current = _get_edge_target(current, policy[current]);
        else
            current = sink;
    }
    return current;
}

// ---------------------------------------------------------------------------
// _compute_cycle_ratio
// ---------------------------------------------------------------------------
double Howard::_compute_cycle_ratio(int start) {
    if (start == sink) return bound;

    double sum_w1 = 0.0;
    double sum_w2 = 0.0;
    int    current = start;
    std::vector<int> cycle_edges;

    do {
        int edge_id = policy[current];
        cycle_edges.push_back(edge_id);
        sum_w1 += _get_gradient(current, edge_id);
        sum_w2 += lengths_vec[edge_id];
        current = _get_edge_target(current, edge_id);
    } while (current != start);

    double ratio = sum_w1 / sum_w2;

    if (best_ratio > ratio) {
        best_ratio      = ratio;
        critical_vertex = start;
        critical_cycle  = cycle_edges;
    }

    return ratio;
}

// ---------------------------------------------------------------------------
// _improve_policy
// ---------------------------------------------------------------------------
bool Howard::_improve_policy(double current_ratio) {
    bool improved = false;

    for (int v = 0; v < V; ++v) {
        if (!bad_vertices[v]) {
            for (int edge_id : g.adj[v]) {
                int    target   = _get_edge_target(v, edge_id);
                double new_dist = _get_gradient(v, edge_id)
                                  - current_ratio * lengths_vec[edge_id]
                                  + distances[target];

                if (distances[v] + EPS > new_dist) {
                    int old_target = _get_edge_target(v, policy[v]);
                    in_edges_list[old_target].erase(v);
                    policy[v] = edge_id;
                    in_edges_list[target].insert(v);
                    distances[v] = new_dist;
                    improved = true;
                }
            }
        } else {
            double new_dist = bound - current_ratio + distances[sink];
            if (distances[v] + EPS > new_dist) {
                distances[v] = new_dist;
            }
        }
    }

    return improved;
}

// ---------------------------------------------------------------------------
// _find_all_cycles
// ---------------------------------------------------------------------------
double Howard::_find_all_cycles() {
    constexpr int WHITE = 0, GRAY = 1, BLACK = 2;

    std::vector<int> color(V, WHITE);
    double min_ratio = INF;

    auto dfs = [&](int start) {
        std::vector<int> stack;
        int v = start;

        while (color[v] == WHITE) {
            color[v] = GRAY;
            stack.push_back(v);
            v = bad_vertices[v] ? sink : _get_edge_target(v, policy[v]);
        }

        if (color[v] == GRAY) {
            double ratio = _compute_cycle_ratio(v);
            if (ratio < min_ratio) min_ratio = ratio;

            auto it = std::find(stack.begin(), stack.end(), v);
            for (auto jt = it; jt != stack.end(); ++jt)
                color[*jt] = BLACK;
        }

        for (int node : stack)
            if (color[node] != BLACK)
                color[node] = BLACK;
    };

    for (int v = 0; v < V; ++v)
        if (color[v] == WHITE)
            dfs(v);

    return min_ratio;
}

// ---------------------------------------------------------------------------
// _make_cycle_vector
// ---------------------------------------------------------------------------
Eigen::VectorXd Howard::_make_cycle_vector() const {
    Eigen::VectorXd edge_cycle = Eigen::VectorXd::Zero(g.m);

    if (!critical_cycle.has_value() || !critical_vertex.has_value())
        return edge_cycle;

    int current = *critical_vertex;
    for (int edge_id : *critical_cycle) {
        int target = _get_edge_target(current, edge_id);
        edge_cycle[edge_id] = (current == g.edges[edge_id].first) ? 1.0 : -1.0;
        current = target;
    }
    return edge_cycle;
}

// ---------------------------------------------------------------------------
// find_optimum_cycle_ratio
// ---------------------------------------------------------------------------
std::pair<double, Eigen::VectorXd> Howard::find_optimum_cycle_ratio() {
    _construct_policy_graph();

    int    iteration = 0;
    double ratio     = INF;

    while (iteration < 100) {
        ratio = _find_all_cycles();
        if (!_improve_policy(ratio)) break;
        ++iteration;
    }
    if (ratio > bound - 1e-10 || !critical_cycle.has_value()) {
        return {INF, Eigen::VectorXd::Zero(g.m)};
    } else {
        return {ratio, _make_cycle_vector()};
    }
}

// ---------------------------------------------------------------------------
// Funzione di interfaccia pubblica
// ---------------------------------------------------------------------------
std::pair<double, Eigen::VectorXd>
minimum_cycle_ratio(const MinCostFlow& g,
                    const Eigen::VectorXd& gradients,
                    const Eigen::VectorXd& lengths)
{
    Howard howard(g, gradients, lengths);
    return howard.find_optimum_cycle_ratio();
}