/**
 * @file feasible_flow.cpp
 * @brief Implementazione di `almost_linear::calc_feasible_flow`.
 *
 * ### Strategia
 *
 * L'algoritmo interior-point richiede un punto di partenza strettamente
 * interno al dominio ammissibile di Φ(f), ovvero un flusso `f` tale che:
 * @code
 *   u_lower < f < u_upper       (capacità)
 *   B^T f = 0                   (conservazione del flusso)
 * @endcode
 *
 * Il punto centrale `f₀ = (u_lower + u_upper) / 2` soddisfa le disuguaglianze
 * di capacità per costruzione, ma in generale **non** bilancia il flusso sui
 * nodi (`B^T f₀ ≠ 0`).
 *
 * Per correggere gli squilibri si aggiunge un nodo fittizio `v*` collegato
 * a ogni nodo sbilanciato tramite archi ausiliari ad alto costo, in modo
 * che la conservazione del flusso sia soddisfatta sull'istanza estesa.
 * Il costo elevato `c = 4·m·U²` garantisce che questi archi ausiliari
 * non facciano parte della soluzione ottima dell'istanza originale.
 *
 * @see feasible_flow.hpp
 * @see AlmostLinearTime::max_flow_with_guess
 */

#include "feasible_flow.hpp"
#include "min_cost_flow_instance.hpp"
#include <Eigen/Dense>
#include <cassert>
#include <numeric>

namespace almost_linear {

/**
 * @brief Calcola un flusso ammissibile iniziale per il loop interior-point.
 *
 * #### Passi dell'algoritmo
 *
 * 1. **Clone**: copia l'istanza originale per non modificarla.
 * 2. **Nodo fittizio**: aggiunge `v*` all'istanza clonata.
 * 3. **Flusso centrale**: inizializza `init_flow = (u_lower + u_upper) / 2`,
 *    strettamente interno al dominio di capacità.
 * 4. **Squilibri nodali**: calcola `d̂ = B^T init_flow` — la divergenza del
 *    flusso centrale su ciascun nodo.
 * 5. **Penalità di bilanciamento**: `c = 4·m·U²` è sufficientemente grande
 *    da rendere gli archi ausiliari non attraenti per l'ottimizzatore.
 * 6. **Archi ausiliari**: per ogni nodo `v` con `d̂[v] ≠ 0`, aggiunge un
 *    arco tra `v*` e `v` (o viceversa) con capacità `[0, 2·|d̂[v]|]` e
 *    flusso iniziale `|d̂[v]|`, annullando lo squilibrio.
 * 7. **Verifica**: controlla che `init_flow.size() == I.m` dopo le aggiunte.
 *
 * @param I_or  Istanza di min-cost flow da cui partire. Non viene modificata.
 * @return Coppia `(I_modified, init_flow)` pronta per il loop MCR.
 *
 * @pre  `I_or.u_lower[e] <= I_or.u_upper[e]` per ogni arco `e`.
 * @post `init_flow.size() == I_modified.m`
 * @post `init_flow[e]` è strettamente compreso tra `u_lower[e]` e `u_upper[e]`
 *       per tutti gli archi originali (garantito dal punto centrale).
 */
std::pair<MinCostFlow, Eigen::VectorXd>
calc_feasible_flow(const MinCostFlow& I_or)
{
    MinCostFlow I = I_or.clone();

    // Aggiunge il nodo fittizio v* per il bilanciamento degli squilibri
    int v_star = I.add_vertex();

    // Domanda di flusso su ogni nodo: zero per tutti (flusso bilanciato)
    Eigen::VectorXd demands = Eigen::VectorXd::Zero(I.n);
    assert(demands.sum() == 0.0);

    // Flusso iniziale: punto centrale dell'intervallo di capacità.
    // Per costruzione: u_lower < init_flow < u_upper (strettamente ammissibile).
    Eigen::VectorXd init_flow = (I.u_lower + I.u_upper) / 2.0;

    // Divergenza del flusso centrale su ciascun nodo: d̂ = B^T f₀.
    // d̂[v] > 0 → nodo v ha eccesso; d̂[v] < 0 → nodo v ha deficit.
    Eigen::VectorXd d_hat = I.B.transpose() * init_flow;

    // Costo degli archi ausiliari: abbastanza grande da non essere attraenti
    // per l'ottimizzatore, ma finito per mantenere il problema ben condizionato.
    double c = 4.0 * I.m * static_cast<double>(I.U) * static_cast<double>(I.U);

    for (int v = 0; v < I.n; ++v) {
        double dh = d_hat[v];
        double d  = demands[v];

        if (dh > d) {
            // Eccesso su v: aggiunge arco v* → v per assorbire l'eccesso.
            // Flusso iniziale sull'arco: dh - d (bilancia esattamente v).
            I.add_edge(v_star, v, c, 0.0, 2.0 * (dh - d));
            int old_size = init_flow.size();
            init_flow.conservativeResize(old_size + 1);
            init_flow[old_size] = dh - d;
        } else if (dh < d) {
            // Deficit su v: aggiunge arco v → v* per compensare il deficit.
            // Flusso iniziale sull'arco: d - dh (bilancia esattamente v).
            I.add_edge(v, v_star, c, 0.0, 2.0 * (d - dh));
            int old_size = init_flow.size();
            init_flow.conservativeResize(old_size + 1);
            init_flow[old_size] = d - dh;
        }
        // Se dh == d: nodo già bilanciato, nessun arco ausiliario necessario.
    }

    // Verifica che init_flow sia allineato con il numero di archi dell'istanza estesa
    assert(init_flow.size() == I.m);

    return {std::move(I), std::move(init_flow)};
}

} // namespace almost_linear