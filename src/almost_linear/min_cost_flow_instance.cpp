/**
 * @file min_cost_flow_instance.cpp
 * @brief Implementazione di `MinCostFlow`: costruttore, clone, factory method,
 *        potenziale Φ, gradienti, lunghezze e metodi di modifica del grafo.
 *
 * ### Note implementative
 *
 * **Parametro α**: `alpha = 1 / log₂(1000·m·U)` è scelto in modo che la
 * barriera sia sufficientemente "piatta" al centro del dominio e "ripida"
 * vicino ai bordi. Valori grandi di `U` o `m` riducono `α`, rendendo la
 * barriera più debole (utile per grandi istanze).
 *
 * **Matrice B**: è mantenuta densa (m×n) per semplicità. Con grafi sparsi
 * di grandi dimensioni, una rappresentazione sparsa (`Eigen::SparseMatrix`)
 * ridurrebbe il costo di `B^T * f` da O(m·n) a O(m).
 *
 * **`add_edge` / `add_vertex`**: entrambi riallocano `B` interamente ad ogni
 * chiamata — O(m·n) per arco aggiunto. Usati solo in `calc_feasible_flow`
 * dove il numero di aggiunte è limitato a `n` (un arco per nodo sbilanciato).
 *
 * @see min_cost_flow_instance.hpp
 */

#include "min_cost_flow_instance.hpp"
#include <stdexcept>
#include <numeric>

/// Costante `ln(2)` precomputata per convertire logaritmi naturali in log₂.
static const double _LOG2 = 0.6931471805599453;

// ---------------------------------------------------------------------------
// Costruttore principale
// ---------------------------------------------------------------------------

/**
 * @brief Inizializza l'istanza calcolando tutti i campi derivati dagli archi.
 *
 * #### Operazioni eseguite
 *
 * 1. Calcola `n` come `max(tutti gli indici di nodo) + 1`.
 * 2. Copia `edges`, `c`, `u_lower`, `u_upper`, `optimal_cost`.
 * 3. Calcola `U = max(|u_upper|, |u_lower|)` e `alpha = 1/log₂(1000·m·U)`.
 * 4. Costruisce la matrice di incidenza `B` (m×n).
 * 5. Popola `undirected_edge_to_indices` con entrambe le direzioni.
 * 6. Costruisce la lista di adiacenza `adj`.
 *
 * @note `c_org` viene inizializzata uguale a `c` — copia dei costi originali
 *       prima di eventuali modifiche successive.
 */
MinCostFlow::MinCostFlow(
    const std::vector<std::pair<int,int>>& edges_,
    const Eigen::VectorXd& c_,
    const Eigen::VectorXd& u_lower_,
    const Eigen::VectorXd& u_upper_,
    long long optimal_cost_
) {
    m = static_cast<int>(edges_.size());

    // n = indice massimo tra tutti i nodi + 1
    int max_node = 0;
    for (auto& [a, b] : edges_)
        max_node = std::max(max_node, std::max(a, b));
    n = max_node + 1;

    edges        = edges_;
    c            = c_;
    c_org        = c_;          // salva i costi originali
    u_lower      = u_lower_;
    u_upper      = u_upper_;
    optimal_cost = optimal_cost_;

    // U = max capacità in valore assoluto — usato per il calcolo di alpha
    U = static_cast<long long>(std::max(u_upper.cwiseAbs().maxCoeff(),
                                        u_lower.cwiseAbs().maxCoeff()));

    // alpha: parametro della barriera logaritmica
    alpha = 1.0 / std::log2(1000.0 * m * static_cast<double>(U));

    // Verifiche di consistenza tra le dimensioni dei vettori
    assert(static_cast<int>(edges.size())   == m);
    assert(static_cast<int>(c.size())       == m);
    assert(static_cast<int>(u_lower.size()) == m);
    assert(static_cast<int>(u_upper.size()) == m);

    // Matrice di incidenza B (m x n): B(e,u)=+1, B(e,v)=-1 per arco (u,v)
    B = Eigen::MatrixXd::Zero(m, n);
    for (int e = 0; e < m; ++e) {
        auto [a, b] = edges[e];
        B(e, a) =  1.0;
        B(e, b) = -1.0;
    }

    // Mappa bidirezionale: (a,b) e (b,a) → lista di indici degli archi tra a e b
    for (int e = 0; e < m; ++e) {
        auto [a, b] = edges[e];
        undirected_edge_to_indices[{a, b}].push_back(e);
        undirected_edge_to_indices[{b, a}].push_back(e);
    }

    // Lista di adiacenza: ogni arco compare in adj[a] e adj[b]
    adj.resize(n);
    for (int e = 0; e < m; ++e) {
        auto [a, b] = edges[e];
        adj[a].push_back(e);
        adj[b].push_back(e);
    }
}

