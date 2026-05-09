/**
 * @file push_relabel.cpp
 * @brief Implementazione dei metodi di PushRelabel.
 *
 * Contiene i costruttori, il dispatcher max_flow() e le implementazioni
 * private dell'algoritmo Push-Relabel per le due modalità di capacità:
 * - `_max_flow_int` per capacità `long long`
 * - `_max_flow_dbl` per capacità `double`
 *
 * Le due implementazioni sono intenzionalmente mantenute separate (invece
 * di usare un template comune) per rendere esplicita la differenza nei
 * confronti numerici: esatti per interi, con soglia EPS per double.
 *
 * @see push_relabel.hpp per la documentazione dell'interfaccia pubblica.
 */

#include "push_relabel.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <stdexcept>
#include <vector>


// ============================================================================
// Costruttore — modalità intera
// ============================================================================

/**
 * @details
 * Il costruttore esegue tre passi:
 *
 * 1. **Raccolta dei nodi**: scorre tutti gli archi di @p graph e registra
 *    ogni estremo in `_index` (se non già presente), assegnandogli un
 *    indice interno progressivo 0-based. L'ordine di visita dipende
 *    dall'iterazione sulla `unordered_map`, quindi non è deterministico,
 *    ma questo non influisce sulla correttezza dell'algoritmo.
 *
 * 2. **Allocazione della lista di adiacenza**: ridimensiona `_adj_int` a
 *    `_n` liste vuote.
 *
 * 3. **Inserimento degli archi residui**: per ogni arco `(u → v, cap)`:
 *    - aggiunge a `_adj_int[u]` l'arco diretto con capacità @p cap;
 *    - aggiunge a `_adj_int[v]` l'arco inverso con capacità 0.
 *    I campi `rev` dei due archi si puntano a vicenda, consentendo
 *    aggiornamenti O(1) durante le operazioni di push.
 */
PushRelabel::PushRelabel(const IntGraph& graph)
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
        int eu = static_cast<int>(_adj_int[u].size()); // posizione del nuovo arco in adj[u]
        int ev = static_cast<int>(_adj_int[v].size()); // posizione del nuovo arco in adj[v]
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
 * la capacità degli archi inversi a `0.0` (double) anziché `0LL`.
 * Il flag `_is_double` viene impostato a `true` per indirizzare le chiamate
 * successive a `_max_flow_dbl` tramite il dispatcher max_flow().
 */
PushRelabel::PushRelabel(const DblGraph& graph)
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
 *
 * Il risultato è incapsulato in un `std::variant<long long, double>`
 * per unificare l'interfaccia pubblica indipendentemente dalla modalità.
 */
