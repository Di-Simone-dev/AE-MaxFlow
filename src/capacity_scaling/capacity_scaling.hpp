#pragma once

/**
 * @file capacity_scaling.hpp
 * @brief Dichiarazione della classe CapacityScaling per il flusso massimo.
 *
 * Implementa l'algoritmo di Capacity Scaling di Gabow (1985) per il calcolo
 * del flusso massimo su grafi orientati con capacità intere o in virgola mobile.
 *
 * ### Idea dell'algoritmo
 * Anziché cercare qualsiasi cammino aumentante, l'algoritmo lavora per fasi
 * su un parametro `delta` (inizialmente pari alla più grande potenza di 2
 * minore o uguale alla capacità massima `U`). In ogni fase si cercano e
 * aumentano solo i cammini nel **delta-grafo residuo**, ovvero il sottografo
 * degli archi con capacità residua ≥ `delta`. Quando non esistono più tali
 * cammini, `delta` viene dimezzato. L'algoritmo termina quando `delta < 1`
 * (interi) o `delta < EPS` (double).
 *
 * ### Complessità
 * - **O(m² log U)** nel caso peggiore, dove `m` è il numero di archi e
 *   `U` è la capacità massima degli archi (Gabow, 1985).
 * - In pratica spesso più veloce di Ford-Fulkerson standard grazie alla
 *   riduzione degli aumenti di piccola entità nelle fasi iniziali.
 *
 * ### Modalità supportate
 * | Costruttore | Tipo capacità | Tipo restituito da max_flow() |
 * |-------------|---------------|-------------------------------|
 * | IntGraph    | `long long`   | `std::get<long long>(result)` |
 * | DblGraph    | `double`      | `std::get<double>(result)`    |
 *
 * @see capacity_scaling.cpp per l'implementazione.
 * @see Gabow, H. N. (1985). *Scaling algorithms for network problems.*
 *      Journal of Computer and System Sciences, 31(2), 148–168.
 *      https://doi.org/10.1016/0022-0000(85)90039-X
 *
 * @author  (autore del progetto)
 * @version 1.0
 */

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <deque>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <vector>
#include <cstdint>

#include "pair_hash.hpp"


// ============================================================================
// CSEdge
// ============================================================================

/**
 * @brief Arco nel grafo residuo di CapacityScaling, templatizzato sul tipo di capacità.
 *
 * Struttura analoga a `ResidualEdge` di PushRelabel: ogni arco originale
 * `(u → v, cap)` genera un arco diretto e uno inverso collegati tramite @p rev.
 *
 * @tparam Cap  Tipo numerico della capacità: `long long` o `double`.
 */
template<typename Cap>
struct CSEdge {
    int to;   ///< Nodo di destinazione (indice interno 0-based).
    int rev;  ///< Posizione dell'arco inverso in `_adj[to]`.
    Cap cap;  ///< Capacità residua corrente dell'arco.
};


// ============================================================================
// CapacityScaling
// ============================================================================

/**
 * @brief Implementazione dell'algoritmo Capacity Scaling (Gabow, 1985).
 *
 * Calcola il flusso massimo su grafi orientati con capacità `long long`
 * (modalità intera) o `double` (modalità virgola mobile). La modalità
 * si sceglie tramite il costruttore appropriato.
 *
 * ### Uso tipico (modalità intera)
 * @code
 * CapacityScaling::IntGraph g;
 * g[{0, 1}] = 10;
 * g[{0, 2}] = 5;
 * g[{1, 3}] = 8;
 * g[{2, 3}] = 7;
 *
 * CapacityScaling cs(g);
 * auto result = cs.max_flow(0, 3);
 * long long flow = std::get<long long>(result); // 13
 * @endcode
 *
 * ### Uso tipico (modalità double)
 * @code
 * CapacityScaling::DblGraph g;
 * g[{0, 1}] = std::sqrt(2.0);
 * g[{1, 2}] = 3.14;
 *
 * CapacityScaling cs(g);
 * auto result = cs.max_flow(0, 2);
 * double flow = std::get<double>(result);
 * @endcode
 *
 * @note I nodi sono identificati da interi arbitrari (non necessariamente
 *       contigui); la classe costruisce internamente una mappatura 0-based.
 *
 * @warning Ogni chiamata a max_flow() modifica il grafo residuo interno.
 *          Per calcoli su grafi diversi, istanziare oggetti separati.
 */