// ---------------------------------------------------------------------------
// clone()
// ---------------------------------------------------------------------------

/**
 * @brief Produce una copia profonda dell'istanza.
 *
 * Copia tutti i campi scalari, i vettori Eigen, la matrice `B`, la mappa
 * `undirected_edge_to_indices` e la lista `adj`. La copia è completamente
 * indipendente dall'originale — modificare un campo della copia non altera
 * l'originale e viceversa.
 *
 * @return Nuova istanza `MinCostFlow` identica a `*this`.
 */
MinCostFlow MinCostFlow::clone() const {
    MinCostFlow obj;
    obj.m            = m;
    obj.n            = n;
    obj.edges        = edges;
    obj.c            = c;
    obj.c_org        = c_org;
    obj.u_lower      = u_lower;
    obj.u_upper      = u_upper;
    obj.optimal_cost = optimal_cost;
    obj.U            = U;
    obj.alpha        = alpha;
    obj.B            = B;
    obj.undirected_edge_to_indices = undirected_edge_to_indices;
    obj.adj          = adj;
    return obj;
}

// ---------------------------------------------------------------------------
// from_max_flow_instance()
// ---------------------------------------------------------------------------

/**
 * @brief Riduce un problema di max-flow a un'istanza di min-cost flow.
 *
 * #### Costruzione
 *
 * - Aggiunge un arco virtuale `(t → s)` con costo `-1` come ultimo arco.
 * - Costi degli archi originali: tutti `0`.
 * - Capacità superiore dell'arco virtuale: `Σ capacities` (bound triviale).
 * - `optimal_cost = -optimal_flow`: minimizzare il costo equivale a
 *   massimizzare il flusso sull'arco `(t → s)`.
 *
 * @note `cap_sum` è calcolato come `long long` per evitare overflow su
 *       istanze con molti archi ad alta capacità (es. frazionari scalati).
 */
MinCostFlow MinCostFlow::from_max_flow_instance(
    const std::vector<std::pair<int,int>>& edges,
    int s, int t,
    long long optimal_flow,
    const std::vector<long long>& capacities,
    const std::vector<long long>* lower_capacities
) {
    // Aggiunge l'arco virtuale (t → s) come ultimo elemento
    auto new_edges = edges;
    new_edges.push_back({t, s});
    int total = static_cast<int>(new_edges.size());

    // Costi: 0 su tutti gli archi originali, -1 sull'arco virtuale
    Eigen::VectorXd c = Eigen::VectorXd::Zero(total);
    c[total - 1] = -1.0;

    // Capacità inferiori: da lower_capacities se fornito, 0 altrimenti
    Eigen::VectorXd u_lower = Eigen::VectorXd::Zero(total);
    if (lower_capacities) {
        for (int i = 0; i < static_cast<int>(lower_capacities->size()); ++i)
            u_lower[i] = (*lower_capacities)[i];
        // u_lower[total-1] = 0: nessun flusso minimo sull'arco virtuale
    }

    // Capacità superiori: dai dati originali + somma totale per l'arco virtuale
    Eigen::VectorXd u_upper(total);
    long long cap_sum = 0;
    for (int i = 0; i < static_cast<int>(capacities.size()); ++i) {
        u_upper[i] = capacities[i];
        cap_sum += capacities[i];
    }
    u_upper[total - 1] = cap_sum;  // bound superiore triviale per l'arco virtuale

    return MinCostFlow(new_edges, c, u_lower, u_upper, -optimal_flow);
}

