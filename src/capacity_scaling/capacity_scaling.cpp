/**
 * @file capacity_scaling.cpp
 * @brief Implementazione dei metodi di CapacityScaling.
 *
 * Contiene i costruttori, il dispatcher max_flow() e le implementazioni
 * private dell'algoritmo Capacity Scaling per le due modalità:
 * - `_max_flow_int` per capacità `long long` (aritmetica esatta).
 * - `_max_flow_dbl` per capacità `double` (confronti con soglia EPS).
 *
 * La struttura è intenzionalmente duplicata (anziché templatizzata) per
 * rendere esplicita la differenza nei confronti numerici tra le due modalità,
 * in modo speculare a quanto fatto in `push_relabel.cpp`.
 *
 * @see capacity_scaling.hpp per la documentazione dell'interfaccia pubblica.
 */

#include "capacity_scaling.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <stdexcept>


// ============================================================================
// Costruttore — modalità intera
// ============================================================================

/**
 * @details
 * Il costruttore esegue tre passi:
 *
 * 1. **Raccolta dei nodi**: scorre tutti gli archi di @p graph e registra
 *    ogni estremo in `_index` (se non già presente), assegnandogli un
 *    indice interno progressivo 0-based.
 *
 * 2. **Allocazione della lista di adiacenza**: ridimensiona `_adj_int` a
 *    `_n` liste vuote.
 *
 * 3. **Inserimento degli archi residui**: per ogni arco `(u → v, cap)`:
 *    - aggiunge a `_adj_int[u]` l'arco diretto con capacità @p cap;
 *    - aggiunge a `_adj_int[v]` l'arco inverso con capacità 0.
 *    I campi `rev` si puntano a vicenda per aggiornamenti O(1) in augment.
 */
CapacityScaling::CapacityScaling(const IntGraph& graph)
    : _is_double(false)
{
    // --- Passo 1: raccolta e indicizzazione dei nodi ---
    for (auto& [edge, _cap] : graph) {
        for (int node : {edge.first, edge.second}) {
            if (_index.find(node) == _index.end()) {
                _index[node] = static_cast<int>(_label.size());
                _label.push_back(node);
            }
        }
    }
    _n = static_cast<int>(_label.size());

    // --- Passo 2: allocazione lista di adiacenza ---
    _adj_int.resize(_n);

    // --- Passo 3: inserimento archi diretti e inversi ---
    for (auto& [edge, cap] : graph) {
        int u  = _index.at(edge.first);
        int v  = _index.at(edge.second);
        int eu = static_cast<int>(_adj_int[u].size()); // posizione futura in adj[u]
        int ev = static_cast<int>(_adj_int[v].size()); // posizione futura in adj[v]
        _adj_int[u].push_back({v, ev, cap});   // arco diretto
        _adj_int[v].push_back({u, eu, 0LL});   // arco inverso (capacità 0)
    }
}


// ============================================================================
// Costruttore — modalità double
// ============================================================================

/**
 * @details
 * Identico al costruttore per IntGraph, ma opera su `_adj_dbl` e imposta
 * la capacità degli archi inversi a `0.0`. Il flag `_is_double` viene
 * impostato a `true` per indirizzare le chiamate successive a `_max_flow_dbl`.
 */
CapacityScaling::CapacityScaling(const DblGraph& graph)
    : _is_double(true)
{
    // --- Passo 1: raccolta e indicizzazione dei nodi ---
    for (auto& [edge, _cap] : graph) {
        for (int node : {edge.first, edge.second}) {
            if (_index.find(node) == _index.end()) {
                _index[node] = static_cast<int>(_label.size());
                _label.push_back(node);
            }
        }
    }
    _n = static_cast<int>(_label.size());

    // --- Passo 2: allocazione lista di adiacenza ---
    _adj_dbl.resize(_n);

    // --- Passo 3: inserimento archi diretti e inversi ---
    for (auto& [edge, cap] : graph) {
        int u  = _index.at(edge.first);
        int v  = _index.at(edge.second);
        int eu = static_cast<int>(_adj_dbl[u].size());
        int ev = static_cast<int>(_adj_dbl[v].size());
        _adj_dbl[u].push_back({v, ev, cap});   // arco diretto
        _adj_dbl[v].push_back({u, eu, 0.0});   // arco inverso (capacità 0)
    }
}


