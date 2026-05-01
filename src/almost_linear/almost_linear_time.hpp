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
    explicit AlmostLinearTime(const std::map<std::pair<int,int>, int>& graph);

    // Ricerca binaria sul valore ottimale — restituisce il valore del max-flow
    int max_flow(int source, int sink);

    // Risolve il max-flow con una stima fissata del valore ottimale
    std::pair<int, Eigen::VectorXd> max_flow_with_guess(
        int source,
        int sink,
        int optimal_flow,
        const std::vector<int>* lower_capacities = nullptr
    );

private:
    std::vector<std::pair<int,int>> _edges;
    std::vector<int>                _capacities;
};