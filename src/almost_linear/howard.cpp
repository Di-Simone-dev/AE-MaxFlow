/**
 * @file howard.cpp
 * @brief Implementazione dell'algoritmo di Howard per il Minimum Cycle Ratio.
 *
 * ### Algoritmo di Howard (policy-iteration per MCR)
 *
 * Dato un grafo orientato con pesi `w(e)` (gradienti) e lunghezze `l(e)`,
 * il Minimum Cycle Ratio è definito come:
 * @code
 *   MCR = min_C { Σ_{e ∈ C} w(e) / Σ_{e ∈ C} l(e) }
 * @endcode
 * dove il minimo è preso su tutti i cicli semplici del grafo.
 *
 * L'algoritmo mantiene una **policy**: per ogni nodo `v`, una scelta locale
 * dell'arco uscente. Il policy graph è un sottografo con esattamente un arco
 * uscente per nodo, garantendo che ogni componente connessa contenga esattamente
 * un ciclo. La policy-iteration migliora iterativamente queste scelte fino a
 * convergenza al ciclo critico globale.
 *
 * ### Convergenza e limite di iterazioni
 *
 * L'algoritmo converge in al più `O(n·m)` iterazioni nel caso peggiore, ma
 * nella pratica converge molto più rapidamente. Il limite di 100 iterazioni
 * è una salvaguardia contro casi degeneri.
 *
 * @see howard.hpp
 * @see AlmostLinearTime::max_flow_with_guess
 */

#include "howard.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

/// Valore sentinella per distanze e ratio non ancora inizializzati.
static constexpr double INF = std::numeric_limits<double>::infinity();

// ---------------------------------------------------------------------------
// Costruttore
// ---------------------------------------------------------------------------

/**
 * @brief Inizializza Howard costruendo la `_edge_cache` e calcolando il bound.
 *
 * Per ogni arco `(u, w)` con indice `edge_id` e gradiente `grad`, inserisce
 * nella cache due entry orientate:
 * - `(u, edge_id) → (w, -grad)`: arco percorso in direzione u→w.
 * - `(w, edge_id) → (u,  grad)`: arco percorso in direzione w→u.
 *
 * @warning Il segno negativo su `(u, edge_id)` replica fedelmente il
 *          comportamento del codice Python originale. Rimuoverlo causa
 *          la mancata convergenza dell'algoritmo su certi grafi.
 */
Howard::Howard(const MinCostFlow& graph,
               const Eigen::VectorXd& gradients,
               const Eigen::VectorXd& lengths)
    : g(graph),
      V(graph.n),
      distances(Eigen::VectorXd::Zero(graph.n)),
      policy(graph.n, -1),
      bad_vertices(graph.n, false),
      in_edges_list(graph.n),
      sink(-1),
      best_ratio(0.0),
      gradients_vec(gradients),
      lengths_vec(lengths)
{
    for (int edge_id = 0; edge_id < static_cast<int>(g.edges.size()); ++edge_id) {
        auto [u, w] = g.edges[edge_id];
        double grad = gradients_vec[edge_id];
        _edge_cache[{u, edge_id}] = {w, -grad};  // ← BUG fedele all'originale Python
        _edge_cache[{w, edge_id}] = {u,  grad};
    }

    bound      = _compute_bound();
    best_ratio = bound;
}

// ---------------------------------------------------------------------------
// _compute_bound
// ---------------------------------------------------------------------------

/**
 * @brief Calcola il limite superiore iniziale sul minimum cycle ratio.
 *
 * Definito come `Σ|w(e)| / min_{l(e)>0} |l(e)|`. Fornisce un valore di
 * inizializzazione per `best_ratio` che è garantito essere ≥ MCR per la
 * disuguaglianza triangolare sui ratio.
 *
 * Archi con lunghezza `< 1e-10` sono ignorati per stabilità numerica.
 *
 * @return Bound superiore sul MCR, o `INF` se tutte le lunghezze sono zero.
 */
double Howard::_compute_bound() const {
    double sum_weights = gradients_vec.cwiseAbs().sum();
    double min_len = INF;
    for (int i = 0; i < lengths_vec.size(); ++i) {
        double al = std::abs(lengths_vec[i]);
        if (al > 1e-10 && al < min_len) min_len = al;
    }
    if (min_len == INF) return INF;
    return sum_weights / min_len;
}

