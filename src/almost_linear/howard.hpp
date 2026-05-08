#pragma once

#include "min_cost_flow_instance.hpp"
#include <unordered_map>
#include <unordered_set>
//#include <set>
//#include <map>
#include <optional>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// EPS: soglia negativa per accettare miglioramenti in _improve_policy.
// ---------------------------------------------------------------------------
static constexpr double EPS = -0.005;

// ---------------------------------------------------------------------------
// Howard — minimum cycle ratio su grafo pesato.
// ---------------------------------------------------------------------------
class Howard {
public:
    Howard(const MinCostFlow& graph,
           const Eigen::VectorXd& gradients,
           const Eigen::VectorXd& lengths);

    // Restituisce (ratio, cycle_vector).
    // cycle_vector[e] = +1/-1/0 a seconda della direzione dell'arco nel ciclo critico.
    std::pair<double, Eigen::VectorXd> find_optimum_cycle_ratio();

private:
    const MinCostFlow& g;
    int V;

    Eigen::VectorXd            distances;
    std::vector<int>           policy;
    std::vector<bool>          bad_vertices;
    std::vector<std::unordered_set<int>> in_edges_list;

    // Cache: (nodo, edge_id) → (nodo_target, gradiente orientato)
    //std::map<std::pair<int,int>, std::pair<int,double>> _edge_cache;
    struct PairHash { //ottimizzazione
        std::size_t operator()(const std::pair<int,int>& p) const noexcept {
            return std::hash<std::int64_t>()((std::int64_t)p.first << 32 | (unsigned)p.second);
        }
    };
    std::unordered_map<std::pair<int,int>, std::pair<int,double>, PairHash> _edge_cache;

    int    sink;
    double bound;
    double best_ratio;

    std::optional<std::vector<int>> critical_cycle;
    std::optional<int>              critical_vertex;

    Eigen::VectorXd gradients_vec;
    Eigen::VectorXd lengths_vec;

    double _compute_bound() const;
    double _get_gradient(int start, int edge_id) const;
    int    _get_edge_target(int start, int edge_id) const;
    void   _construct_policy_graph();
    int    _find_cycle_vertex(int start) const;
    double _compute_cycle_ratio(int start);
    bool   _improve_policy(double current_ratio);
    double _find_all_cycles();
    Eigen::VectorXd _make_cycle_vector() const;
};

// ---------------------------------------------------------------------------
// Funzione di interfaccia pubblica
// ---------------------------------------------------------------------------
std::pair<double, Eigen::VectorXd>
minimum_cycle_ratio(const MinCostFlow& g,
                    const Eigen::VectorXd& gradients,
                    const Eigen::VectorXd& lengths);