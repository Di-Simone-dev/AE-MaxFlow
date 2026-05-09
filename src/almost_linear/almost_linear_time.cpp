/**
 * @file almost_linear_time.cpp
 * @brief Implementazione del solver AlmostLinearTime per il problema di massimo flusso.
 *
 * Contiene l'implementazione del costruttore, della ricerca binaria @ref max_flow
 * e del loop di ottimizzazione interior-point @ref max_flow_with_guess.
 *
 * ### Schema dell'algoritmo
 *
 * 1. **Ricerca binaria** (`max_flow`): individua il massimo valore di flusso
 *    raggiungibile nell'intervallo [0, Σ cap(source, v)] invocando
 *    `max_flow_with_guess` per ogni candidato `mid`.
 *
 * 2. **Costruzione min-cost flow** (`max_flow_with_guess`): trasforma il problema
 *    di max-flow con valore target in un'istanza di min-cost flow con costo
 *    ottimale noto (`-optimal_flow`), aggiungendo un arco virtuale t→s.
 *
 * 3. **Flusso ammissibile iniziale**: `almost_linear::calc_feasible_flow` produce
 *    un flusso interno al dominio ammissibile di Φ(f).
 *
 * 4. **Loop MCR**: ad ogni iterazione si calcola il minimum cycle ratio tramite
 *    Howard, e si aggiorna il flusso nella direzione del ciclo critico con
 *    step-size `η · upscale`, garantendo la decrescita stretta di Φ(f).
 *
 * @see almost_linear_time.hpp
 * @see howard.hpp
 * @see feasible_flow.hpp
 * @see min_cost_flow_instance.hpp
 */

#include "almost_linear_time.hpp"
#include <iostream>
#include <cmath>
#include <cassert>
#include <limits>
#include <numeric>

// ---------------------------------------------------------------------------
// Costruttore
// ---------------------------------------------------------------------------

/**
 * @brief Costruisce il solver popolando i vettori interni di archi e capacità.
 *
 * L'iterazione sulla @c std::map avviene in ordine lessicografico delle chiavi,
 * garantendo un ordinamento deterministico indipendente dalla piattaforma.
 *
 * @param graph Mappa da arco (u, v) a capacità intera a 64 bit.
 */
AlmostLinearTime::AlmostLinearTime(const std::map<std::pair<int,int>, long long>& graph) {
    for (auto& [edge, cap] : graph) {
        _edges.push_back(edge);
        _capacities.push_back(cap);
    }
}

// ---------------------------------------------------------------------------
// max_flow  —  ricerca binaria sul valore ottimale
// ---------------------------------------------------------------------------

/**
 * @brief Calcola il valore del massimo flusso tramite ricerca binaria.
 *
 * Il limite superiore della ricerca è la somma delle capacità degli archi
 * uscenti da @p source. Ad ogni iterazione si testa il valore @c mid:
 * - Se `max_flow_with_guess` restituisce un flusso < @c mid, il valore
 *   @c mid non è raggiungibile → si abbassa @c high.
 * - Altrimenti → si alza @c low.
 *
 * La ricerca termina quando @c low == @c high, ovvero quando l'intervallo
 * si è ristretto a un singolo valore.
 *
 * @param source Indice del nodo sorgente (0-based).
 * @param sink   Indice del nodo pozzo (0-based).
 * @return Valore del massimo flusso da @p source a @p sink.
 */
