#pragma once

/**
 * @file push_relabel.hpp
 * @brief Implementazione dell'algoritmo Push-Relabel per il flusso massimo.
 *
 * Questo file dichiara la classe PushRelabel, che implementa l'algoritmo
 * di Goldberg & Tarjan (1988) per il calcolo del flusso massimo su grafi
 * orientati con capacità intere o in virgola mobile.
 *
 * ### Modalità supportate
 * La classe supporta due modalità di capacità, selezionate tramite il
 * costruttore:
 *
 * - **Modalità intera** (`long long`): per grafi con capacità intere o
 *   razionali già scalate. Usa aritmetica esatta, senza soglie epsilon.
 *
 * - **Modalità double**: per grafi con capacità irrazionali (es. `sqrt(2)`,
 *   `pi`). Usa una soglia epsilon per i confronti con zero, evitando push
 *   infiniti causati da rumore floating-point.
 *
 * ### Ottimizzazioni implementate
 * Entrambe le modalità incorporano:
 * - **FIFO selection rule**: i nodi attivi vengono gestiti con una coda FIFO.
 * - **Gap heuristic**: se un livello di altezza diventa vuoto, tutti i nodi
 *   sopra di esso vengono sollevati a `n+1`, interrompendo percorsi inutili.
 * - **Current-arc heuristic**: ogni nodo mantiene un puntatore all'arco
 *   corrente, evitando di riesaminare archi già esauriti.
 *
 * ### Complessità
 * L'algoritmo Push-Relabel con FIFO ha complessità O(V³) nel caso peggiore.
 *
 * ### Riferimento bibliografico
 * @see Goldberg, A. V., & Tarjan, R. E. (1988).
 *      *A new approach to the maximum-flow problem.*
 *      Journal of the ACM, 35(4), 921–940.
 *      https://doi.org/10.1145/48014.61051
 *
 * @author  (autore del progetto)
 * @version 1.0
 */

#include <deque>
#include <unordered_map>
#include <variant>
#include <vector>
#include <limits>
#include <stdexcept>

#include "pair_hash.hpp"


// ============================================================================
// ResidualEdge
// ============================================================================

/**
 * @brief Arco nel grafo residuo, templatizzato sul tipo di capacità.
 *
 * Ogni arco originale (u → v, capacità c) genera due istanze di ResidualEdge:
 * - arco diretto:  `{to=v, rev=idx_inv, cap=c}`
 * - arco inverso:  `{to=u, rev=idx_dir, cap=0}`
 *
 * Il campo @p rev permette di aggiornare la capacità dell'arco opposto
 * in tempo O(1) durante le operazioni di push.
 *
 * @tparam Cap  Tipo numerico della capacità: `long long` o `double`.
 */
template<typename Cap>
struct ResidualEdge {
    int to;   ///< Nodo di destinazione (indice interno 0-based).
    int rev;  ///< Posizione dell'arco inverso in `_adj[to]`.
    Cap cap;  ///< Capacità residua corrente dell'arco.
};


// ============================================================================
// PushRelabel
// ============================================================================

/**
 * @brief Implementazione dell'algoritmo Push-Relabel (Goldberg & Tarjan, 1988).
 *
 * Calcola il flusso massimo su un grafo orientato con capacità degli archi
 * di tipo `long long` (modalità intera) oppure `double` (modalità virgola
 * mobile). Il tipo si sceglie tramite il costruttore appropriato.
 *
 * ### Uso tipico (modalità intera)
 * @code
 * PushRelabel::IntGraph g;
 * g[{0, 1}] = 10;
 * g[{0, 2}] = 5;
 * g[{1, 3}] = 8;
 * g[{2, 3}] = 7;
 *
 * PushRelabel pr(g);
 * auto result = pr.max_flow(0, 3);
 * long long flow = std::get<long long>(result); // 13
 * @endcode
 *
 * ### Uso tipico (modalità double)
 * @code
 * PushRelabel::DblGraph g;
 * g[{0, 1}] = std::sqrt(2.0);
 * g[{1, 2}] = 3.14;
 *
 * PushRelabel pr(g);
 * auto result = pr.max_flow(0, 2);
 * double flow = std::get<double>(result);
 * @endcode
 *
 * @note I nodi del grafo sono identificati da interi arbitrari (non
 *       necessariamente contigui); la classe costruisce internamente una
 *       mappatura 0-based.
 *
 * @warning Ogni chiamata a max_flow() modifica il grafo residuo interno.
 *          Per calcoli multipli su grafi diversi, istanziare oggetti separati.
 */