// ============================================================================
// max_flow — dispatcher
// ============================================================================

/**
 * @details
 * Il metodo:
 * 1. Verifica che @p source e @p sink esistano in `_index`, lanciando
 *    `std::out_of_range` in caso contrario.
 * 2. Gestisce il caso degenere `source == sink` restituendo 0 senza
 *    eseguire l'algoritmo.
 * 3. Delega a `_max_flow_int()` o `_max_flow_dbl()` in base al flag
 *    `_is_double` impostato dal costruttore.
 */
std::variant<long long, double> CapacityScaling::max_flow(int source, int sink) {
    auto it_s = _index.find(source);
    auto it_t = _index.find(sink);
    if (it_s == _index.end()) throw std::out_of_range("source non trovato nel grafo");
    if (it_t == _index.end()) throw std::out_of_range("sink non trovato nel grafo");

    int s = it_s->second;
    int t = it_t->second;

    // Caso degenere: sorgente e pozzo coincidono
    if (s == t) return _is_double ? std::variant<long long, double>(0.0)
                                  : std::variant<long long, double>(0LL);

    if (_is_double)
        return _max_flow_dbl(s, t);
    else
        return _max_flow_int(s, t);
}


// ============================================================================
// _bfs_int — BFS nel delta-grafo residuo (long long)
// ============================================================================

/**
 * @details
 * Esegue una BFS standard a partire da @p s, visitando solo gli archi con
 * capacità residua ≥ @p delta (delta-grafo residuo). La ricerca termina
 * non appena @p t viene estratto dalla coda, restituendo il vettore
 * dei predecessori per la ricostruzione del cammino.
 *
 * Il vettore `parent` codifica il cammino: `parent[v] = {u, idx}` indica
 * che @p v è stato raggiunto da @p u tramite l'arco `_adj_int[u][idx]`.
 * La sorgente @p s è inizializzata con `parent[s] = {s, -1}` (sentinella
 * per la terminazione del ciclo di ricostruzione in _augment_int()).
 *
 * @return `std::nullopt` se @p t non è raggiungibile da @p s nel
 *         delta-grafo residuo con la soglia @p delta corrente.
 */
std::optional<CapacityScaling::Parent>
CapacityScaling::_bfs_int(int s, int t, long long delta) const {
    Parent parent(_n, {-1, -1});
    std::vector<bool> visited(_n, false);
    parent[s]  = {s, -1}; // sentinella: s è la propria sorgente
    visited[s] = true;

    std::deque<int> queue;
    queue.push_back(s);

    while (!queue.empty()) {
        int u = queue.front(); queue.pop_front();
        if (u == t) return parent; // cammino trovato: restituisce subito

        for (int idx = 0; idx < static_cast<int>(_adj_int[u].size()); ++idx) {
            const auto& e = _adj_int[u][idx];
            // Arco ammissibile nel delta-grafo: cap >= delta (confronto esatto)
            if (!visited[e.to] && e.cap >= delta) {
                visited[e.to] = true;
                parent[e.to]  = {u, idx};
                queue.push_back(e.to);
            }
        }
    }
    return std::nullopt; // t non raggiungibile con la soglia delta corrente
}


// ============================================================================
// _bfs_dbl — BFS nel delta-grafo residuo (double)
// ============================================================================

/**
 * @details
 * Identica a _bfs_int(), con l'unica differenza che la condizione di
 * ammissibilità è `cap ≥ delta - EPS` anziché `cap >= delta`.
 *
 * La tolleranza EPS evita di escludere archi la cui capacità residua è
 * leggermente sotto @p delta per effetto di errori di arrotondamento
 * floating-point accumulati nelle operazioni di augment precedenti.
 */
