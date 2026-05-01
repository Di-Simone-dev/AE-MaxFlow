#pragma once

// howard.hpp
// Traduzione di src/almost_linear/howard.py
// Howard's algorithm - Minimum Cycle Ratio (Karp/Howard/Gabow)

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// Struttura MinCostFlow (subset dei campi usati da Howard)
// ---------------------------------------------------------------------------

struct MinCostFlow {
    int m;                                   // numero di archi
    int n;                                   // numero di nodi
    std::vector<std::pair<int,int>> edges;   // edges[e] = (u, w)
    std::vector<std::vector<int>> adj;       // adj[v] = lista indici archi incidenti a v
};

// ---------------------------------------------------------------------------
// Howard
// ---------------------------------------------------------------------------

static constexpr double INF_H = std::numeric_limits<double>::infinity();
static constexpr double EPS_H = -0.005;  // soglia tolleranza numerica in _improve_policy

class Howard {
public:
    Howard(const MinCostFlow& graph,
           const std::vector<double>& gradients,
           const std::vector<double>& lengths)
        : g(graph)
        , gradients(gradients)
        , lengths(lengths)
        , V(graph.n)
        , distances(graph.n, 0.0)
        , policy(graph.n, -1)
        , bad_vertices(graph.n, false)
        , in_edges_list(graph.n)
        , sink(-1)
        , best_ratio(0.0)
        , critical_cycle(std::nullopt)
        , critical_vertex(std::nullopt)
    {
        // Costruisce _edge_cache: (nodo, edge_id) -> (target, gradiente orientato)
        // ⚠️ BUG nel Python originale: il segno di u è invertito. Mantenuto fedele.
        for (int edge_id = 0; edge_id < g.m; ++edge_id) {
            auto [u, w] = g.edges[edge_id];
            double grad = gradients[edge_id];
            _edge_cache[{u, edge_id}] = {w, -grad};  // ← BUG ereditato
            _edge_cache[{w, edge_id}] = {u,  grad};
        }

        bound      = _compute_bound();
        best_ratio = bound;
    }

    // Punto d'ingresso principale
    std::pair<double, std::vector<double>> find_optimum_cycle_ratio() {
        _construct_policy_graph();

        int    iteration = 0;
        double ratio     = INF_H;

        while (iteration < 100) {
            ratio = _find_all_cycles();
            if (!_improve_policy(ratio)) break;
            ++iteration;
        }

        if (ratio > bound - 1e-10 || !critical_cycle.has_value()) {
            return {INF_H, std::vector<double>(g.m, 0.0)};
        }
        return {ratio, _build_cycle_vector()};
    }

private:
    // ------------------------------------------------------------------
    // Riferimenti e stato
    // ------------------------------------------------------------------
    const MinCostFlow&        g;
    const std::vector<double> gradients;
    const std::vector<double> lengths;

    int V;
    std::vector<double>              distances;
    std::vector<int>                 policy;
    std::vector<bool>                bad_vertices;
    std::vector<std::unordered_set<int>> in_edges_list;

    // Chiave: hash di (nodo, edge_id)
    struct PairHash {
        size_t operator()(const std::pair<int,int>& p) const {
            return std::hash<int64_t>{}(static_cast<int64_t>(p.first) << 32 | p.second);
        }
    };
    std::unordered_map<std::pair<int,int>, std::pair<int,double>, PairHash> _edge_cache;

    int    sink;
    double bound;
    double best_ratio;

    std::optional<std::vector<int>> critical_cycle;
    std::optional<int>              critical_vertex;

    // ------------------------------------------------------------------
    // Helpers cache
    // ------------------------------------------------------------------
    double _get_gradient(int start, int edge_id) const {
        return _edge_cache.at({start, edge_id}).second;
    }
    int _get_edge_target(int start, int edge_id) const {
        return _edge_cache.at({start, edge_id}).first;
    }

    // ------------------------------------------------------------------
    // Bound superiore al cycle ratio
    // ------------------------------------------------------------------
    double _compute_bound() const {
        double sum_w = 0.0;
        for (double g : gradients) sum_w += std::abs(g);

        double min_len = INF_H;
        for (double l : lengths)
            if (std::abs(l) > 1e-10)
                min_len = std::min(min_len, std::abs(l));

        return (min_len == INF_H) ? INF_H : sum_w / min_len;
    }