class PushRelabel {
public:

    /**
     * @brief Tipo di grafo per la modalità intera.
     *
     * Mappa `(u, v) → capacità` con capacità di tipo `long long`.
     * Chiavi duplicate non sono supportate: ogni coppia (u, v) deve apparire
     * al più una volta.
     */
    using IntGraph = std::unordered_map<std::pair<int,int>, long long, PairHash>;

    /**
     * @brief Tipo di grafo per la modalità double.
     *
     * Mappa `(u, v) → capacità` con capacità di tipo `double`.
     * Chiavi duplicate non sono supportate: ogni coppia (u, v) deve apparire
     * al più una volta.
     */
    using DblGraph = std::unordered_map<std::pair<int,int>, double, PairHash>;


    // -------------------------------------------------------------------------
    // Costruttori
    // -------------------------------------------------------------------------

    /**
     * @brief Costruisce un oggetto PushRelabel in modalità intera.
     *
     * Costruisce il grafo residuo interno a partire da @p graph.
     * Per ogni arco `(u, v, cap)` vengono aggiunti:
     * - un arco diretto con capacità @p cap;
     * - un arco inverso con capacità 0 (per il flusso di ritorno).
     *
     * Tutti i nodi presenti come estremi degli archi vengono registrati
     * automaticamente; non è necessario dichiararli in anticipo.
     *
     * @param graph  Mappa degli archi con le rispettive capacità intere.
     *
     * @post `max_flow()` restituirà un `std::variant` con `long long`.
     */
    explicit PushRelabel(const IntGraph& graph);

    /**
     * @brief Costruisce un oggetto PushRelabel in modalità double.
     *
     * Identico al costruttore intero, ma le capacità sono di tipo `double`.
     * I confronti con zero vengono effettuati con la soglia PushRelabel::EPS
     * per evitare push infiniti dovuti a errori di arrotondamento.
     *
     * @param graph  Mappa degli archi con le rispettive capacità floating-point.
     *
     * @post `max_flow()` restituirà un `std::variant` con `double`.
     */
    explicit PushRelabel(const DblGraph& graph);


    // -------------------------------------------------------------------------
    // Interfaccia pubblica
    // -------------------------------------------------------------------------

    /**
     * @brief Calcola il flusso massimo da @p source a @p sink.
     *
     * Esegue l'algoritmo Push-Relabel con FIFO, gap heuristic e current-arc
     * sul grafo residuo costruito dal costruttore. Il grafo residuo viene
     * modificato in-place: le capacità riflettono il flusso inviato dopo
     * il ritorno di questa funzione.
     *
     * Il tipo del valore restituito dipende dal costruttore usato:
     * | Costruttore | Tipo nel variant    | Come accedere              |
     * |-------------|---------------------|----------------------------|
     * | IntGraph    | `long long`         | `std::get<long long>(res)` |
     * | DblGraph    | `double`            | `std::get<double>(res)`    |
     *
     * @param source  Identificatore del nodo sorgente (deve essere nel grafo).
     * @param sink    Identificatore del nodo pozzo   (deve essere nel grafo).
     *
     * @return Valore del flusso massimo come `std::variant<long long, double>`.
     *
     * @throws std::out_of_range  Se @p source o @p sink non sono presenti
     *                            nel grafo.
     *
     * @note Se `source == sink`, il flusso restituito è 0 (senza eseguire
     *       l'algoritmo).
     */
    std::variant<long long, double> max_flow(int source, int sink);


private:

    // -------------------------------------------------------------------------
    // Stato interno
    // -------------------------------------------------------------------------

    int _n; ///< Numero di nodi nel grafo (dimensione degli array interni).

