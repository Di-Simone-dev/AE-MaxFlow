/*
 * push_relabel.cpp
 * ----------------
 * Implementazione di PushRelabel.
 * Traduzione diretta da push_relabel.py — struttura e commenti mantenuti
 * allineati alla versione Python per facilitare il confronto.
 */

#include "push_relabel.hpp"

#include <algorithm>   // std::min
#include <deque>
#include <stdexcept>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// Costruttore
// ──────────────────────────────────────────────────────────────────────────────

PushRelabel::PushRelabel(
    const std::unordered_map<std::pair<int,int>, long long, PairHash>& graph)
{
    // ── mapping label ↔ indice ───────────────────────────────────────────────
    // Itera sugli archi nello stesso ordine del Python (dict.keys()).
    // In C++ unordered_map non garantisce ordine, ma l'algoritmo è
    // indipendente dall'ordine di numerazione dei nodi.
    for (auto& [edge, _cap] : graph) {
        for (int node : {edge.first, edge.second}) {
            if (_index.find(node) == _index.end()) {
                _index[node] = static_cast<int>(_label.size());
                _label.push_back(node);
            }
        }
    }

    _n = static_cast<int>(_label.size());

    // ── grafo residuo ────────────────────────────────────────────────────────
    // _adj[u] = lista di ResidualEdge{to, rev, cap}
    // Per ogni arco originale (u→v, cap):
    //   - arco forward:  adj[u] += {v, pos_in_adj[v], cap}
    //   - arco reverse:  adj[v] += {u, pos_in_adj[u], 0}
    // Il campo rev permette di raggiungere l'arco simmetrico in O(1).
    _adj.resize(_n);

    for (auto& [edge, cap] : graph) {
        int u = _index.at(edge.first);
        int v = _index.at(edge.second);

        int eu = static_cast<int>(_adj[u].size());  // posizione del forward in adj[u]
        int ev = static_cast<int>(_adj[v].size());  // posizione del reverse in adj[v]

        _adj[u].push_back({v, ev, cap});   // forward:  capacità originale
        _adj[v].push_back({u, eu, 0LL});   // reverse:  capacità = 0
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// _bfs_heights — BFS inversa dal sink per precomputare le altezze iniziali
// ──────────────────────────────────────────────────────────────────────────────

std::vector<int> PushRelabel::_bfs_heights(int t) const {
    const int n = _n;

    // Inizializza tutte le altezze a 2*n (sentinella = "non raggiunto")
    std::vector<int> h(n, 2 * n);
    h[t] = 0;

    // Costruisci il grafo inverso: per ogni arco u→v con cap>0, aggiungi v→u
    // (in Python: radj[v].append(u) per ogni arco in _adj[u] con res > 0)
    std::vector<std::vector<int>> radj(n);
    for (int u = 0; u < n; ++u) {
        for (auto& e : _adj[u]) {
            if (e.cap > 0) {
                radj[e.to].push_back(u);
            }
        }
    }

    // BFS standard dalla sorgente t nel grafo inverso
    std::deque<int> queue;
    queue.push_back(t);

    while (!queue.empty()) {
        int v = queue.front();
        queue.pop_front();

        for (int u : radj[v]) {
            if (h[u] == 2 * n) {          // non ancora visitato
                h[u] = h[v] + 1;
                queue.push_back(u);
            }
        }
    }

    return h;
}

// ──────────────────────────────────────────────────────────────────────────────
// max_flow — algoritmo principale Push-Relabel FIFO + gap heuristic
// ──────────────────────────────────────────────────────────────────────────────

long long PushRelabel::max_flow(int source, int sink) {
    // Converti le label originali in indici interni
    auto it_s = _index.find(source);
    auto it_t = _index.find(sink);
    if (it_s == _index.end())
        throw std::out_of_range("source non trovato nel grafo");
    if (it_t == _index.end())
        throw std::out_of_range("sink non trovato nel grafo");

    const int s = it_s->second;
    const int t = it_t->second;

    if (s == t) return 0LL;

    const int n = _n;
    auto& adj   = _adj;

    // ── Precomputazione altezze iniziali ─────────────────────────────────────
    // BFS inversa dal sink per stimare le distanze vere → altezze ammissibili
    std::vector<int> h = _bfs_heights(t);
    h[s] = n;   // la sorgente parte sempre a n (convenzione Push-Relabel)

    // Normalizza: nodi irraggiungibili (h ≥ n, tranne s) → n+1
    for (int v = 0; v < n; ++v) {
        if (v != s && h[v] >= n) {
            h[v] = n + 1;
        }
    }

    // cnt[k] = numero di nodi con altezza k (per gap heuristic)
    std::vector<int> cnt(2 * n + 2, 0);
    for (int v = 0; v < n; ++v) {
        cnt[h[v]]++;
    }

    std::vector<long long> excess(n, 0LL);   // eccesso per ogni nodo
    std::vector<int>       cur(n, 0);         // current-arc pointer

    // ── Pre-saturazione degli archi uscenti da s ──────────────────────────────
    // Invia tutto il flusso possibile da s verso i vicini diretti.
    // In Python: for edge in adj[s]: if cap > 0: satura
    for (auto& edge : adj[s]) {
        if (edge.cap > 0) {
            long long cap = edge.cap;
            edge.cap = 0;
            adj[edge.to][edge.rev].cap += cap;
            excess[edge.to] += cap;
            excess[s]       -= cap;
        }
    }

    // ── Coda FIFO dei nodi attivi ─────────────────────────────────────────────
    // Attivo = excess > 0 e nodo ≠ s, ≠ t
    std::deque<int>       queue;
    std::vector<bool>     in_queue(n, false);

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

        // Discharge: scarica l'eccesso di u tramite push / relabel
        while (excess[u] > 0) {

            if (cur[u] == static_cast<int>(adj[u].size())) {
                // ── RELABEL con gap heuristic ─────────────────────────────────
                int old_h = h[u];
                cnt[old_h]--;

                // GAP: se il livello old_h è rimasto vuoto e 0 < old_h < n,
                // tutti i nodi con old_h < h[v] < n non possono mai raggiungere
                // il sink → li alziamo a n+1 (componente disconnessa).
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

                // Nuovo h[u] = 1 + min altezza tra i vicini raggiungibili
                int min_h = 2 * n;
                for (auto& edge : adj[u]) {
                    if (edge.cap > 0) {
                        min_h = std::min(min_h, h[edge.to]);
                    }
                }

                h[u] = min_h + 1;
                cnt[h[u]]++;
                cur[u] = 0;  // reset current-arc

                // Se l'altezza supera il limite, il nodo non raggiungerà mai t
                if (h[u] > 2 * n) break;

            } else {
                // ── Tenta PUSH lungo il current-arc ──────────────────────────
                ResidualEdge& edge = adj[u][cur[u]];
                int      v   = edge.to;
                long long res = edge.cap;

                if (res > 0 && h[u] == h[v] + 1) {
                    // Arco ammissibile: push del bottleneck tra eccesso e cap residua
                    long long delta = std::min(excess[u], res);

                    edge.cap                -= delta;
                    adj[v][edge.rev].cap    += delta;
                    excess[u]               -= delta;
                    excess[v]               += delta;

                    // Se v non è in coda e ha eccesso, aggiungilo
                    if (v != s && v != t && !in_queue[v] && excess[v] > 0) {
                        queue.push_back(v);
                        in_queue[v] = true;
                    }
                } else {
                    // Arco non ammissibile: avanza il current-arc pointer
                    cur[u]++;
                }
            }
        }
    }

    // L'eccesso del sink alla fine è il valore del flusso massimo
    return excess[t];
}
