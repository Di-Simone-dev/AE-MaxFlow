/**
 * @file howard.hpp
 * @brief Dichiarazione dell'algoritmo di Howard per il Minimum Cycle Ratio (MCR)
 *        su grafi orientati pesati.
 *
 * L'algoritmo di Howard è un metodo iterativo di tipo policy-iteration che
 * trova il ciclo con il minimo rapporto peso/lunghezza in un grafo orientato.
 * In questo contesto è usato come subroutine del loop interior-point di
 * @ref AlmostLinearTime: ad ogni iterazione identifica il ciclo critico lungo
 * cui aggiornare il flusso corrente.
 *
 * ### Schema dell'algoritmo
 *
 * 1. **Costruzione del policy graph** (`_construct_policy_graph`): associa
 *    ad ogni nodo l'arco uscente con gradiente massimo.
 * 2. **Ricerca dei cicli** (`_find_all_cycles`): DFS sul policy graph per
 *    trovare tutti i cicli e calcolare il loro ratio.
 * 3. **Miglioramento della policy** (`_improve_policy`): aggiorna le scelte
 *    locali dei nodi per ridurre il ratio corrente.
 * 4. **Vettore del ciclo** (`_make_cycle_vector`): produce il vettore
 *    indicatore ±1 del ciclo critico sugli archi del grafo.
 *
 * @see howard.cpp
 * @see AlmostLinearTime::max_flow_with_guess
 * @see min_cost_flow_instance.hpp
 */

#pragma once

#include "min_cost_flow_instance.hpp"
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <utility>
#include <vector>

/**
 * @brief Soglia negativa per accettare miglioramenti in `_improve_policy`.
 *
 * Un aggiornamento della policy viene accettato se la nuova distanza è
 * inferiore a `distances[v] + EPS`, ovvero se il miglioramento supera
 * questa soglia di guardia. Il valore negativo evita cicli infiniti dovuti
 * a oscillazioni numeriche attorno allo zero.
 */
static constexpr double EPS = -0.005;

/**
 * @class Howard
 * @brief Implementazione dell'algoritmo di Howard per il Minimum Cycle Ratio.
 *
 * Data un'istanza di min-cost flow con vettori di gradienti e lunghezze sugli
 * archi, trova il ciclo con il minimo rapporto `Σ gradients / Σ lengths`
 * tramite policy-iteration.
 *
 * L'interfaccia pubblica è ridotta a un solo metodo, @ref find_optimum_cycle_ratio,
 * che orchestra l'intera esecuzione. La funzione libera @ref minimum_cycle_ratio
 * è il punto di ingresso consigliato dall'esterno.
 *
 * @note La cache `_edge_cache` usa una `unordered_map` con hash su `int64_t`
 *       per lookup O(1) invece di O(log n) della `std::map` originale.
 *
 * @warning Il costruttore replica intenzionalmente un bug del codice Python
 *          originale (segno invertito del gradiente per il nodo sorgente `u`).
 *          Vedere il commento nel costruttore per i dettagli.
 */
class Howard {
public:

    /**
     * @brief Costruisce l'istanza di Howard e inizializza la policy graph cache.
     *
     * Per ogni arco `(u, w)` popola `_edge_cache` con due entry:
     * - `(u, edge_id) → (w, -grad)` — direzione u→w con gradiente negato.
     * - `(w, edge_id) → (u,  grad)` — direzione w→u con gradiente originale.
     *
     * @warning Il segno negativo su `u` replica fedelmente un bug del codice
     *          Python originale. Rimuoverlo causa divergenza dell'algoritmo.
     *
     * Calcola inoltre il bound iniziale `best_ratio = _compute_bound()`.
     *
     * @param graph     Istanza di min-cost flow (grafo, archi, matrici).
     * @param gradients Vettore dei gradienti di Φ(f) sugli archi.
     * @param lengths   Vettore delle lunghezze sugli archi (denominatore del ratio).
     */
    Howard(const MinCostFlow& graph,
           const Eigen::VectorXd& gradients,
           const Eigen::VectorXd& lengths);