long long AlmostLinearTime::max_flow(int source, int sink) {
    // Limite superiore: somma delle capacità uscenti da source
    long long max_possible_flow = 0;
    for (long long e = 0; e < static_cast<long long>(_edges.size()); ++e)
        if (_edges[e].first == source)
            max_possible_flow += _capacities[e];

    long long iters = 0;
    long long low = 0, high = max_possible_flow + 1;
    long long mf = 0;

    while (low < high) {
        ++iters;
        std::cout << "iterazioni ricerca binaria max_flow = " << iters << "\n";

        /// @note `mid` è calcolato come `long long` per evitare overflow
        ///       su istanze con capacità elevate (es. frazionarie scalate).
        long long mid = (low + high) / 2;

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

/**
 * @brief Risolve il max-flow con una stima fissata del valore ottimale.
 *
 * ### Dettagli implementativi
 *
 * **Parametri del loop**:
 * - `threshold = 1e-5`: tolleranza sulla convergenza di `c·f - optimal_cost`.
 * - `kappa = 0.9999`: fattore di damping per lo step-size `η`.
 * - `upscale = 1000.0`: fattore di scala applicato all'aggiornamento del flusso
 *   per mantenere il flusso lontano dai bordi del dominio ammissibile di Φ.
 *
 * **Step-size**: `η = -(κ²) / (50 · ∇Φ · d)`, dove `d` è il vettore del ciclo
 * critico. Moltiplicato per `upscale` garantisce passi di dimensione adeguata.
 *
 * **Invarianti verificate ad ogni iterazione** (via `assert`):
 * - Conservazione del flusso: `||B^T f||_∞ < 1e-10`
 * - Ciclo critico con ratio negativo: `min_ratio < 0`
 * - Decrescita stretta del potenziale: `Φ(f_new) < Φ(f_old)`
 *
 * @param source            Indice del nodo sorgente (0-based).
 * @param sink              Indice del nodo pozzo (0-based).
 * @param optimal_flow      Stima del valore ottimale del flusso (target).
 * @param lower_capacities  Capacità inferiori sugli archi (nullptr → tutti zero).
 * @return Coppia `(flow_value, edge_flows)`:
 *         - `flow_value`: valore intero del flusso sull'arco virtuale t→s.
 *         - `edge_flows`: vettore dei flussi arrotondati sugli archi originali.
 */
std::pair<long long, Eigen::VectorXd> AlmostLinearTime::max_flow_with_guess(
    int source,
    int sink,
    long long optimal_flow,
    const std::vector<long long>* lower_capacities
) {
    // ── Costruzione istanza min-cost flow ────────────────────────────────
    /// Aggiunge un arco virtuale source→sink con costo -1 e capacità `optimal_flow`,
    /// impostando `optimal_cost = -optimal_flow` come lower bound sul costo.
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
    /// `calc_feasible_flow` aggiunge archi ausiliari da/verso un nodo fittizio
    /// v* per bilanciare gli squilibri di flusso, producendo un punto interno
    /// al dominio ammissibile di Φ(f).
    auto [I2, cur_flow] = almost_linear::calc_feasible_flow(I);
    I = std::move(I2);

    // ── Parametri dell'algoritmo ─────────────────────────────────────────
    const double threshold = 1e-5;   ///< Tolleranza di convergenza su c·f - opt
    const double kappa     = 0.9999; ///< Damping factor per lo step-size η
    const double upscale   = 1000.0; ///< Scala dell'aggiornamento del flusso

    int i = 0;
    double cur_phi = I.phi(cur_flow);

    // ── Loop di ottimizzazione ────────────────────────────────────────────
    while (I.c.dot(cur_flow) - static_cast<double>(I.optimal_cost) >= threshold) {
        ++i;

        // Sanity check: conservazione del flusso (B^T f deve essere zero)
        double flow_conservation = (I.B.transpose() * cur_flow).cwiseAbs().maxCoeff();
        assert(flow_conservation < 1e-10 && "Flow conservation has been broken");

        // Passo 1 – gradienti e lunghezze rispetto a Φ(f)
        Eigen::VectorXd gradients = I.calc_gradients(cur_flow);
        Eigen::VectorXd lengths   = I.calc_lengths(cur_flow);

        // Passo 2 – minimum cycle ratio (algoritmo di Howard)
        /// Howard restituisce il ciclo con ratio minimo (negativo) e il
        /// vettore indicatore del ciclo nel grafo di policy.
        auto [min_ratio, min_ratio_cycle] = minimum_cycle_ratio(I, gradients, lengths);

        assert(min_ratio < 0.0 && "Minimum cycle ratio is not negative");

        // Passo 3 – aggiornamento del flusso lungo il ciclo critico
        /// η garantisce che Φ(f + η·upscale·d) < Φ(f) per la struttura
        /// self-concordante della barriera logaritmica.
        double eta = -(kappa * kappa) / (50.0 * gradients.dot(min_ratio_cycle));
        Eigen::VectorXd augment_cycle = min_ratio_cycle * (eta * upscale);
        cur_flow += augment_cycle;

        // Passo 4 – verifica che Φ(f) sia strettamente decrescente
        double new_phi = I.phi(cur_flow);
        if (!std::isfinite(new_phi))
            break;  // Φ esplode → siamo sul bordo del dominio, fermati

        assert(new_phi < cur_phi && "Phi(f) has not decreased");
        cur_phi = new_phi;
    }

    std::cout << "rounded flow: " << std::round(cur_flow[original_m - 1]) << "\n";

    // Arrotonda e restituisce il flusso finale
    long long flow_value = static_cast<long long>(std::round(cur_flow[flow_idx]));
    Eigen::VectorXd edge_flows = cur_flow.head(flow_idx).array().round();

    return {flow_value, edge_flows};
}