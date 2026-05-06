/*
 * push_relabel.cpp
 * ----------------
 * Implementazione della classe PushRelabel.
 *
 * Vedi push_relabel.hpp per la descrizione dell'algoritmo, la complessità
 * e le istruzioni d'uso.
 */

#include "push_relabel.hpp"

#include <algorithm>   // std::min
#include <deque>
#include <stdexcept>
#include <vector>


// ============================================================================
// Costruttore
// ============================================================================

PushRelabel::PushRelabel(
    const std::unordered_map<std::pair<int,int>, long long, PairHash>& graph)
{
    // ── Fase 1: assegna un indice interno a ogni label distinta ──────────────
    //
    // Scorre tutti gli archi e registra ogni nodo la prima volta che appare
    // (come sorgente o come destinazione). L'ordine di assegnazione dipende
    // dall'iterazione dell'unordered_map (non deterministico), ma l'algoritmo
    // è indipendente dalla numerazione scelta.
    for (auto& [edge, _cap] : graph) {
        for (int node : {edge.first, edge.second}) {
            if (_index.find(node) == _index.end()) {
                _index[node] = static_cast<int>(_label.size());
                _label.push_back(node);
            }
        }
    }

    _n = static_cast<int>(_label.size());

    // ── Fase 2: costruisce il grafo residuo ───────────────────────────────────
    //
    // Per ogni arco originale u → v con capacità c:
    //   - Aggiunge l'arco forward  u → v  con cap = c   (posizione eu in adj[u])
    //   - Aggiunge l'arco backward v → u  con cap = 0   (posizione ev in adj[v])
    //
    // I campi `rev` si incrociano: forward.rev = ev, backward.rev = eu.
    // Questo permette di trovare l'arco simmetrico in O(1) durante il push.
    _adj.resize(_n);

    for (auto& [edge, cap] : graph) {
        int u = _index.at(edge.first);
        int v = _index.at(edge.second);

        int eu = static_cast<int>(_adj[u].size());   // indice del forward  in adj[u]
        int ev = static_cast<int>(_adj[v].size());   // indice del backward in adj[v]

        _adj[u].push_back({v, ev, cap});    // forward:  capacità originale
        _adj[v].push_back({u, eu, 0LL});    // backward: capacità 0 (nessun flusso iniziale)
    }
}


// ============================================================================
// _bfs_heights — BFS inversa per il calcolo delle altezze iniziali
// ============================================================================

std::vector<int> PushRelabel::_bfs_heights(int t) const {
    const int n = _n;

    // h[v] = distanza di v dal sink t nel grafo residuo inverso.
    // Inizializzato a 2*n come sentinella per "nodo non ancora raggiunto".
    std::vector<int> h(n, 2 * n);
    h[t] = 0;

    // Costruisce il grafo inverso (radj):
    //   per ogni arco u → v con cap > 0 nel grafo residuo, aggiunge v → u in radj.
    // Serve a percorrere il grafo "al contrario" partendo da t.
    std::vector<std::vector<int>> radj(n);
    for (int u = 0; u < n; ++u) {
        for (auto& e : _adj[u]) {
            if (e.cap > 0) {
                radj[e.to].push_back(u);
            }
        }
    }

    // BFS standard da t nel grafo inverso: propaga le distanze livello per livello.
    std::deque<int> queue;
    queue.push_back(t);

    while (!queue.empty()) {
        int v = queue.front();
        queue.pop_front();

        for (int u : radj[v]) {
            if (h[u] == 2 * n) {        // nodo non ancora visitato
                h[u] = h[v] + 1;
                queue.push_back(u);
            }
        }
    }

    return h;
}


// ============================================================================
// max_flow — algoritmo principale Push-Relabel con FIFO e gap heuristic
// ============================================================================