std::optional<CapacityScaling::Parent>
CapacityScaling::_bfs_dbl(int s, int t, double delta) const {
    Parent parent(_n, {-1, -1});
    std::vector<bool> visited(_n, false);
    parent[s]  = {s, -1};
    visited[s] = true;

    std::deque<int> queue;
    queue.push_back(s);

    while (!queue.empty()) {
        int u = queue.front(); queue.pop_front();
        if (u == t) return parent;

        for (int idx = 0; idx < static_cast<int>(_adj_dbl[u].size()); ++idx) {
            const auto& e = _adj_dbl[u][idx];
            // Arco ammissibile: cap >= delta con tolleranza EPS
            if (!visited[e.to] && e.cap >= delta - EPS) {
                visited[e.to] = true;
                parent[e.to]  = {u, idx};
                queue.push_back(e.to);
            }
        }
    }
    return std::nullopt;
}


// ============================================================================
// _augment_int — aumento di flusso su long long
// ============================================================================

/**
 * @details
 * L'aumento avviene in due passate sul cammino da @p t a @p s
 * (risalendo tramite @p parent):
 *
 * 1. **Calcolo del bottleneck**: la quantità massima di flusso inviabile
 *    è il minimo delle capacità residue degli archi sul cammino.
 *    Inizializzato a `LLONG_MAX` e aggiornato ad ogni arco.
 *
 * 2. **Aggiornamento residui**: per ogni arco `(u → v)` sul cammino,
 *    la capacità residua diretta viene decrementata di `bn` e quella
 *    inversa incrementata di `bn` (invariante del grafo residuo).
 *
 * @note Il ciclo usa `parent[s] = {s, -1}` come condizione di arresto:
 *       quando `v == s`, `parent[v].first == s == v`, quindi il loop
 *       `for (int v = t; v != s; )` termina correttamente.
 */
long long CapacityScaling::_augment_int(int s, int t, const Parent& parent) {
    // --- Passo 1: bottleneck ---
    long long bn = std::numeric_limits<long long>::max();
    for (int v = t; v != s; ) {
        auto [u, idx] = parent[v];
        bn = std::min(bn, _adj_int[u][idx].cap);
        v  = u;
    }

    // --- Passo 2: aggiornamento capacità residue ---
    for (int v = t; v != s; ) {
        auto [u, idx] = parent[v];
        int rev = _adj_int[u][idx].rev;
        _adj_int[u][idx].cap -= bn; // satura parzialmente l'arco diretto
        _adj_int[v][rev].cap += bn; // aggiorna l'arco inverso
        v = u;
    }
    return bn;
}


// ============================================================================
// _augment_dbl — aumento di flusso su double
// ============================================================================

/**
 * @details
 * Identico a _augment_int() ma opera su `double`. Il bottleneck viene
 * inizializzato a `+∞` (`std::numeric_limits<double>::infinity()`) anziché
 * a `LLONG_MAX`.
 *
 * @note A causa degli errori di arrotondamento floating-point, la capacità
 *       residua di un arco potrebbe diventare leggermente negativa (ordine
 *       EPS) dopo un augment. Questo viene gestito dalla soglia EPS nelle
 *       BFS successive.
 */
double CapacityScaling::_augment_dbl(int s, int t, const Parent& parent) {
    // --- Passo 1: bottleneck ---
    double bn = std::numeric_limits<double>::infinity();
    for (int v = t; v != s; ) {
        auto [u, idx] = parent[v];
        bn = std::min(bn, _adj_dbl[u][idx].cap);
        v  = u;
    }

    // --- Passo 2: aggiornamento capacità residue ---
    for (int v = t; v != s; ) {
        auto [u, idx] = parent[v];
        int rev = _adj_dbl[u][idx].rev;
        _adj_dbl[u][idx].cap -= bn;
        _adj_dbl[v][rev].cap += bn;
        v = u;
    }
    return bn;
}