// ---------------------------------------------------------------------------
// Accesso alla cache
// ---------------------------------------------------------------------------

/**
 * @brief Restituisce il gradiente orientato dell'arco `edge_id` visto da `start`.
 *
 * Il segno del gradiente dipende dalla direzione di percorrenza rispetto
 * all'orientamento originale dell'arco — vedere il costruttore per i dettagli.
 *
 * @param start   Nodo di partenza.
 * @param edge_id Indice dell'arco nella lista `g.edges`.
 * @return Gradiente orientato (può essere negativo).
 * @throws std::out_of_range se la coppia `(start, edge_id)` non è in cache.
 */
double Howard::_get_gradient(int start, int edge_id) const {
    return _edge_cache.at({start, edge_id}).second;
}

/**
 * @brief Restituisce il nodo di arrivo dell'arco `edge_id` percorso da `start`.
 *
 * @param start   Nodo di partenza.
 * @param edge_id Indice dell'arco nella lista `g.edges`.
 * @return Indice del nodo di destinazione.
 * @throws std::out_of_range se la coppia `(start, edge_id)` non è in cache.
 */
int Howard::_get_edge_target(int start, int edge_id) const {
    return _edge_cache.at({start, edge_id}).first;
}

// ---------------------------------------------------------------------------
// _construct_policy_graph
// ---------------------------------------------------------------------------

/**
 * @brief Costruisce il policy graph scegliendo per ogni nodo l'arco con
 *        gradiente massimo.
 *
 * Nodi senza archi uscenti (`best_edge == -1`) sono marcati come `bad_vertices`
 * e reindirizzati al `sink` fittizio. Il primo nodo bad trovato diventa il
 * `sink`; i successivi vengono collegati ad esso tramite `in_edges_list`.
 *
 * Aggiorna `in_edges_list[target]` per ogni nodo, mantenendo la struttura
 * degli archi entranti nel policy graph (usata da `_improve_policy`).
 */
void Howard::_construct_policy_graph() {
    for (int v = 0; v < V; ++v) {
        int    best_edge   = -1;
        double best_weight = -INF;

        for (int edge_id : g.adj[v]) {
            double grad = _get_gradient(v, edge_id);
            if (grad > best_weight) {
                best_weight = grad;
                best_edge   = edge_id;
            }
        }

        if (best_edge == -1) {
            // Nodo senza archi uscenti: usa il sink fittizio
            if (sink == -1) sink = v;
            bad_vertices[v] = true;
            in_edges_list[sink].insert(v);
        } else {
            int target = _get_edge_target(v, best_edge);
            in_edges_list[target].insert(v);
            policy[v] = best_edge;
        }
    }
}

// ---------------------------------------------------------------------------
// _find_cycle_vertex
// ---------------------------------------------------------------------------

/**
 * @brief Trova un nodo sul ciclo nel policy graph a partire da `start`.
 *
 * Segue la policy corrente fino a incontrare un nodo già visitato.
 * I `bad_vertices` vengono reindirizzati al `sink`.
 * La detection usa `unordered_set` per lookup O(1) a ogni passo.
 *
 * @param start Nodo di partenza della visita.
 * @return Primo nodo ripetuto — garantito essere su un ciclo.
 */
int Howard::_find_cycle_vertex(int start) const {
    int current = start;
    std::unordered_set<int> visited;

    while (visited.find(current) == visited.end()) {
        visited.insert(current);
        if (!bad_vertices[current])
            current = _get_edge_target(current, policy[current]);
        else
            current = sink;
    }
    return current;
}

// ---------------------------------------------------------------------------
// _compute_cycle_ratio
// ---------------------------------------------------------------------------

/**
 * @brief Calcola il cycle ratio del ciclo contenente `start` e aggiorna
 *        il ciclo critico se il ratio è migliorante.
 *
 * Percorre il ciclo a partire da `start` fino a tornare a `start`,
 * accumulando `Σ gradients` e `Σ lengths`. Il ratio è il loro rapporto.
 *
 * Se `ratio < best_ratio`, aggiorna `best_ratio`, `critical_vertex` e
 * `critical_cycle` con il ciclo corrente.
 *
 * @param start Nodo di partenza (deve essere su un ciclo nel policy graph).
 * @return Cycle ratio del ciclo, o `bound` se `start == sink`.
 */