std::variant<long long, double> PushRelabel::max_flow(int source, int sink) {
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
// _bfs_heights_int — BFS inversa per modalità intera
// ============================================================================

/**
 * @details
 * La BFS inversa parte dal nodo pozzo @p t e risale verso la sorgente
 * attraverso gli archi con capacità residua > 0 (nella direzione opposta).
 *
 * **Perché la BFS inversa?**
 * L'altezza `h[v]` rappresenta una stima della distanza BFS da @p v a @p t
 * nel grafo residuo. Partire da @p t e propagare all'indietro è equivalente
 * ma richiede un'unica passata O(V + E).
 *
 * **Costruzione del grafo inverso**:
 * Per ogni arco `(u → e.to, cap)` con `cap > 0`, si aggiunge `u` alla
 * lista di adiacenza inversa di `e.to`. Questo rappresenta "da @p t posso
 * tornare a @p u attraverso @p e.to".
 *
 * I nodi non raggiungibili da @p t ricevono altezza `2 * _n`, che funge
 * da sentinella: durante l'algoritmo principale, questi nodi verranno
 * elevati a `n+1` per escluderli dai percorsi attivi.
 */
std::vector<int> PushRelabel::_bfs_heights_int(int t) const {
    const int n = _n;
    std::vector<int> h(n, 2 * n); // altezza sentinella per nodi non raggiunti
    h[t] = 0;

    // Costruzione del grafo inverso: radj[v] = nodi u tali che esiste (u→v) con cap > 0
    std::vector<std::vector<int>> radj(n);
    for (int u = 0; u < n; ++u)
        for (auto& e : _adj_int[u])
            if (e.cap > 0)
                radj[e.to].push_back(u);

    // BFS standard a partire dal pozzo
    std::deque<int> queue;
    queue.push_back(t);
    while (!queue.empty()) {
        int v = queue.front(); queue.pop_front();
        for (int u : radj[v]) {
            if (h[u] == 2 * n) { // nodo non ancora visitato
                h[u] = h[v] + 1;
                queue.push_back(u);
            }
        }
    }
    return h;
}


// ============================================================================
// _bfs_heights_dbl — BFS inversa per modalità double
// ============================================================================

/**
 * @details
 * Identico a _bfs_heights_int(), con l'unica differenza che un arco è
 * considerato presente nel grafo residuo se la sua capacità supera la
 * soglia `EPS` (anziché essere strettamente positiva).
 *
 * Questo evita di includere archi la cui capacità residua è puramente
 * artefatto di errori di arrotondamento floating-point.
 */
std::vector<int> PushRelabel::_bfs_heights_dbl(int t) const {
    const int n = _n;
    std::vector<int> h(n, 2 * n);
    h[t] = 0;

    // Grafo inverso: considera solo archi con capacità > EPS
    std::vector<std::vector<int>> radj(n);
    for (int u = 0; u < n; ++u)
        for (auto& e : _adj_dbl[u])
            if (e.cap > EPS)
                radj[e.to].push_back(u);

    std::deque<int> queue;
    queue.push_back(t);
    while (!queue.empty()) {
        int v = queue.front(); queue.pop_front();
        for (int u : radj[v]) {
            if (h[u] == 2 * n) { h[u] = h[v] + 1; queue.push_back(u); }
        }
    }
    return h;
}


// ============================================================================
// _max_flow_int — algoritmo Push-Relabel su long long
// ============================================================================

/**
 * @details
 * ### Struttura dell'algoritmo
 *
 * **Inizializzazione**:
 * - Le altezze vengono calcolate con _bfs_heights_int() e poi corrette:
 *   la sorgente @p s riceve altezza `n` (regola standard), i nodi non
 *   raggiungibili dal pozzo vengono portati a `n+1`.
 * - Il vettore `cnt[h]` conta quanti nodi hanno altezza `h` (usato dalla
 *   gap heuristic).
 * - **Pre-saturazione**: tutti gli archi uscenti da @p s vengono saturati
 *   immediatamente; i nodi destinatari con eccesso > 0 vengono inseriti
 *   nella coda FIFO.
 *
 * **Ciclo principale** (finché la coda non è vuota):
 *
 * Per ogni nodo attivo `u` estratto dalla coda:
 *
 * - **PUSH** (se l'arco corrente `(u, cur[u])` è ammissibile):
 *   Un arco `(u → v)` è ammissibile se `cap > 0` e `h[u] == h[v] + 1`.
 *   Si invia `delta = min(excess[u], cap)` unità di flusso:
 *   `cap[u→v] -= delta`, `cap[v→u] += delta`, aggiornando gli eccessi.
 *   Se @p v diventa attivo (e non è già in coda), viene accodato.
 *
 * - **RELABEL** (se nessun arco ammissibile è disponibile, ovvero
 *   `cur[u]` ha raggiunto la fine della lista):
 *   La nuova altezza di @p u è `min_h + 1`, dove `min_h` è l'altezza
 *   minima tra i vicini con capacità residua > 0.
 *   Dopo il relabel, `cur[u]` viene azzerato per riesaminare tutti gli archi.
 *
 * - **Gap heuristic**: se dopo un relabel il vecchio livello `old_h`
 *   diventa vuoto (`cnt[old_h] == 0`) e `0 < old_h < n`, tutti i nodi
 *   con altezza `old_h < h[v] < n` vengono portati a `n+1`. Questi nodi
 *   non possono più raggiungere il pozzo attraverso percorsi ammissibili,
 *   quindi elevarli evita relabel inutili.
 *
 * **Terminazione**: il flusso massimo è `excess[t]` (eccesso accumulato
 * al pozzo al termine dell'algoritmo).
 *
 * @note La current-arc heuristic è implementata tramite il vettore `cur`:
 *       `cur[u]` è l'indice del prossimo arco da esaminare per il nodo `u`.
 *       Viene azzerato solo dopo un relabel, evitando di riesaminare archi
 *       già risultati non ammissibili nell'iterazione corrente.
 */
long long PushRelabel::_max_flow_int(int s, int t) {
    const int n = _n;
    auto& adj   = _adj_int;

    // --- Inizializzazione delle altezze ---
    std::vector<int> h = _bfs_heights_int(t);
    h[s] = n; // la sorgente ha altezza n per regola standard
    // Nodi non raggiungibili dal pozzo: portati a n+1 (fuori dall'intervallo attivo)
    for (int v = 0; v < n; ++v)
        if (v != s && h[v] >= n) h[v] = n + 1;

    // Contatore per la gap heuristic: cnt[k] = numero di nodi con altezza k
    std::vector<int> cnt(2 * n + 2, 0);
    for (int v = 0; v < n; ++v) cnt[h[v]]++;

    std::vector<long long> excess(n, 0LL); // eccesso per ogni nodo
    std::vector<int>       cur(n, 0);      // current-arc: indice arco corrente per ogni nodo

    // --- Pre-saturazione degli archi uscenti dalla sorgente ---
    for (auto& edge : adj[s]) {
        if (edge.cap > 0) {
            long long cap = edge.cap;
            edge.cap = 0;                         // satura l'arco diretto
            adj[edge.to][edge.rev].cap += cap;    // aumenta l'arco inverso
            excess[edge.to] += cap;
            excess[s]       -= cap;
        }
    }

    // Inserimento in coda dei nodi attivi (eccetto s e t)
    std::deque<int>   queue;
    std::vector<bool> in_queue(n, false);
    for (int v = 0; v < n; ++v)
        if (v != s && v != t && excess[v] > 0) { queue.push_back(v); in_queue[v] = true; }

    // --- Ciclo principale ---
    while (!queue.empty()) {
        int u = queue.front(); queue.pop_front(); in_queue[u] = false;

        while (excess[u] > 0) {
            if (cur[u] == static_cast<int>(adj[u].size())) {
                // ---- RELABEL ----
                int old_h = h[u];
                cnt[old_h]--;

                // Gap heuristic: se il livello old_h si è svuotato,
                // tutti i nodi con altezza compresa tra old_h e n sono
                // disconnessi dal pozzo → li portiamo a n+1.
                if (cnt[old_h] == 0 && old_h > 0 && old_h < n) {
                    for (int v = 0; v < n; ++v) {
                        if (v != s && h[v] > old_h && h[v] < n) {
                            cnt[h[v]]--;
                            h[v] = n + 1;
                            cnt[h[v]]++;
                            cur[v] = 0; // reimposta current-arc
                        }
                    }
                }

                // Nuova altezza: minima tra i vicini raggiungibili + 1
                int min_h = 2 * n;
                for (auto& edge : adj[u])
                    if (edge.cap > 0)
                        min_h = std::min(min_h, h[edge.to]);

                h[u] = min_h + 1;
                cnt[h[u]]++;
                cur[u] = 0; // reimposta current-arc dopo il relabel
                if (h[u] > 2 * n) break; // u non può più scaricare flusso

            } else {
                // ---- PUSH ----
                ResidualEdge<long long>& edge = adj[u][cur[u]];
                int       v   = edge.to;
                long long res = edge.cap;

                if (res > 0 && h[u] == h[v] + 1) {
                    // Arco ammissibile: invia min(excess[u], res) unità
                    long long delta = std::min(excess[u], res);
                    edge.cap                -= delta;
                    adj[v][edge.rev].cap    += delta;
                    excess[u]               -= delta;
                    excess[v]               += delta;
                    // Accoda v se diventa attivo
                    if (v != s && v != t && !in_queue[v] && excess[v] > 0) {
                        queue.push_back(v); in_queue[v] = true;
                    }
                } else {
                    cur[u]++; // arco non ammissibile: avanza il puntatore corrente
                }
            }
        }
    }

    return excess[t]; // il flusso massimo è l'eccesso accumulato al pozzo
}


// ============================================================================
// _max_flow_dbl — algoritmo Push-Relabel su double
// ============================================================================

/**
 * @details
 * Identico a _max_flow_int() nella struttura e nella logica, con le
 * seguenti differenze dovute all'aritmetica floating-point:
 *
 * | Aspetto                  | Modalità intera     | Modalità double       |
 * |--------------------------|---------------------|-----------------------|
 * | Test capacità residua    | `cap > 0`           | `cap > EPS`           |
 * | Test eccesso attivo      | `excess > 0`        | `excess > EPS`        |
 * | Tipo di `delta`          | `long long`         | `double`              |
 *
 * L'uso di EPS previene situazioni in cui errori di arrotondamento
 * producono capacità residue di ordine 1e-15 (essenzialmente zero) che
 * verrebbero erroneamente trattate come positive, causando push infiniti
 * o relabel non terminanti.
 *
 * @note Il risultato potrebbe differire di O(n · EPS) dal valore esatto
 *       a causa dell'accumulo di errori floating-point nelle operazioni di
 *       push; per la maggior parte delle applicazioni pratiche questo
 *       errore è trascurabile.
 */
double PushRelabel::_max_flow_dbl(int s, int t) {
    const int n = _n;
    auto& adj   = _adj_dbl;

    // --- Inizializzazione delle altezze ---
    std::vector<int> h = _bfs_heights_dbl(t);
    h[s] = n;
    for (int v = 0; v < n; ++v)
        if (v != s && h[v] >= n) h[v] = n + 1;

    std::vector<int> cnt(2 * n + 2, 0);
    for (int v = 0; v < n; ++v) cnt[h[v]]++;

    std::vector<double> excess(n, 0.0);
    std::vector<int>    cur(n, 0);

    // --- Pre-saturazione degli archi uscenti dalla sorgente ---
    for (auto& edge : adj[s]) {
        if (edge.cap > EPS) {
            double cap = edge.cap;
            edge.cap = 0.0;
            adj[edge.to][edge.rev].cap += cap;
            excess[edge.to] += cap;
            excess[s]       -= cap;
        }
    }

    std::deque<int>   queue;
    std::vector<bool> in_queue(n, false);
    for (int v = 0; v < n; ++v)
        if (v != s && v != t && excess[v] > EPS) { queue.push_back(v); in_queue[v] = true; }

    // --- Ciclo principale ---
    while (!queue.empty()) {
        int u = queue.front(); queue.pop_front(); in_queue[u] = false;

        while (excess[u] > EPS) {
            if (cur[u] == static_cast<int>(adj[u].size())) {
                // ---- RELABEL ----
                int old_h = h[u];
                cnt[old_h]--;

                // Gap heuristic (identica alla versione intera)
                if (cnt[old_h] == 0 && old_h > 0 && old_h < n) {
                    for (int v = 0; v < n; ++v) {
                        if (v != s && h[v] > old_h && h[v] < n) {
                            cnt[h[v]]--;
                            h[v] = n + 1;
                            cnt[h[v]]++;
                            cur[v] = 0;
                        }
                    }
                }

                // Nuova altezza: archi con cap > EPS considerati residui
                int min_h = 2 * n;
                for (auto& edge : adj[u])
                    if (edge.cap > EPS)
                        min_h = std::min(min_h, h[edge.to]);

                h[u] = min_h + 1;
                cnt[h[u]]++;
                cur[u] = 0;
                if (h[u] > 2 * n) break;

            } else {
                // ---- PUSH ----
                ResidualEdge<double>& edge = adj[u][cur[u]];
                int    v   = edge.to;
                double res = edge.cap;

                if (res > EPS && h[u] == h[v] + 1) {
                    // Arco ammissibile: invia min(excess[u], res) unità
                    double delta = std::min(excess[u], res);
                    edge.cap                -= delta;
                    adj[v][edge.rev].cap    += delta;
                    excess[u]               -= delta;
                    excess[v]               += delta;
                    if (v != s && v != t && !in_queue[v] && excess[v] > EPS) {
                        queue.push_back(v); in_queue[v] = true;
                    }
                } else {
                    cur[u]++; // arco non ammissibile: avanza il puntatore corrente
                }
            }
        }
    }

    return excess[t];
}