    /**
     * @brief Esegue l'algoritmo di Howard e restituisce il minimum cycle ratio.
     *
     * Itera al massimo 100 volte il ciclo:
     * 1. `_find_all_cycles()` — calcola il ratio minimo sul policy graph corrente.
     * 2. `_improve_policy(ratio)` — aggiorna la policy; se nessun miglioramento
     *    è possibile, termina anticipatamente.
     *
     * Se al termine il ratio supera `bound - 1e-10` oppure nessun ciclo critico
     * è stato trovato, restituisce `(INF, zero_vector)` — segnale che il flusso
     * corrente è già ottimale o numericamente degenere.
     *
     * @return Coppia `(min_ratio, cycle_vector)`:
     *         - `min_ratio`: valore del minimum cycle ratio (negativo se esiste
     *           un ciclo migliorante).
     *         - `cycle_vector`: vettore di dimensione `g.m` con valori in
     *           `{-1, 0, +1}` che indica la direzione di percorrenza di ogni
     *           arco nel ciclo critico.
     */
    std::pair<double, Eigen::VectorXd> find_optimum_cycle_ratio();

private:
    const MinCostFlow& g; ///< Riferimento all'istanza di min-cost flow.
    int V;                ///< Numero di nodi del grafo.

    Eigen::VectorXd  distances;    ///< Distanze correnti per la policy-iteration.
    std::vector<int> policy;       ///< `policy[v]` = edge_id scelto da `v` nella policy corrente.
    std::vector<bool> bad_vertices; ///< `true` se il nodo non ha archi uscenti (reindirizzato a sink).
    std::vector<std::unordered_set<int>> in_edges_list; ///< Archi entranti nel policy graph per ogni nodo.

    /**
     * @brief Hash per `std::pair<int,int>` basato su codifica a 64 bit.
     *
     * Combina i due interi in un `int64_t` con shift di 32 bit, garantendo
     * assenza di collisioni per valori di nodo e edge_id entro 2^31.
     * Sostituisce la `std::map` originale (O(log n)) con lookup O(1).
     */
    struct PairHash {
        std::size_t operator()(const std::pair<int,int>& p) const noexcept {
            return std::hash<std::int64_t>()((std::int64_t)p.first << 32 | (unsigned)p.second);
        }
    };

    /// Cache `(nodo, edge_id) → (nodo_target, gradiente orientato)`.
    /// Evita ricalcoli ripetuti durante la policy-iteration.
    std::unordered_map<std::pair<int,int>, std::pair<int,double>, PairHash> _edge_cache;

    int    sink;       ///< Nodo pozzo fittizio per i `bad_vertices` (−1 se non ancora assegnato).
    double bound;      ///< Limite superiore iniziale sul cycle ratio (`sum_grad / min_len`).
    double best_ratio; ///< Miglior ratio trovato finora; inizializzato a `bound`.

    std::optional<std::vector<int>> critical_cycle;  ///< Sequenza di edge_id del ciclo critico.
    std::optional<int>              critical_vertex;  ///< Nodo di partenza del ciclo critico.

    Eigen::VectorXd gradients_vec; ///< Copia locale del vettore dei gradienti.
    Eigen::VectorXd lengths_vec;   ///< Copia locale del vettore delle lunghezze.

    /**
     * @brief Calcola il bound iniziale sul minimum cycle ratio.
     *
     * Il bound è definito come `Σ|gradients| / min(|lengths|)`, ovvero il
     * rapporto tra la somma dei pesi assoluti e la lunghezza minima non nulla.
     * Fornisce un limite superiore garantito sul ratio di qualsiasi ciclo.
     *
     * @return Valore del bound, o `INF` se tutte le lunghezze sono zero.
     */
    double _compute_bound() const;

    /**
     * @brief Restituisce il gradiente orientato dell'arco `edge_id` a partire da `start`.
     * @param start   Nodo di partenza.
     * @param edge_id Indice dell'arco.
     * @return Gradiente orientato dalla prospettiva di `start`.
     */
    double _get_gradient(int start, int edge_id) const;

