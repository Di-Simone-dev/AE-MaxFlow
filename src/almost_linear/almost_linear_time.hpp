#pragma once
#include "min_cost_flow_instance.hpp"
#include "feasible_flow.hpp"
#include "howard.hpp"
#include <Eigen/Dense>
#include <vector>
#include <map>
#include <utility>
#include <optional>

/**
 * @file almost_linear_time.hpp
 * @brief Dichiarazione del solver per il problema di massimo flusso con
 *        complessità quasi-lineare basato su interior-point method.
 *
 * L'algoritmo riduce il max-flow a una sequenza di istanze di min-cost flow,
 * ciascuna risolta tramite l'algoritmo di Howard per il minimum cycle ratio
 * (MCR) su un grafo pesato. La ricerca binaria sul valore ottimale del flusso
 * garantisce la correttezza del risultato finale.
 *
 * @see almost_linear_time.cpp
 * @see howard.hpp
 * @see min_cost_flow_instance.hpp
 */

/**
 * @class AlmostLinearTime
 * @brief Solver per il problema di massimo flusso con complessità quasi-lineare.
 *
 * Implementa un algoritmo di tipo interior-point che:
 * 1. Esegue una ricerca binaria sul valore del flusso ottimale.
 * 2. Per ogni candidato, costruisce un'istanza di min-cost flow tramite
 *    @ref MinCostFlow::from_max_flow_instance.
 * 3. Calcola un flusso ammissibile iniziale con @ref almost_linear::calc_feasible_flow.
 * 4. Itera aggiornamenti del flusso lungo il ciclo di minimum cycle ratio
 *    (trovato con l'algoritmo di Howard) fino a convergenza del potenziale Φ(f).
 *
 * @note Il grafo è rappresentato come mappa di archi orientati con capacità
 *       intere a 64 bit per evitare overflow su istanze con capacità elevate
 *       (es. capacità frazionarie scalate prima della chiamata).
 *
 * @warning L'algoritmo assume che il grafo sia connesso e che source ≠ sink.
 *          Grafi con source isolato producono flusso 0 senza eccezioni.
 */
class AlmostLinearTime {
public:

    /**
     * @brief Costruisce il solver a partire da un grafo con capacità intere.
     *
     * Popola i vettori interni @ref _edges e @ref _capacities iterando sulla
     * mappa in ingresso. L'ordine di iterazione è determinato dal comparatore
     * di default di @c std::map (lessicografico sulla coppia di nodi).
     *
     * @param graph Mappa da arco (u, v) a capacità. Le chiavi sono coppie
     *              di indici di nodo (0-based). I valori devono essere ≥ 0.
     */
    explicit AlmostLinearTime(const std::map<std::pair<int,int>, long long>& graph);

    /**
     * @brief Calcola il valore del massimo flusso tra source e sink.
     *
     * Esegue una ricerca binaria sull'intervallo [0, Σ cap(source, v)],
     * invocando @ref max_flow_with_guess per ogni candidato @c mid.
     * La ricerca termina quando @c low == @c high, restituendo il massimo
     * valore di flusso raggiungibile.
     *
     * Complessità: O(log(U) · T_guess), dove U è la somma delle capacità
     * uscenti da source e T_guess è il costo di una singola chiamata a
     * @ref max_flow_with_guess.
     *
     * @param source Indice del nodo sorgente (0-based).
     * @param sink   Indice del nodo pozzo (0-based).
     * @return Valore intero del massimo flusso da source a sink.
     */
    long long max_flow(int source, int sink);

    /**
     * @brief Risolve il max-flow con una stima fissata del valore ottimale.
     *
     * Costruisce l'istanza di min-cost flow corrispondente al flusso target
     * @p optimal_flow, calcola un flusso ammissibile iniziale e itera
     * aggiornamenti lungo il ciclo di minimum cycle ratio fino a convergenza.
     *
     * Il loop di ottimizzazione termina quando:
     *   - @c c·f - optimal_cost < threshold  (convergenza), oppure
     *   - Φ(f) non è finito dopo un aggiornamento (bordo del dominio).
     *
     * Ad ogni iterazione vengono verificate (via @c assert) le invarianti:
     *   - Conservazione del flusso: @c ||B^T f||_∞ < 1e-10
     *   - Ciclo con ratio negativo: @c min_ratio < 0
     *   - Decrescita stretta del potenziale: @c Φ(f_new) < Φ(f_old)
     *
     * @param source            Indice del nodo sorgente (0-based).
     * @param sink              Indice del nodo pozzo (0-based).
     * @param optimal_flow      Stima del valore ottimale del flusso.
     * @param lower_capacities  Capacità inferiori sugli archi (opzionale,
     *                          default nullptr → tutti zero).
     * @return Coppia (valore del flusso sull'arco virtuale s→t,
     *         vettore dei flussi sugli archi originali arrotondati).
     */
    std::pair<long long, Eigen::VectorXd> max_flow_with_guess(
        int source,
        int sink,
        long long optimal_flow,
        const std::vector<long long>* lower_capacities = nullptr
    );

private:
    /// @brief Lista degli archi del grafo, in corrispondenza 1:1 con @ref _capacities.
    std::vector<std::pair<int,int>> _edges;

    /// @brief Capacità degli archi, indicizzate come @ref _edges.
    ///        Valori a 64 bit per supportare istanze con capacità frazionarie scalate.
    std::vector<long long> _capacities;
};