#include "almost_linear_time.hpp"
#include <iostream>
#include <cmath>
#include <cassert>
#include <limits>
#include <numeric>

// ---------------------------------------------------------------------------
// Costruttore
// ---------------------------------------------------------------------------
AlmostLinearTime::AlmostLinearTime(const std::map<std::pair<int,int>, int>& graph) {
    for (auto& [edge, cap] : graph) {
        _edges.push_back(edge);
        _capacities.push_back(cap);
    }
}

// ---------------------------------------------------------------------------
// max_flow  —  ricerca binaria sul valore ottimale
// ---------------------------------------------------------------------------
int AlmostLinearTime::max_flow(int source, int sink) {
    // Limite superiore: somma delle capacità uscenti da source
    int max_possible_flow = 0;
    for (int e = 0; e < static_cast<int>(_edges.size()); ++e)
        if (_edges[e].first == source)
            max_possible_flow += _capacities[e];

    int iters = 0;
    int low = 0, high = max_possible_flow + 1;
    int mf = 0;

    while (low < high) {
        ++iters;
        std::cout << "iterazioni ricerca binaria max_flow = " << iters << "\n";
        int mid = (low + high) / 2;

        auto [flow_val, flows] = max_flow_with_guess(source, sink, mid);
        mf = flow_val;

        if (mf < mid)
            high = mid;       // mid non raggiungibile → abbassa il limite
        else
            low = mid + 1;    // mid raggiungibile → alza il limite
    }

    return mf;
}

// ---------------------------------------------------------------------------
// max_flow_with_guess  —  risolve con stima fissata del flusso ottimale
// ---------------------------------------------------------------------------
std::pair<int, Eigen::VectorXd> AlmostLinearTime::max_flow_with_guess(
    int source,
    int sink,
    int optimal_flow,
    const std::vector<int>* lower_capacities
) {
    // ── Costruzione istanza min-cost flow ────────────────────────────────
    MinCostFlow I = MinCostFlow::from_max_flow_instance(
        _edges,
        source,
        sink,
        optimal_flow,
        _capacities,
        lower_capacities
    );

    int original_m = I.m;
    int flow_idx   = original_m - 1;  // indice arco virtuale s→t

    // ── Flusso ammissibile iniziale ──────────────────────────────────────
    auto [I2, cur_flow] = calc_feasible_flow(I);
    I = std::move(I2);

    // ── Parametri dell'algoritmo ─────────────────────────────────────────
    const double threshold = 1e-5;
    const double kappa     = 0.9999;
    const double upscale   = 1000.0;

    int i = 0;
    double cur_phi = I.phi(cur_flow);

    // ── Loop di ottimizzazione ────────────────────────────────────────────
    while (I.c.dot(cur_flow) - static_cast<double>(I.optimal_cost) >= threshold) {
        ++i;

        // Sanity check: conservazione del flusso
        double flow_conservation = (I.B.transpose() * cur_flow).cwiseAbs().maxCoeff();
        assert(flow_conservation < 1e-10 && "Flow conservation has been broken");

        // Passo 1 – gradienti e lunghezze rispetto a Φ(f)
        Eigen::VectorXd gradients = I.calc_gradients(cur_flow);
        Eigen::VectorXd lengths   = I.calc_lengths(cur_flow);

        // Passo 2 – minimum cycle ratio (algoritmo di Howard)
        auto [min_ratio, min_ratio_cycle] = minimum_cycle_ratio(I, gradients, lengths);

        assert(min_ratio < 0.0 && "Minimum cycle ratio is not negative");

        // Passo 3 – aggiornamento del flusso lungo il ciclo
        double eta = -(kappa * kappa) / (50.0 * gradients.dot(min_ratio_cycle));
        Eigen::VectorXd augment_cycle = min_ratio_cycle * (eta * upscale);
        cur_flow += augment_cycle;

        // Passo 4 – verifica che Φ(f) sia strettamente decrescente
        double new_phi = I.phi(cur_flow);
        if (!std::isfinite(new_phi))
            break;  // Φ esplode → siamo sul bordo del dominio

        assert(new_phi < cur_phi && "Phi(f) has not decreased");
        cur_phi = new_phi;
    }

    std::cout << "rounded flow: " << std::round(cur_flow[original_m - 1]) << "\n";

    // Arrotonda e restituisce
    int flow_value = static_cast<int>(std::round(cur_flow[flow_idx]));
    Eigen::VectorXd edge_flows = cur_flow.head(flow_idx).array().round();

    return {flow_value, edge_flows};
}