class CapacityScaling {
public:

    /**
     * @brief Tipo di grafo per la modalità intera.
     *
     * Mappa `(u, v) → capacità` con capacità di tipo `long long`.
     * Ogni coppia `(u, v)` deve apparire al più una volta.
     */
    using IntGraph = std::unordered_map<std::pair<int,int>, long long, PairHash>;

    /**
     * @brief Tipo di grafo per la modalità double.
     *
     * Mappa `(u, v) → capacità` con capacità di tipo `double`.
     * Ogni coppia `(u, v)` deve apparire al più una volta.
     */
    using DblGraph = std::unordered_map<std::pair<int,int>, double, PairHash>;


    // -------------------------------------------------------------------------
    // Costruttori
    // -------------------------------------------------------------------------

    /**
     * @brief Costruisce un oggetto CapacityScaling in modalità intera.
     *
     * Per ogni arco `(u → v, cap)` vengono aggiunti al grafo residuo:
     * - un arco diretto con capacità @p cap;
     * - un arco inverso con capacità 0.
     *
     * Tutti i nodi sono registrati automaticamente.
     *
     * @param graph  Mappa degli archi con capacità intere.
     * @post `max_flow()` restituirà un `std::variant` con `long long` attivo.
     */
    explicit CapacityScaling(const IntGraph& graph);

    /**
     * @brief Costruisce un oggetto CapacityScaling in modalità double.
     *
     * Identico al costruttore intero ma con capacità `double`. I confronti
     * con zero usano la soglia CapacityScaling::EPS.
     *
     * @param graph  Mappa degli archi con capacità floating-point.
     * @post `max_flow()` restituirà un `std::variant` con `double` attivo.
     */
    explicit CapacityScaling(const DblGraph& graph);


    // -------------------------------------------------------------------------
    // Interfaccia pubblica
    // -------------------------------------------------------------------------

    /**
     * @brief Calcola il flusso massimo da @p source a @p sink.
     *
     * Esegue l'algoritmo Capacity Scaling sul grafo residuo costruito
     * dal costruttore. Il grafo residuo viene modificato in-place.
     *
     * Il tipo del valore restituito dipende dal costruttore usato:
     * | Costruttore | Tipo nel variant    | Come accedere              |
     * |-------------|---------------------|----------------------------|
     * | IntGraph    | `long long`         | `std::get<long long>(res)` |
     * | DblGraph    | `double`            | `std::get<double>(res)`    |
     *
     * @param source  Identificatore del nodo sorgente.
     * @param sink    Identificatore del nodo pozzo.
     * @return        Flusso massimo come `std::variant<long long, double>`.
     *
     * @throws std::out_of_range  Se @p source o @p sink non sono nel grafo.
     * @note Se `source == sink`, restituisce 0 senza eseguire l'algoritmo.
     */
    std::variant<long long, double> max_flow(int source, int sink);


private:

    // -------------------------------------------------------------------------
    // Stato interno
    // -------------------------------------------------------------------------

    int  _n;                  ///< Numero di nodi nel grafo.
    bool _is_double = false;  ///< `true` se il grafo usa capacità `double`.

    /**
     * @brief Soglia epsilon per i confronti in modalità double.
     *
     * Un arco con capacità < EPS è considerato saturo nel delta-grafo residuo.
     * La fase termina quando `delta < EPS`.
     */
    static constexpr double EPS = 1e-9;

    /**
     * @brief Mappa da identificatore esterno del nodo a indice interno 0-based.
     */
    std::unordered_map<int, int> _index;

    /**
     * @brief Mappa inversa: da indice interno a identificatore esterno.
     */
    std::vector<int> _label;

    /**
     * @brief Lista di adiacenza residua per la modalità intera.
     * Popolato solo dal costruttore IntGraph.
     */
    std::vector<std::vector<CSEdge<long long>>> _adj_int;

