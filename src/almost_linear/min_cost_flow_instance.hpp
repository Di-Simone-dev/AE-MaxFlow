#pragma once
#include <Eigen/Dense>
#include <vector>
#include <map>
#include <tuple>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <unordered_map>//per ottimizzare
class MinCostFlow {
public:
    int m;       // numero di archi
    int n;       // numero di nodi
    std::vector<std::pair<int,int>> edges;
    Eigen::VectorXd c;
    Eigen::VectorXd c_org;
    Eigen::VectorXd u_lower;
    Eigen::VectorXd u_upper;
    long long optimal_cost;
    long long U;
    double alpha;
    Eigen::MatrixXd B;  // matrice di incidenza (m x n)
    //std::map<std::pair<int,int>, std::vector<int>> undirected_edge_to_indices;
    struct PairHash {
        std::size_t operator()(const std::pair<int,int>& p) const noexcept {
            return std::hash<std::int64_t>()((std::int64_t)p.first << 32 | (unsigned)p.second);
        }
    };
    std::unordered_map<std::pair<int,int>, std::vector<int>, PairHash> undirected_edge_to_indices;
    std::vector<std::vector<int>> adj;

    // Costruttore principale
    MinCostFlow(
        const std::vector<std::pair<int,int>>& edges,
        const Eigen::VectorXd& c,
        const Eigen::VectorXd& u_lower,
        const Eigen::VectorXd& u_upper,
        long long optimal_cost
    );

    // Costruttore privato usato da clone()
    MinCostFlow() = default;

    MinCostFlow clone() const;

    static MinCostFlow from_max_flow_instance(
        const std::vector<std::pair<int,int>>& edges,
        int s,
        int t,
        long long optimal_flow,
        const std::vector<long long>& capacities,
        const std::vector<long long>* lower_capacities = nullptr
    );

    double phi(const Eigen::VectorXd& f) const;
    Eigen::VectorXd calc_gradients(const Eigen::VectorXd& f) const;
    Eigen::VectorXd calc_lengths(const Eigen::VectorXd& f) const;

    std::vector<int> edges_between(int a, int b) const;
    int add_vertex();
    void add_edge(int a, int b, double c, double u_lower, double u_upper);
};