// ---------------------------------------------------------------------------
// phi()
// ---------------------------------------------------------------------------

/**
 * @brief Valuta il potenziale Φ(f) = termine obiettivo + barriere di capacità.
 *
 * Il termine obiettivo usa `log₂` (implementato come `log / _LOG2`) per
 * coerenza con il parametro `alpha` che è definito rispetto a `log₂`.
 * Le barriere sono somme di potenze negative: divergono a `+∞` quando `f`
 * si avvicina ai bordi `u_lower` o `u_upper`.
 *
 * @param f Flusso corrente, strettamente ammissibile: `u_lower < f < u_upper`.
 * @return Valore scalare Φ(f).
 */
double MinCostFlow::phi(const Eigen::VectorXd& f) const {
    double cur_cost = c.dot(f);

    // Termine obiettivo: penalizza il costo corrente rispetto all'ottimo
    double objective = 20.0 * m * std::log(cur_cost - optimal_cost) / _LOG2;

    // Barriere sulle capacità: divergono quando f → u_upper o f → u_lower
    Eigen::VectorXd upper_barriers = (u_upper - f).array().pow(-alpha);
    Eigen::VectorXd lower_barriers = (f - u_lower).array().pow(-alpha);

    double barrier = (upper_barriers + lower_barriers).sum();

    return objective + barrier;
}

// ---------------------------------------------------------------------------
// calc_gradients()
// ---------------------------------------------------------------------------

/**
 * @brief Calcola il gradiente ∇Φ(f) sugli archi.
 *
 * Derivata di Φ rispetto a `f_e`:
 * @code
 *   ∂Φ/∂f_e = 20·m·c_e / (c·f - opt)      ← termine obiettivo
 *             + α·(u_e - f_e)^(-1-α)        ← barriera superiore
 *             - α·(f_e - l_e)^(-1-α)        ← barriera inferiore
 * @endcode
 *
 * @param f Flusso corrente (dimensione `m`), strettamente ammissibile.
 * @return Vettore gradiente di dimensione `m`.
 */
Eigen::VectorXd MinCostFlow::calc_gradients(const Eigen::VectorXd& f) const {
    double cur_cost = c.dot(f);

    // Contributo del termine obiettivo: scalare · c
    Eigen::VectorXd objective = (20.0 * m / (cur_cost - optimal_cost)) * c;

    // Derivata delle barriere: +α·(u-f)^(-1-α) - α·(f-l)^(-1-α)
    Eigen::VectorXd left  = alpha * (u_upper - f).array().pow(-1.0 - alpha).matrix();
    Eigen::VectorXd right = alpha * (f - u_lower).array().pow(-1.0 - alpha).matrix();

    return objective + left - right;
}

// ---------------------------------------------------------------------------
// calc_lengths()
// ---------------------------------------------------------------------------

/**
 * @brief Calcola le lunghezze sugli archi per l'algoritmo di Howard.
 *
 * Le lunghezze sono proporzionali alla curvatura della barriera in `f`:
 * @code
 *   lengths_e = (u_e - f_e)^(-1-α) + (f_e - l_e)^(-1-α)
 * @endcode
 * Sono sempre strettamente positive nel dominio ammissibile, garantendo
 * che il ratio `Σ gradients / Σ lengths` sia ben definito in Howard.
 *
 * @param f Flusso corrente (dimensione `m`), strettamente ammissibile.
 * @return Vettore delle lunghezze di dimensione `m` (tutti > 0).
 */
Eigen::VectorXd MinCostFlow::calc_lengths(const Eigen::VectorXd& f) const {
    Eigen::VectorXd left  = (u_upper - f).array().pow(-1.0 - alpha).matrix();
    Eigen::VectorXd right = (f - u_lower).array().pow(-1.0 - alpha).matrix();
    return left + right;
}

// ---------------------------------------------------------------------------
// edges_between()
// ---------------------------------------------------------------------------