    // ------------------------------------------------------------------
    // Costruisce la policy iniziale (greedy: arco con gradiente massimo)
    // ------------------------------------------------------------------
    void _construct_policy_graph() {
        for (int v = 0; v < V; ++v) {
            int    best_edge   = -1;
            double best_weight = -INF_H;

            for (int edge_id : g.adj[v]) {
                double grad = _get_gradient(v, edge_id);
                if (grad > best_weight) {
                    best_weight = grad;
                    best_edge   = edge_id;
                }
            }

            if (best_edge == -1) {
                // Nodo senza archi uscenti → bad vertex, collegato al sink
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

    // ------------------------------------------------------------------
    // Calcola il cycle ratio del ciclo che parte da `start`
    // ------------------------------------------------------------------
    double _compute_cycle_ratio(int start) {
        if (start == sink) return bound;

        double sum_w1 = 0.0, sum_w2 = 0.0;
        std::vector<int> cycle_edges;
        int current = start;

        do {
            int edge_id = policy[current];
            cycle_edges.push_back(edge_id);
            sum_w1 += _get_gradient(current, edge_id);
            sum_w2 += lengths[edge_id];
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

    // ------------------------------------------------------------------
    // DFS colorata O(V): trova tutti i cicli nella policy corrente
    // ------------------------------------------------------------------
    double _find_all_cycles() {
        enum Color { WHITE = 0, GRAY = 1, BLACK = 2 };
        std::vector<int> color(V, WHITE);
        double min_ratio = INF_H;

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

                // Marca i nodi del ciclo come BLACK
                auto it = std::find(stack.begin(), stack.end(), v);
                for (auto jt = it; jt != stack.end(); ++jt)
                    color[*jt] = BLACK;
            }

            // Marca la coda come BLACK
            for (int node : stack)
                if (color[node] != BLACK)
                    color[node] = BLACK;
        };

        for (int v = 0; v < V; ++v)
            if (color[v] == WHITE)
                dfs(v);

        return min_ratio;
    }

    // ------------------------------------------------------------------
    // Tenta di migliorare la policy corrente
    // ------------------------------------------------------------------
    bool _improve_policy(double current_ratio) {
        bool improved = false;

        for (int v = 0; v < V; ++v) {
            if (!bad_vertices[v]) {
                for (int edge_id : g.adj[v]) {
                    int    target   = _get_edge_target(v, edge_id);
                    double new_dist = _get_gradient(v, edge_id)
                                    - current_ratio * lengths[edge_id]
                                    + distances[target];

                    if (distances[v] + EPS_H > new_dist) {
                        int old_target = _get_edge_target(v, policy[v]);
                        in_edges_list[old_target].erase(v);
                        policy[v] = edge_id;
                        in_edges_list[target].insert(v);
                        distances[v] = new_dist;
                        improved = true;
                    }
                }
            } else {
                // bad vertex: distanza dipende dal sink
                double new_dist = bound - current_ratio + distances[sink];
                if (distances[v] + EPS_H > new_dist) {
                    distances[v] = new_dist;
                }
            }
        }

        return improved;
    }

    // ------------------------------------------------------------------
    // Costruisce il cycle vector (+1/-1/0) dal ciclo critico
    // ------------------------------------------------------------------
    std::vector<double> _build_cycle_vector() const {
        std::vector<double> edge_cycle(g.m, 0.0);
        if (!critical_cycle.has_value() || !critical_vertex.has_value())
            return edge_cycle;

        int current = *critical_vertex;
        for (int edge_id : *critical_cycle) {
            int target = _get_edge_target(current, edge_id);
            edge_cycle[edge_id] = (current == g.edges[edge_id].first) ? +1.0 : -1.0;
            current = target;
        }

        return edge_cycle;
    }
};

// ---------------------------------------------------------------------------
// Funzione di interfaccia pubblica
// ---------------------------------------------------------------------------

inline std::pair<double, std::vector<double>> minimum_cycle_ratio(
    const MinCostFlow&        g,
    const std::vector<double>& gradients,
    const std::vector<double>& lengths)
{
    Howard howard(g, gradients, lengths);
    return howard.find_optimum_cycle_ratio();
}