#pragma once
#include "../min_cost_flow_instance.hpp"
#include <Eigen/Dense>
#include <utility>

namespace almost_linear
{
    std::pair<MinCostFlow, Eigen::VectorXd> calc_feasible_flow(const MinCostFlow& I_or);
}
