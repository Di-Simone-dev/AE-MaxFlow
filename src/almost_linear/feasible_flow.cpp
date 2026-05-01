#include "feasible_flow.h"
#include "min_cost_flow_instance.h"
#include <Eigen/Dense>
#include <cassert>
#include <numeric>

/**
 * calc_feasible_flow
 *
 * Dato un'istanza MinCostFlow, costruisce un'istanza estesa con un nodo
 * ausiliario v* e gli archi necessari per rendere il flusso iniziale ammissibile.
 *
 * Restituisce:
 *   - I      : la nuova istanza clonata e modificata
 *   - init_flow : il vettore di flusso iniziale (Eigen::VectorXd)
 */
std::pair<MinCostFlow, Eigen::VectorXd> calc_feasible_flow(const MinCostFlow& I_or) {
    MinCostFlow I = I_or.clone();

    // Aggiunge il nodo ausiliario v*
    int v_star = I.add_vertex();

    // TODO: Sostituire con i demand reali quando implementati
    Eigen::VectorXd demands = Eigen::VectorXd::Zero(I.n);
    assert(demands.sum() == 0.0);

    // Flusso iniziale: media tra lower e upper bound
    Eigen::VectorXd init_flow = (I.u_lower + I.u_upper) / 2.0;

    // d_hat = B^T * init_flow  (eccesso/difetto di flusso per ogni nodo)
    Eigen::VectorXd d_hat = I.B.transpose() * init_flow;

    double c = 4.0 * I.m * static_cast<double>(I.U) * static_cast<double>(I.U);

    for (int v = 0; v < I.n; ++v) {
        double dh = d_hat[v];
        double d  = demands[v];

        if (dh > d) {
            // Nodo v ha eccesso: aggiungi arco v* -> v
            I.add_edge(v_star, v, c, 0.0, 2.0 * (dh - d));
            // Appendi il flusso iniziale del nuovo arco
            int old_size = init_flow.size();
            init_flow.conservativeResize(old_size + 1);
            init_flow[old_size] = dh - d;
        } else if (dh < d) {
            // Nodo v ha difetto: aggiungi arco v -> v*
            I.add_edge(v, v_star, c, 0.0, 2.0 * (d - dh));
            int old_size = init_flow.size();
            init_flow.conservativeResize(old_size + 1);
            init_flow[old_size] = d - dh;
        }
    }

    assert(init_flow.size() == I.m);

    return {std::move(I), std::move(init_flow)};
}