    /**
     * @brief Mappa da identificatore esterno del nodo a indice interno 0-based.
     *
     * Permette di supportare identificatori di nodi arbitrari (non contigui).
     */
    std::unordered_map<int, int> _index;

    /**
     * @brief Mappa inversa: da indice interno a identificatore esterno.
     *
     * `_label[i]` restituisce l'identificatore originale del nodo con indice
     * interno `i`.
     */
    std::vector<int> _label;

    /**
     * @brief Lista di adiacenza residua per la modalità intera.
     *
     * `_adj_int[u]` contiene tutti gli archi residui uscenti dal nodo `u`
     * (sia diretti che inversi). Popolato solo dal costruttore IntGraph.
     */
    std::vector<std::vector<ResidualEdge<long long>>> _adj_int;

    /**
     * @brief Lista di adiacenza residua per la modalità double.
     *
     * `_adj_dbl[u]` contiene tutti gli archi residui uscenti dal nodo `u`
     * (sia diretti che inversi). Popolato solo dal costruttore DblGraph.
     */
    std::vector<std::vector<ResidualEdge<double>>> _adj_dbl;

    bool _is_double; ///< `true` se il grafo usa capacità `double`, `false` per `long long`.

    /**
     * @brief Soglia epsilon per la modalità double.
     *
     * Un arco residuo con capacità < EPS è considerato saturo.
     * Un nodo con eccesso < EPS è considerato inattivo.
     * Evita push infiniti causati da errori di arrotondamento floating-point.
     */
    static constexpr double EPS = 1e-9;


    // -------------------------------------------------------------------------
    // Metodi privati
    // -------------------------------------------------------------------------

    /**
     * @brief Calcola le altezze iniziali via BFS inversa (modalità intera).
     *
     * Percorre il grafo residuo al contrario (dagli archi con capacità > 0)
     * a partire dal nodo pozzo @p t, assegnando a ogni nodo la distanza BFS
     * da @p t nel grafo residuo. I nodi non raggiungibili ricevono altezza
     * `2 * _n` (valore sentinella).
     *
     * Questa inizializzazione fornisce altezze ammissibili ottimali e riduce
     * significativamente il numero di operazioni di relabel.
     *
     * @param t  Indice interno del nodo pozzo.
     * @return   Vettore di altezze di dimensione `_n`.
     */
    std::vector<int> _bfs_heights_int(int t) const;

    /**
     * @brief Calcola le altezze iniziali via BFS inversa (modalità double).
     *
     * Identico a _bfs_heights_int(), ma considera saturo un arco con
     * capacità ≤ EPS (anziché capacità == 0).
     *
     * @param t  Indice interno del nodo pozzo.
     * @return   Vettore di altezze di dimensione `_n`.
     */
    std::vector<int> _bfs_heights_dbl(int t) const;

    /**
     * @brief Esegue l'algoritmo Push-Relabel su capacità `long long`.
     *
     * Implementa il ciclo principale con:
     * - **Pre-saturazione** degli archi uscenti dalla sorgente @p s.
     * - **Coda FIFO** dei nodi attivi (eccesso > 0).
     * - **Push**: invia flusso lungo un arco ammissibile (h[u] == h[v] + 1).
     * - **Relabel**: solleva l'altezza di @p u quando nessun arco ammissibile
     *   è disponibile.
     * - **Gap heuristic**: se un livello di altezza si svuota, i nodi sopra
     *   di esso vengono portati a `n+1` per interrompere percorsi inutili.
     *
     * @param s  Indice interno della sorgente.
     * @param t  Indice interno del pozzo.
     * @return   Valore del flusso massimo come `long long`.
     */
    long long _max_flow_int(int s, int t);

    /**
     * @brief Esegue l'algoritmo Push-Relabel su capacità `double`.
     *
     * Logicamente identico a _max_flow_int(), con le seguenti differenze:
     * - I confronti `cap > 0` diventano `cap > EPS`.
     * - I confronti `excess > 0` diventano `excess > EPS`.
     * - `std::min` opera su `double` anziché `long long`.
     *
     * @param s  Indice interno della sorgente.
     * @param t  Indice interno del pozzo.
     * @return   Valore del flusso massimo come `double`.
     */
    double _max_flow_dbl(int s, int t);
};