double Howard::_compute_cycle_ratio(int start) {
    if (start == sink) return bound;

    double sum_w1 = 0.0;
    double sum_w2 = 0.0;
    int    current = start;
    std::vector<int> cycle_edges;

    do {
        int edge_id = policy[current];
        cycle_edges.push_back(edge_id);
        sum_w1 += _get_gradient(current, edge_id);
        sum_w2 += lengths_vec[edge_id];
        current = _get_edge_target(current, edge_id);
    } while (current != start);

    double ratio = sum_w1 / sum_w2;

    if (best_ratio > ratio) {
        best_ratio      = ratio;
        critical_vertex = start;
        critical_cycle  = cycle_edges;
    }

    return ratio;
}

// ---------------------------------------------------------------------------
// _improve_policy
// ---------------------------------------------------------------------------

/**
 * @brief Aggiorna la policy di ogni nodo se esiste un arco localmente migliorante.
 *
 * Per ogni nodo `v` non bad, esamina tutti gli archi uscenti e calcola la
 * distanza potenziale:
 * @code
 *   new_dist = grad(v, e) - current_ratio * len(e) + distances[target]
 * @endcode
 * Se `new_dist < distances[v] + EPS`, aggiorna `policy[v]` e `distances[v]`,
 * mantenendo `in_edges_list` coerente con la nuova scelta.
 *
 * Per i `bad_vertices`, aggiorna solo `distances[v]` usando il `sink`.
 *
 * @param current_ratio Ratio corrente del ciclo trovato da `_find_all_cycles`.
 * @return `true` se almeno una policy è cambiata (iterazione non terminata).
 *         `false` se nessun miglioramento è possibile (convergenza).
 */
bool Howard::_improve_policy(double current_ratio) {
    bool improved = false;

    for (int v = 0; v < V; ++v) {
        if (!bad_vertices[v]) {
            for (int edge_id : g.adj[v]) {
                int    target   = _get_edge_target(v, edge_id);
                double new_dist = _get_gradient(v, edge_id)
                                  - current_ratio * lengths_vec[edge_id]
                                  + distances[target];

                if (distances[v] + EPS > new_dist) {
                    // Aggiorna in_edges_list rimuovendo v dal vecchio target
                    int old_target = _get_edge_target(v, policy[v]);
                    in_edges_list[old_target].erase(v);
                    policy[v] = edge_id;
                    in_edges_list[target].insert(v);
                    distances[v] = new_dist;
                    improved = true;
                }
            }
        } else {
            // bad_vertex: distanza aggiornata tramite arco fittizio verso sink
            double new_dist = bound - current_ratio + distances[sink];
            if (distances[v] + EPS > new_dist) {
                distances[v] = new_dist;
            }
        }
    }

    return improved;
}

// ---------------------------------------------------------------------------
// _find_all_cycles
// ---------------------------------------------------------------------------

/**
 * @brief Trova tutti i cicli nel policy graph corrente e restituisce il MCR.
 *
 * Esegue una DFS su tutti i nodi non ancora visitati. La colorazione a tre
 * stati (WHITE → GRAY → BLACK) identifica i back-edge della DFS, che
 * corrispondono ai cicli nel policy graph.
 *
 * Per ogni ciclo trovato, invoca `_compute_cycle_ratio` che aggiorna
 * `best_ratio` e `critical_cycle` se il ratio è migliorante.
 *
 * @return Valore minimo del cycle ratio tra tutti i cicli del policy graph,
 *         o `INF` se il policy graph è aciclico.
 */
double Howard::_find_all_cycles() {
    constexpr int WHITE = 0, GRAY = 1, BLACK = 2;

    std::vector<int> color(V, WHITE);
    double min_ratio = INF;

    auto dfs = [&](int start) {
        std::vector<int> stack;
        int v = start;

        // Segui la policy fino a trovare un nodo già in stack (GRAY) o già processato (BLACK)
        while (color[v] == WHITE) {
            color[v] = GRAY;
            stack.push_back(v);
            v = bad_vertices[v] ? sink : _get_edge_target(v, policy[v]);
        }

        if (color[v] == GRAY) {
            // Back-edge trovato: v è il nodo di ingresso al ciclo
            double ratio = _compute_cycle_ratio(v);
            if (ratio < min_ratio) min_ratio = ratio;

            // Colora in BLACK tutti i nodi del ciclo nello stack
            auto it = std::find(stack.begin(), stack.end(), v);
            for (auto jt = it; jt != stack.end(); ++jt)
                color[*jt] = BLACK;
        }

        // Colora in BLACK i nodi della coda (non sul ciclo)
        for (int node : stack)
            if (color[node] != BLACK)
                color[node] = BLACK;
    };

    for (int v = 0; v < V; ++v)
        if (color[v] == WHITE)
            dfs(v);

    return min_ratio;
}

