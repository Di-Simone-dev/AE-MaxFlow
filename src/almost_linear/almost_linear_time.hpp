#pragma once
#include "min_cost_flow_instance.hpp"
#include "feasible_flow.hpp"
#include "howard.hpp"
#include <Eigen/Dense>
#include <vector>
#include <map>
#include <utility>
#include <optional>

class AlmostLinearTime {
public:
    explicit AlmostLinearTime(const std::map<std::pair<int,int>, long long>& graph);

    // Ricerca binaria sul valore ottimale — restituisce il valore del max-flow
    long long max_flow(int source, int sink);

    // Risolve il max-flow con una stima fissata del valore ottimale
    std::pair<long long, Eigen::VectorXd> max_flow_with_guess(
        int source,
        int sink,
        long long optimal_flow,
        const std::vector<long long>* lower_capacities = nullptr
    );

private:
    std::vector<std::pair<int,int>> _edges;
    std::vector<long long>  _capacities;
};