    /**
     * @brief Restituisce il nodo di destinazione dell'arco `edge_id` a partire da `start`.
     * @param start   Nodo di partenza.
     * @param edge_id Indice dell'arco.
     * @return Indice del nodo di arrivo.
     */
    int _get_edge_target(int start, int edge_id) const;

    /**
     * @brief Costruisce il policy graph associando a ogni nodo l'arco uscente
     *        con gradiente massimo.
     *
     * I nodi senza archi uscenti (`bad_vertices`) vengono reindirizzati al
     * primo nodo bad incontrato, usato come `sink` fittizio.
     */
    void _construct_policy_graph();

    /**
     * @brief Trova un nodo appartenente a un ciclo nel policy graph a partire da `start`.
     *
     * Segue la policy da `start` fino a visitare un nodo già visto, usando
     * un `unordered_set` per il rilevamento in O(1) per lookup.
     *
     * @param start Nodo di partenza della visita.
     * @return Indice del primo nodo ripetuto (appartenente al ciclo).
     */
    int _find_cycle_vertex(int start) const;

    /**
     * @brief Calcola il cycle ratio del ciclo contenente `start` e aggiorna
     *        `best_ratio` e `critical_cycle` se il ratio è migliorante.
     *
     * @param start Nodo di partenza del ciclo (deve appartenere a un ciclo
     *              nel policy graph corrente).
     * @return Ratio del ciclo: `Σ gradients / Σ lengths` sugli archi del ciclo.
     */
    double _compute_cycle_ratio(int start);

    /**
     * @brief Aggiorna la policy di ogni nodo se esiste un arco migliorante.
     *
     * Per ogni nodo `v`, esamina tutti gli archi uscenti e aggiorna la policy
     * se la nuova distanza potenziale è inferiore a `distances[v] + EPS`.
     *
     * @param current_ratio Ratio corrente usato come parametro di riduzione
     *                      nella valutazione delle distanze.
     * @return `true` se almeno una policy è stata aggiornata, `false` se
     *         nessun miglioramento è possibile (condizione di terminazione).
     */
    bool _improve_policy(double current_ratio);

    /**
     * @brief Trova tutti i cicli nel policy graph corrente tramite DFS e
     *        restituisce il minimum cycle ratio.
     *
     * Usa colorazione a tre stati (WHITE/GRAY/BLACK) per rilevare i back-edge
     * della DFS. Per ogni ciclo trovato invoca `_compute_cycle_ratio`.
     *
     * @return Valore minimo del cycle ratio tra tutti i cicli trovati,
     *         o `INF` se nessun ciclo è presente.
     */
    double _find_all_cycles();

    /**
     * @brief Costruisce il vettore indicatore del ciclo critico.
     *
     * Percorre il ciclo critico a partire da `critical_vertex` e assegna
     * `+1` agli archi percorsi nella direzione originale, `-1` agli archi
     * percorsi in direzione inversa.
     *
     * @return Vettore di dimensione `g.m` con valori in `{-1, 0, +1}`.
     *         Vettore zero se `critical_cycle` o `critical_vertex` non sono
     *         stati impostati.
     */
    Eigen::VectorXd _make_cycle_vector() const;
};

/**
 * @brief Funzione di interfaccia pubblica per il Minimum Cycle Ratio.
 *
 * Crea un'istanza temporanea di @ref Howard e ne esegue l'algoritmo.
 * È il punto di ingresso consigliato da @ref AlmostLinearTime::max_flow_with_guess.
 *
 * @param g         Istanza di min-cost flow (grafo, archi, matrici).
 * @param gradients Vettore dei gradienti di Φ(f) sugli archi (dimensione `m`).
 * @param lengths   Vettore delle lunghezze sugli archi (dimensione `m`).
 * @return Coppia `(min_ratio, cycle_vector)` — vedere @ref Howard::find_optimum_cycle_ratio.
 */
std::pair<double, Eigen::VectorXd>
minimum_cycle_ratio(const MinCostFlow& g,
                    const Eigen::VectorXd& gradients,
                    const Eigen::VectorXd& lengths);