/**
 * @file feasible_flow.hpp
 * @brief Dichiarazione di `calc_feasible_flow`, funzione che produce un flusso
 *        ammissibile iniziale per il loop di ottimizzazione interior-point.
 *
 * @see feasible_flow.cpp
 * @see min_cost_flow_instance.hpp
 */

#pragma once
#include "min_cost_flow_instance.hpp"
#include <Eigen/Dense>
#include <utility>

/**
 * @namespace almost_linear
 * @brief Namespace del solver di max-flow con complessità quasi-lineare.
 *
 * Raccoglie le componenti interne dell'algoritmo: calcolo del flusso
 * ammissibile iniziale, minimum cycle ratio (Howard) e l'istanza
 * di min-cost flow.
 */
namespace almost_linear
{
    /**
     * @brief Calcola un flusso ammissibile iniziale strettamente interno
     *        al dominio ammissibile di Φ(f).
     *
     * Partendo dal flusso centrale `f₀ = (u_lower + u_upper) / 2`, calcola
     * gli squilibri nodali `d̂ = B^T f₀` e li bilancia aggiungendo archi
     * ausiliari da/verso un nodo fittizio `v*`:
     * - Se `d̂[v] > 0` (eccesso): aggiunge arco `v* → v` con flusso `d̂[v]`.
     * - Se `d̂[v] < 0` (deficit): aggiunge arco `v → v*` con flusso `-d̂[v]`.
     *
     * Gli archi ausiliari hanno costo `c = 4·m·U²` (penalità elevata) e
     * capacità superiore `2·|d̂[v]|`, in modo che il flusso risultante
     * soddisfi la conservazione del flusso su tutti i nodi originali.
     *
     * @param I_or  Istanza di min-cost flow originale (non modificata).
     *              Viene clonata internamente prima di qualsiasi modifica.
     * @return Coppia `(I_modified, init_flow)`:
     *         - `I_modified`: istanza estesa con il nodo `v*` e gli archi
     *           ausiliari aggiunti.
     *         - `init_flow`: vettore di flusso di dimensione `I_modified.m`,
     *           strettamente ammissibile rispetto ai vincoli di capacità e
     *           conservazione del flusso.
     *
     * @pre  `I_or` deve essere una istanza valida con `u_lower ≤ u_upper`.
     * @post `init_flow.size() == I_modified.m`
     * @post `(I_modified.B.T * init_flow).isZero(1e-10)`
     */
    std::pair<MinCostFlow, Eigen::VectorXd> calc_feasible_flow(const MinCostFlow& I_or);

} // namespace almost_linear