    /**
     * @brief Lista di adiacenza residua per la modalità double.
     * Popolato solo dal costruttore DblGraph.
     */
    std::vector<std::vector<CSEdge<double>>> _adj_dbl;

    /**
     * @brief Tipo del vettore dei predecessori per la ricostruzione del cammino.
     *
     * `parent[v] = {u, idx}` indica che il predecessore di @p v sul cammino
     * trovato dalla BFS è @p u, e che l'arco usato è `_adj[u][idx]`.
     */
    using Parent = std::vector<std::pair<int,int>>;


    // -------------------------------------------------------------------------
    // Metodi privati
    // -------------------------------------------------------------------------

    /**
     * @brief BFS nel delta-grafo residuo intero.
     *
     * Cerca un cammino da @p s a @p t nel sottografo degli archi con
     * capacità residua ≥ @p delta (aritmetica esatta su `long long`).
     *
     * @param s      Indice interno della sorgente.
     * @param t      Indice interno del pozzo.
     * @param delta  Soglia di capacità minima per gli archi ammissibili.
     * @return       Vettore dei predecessori se esiste un cammino,
     *               `std::nullopt` altrimenti.
     */
    std::optional<Parent> _bfs_int(int s, int t, long long delta) const;

    /**
     * @brief BFS nel delta-grafo residuo double.
     *
     * Identica a _bfs_int(), ma un arco è ammissibile se
     * `cap ≥ delta - EPS` per compensare gli errori di arrotondamento.
     *
     * @param s      Indice interno della sorgente.
     * @param t      Indice interno del pozzo.
     * @param delta  Soglia di capacità minima (con tolleranza EPS).
     * @return       Vettore dei predecessori se esiste un cammino,
     *               `std::nullopt` altrimenti.
     */
    std::optional<Parent> _bfs_dbl(int s, int t, double delta) const;

    /**
     * @brief Aumenta il flusso lungo il cammino trovato (modalità intera).
     *
     * Calcola il bottleneck (capacità minima sul cammino da @p s a @p t)
     * e aggiorna le capacità residue di tutti gli archi sul cammino.
     *
     * @param s       Indice interno della sorgente.
     * @param t       Indice interno del pozzo.
     * @param parent  Vettore dei predecessori (output di _bfs_int()).
     * @return        Quantità di flusso inviata (bottleneck del cammino).
     */
    long long _augment_int(int s, int t, const Parent& parent);

    /**
     * @brief Aumenta il flusso lungo il cammino trovato (modalità double).
     *
     * Identico a _augment_int() ma opera su `double`.
     * Il bottleneck viene inizializzato a `+∞` (`std::numeric_limits<double>::infinity()`).
     *
     * @param s       Indice interno della sorgente.
     * @param t       Indice interno del pozzo.
     * @param parent  Vettore dei predecessori (output di _bfs_dbl()).
     * @return        Quantità di flusso inviata.
     */
    double _augment_dbl(int s, int t, const Parent& parent);

    /**
     * @brief Esegue l'algoritmo Capacity Scaling su capacità `long long`.
     *
     * Calcola `U` come capacità massima degli archi, imposta `delta` alla
     * più grande potenza di 2 ≤ `U`, poi itera dimezzando `delta` fino a 1.
     * In ogni fase esegue aumenti BFS finché il delta-grafo residuo non ha
     * più cammini aumentanti.
     *
     * @param s  Indice interno della sorgente.
     * @param t  Indice interno del pozzo.
     * @return   Valore del flusso massimo come `long long`.
     */
    long long _max_flow_int(int s, int t);

    /**
     * @brief Esegue l'algoritmo Capacity Scaling su capacità `double`.
     *
     * Identico a _max_flow_int() con le differenze:
     * - `delta` iniziale calcolato come `2^floor(log2(U))`.
     * - Il ciclo termina quando `delta < EPS` (anziché `delta < 1`).
     * - I confronti di capacità usano la tolleranza EPS.
     *
     * @param s  Indice interno della sorgente.
     * @param t  Indice interno del pozzo.
     * @return   Valore del flusso massimo come `double`.
     */
    double _max_flow_dbl(int s, int t);
};