// ---------------------------------------------------------------------------
// _make_cycle_vector
// ---------------------------------------------------------------------------

/**
 * @brief Costruisce il vettore indicatore ±1 del ciclo critico.
 *
 * Percorre il ciclo critico a partire da `critical_vertex` seguendo
 * `critical_cycle`. Per ogni arco:
 * - `+1` se percorso nella direzione originale `(u → w)` di `g.edges[edge_id]`.
 * - `-1` se percorso in direzione inversa.
 *
 * Questo vettore è usato da @ref AlmostLinearTime::max_flow_with_guess come
 * direzione di aggiornamento del flusso.
 *
 * @return Vettore di dimensione `g.m` con valori in `{-1, 0, +1}`.
 *         Vettore zero se `critical_cycle` o `critical_vertex` sono assenti.
 */
Eigen::VectorXd Howard::_make_cycle_vector() const {
    Eigen::VectorXd edge_cycle = Eigen::VectorXd::Zero(g.m);

    if (!critical_cycle.has_value() || !critical_vertex.has_value())
        return edge_cycle;

    int current = *critical_vertex;
    for (int edge_id : *critical_cycle) {
        int target = _get_edge_target(current, edge_id);
        // +1 se direzione concorde con l'orientamento originale dell'arco, -1 altrimenti
        edge_cycle[edge_id] = (current == g.edges[edge_id].first) ? 1.0 : -1.0;
        current = target;
    }
    return edge_cycle;
}

// ---------------------------------------------------------------------------
// find_optimum_cycle_ratio
// ---------------------------------------------------------------------------

/**
 * @brief Esegue l'intera policy-iteration e restituisce il minimum cycle ratio.
 *
 * Costruisce il policy graph iniziale, poi itera `_find_all_cycles` +
 * `_improve_policy` fino a convergenza o al limite di 100 iterazioni.
 *
 * Condizione di fallback: se il ratio finale è ≥ `bound - 1e-10` oppure
 * nessun ciclo critico è stato identificato, restituisce `(INF, zero_vector)`
 * — segnale che il flusso corrente è già ottimale o il problema è degenere.
 *
 * @return Coppia `(min_ratio, cycle_vector)` — vedere @ref Howard::find_optimum_cycle_ratio
 *         nell'header per la semantica completa.
 */
std::pair<double, Eigen::VectorXd> Howard::find_optimum_cycle_ratio() {
    _construct_policy_graph();

    int    iteration = 0;
    double ratio     = INF;

    while (iteration < 100) {
        ratio = _find_all_cycles();
        if (!_improve_policy(ratio)) break;  // Convergenza: nessuna policy migliorata
        ++iteration;
    }

    if (ratio > bound - 1e-10 || !critical_cycle.has_value()) {
        // Nessun ciclo migliorante trovato: flusso già ottimale o problema degenere
        return {INF, Eigen::VectorXd::Zero(g.m)};
    } else {
        return {ratio, _make_cycle_vector()};
    }
}

// ---------------------------------------------------------------------------
// Funzione di interfaccia pubblica
// ---------------------------------------------------------------------------

/**
 * @brief Wrapper pubblico che istanzia Howard ed esegue l'algoritmo MCR.
 *
 * Unico punto di ingresso esterno consigliato. Crea un oggetto @ref Howard
 * temporaneo, esegue `find_optimum_cycle_ratio` e restituisce il risultato.
 *
 * @param g         Istanza di min-cost flow con il grafo e le matrici.
 * @param gradients Vettore dei gradienti di Φ(f) sugli archi (dimensione `m`).
 * @param lengths   Vettore delle lunghezze sugli archi (dimensione `m`).
 * @return Coppia `(min_ratio, cycle_vector)`.
 */
std::pair<double, Eigen::VectorXd>
minimum_cycle_ratio(const MinCostFlow& g,
                    const Eigen::VectorXd& gradients,
                    const Eigen::VectorXd& lengths)
{
    Howard howard(g, gradients, lengths);
    return howard.find_optimum_cycle_ratio();
}