#pragma once
#include <Eigen/Dense>
#include <vector>
#include <map>
#include <tuple>
#include <cassert>
#include <cmath>
#include <algorithm>

class MinCostFlow {
public:
    int m;       // numero di archi
    int n;       // numero di nodi
    std::vector<std::pair<int,int>> edges;
    Eigen::VectorXd c;
    Eigen::VectorXd c_org;
    Eigen::VectorXd u_lower;
    Eigen::VectorXd u_upper;
    int optimal_cost;
    int U;
    double alpha;
    Eigen::MatrixXd B;  // matrice di incidenza (m x n)
    std::map<std::pair<int,int>, std::vector<int>> undirected_edge_to_indices;
    std::vector<std::vector<int>> adj;

    // Costruttore principale
    MinCostFlow(
        const std::vector<std::pair<int,int>>& edges,
        const Eigen::VectorXd& c,
        const Eigen::VectorXd& u_lower,
        const Eigen::VectorXd& u_upper,
        int optimal_cost
    );

    // Costruttore privato usato da clone()
    MinCostFlow() = default;

    MinCostFlow clone() const;

    static MinCostFlow from_max_flow_instance(
        const std::vector<std::pair<int,int>>& edges,
        int s,
        int t,
        int optimal_flow,
        const std::vector<int>& capacities,
        const std::vector<int>* lower_capacities = nullptr
    );

    double phi(const Eigen::VectorXd& f) const;
    Eigen::VectorXd calc_gradients(const Eigen::VectorXd& f) const;
    Eigen::VectorXd calc_lengths(const Eigen::VectorXd& f) const;

    std::vector<int> edges_between(int a, int b) const;
    int add_vertex();
    void add_edge(int a, int b, double c, double u_lower, double u_upper);
};