#pragma once

/*
 * ============================================================================
 * Push-Relabel Max Flow  —  Goldberg & Tarjan (1988)
 * ============================================================================
 *
 * Algoritmo per il calcolo del flusso massimo in una rete orientata e
 * capacitata. Implementa tre ottimizzazioni classiche:
 *
 *   1. FIFO node selection
 *      I nodi attivi (excess > 0) vengono gestiti in una coda FIFO.
 *      Garantisce complessità O(V³) nel caso peggiore.
 *
 *   2. Gap heuristic
 *      Se un livello di altezza k diventa vuoto (0 < k < n), tutti i nodi
 *      con altezza h ∈ (k, n) non potranno mai raggiungere il sink: vengono
 *      elevati a n+1, eliminando costosi relabel inutili.
 *
 *   3. Current-arc (advance pointer)
 *      Ogni nodo mantiene un puntatore all'arco corrente nella sua lista di
 *      adiacenza. Gli archi già esaminati vengono saltati in O(1) ammortizzato.
 *
 * Complessità teorica  : O(V³)         — FIFO senza gap heuristic
 * Complessità pratica  : O(V² √E)      — con gap heuristic (bound empirico)
 *
 * Riferimento
 * -----------
 *   Goldberg, A. V., & Tarjan, R. E. (1988).
 *   "A new approach to the maximum-flow problem."
 *   Journal of the ACM, 35(4), 921–940.
 *   https://doi.org/10.1145/48014.61051
 *
 * Utilizzo
 * --------
 *   // I nodi sono identificati da interi (come nel formato DIMACS).
 *   std::unordered_map<std::pair<int,int>, long long, PairHash> graph = {
 *       {{1, 2}, 10}, {{1, 3}, 8}, {{2, 4}, 5}, ...
 *   };
 *   PushRelabel pr(graph);
 *   long long flow = pr.max_flow(1, 4);   // flusso massimo da 1 a 4
 * ============================================================================
 */

#include <deque>
#include <unordered_map>
#include <vector>
#include <limits>
#include <stdexcept>


// ============================================================================
// ResidualEdge — un arco nel grafo residuo
// ============================================================================

/**
 * Rappresenta un arco orientato nel grafo residuo.
 *
 * Per ogni arco originale u → v con capacità c vengono creati due archi:
 *   - forward  u → v  con cap = c   (arco diretto)
 *   - backward v → u  con cap = 0   (arco inverso, per annullare il flusso)
 *
 * Il campo `rev` è l'indice dell'arco simmetrico nella lista di adiacenza
 * del nodo opposto, consentendo l'aggiornamento della capacità residua in O(1).
 */
struct ResidualEdge {
    int       to;   // nodo di destinazione (indice interno, 0-based)
    int       rev;  // posizione dell'arco inverso in _adj[to]
    long long cap;  // capacità residua corrente (≥ 0)
};


// ============================================================================
// PairHash — hasher per std::pair<int,int>
// ============================================================================

/**
 * Funzione hash per coppie di interi, necessaria per usare
 * std::unordered_map<std::pair<int,int>, ...>.
 *
 * Combina i due hash con un golden-ratio mix per ridurre le collisioni
 * rispetto al semplice XOR (che produce 0 per coppie simmetriche).
 */
struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const noexcept {
        std::size_t h1 = std::hash<int>{}(p.first);
        std::size_t h2 = std::hash<int>{}(p.second);
        // Mix di Fibonacci / Knuth: moltiplica per la costante golden-ratio
        return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};


// ============================================================================
// PushRelabel — classe principale
// ============================================================================

class PushRelabel {
public:

    /**
     * Costruisce il grafo residuo a partire dalla mappa degli archi.
     *
     * @param graph  Mappa (u, v) → capacità.
     *               u e v sono label intere arbitrarie (stile DIMACS).
     *               Archi paralleli non sono supportati: ogni coppia (u,v)
     *               deve apparire al più una volta.
     *
     * Il costruttore:
     *   1. Assegna a ogni label un indice interno contiguo [0, n).
     *   2. Per ogni arco originale crea un arco forward e uno backward.
     */
    explicit PushRelabel(
        const std::unordered_map<std::pair<int,int>, long long, PairHash>& graph);

    /**
     * Calcola il flusso massimo tra source e sink.
     *
     * @param source  Label del nodo sorgente.
     * @param sink    Label del nodo pozzo.
     * @return        Valore del flusso massimo (≥ 0).
     *
     * @throws std::out_of_range  Se source o sink non sono presenti nel grafo.
     *
     * Nota: la chiamata modifica le capacità residue interne (_adj).
     *       Per eseguire più query sullo stesso grafo occorre ricostruire
     *       l'oggetto oppure salvare e ripristinare _adj.
     */
    long long max_flow(int source, int sink);

private:

    // ── Dati del grafo ────────────────────────────────────────────────────────

    int _n;                                        // numero di nodi
    std::vector<std::vector<ResidualEdge>> _adj;   // _adj[u]: lista archi uscenti da u

    // Mappatura bidirezionale label ↔ indice interno
    std::unordered_map<int, int> _index;   // label  → indice [0, n)
    std::vector<int>             _label;   // indice → label  (utile per debug)

    // ── Metodi privati ────────────────────────────────────────────────────────

    /**
     * Calcola le altezze iniziali con una BFS inversa dal sink.
     *
     * Percorre il grafo residuo al contrario (da t verso s): la distanza
     * minima di ogni nodo dal sink nel grafo residuo fornisce una stima
     * delle altezze ammissibili, che permette all'algoritmo di iniziare
     * con relabel già ottimali anziché dover risalire da 0.
     *
     * @param t  Indice interno del sink.
     * @return   Vettore h di dimensione n; h[v] = distanza di v da t,
     *           oppure 2*n se v è irraggiungibile dal sink.
     */
    std::vector<int> _bfs_heights(int t) const;
};