long long PushRelabel::max_flow(int source, int sink) {

    // ── Validazione input ────────────────────────────────────────────────────
    auto it_s = _index.find(source);
    auto it_t = _index.find(sink);
    if (it_s == _index.end())
        throw std::out_of_range("source non trovato nel grafo");
    if (it_t == _index.end())
        throw std::out_of_range("sink non trovato nel grafo");

    const int s = it_s->second;
    const int t = it_t->second;

    if (s == t) return 0LL;   // caso degenere

    const int n = _n;
    auto& adj   = _adj;

    // ── Inizializzazione delle altezze ───────────────────────────────────────
    //
    // La BFS inversa fornisce una stima delle distanze reali dal sink.
    // Questa precomputazione evita relabel ridondanti all'avvio e
    // accelera significativamente la convergenza in pratica.
    std::vector<int> h = _bfs_heights(t);
    h[s] = n;   // la sorgente parte sempre a h = n (convenzione Push-Relabel)

    // Nodi irraggiungibili dal sink (h >= n, escluso s) → h = n+1.
    // Questi nodi non potranno mai portare flusso al sink: li isoliamo subito.
    for (int v = 0; v < n; ++v) {
        if (v != s && h[v] >= n) {
            h[v] = n + 1;
        }
    }

    // cnt[k] = numero di nodi con altezza esattamente k.
    // Usato dalla gap heuristic per rilevare livelli vuoti in O(1).
    std::vector<int> cnt(2 * n + 2, 0);
    for (int v = 0; v < n; ++v) {
        cnt[h[v]]++;
    }

    std::vector<long long> excess(n, 0LL);  // eccesso di flusso per ogni nodo
    std::vector<int>       cur(n, 0);       // current-arc pointer per ogni nodo

    // ── Pre-saturazione degli archi uscenti dalla sorgente ───────────────────
    //
    // Invia immediatamente tutto il flusso possibile da s verso i vicini diretti.
    // Questo è equivalente a inizializzare il flusso con il taglio banale
    // {s} vs {V\{s}}, e genera i primi nodi attivi nella coda FIFO.
    for (auto& edge : adj[s]) {
        if (edge.cap > 0) {
            long long cap = edge.cap;
            edge.cap = 0;                         // satura l'arco forward
            adj[edge.to][edge.rev].cap += cap;    // aggiorna l'arco backward
            excess[edge.to] += cap;
            excess[s]       -= cap;
        }
    }

    // ── Coda FIFO dei nodi attivi ─────────────────────────────────────────────
    //
    // Un nodo è "attivo" se ha excess > 0 e non è né s né t.
    // La politica FIFO (anziché per altezza) garantisce O(V³) nel caso peggiore.
    std::deque<int>   queue;
    std::vector<bool> in_queue(n, false);

    for (int v = 0; v < n; ++v) {
        if (v != s && v != t && excess[v] > 0) {
            queue.push_back(v);
            in_queue[v] = true;
        }
    }

    // ── Ciclo principale ──────────────────────────────────────────────────────
    while (!queue.empty()) {
        int u = queue.front();
        queue.pop_front();
        in_queue[u] = false;

        // DISCHARGE: scarica tutto l'eccesso di u con una sequenza di push e relabel.
        // Il ciclo termina quando excess[u] == 0 oppure h[u] > 2*n (nodo isolato).
        while (excess[u] > 0) {

            if (cur[u] == static_cast<int>(adj[u].size())) {
                // ── RELABEL ───────────────────────────────────────────────────
                //
                // Il current-arc pointer ha raggiunto la fine della lista:
                // nessun arco ammissibile è disponibile. Occorre alzare h[u].

                int old_h = h[u];
                cnt[old_h]--;

                // ── GAP HEURISTIC ─────────────────────────────────────────────
                //
                // Se il livello old_h è ora vuoto (cnt = 0) e 0 < old_h < n,
                // nessun nodo potrà mai raggiungere il sink attraverso quel livello.
                // Tutti i nodi con old_h < h[v] < n vengono alzati a n+1
                // (equivalente a marcarli come irraggiungibili), eliminando
                // potenzialmente molti relabel futuri.
                if (cnt[old_h] == 0 && old_h > 0 && old_h < n) {
                    for (int v = 0; v < n; ++v) {
                        if (v != s && h[v] > old_h && h[v] < n) {
                            cnt[h[v]]--;
                            h[v] = n + 1;
                            cnt[h[v]]++;
                            cur[v] = 0;   // reset current-arc (lista sarà riesaminata)
                        }
                    }
                }

                // Nuovo h[u] = 1 + min{h[v] : (u,v) arco ammissibile nel residuo}
                // Questa è la scelta minima che mantiene la proprietà di validità
                // dell'altezza: h[u] ≤ h[v] + 1 per ogni arco (u,v) con cap > 0.
                int min_h = 2 * n;
                for (auto& edge : adj[u]) {
                    if (edge.cap > 0) {
                        min_h = std::min(min_h, h[edge.to]);
                    }
                }

                h[u] = min_h + 1;
                cnt[h[u]]++;
                cur[u] = 0;   // reset: riesamina tutti gli archi con la nuova altezza

                // Se l'altezza supera 2*n, u non raggiungerà mai il sink:
                // interrompe il discharge (excess[u] rimarrà positivo ma inerte).
                if (h[u] > 2 * n) break;

            } else {
                // ── PUSH ──────────────────────────────────────────────────────
                //
                // Tenta di inviare flusso lungo il current-arc (u, v).
                // Un push è ammissibile se:
                //   - cap > 0   (arco saturo nel residuo → impossibile)
                //   - h[u] == h[v] + 1  (arco "in discesa" nella funzione altezza)
                ResidualEdge& edge = adj[u][cur[u]];
                int       v   = edge.to;
                long long res = edge.cap;

                if (res > 0 && h[u] == h[v] + 1) {
                    // Push del bottleneck: min tra eccesso disponibile e capacità residua
                    long long delta = std::min(excess[u], res);

                    edge.cap                -= delta;    // riduce cap forward
                    adj[v][edge.rev].cap    += delta;    // aumenta cap backward
                    excess[u]               -= delta;
                    excess[v]               += delta;

                    // Se v è diventato attivo e non è ancora in coda, aggiungilo
                    if (v != s && v != t && !in_queue[v] && excess[v] > 0) {
                        queue.push_back(v);
                        in_queue[v] = true;
                    }
                } else {
                    // Arco non ammissibile: avanza il pointer al prossimo arco
                    cur[u]++;
                }
            }
        }
    }

    // L'eccesso accumulato nel sink è esattamente il valore del flusso massimo.
    // (Per conservazione del flusso: tutto ciò che è entrato in t non può uscire.)
    return excess[t];
}