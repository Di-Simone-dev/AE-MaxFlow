#include "feasible_flow.hpp"
#include "min_cost_flow_instance.hpp"
#include <Eigen/Dense>
#include <cassert>
#include <numeric>

namespace almost_linear {

std::pair<MinCostFlow, Eigen::VectorXd>
calc_feasible_flow(const MinCostFlow& I_or)
{
    MinCostFlow I = I_or.clone();

    int v_star = I.add_vertex();

    Eigen::VectorXd demands = Eigen::VectorXd::Zero(I.n);
    assert(demands.sum() == 0.0);

    Eigen::VectorXd init_flow = (I.u_lower + I.u_upper) / 2.0;

    Eigen::VectorXd d_hat = I.B.transpose() * init_flow;

    double c = 4.0 * I.m * static_cast<double>(I.U) * static_cast<double>(I.U);

    for (int v = 0; v < I.n; ++v) {
        double dh = d_hat[v];
        double d  = demands[v];

        if (dh > d) {
            I.add_edge(v_star, v, c, 0.0, 2.0 * (dh - d));
            int old_size = init_flow.size();
            init_flow.conservativeResize(old_size + 1);
            init_flow[old_size] = dh - d;
        } else if (dh < d) {
            I.add_edge(v, v_star, c, 0.0, 2.0 * (d - dh));
            int old_size = init_flow.size();
            init_flow.conservativeResize(old_size + 1);
            init_flow[old_size] = d - dh;
        }
    }

    assert(init_flow.size() == I.m);

    return {std::move(I), std::move(init_flow)};
}

} // namespace almost_linear