// ============================================================================
// _max_flow_int — algoritmo Capacity Scaling su long long
// ============================================================================

/**
 * @details
 * ### Struttura dell'algoritmo
 *
 * **Calcolo di `U` e `delta` iniziale**:
 * `U` è la capacità massima tra tutti gli archi (solo archi diretti, con
 * capacità > 0). `delta` viene impostato alla più grande potenza di 2 ≤ `U`,
 * garantendo al più `O(log U)` fasi.
 *
 * **Ciclo principale** (fasi di scaling):
 * Per ogni valore di `delta` (da `U` arrotondato a potenza di 2 fino a 1):
 * - Si eseguono BFS nel delta-grafo residuo finché esiste un cammino
 *   aumentante; ogni cammino trovato viene aumentato con _augment_int().
 * - Quando non esistono più cammini, `delta` viene dimezzato.
 *
 * **Numero di aumenti per fase**: al più O(m) per fase (Gabow, 1985),
 * per un totale di O(m log U) aumenti, ciascuno O(m) → complessità O(m² log U).
 *
 * @return Flusso massimo come `long long`.
 */
long long CapacityScaling::_max_flow_int(int s, int t) {
    // Calcola U = capacità massima degli archi diretti
    long long U = 0;
    for (auto& edges : _adj_int)
        for (auto& e : edges)
            U = std::max(U, e.cap);

    if (U == 0) return 0LL; // grafo senza archi con capacità positiva

    // Delta iniziale: più grande potenza di 2 <= U
    long long delta = 1LL;
    while (delta * 2 <= U) delta *= 2;

    long long total = 0LL;
    while (delta >= 1) {
        // Fase: aumenta finché esistono cammini nel delta-grafo residuo
        while (true) {
            auto path = _bfs_int(s, t, delta);
            if (!path.has_value()) break;
            total += _augment_int(s, t, *path);
        }
        delta /= 2; // prossima fase con soglia dimezzata
    }
    return total;
}


// ============================================================================
// _max_flow_dbl — algoritmo Capacity Scaling su double
// ============================================================================

/**
 * @details
 * Identico a _max_flow_int() con le seguenti differenze:
 *
 * | Aspetto                    | Modalità intera            | Modalità double                   |
 * |----------------------------|----------------------------|-----------------------------------|
 * | Delta iniziale             | Potenza di 2 ≤ U (loop)   | `2^floor(log2(U))` (std::pow)     |
 * | Condizione di terminazione | `delta >= 1`               | `delta >= EPS`                    |
 * | Ammissibilità arco (BFS)   | `cap >= delta`             | `cap >= delta - EPS`              |
 * | Bottleneck iniziale        | `LLONG_MAX`                | `+∞`                              |
 *
 * Usare `std::pow(2.0, std::floor(std::log2(U)))` è equivalente al loop
 * intero ma più idiomatico per il tipo `double`.
 *
 * @return Flusso massimo come `double`.
 */
double CapacityScaling::_max_flow_dbl(int s, int t) {
    // Calcola U = capacità massima degli archi diretti
    double U = 0.0;
    for (auto& edges : _adj_dbl)
        for (auto& e : edges)
            U = std::max(U, e.cap);

    if (U < EPS) return 0.0; // grafo senza archi con capacità significativa

    // Delta iniziale: 2^floor(log2(U)) — equivalente al loop intero ma per double
    double delta = std::pow(2.0, std::floor(std::log2(U)));

    double total = 0.0;
    while (delta >= EPS) {
        // Fase: aumenta finché esistono cammini nel delta-grafo residuo
        while (true) {
            auto path = _bfs_dbl(s, t, delta);
            if (!path.has_value()) break;
            total += _augment_dbl(s, t, *path);
        }
        delta /= 2.0; // prossima fase con soglia dimezzata
    }
    return total;
}