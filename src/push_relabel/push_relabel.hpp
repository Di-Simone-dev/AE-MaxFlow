#pragma once

/*
 * Push-Relabel Max Flow  —  Goldberg & Tarjan (1988)
 * ===================================================
 * Implementazione efficiente con:
 *   - FIFO node selection  → O(V³) nel caso peggiore
 *   - Gap heuristic        → drastica riduzione pratica dei relabel
 *   - Current-arc (advance pointer) → ogni push ammissibile trovato in O(1) ammortizzato
 *
 * Complessità : O(V² √E)  con gap heuristic (bound pratico)
 *               O(V³)      senza (bound teorico FIFO)
 *
 * Reference   : Goldberg, A. V., & Tarjan, R. E. (1988).
 *               "A new approach to the maximum-flow problem."
 *               Journal of the ACM, 35(4), 921–940.
 *               https://doi.org/10.1145/48014.61051
 *
 * Interfaccia
 * -----------
 *   // I nodi sono interi (come nel formato DIMACS)
 *   std::map<std::pair<int,int>, long long> graph = {
 *       {{1, 2}, 10}, {{2, 3}, 5}, ...
 *   };
 *   PushRelabel pr(graph);
 *   long long flow = pr.max_flow(source, sink);
 */

#include <deque>
#include <unordered_map>
#include <vector>
#include <limits>
#include <stdexcept>

// ──────────────────────────────────────────────────────────────────────────────
// Struttura interna: un arco nel grafo residuo
// ──────────────────────────────────────────────────────────────────────────────

struct ResidualEdge {
    int  to;        // nodo di destinazione (indice interno)
    int  rev;       // posizione dell'arco inverso in adj[to]
    long long cap;  // capacità residua corrente
};

// ──────────────────────────────────────────────────────────────────────────────
// Hasher per std::pair<int,int> (usato dall'unordered_map del grafo)
// ──────────────────────────────────────────────────────────────────────────────

struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const noexcept {
        // Combina i due hash con un golden-ratio mix (migliore distribuzione di XOR puro)
        std::size_t h1 = std::hash<int>{}(p.first);
        std::size_t h2 = std::hash<int>{}(p.second);
        return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// Classe principale
// ──────────────────────────────────────────────────────────────────────────────

class PushRelabel {
public:
    /*
     * Costruttore
     * -----------
     * graph: mappa (u, v) → capacità, dove u e v sono interi (label DIMACS).
     *        Traduzione 1:1 del costruttore Python: costruisce il grafo residuo
     *        con archi forward (cap originale) e archi reverse (cap = 0).
     */
    explicit PushRelabel(const std::unordered_map<std::pair<int,int>, long long, PairHash>& graph);

    /*
     * max_flow(source, sink)
     * ----------------------
     * Calcola e restituisce il flusso massimo da source a sink.
     * source e sink sono le label originali (interi DIMACS).
     * Lancia std::out_of_range se source o sink non sono nel grafo.
     */
    long long max_flow(int source, int sink);

private:
    int _n;                                   // numero di nodi
    std::vector<std::vector<ResidualEdge>> _adj;  // grafo residuo: _adj[u] = lista archi uscenti

    // Mappa label originale → indice interno (0…n-1)
    std::unordered_map<int, int> _index;
    // Mappa inversa: indice → label (non usata nell'algoritmo, utile per debug)
    std::vector<int> _label;

    /*
     * _bfs_heights(t)
     * ---------------
     * BFS inversa dal sink t: calcola l'altezza iniziale di ogni nodo come
     * distanza dal sink nel grafo residuo inverso.
     * I nodi irraggiungibili ricevono altezza 2*n (sentinella).
     * Corrisponde esattamente a _bfs_heights() del Python.
     */
    std::vector<int> _bfs_heights(int t) const;
};