/**
 * @brief Restituisce gli indici degli archi tra `a` e `b` (lookup O(1)).
 *
 * La ricerca è non orientata: `edges_between(a, b)` restituisce gli stessi
 * archi di `edges_between(b, a)`, poiché `undirected_edge_to_indices` è
 * popolata in entrambe le direzioni nel costruttore.
 *
 * @param a Primo nodo.
 * @param b Secondo nodo.
 * @return Lista degli indici degli archi tra `a` e `b`. Vuota se non esistono.
 */
std::vector<int> MinCostFlow::edges_between(int a, int b) const {
    auto it = undirected_edge_to_indices.find({a, b});
    if (it == undirected_edge_to_indices.end())
        return {};
    return it->second;
}

// ---------------------------------------------------------------------------
// add_vertex()
// ---------------------------------------------------------------------------

/**
 * @brief Aggiunge un nodo isolato e restituisce il suo indice.
 *
 * Estende `B` con una colonna di zeri (nodo non ancora connesso ad archi)
 * e aggiunge una lista di adiacenza vuota. Il nuovo nodo ha indice `n - 1`
 * dopo l'aggiornamento di `n`.
 *
 * @return Indice del nuovo nodo.
 *
 * @warning Riallocat l'intera matrice `B` — O(m·n). Accettabile se chiamato
 *          raramente (es. una volta sola in `calc_feasible_flow`).
 */
int MinCostFlow::add_vertex() {
    n += 1;
    // Estende B di una colonna (nuovo nodo isolato)
    Eigen::MatrixXd new_B(m, n);
    new_B.leftCols(n - 1) = B;
    new_B.col(n - 1).setZero();
    B = std::move(new_B);
    adj.push_back({});
    return n - 1;
}

// ---------------------------------------------------------------------------
// add_edge()
// ---------------------------------------------------------------------------

/**
 * @brief Aggiunge un arco orientato `(a → b)` al grafo aggiornando tutte le strutture.
 *
 * #### Strutture aggiornate
 * - `edges`, `adj[a]`, `adj[b]`: append dell'indice del nuovo arco.
 * - `undirected_edge_to_indices[{a,b}]` e `[{b,a}]`: append dell'indice.
 * - `c`, `u_lower`, `u_upper`: `conservativeResize` + assegnazione dell'ultimo elemento.
 * - `B`: riallocazione con una riga aggiuntiva per il nuovo arco.
 * - `m`: incrementato di 1.
 *
 * @param a       Nodo di partenza (indice < `n`).
 * @param b       Nodo di arrivo (indice < `n`).
 * @param c_val   Costo dell'arco.
 * @param ul      Capacità inferiore dell'arco.
 * @param uu      Capacità superiore dell'arco.
 *
 * @pre `a < n && b < n` (verificato via `assert`).
 * @warning Ogni chiamata riallocat `B` interamente — O(m·n). Usare il costruttore
 *          principale per aggiungere molti archi in un colpo solo.
 */
void MinCostFlow::add_edge(int a, int b, double c_val, double ul, double uu) {
    assert(a < n);
    assert(b < n);

    edges.push_back({a, b});
    int e = static_cast<int>(edges.size()) - 1;

    // Aggiorna la mappa bidirezionale e la lista di adiacenza
    undirected_edge_to_indices[{a, b}].push_back(e);
    undirected_edge_to_indices[{b, a}].push_back(e);
    adj[a].push_back(e);
    adj[b].push_back(e);

    m += 1;

    // Estendi i vettori Eigen preservando i valori esistenti
    c.conservativeResize(m);       c[m - 1] = c_val;
    u_lower.conservativeResize(m); u_lower[m - 1] = ul;
    u_upper.conservativeResize(m); u_upper[m - 1] = uu;

    // Aggiunge una riga a B per il nuovo arco (riallocazione completa)
    Eigen::MatrixXd new_B(m, n);
    new_B.topRows(m - 1) = B;
    new_B.row(m - 1).setZero();
    new_B(m - 1, a) =  1.0;
    new_B(m - 1, b) = -1.0;
    B = std::move(new_B);
}