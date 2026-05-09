/**
 * @file min_cost_flow_instance.hpp
 * @brief Dichiarazione della struttura dati `MinCostFlow` che rappresenta
 *        un'istanza del problema di min-cost flow con barriera logaritmica.
 *
 * `MinCostFlow` è la struttura centrale dell'algoritmo interior-point: contiene
 * il grafo, i vettori di capacità e costo, la matrice di incidenza `B` e i
 * metodi per valutare il potenziale Φ(f) e i suoi gradienti.
 *
 * ### Funzione potenziale Φ(f)
 *
 * Il potenziale combinato costo-barriera è definito come:
 * @code
 *   Φ(f) = 20·m·log₂(c·f - opt) + Σ_e [ (u_e - f_e)^(-α) + (f_e - l_e)^(-α) ]
 * @endcode
 * dove `α = 1 / log₂(1000·m·U)` è il parametro della barriera, `opt` è il
 * costo ottimale noto e `[l_e, u_e]` sono le capacità inferiore/superiore
 * dell'arco `e`. Il dominio ammissibile è l'insieme aperto `l < f < u`.
 *
 * @see min_cost_flow_instance.cpp
 * @see AlmostLinearTime
 * @see almost_linear::calc_feasible_flow
 */

#pragma once
#include <Eigen/Dense>
#include <vector>
#include <map>
#include <tuple>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <unordered_map>

/**
 * @class MinCostFlow
 * @brief Struttura dati e metodi per un'istanza di min-cost flow con barriera.
 *
 * Mantiene il grafo come lista di archi + lista di adiacenza, i vettori di
 * costo e capacità come `Eigen::VectorXd`, la matrice di incidenza `B` (m×n)
 * e la mappa bidirezionale `undirected_edge_to_indices` per lookup rapido
 * degli archi tra due nodi.
 *
 * I metodi `phi`, `calc_gradients` e `calc_lengths` implementano la barriera
 * logaritmica usata dal loop MCR di @ref AlmostLinearTime.
 *
 * @note I campi `m`, `n`, `edges`, `adj` e le matrici Eigen sono pubblici
 *       per consentire accesso diretto da Howard e `calc_feasible_flow`.
 *       Modificarli direttamente è sconsigliato: usare `add_edge`/`add_vertex`.
 */
class MinCostFlow {
public:
    int m; ///< Numero di archi corrente (aggiornato da `add_edge`).
    int n; ///< Numero di nodi corrente (aggiornato da `add_vertex`).

    std::vector<std::pair<int,int>> edges; ///< Lista degli archi: `edges[e] = (u, v)`.

    Eigen::VectorXd c;     ///< Vettore dei costi sugli archi (dimensione `m`).
    Eigen::VectorXd c_org; ///< Copia dei costi originali prima di eventuali modifiche.
    Eigen::VectorXd u_lower; ///< Capacità inferiori sugli archi (dimensione `m`).
    Eigen::VectorXd u_upper; ///< Capacità superiori sugli archi (dimensione `m`).

    long long optimal_cost; ///< Costo ottimale noto dell'istanza (negativo per max-flow).
    long long U;            ///< Massima capacità in valore assoluto: `max(|u_upper|, |u_lower|)`.
    double alpha;           ///< Parametro della barriera: `1 / log₂(1000·m·U)`.

    Eigen::MatrixXd B; ///< Matrice di incidenza (m×n): `B(e,u)=+1`, `B(e,v)=-1` per arco `(u,v)`.

    /**
     * @brief Hash per `std::pair<int,int>` basato su codifica a 64 bit.
     *
     * Combina i due interi in un `int64_t` via shift di 32 bit, garantendo
     * assenza di collisioni per indici di nodo entro 2^31. Sostituisce la
     * `std::map` originale (O(log n)) con lookup O(1) ammortizzato.
     */
    struct PairHash {
        std::size_t operator()(const std::pair<int,int>& p) const noexcept {
            return std::hash<std::int64_t>()((std::int64_t)p.first << 32 | (unsigned)p.second);
        }
    };

    /// Mappa bidirezionale da coppia di nodi `(a,b)` agli indici degli archi tra `a` e `b`.
    /// Popolata sia con `(a,b)` che con `(b,a)` per lookup non orientato in O(1).
    std::unordered_map<std::pair<int,int>, std::vector<int>, PairHash> undirected_edge_to_indices;

    /// Lista di adiacenza: `adj[v]` contiene gli indici degli archi incidenti su `v`
    /// (sia entranti che uscenti, poiché il grafo è trattato come non orientato da Howard).
    std::vector<std::vector<int>> adj;

    /**
     * @brief Costruttore principale: inizializza l'istanza da archi, costi e capacità.
     *
     * Calcola automaticamente `n` come `max(indici di nodo) + 1`, costruisce la
     * matrice di incidenza `B`, la mappa `undirected_edge_to_indices`, la lista
     * di adiacenza `adj`, e i parametri `U` e `alpha` della barriera.
     *
     * @param edges        Lista degli archi orientati `(u, v)`.
     * @param c            Vettore dei costi (dimensione `|edges|`).
     * @param u_lower      Capacità inferiori (dimensione `|edges|`).
     * @param u_upper      Capacità superiori (dimensione `|edges|`).
     * @param optimal_cost Costo ottimale noto. Per istanze di max-flow è `-optimal_flow`.
     *
     * @pre `c.size() == u_lower.size() == u_upper.size() == edges.size()`
     * @pre `u_lower[e] <= u_upper[e]` per ogni arco `e`.
     */
    MinCostFlow(
        const std::vector<std::pair<int,int>>& edges,
        const Eigen::VectorXd& c,
        const Eigen::VectorXd& u_lower,
        const Eigen::VectorXd& u_upper,
        long long optimal_cost
    );

    /**
     * @brief Costruttore di default: produce un'istanza vuota.
     *
     * Usato internamente da `clone()` per costruire l'oggetto di destinazione
     * prima di copiare i campi. Non inizializza nessun membro.
     */
    MinCostFlow() = default;

    /**
     * @brief Produce una copia profonda dell'istanza corrente.
     *
     * Tutti i campi (vettori Eigen, matrici, mappe, liste) vengono copiati.
     * Usato da `calc_feasible_flow` per non modificare l'istanza originale.
     *
     * @return Copia indipendente di `*this`.
     */
    MinCostFlow clone() const;

    /**
     * @brief Factory method: costruisce un'istanza di min-cost flow a partire
     *        da un problema di max-flow con valore target.
     *
     * Aggiunge un arco virtuale `(t → s)` con costo `-1` e capacità
     * `[0, Σ capacities]`, impostando `optimal_cost = -optimal_flow`.
     * Gli archi originali hanno costo `0` e capacità `[lower, upper]`.
     *
     * Questa riduzione garantisce che minimizzare il costo equivalga a
     * massimizzare il flusso sull'arco virtuale.
     *
     * @param edges             Archi del grafo originale.
     * @param s                 Nodo sorgente.
     * @param t                 Nodo pozzo.
     * @param optimal_flow      Valore target del flusso (stima della ricerca binaria).
     * @param capacities        Capacità superiori degli archi originali.
     * @param lower_capacities  Capacità inferiori (nullptr → tutti zero).
     * @return Istanza di min-cost flow pronta per il loop interior-point.
     */
    static MinCostFlow from_max_flow_instance(
        const std::vector<std::pair<int,int>>& edges,
        int s,
        int t,
        long long optimal_flow,
        const std::vector<long long>& capacities,
        const std::vector<long long>* lower_capacities = nullptr
    );

    /**
     * @brief Valuta il potenziale Φ(f) nel punto `f`.
     *
     * @code
     *   Φ(f) = 20·m·log₂(c·f - opt) + Σ_e [ (u_e - f_e)^(-α) + (f_e - l_e)^(-α) ]
     * @endcode
     *
     * @param f Vettore di flusso corrente (dimensione `m`), strettamente interno
     *          al dominio ammissibile `u_lower < f < u_upper`.
     * @return Valore scalare di Φ(f). Può essere `+∞` se `f` è sul bordo del dominio.
     *
     * @pre `(u_lower.array() < f.array()).all() && (f.array() < u_upper.array()).all()`
     * @pre `c.dot(f) > optimal_cost`
     */
    double phi(const Eigen::VectorXd& f) const;

    /**
     * @brief Calcola il gradiente ∇Φ(f) rispetto agli archi.
     *
     * @code
     *   ∇Φ(f)_e = 20·m·c_e / (c·f - opt)
     *             + α·(u_e - f_e)^(-1-α)
     *             - α·(f_e - l_e)^(-1-α)
     * @endcode
     *
     * @param f Vettore di flusso corrente (dimensione `m`).
     * @return Vettore gradiente di dimensione `m`.
     */
    Eigen::VectorXd calc_gradients(const Eigen::VectorXd& f) const;

    /**
     * @brief Calcola le lunghezze sugli archi usate dall'algoritmo di Howard.
     *
     * Le lunghezze sono definite come la somma delle derivate seconde parziali
     * della barriera rispetto a ciascun arco:
     * @code
     *   lengths_e = (u_e - f_e)^(-1-α) + (f_e - l_e)^(-1-α)
     * @endcode
     * Sono sempre positive nel dominio ammissibile, garantendo che il ratio
     * di Howard sia ben definito.
     *
     * @param f Vettore di flusso corrente (dimensione `m`).
     * @return Vettore delle lunghezze di dimensione `m` (tutti positivi).
     */
    Eigen::VectorXd calc_lengths(const Eigen::VectorXd& f) const;

    /**
     * @brief Restituisce gli indici degli archi tra i nodi `a` e `b`.
     *
     * Lookup O(1) grazie a `undirected_edge_to_indices`. La ricerca è non
     * orientata: restituisce archi sia nella direzione `(a,b)` che `(b,a)`.
     *
     * @param a Primo nodo.
     * @param b Secondo nodo.
     * @return Lista degli indici degli archi tra `a` e `b` (vuota se assenti).
     */
    std::vector<int> edges_between(int a, int b) const;

    /**
     * @brief Aggiunge un nuovo nodo isolato al grafo e restituisce il suo indice.
     *
     * Incrementa `n`, estende la matrice `B` con una colonna di zeri e
     * aggiunge una lista di adiacenza vuota per il nuovo nodo.
     *
     * @return Indice del nuovo nodo (`n - 1` dopo l'aggiunta).
     */
    int add_vertex();

    /**
     * @brief Aggiunge un nuovo arco orientato `(a → b)` al grafo.
     *
     * Aggiorna `edges`, `adj`, `undirected_edge_to_indices`, i vettori `c`,
     * `u_lower`, `u_upper` e la matrice `B` (aggiungendo una riga).
     * Incrementa `m`.
     *
     * @param a     Nodo di partenza (deve essere < `n`).
     * @param b     Nodo di arrivo (deve essere < `n`).
     * @param c     Costo dell'arco.
     * @param u_lower Capacità inferiore dell'arco.
     * @param u_upper Capacità superiore dell'arco.
     *
     * @pre `a < n && b < n`
     * @warning Ogni chiamata riallocat l'intera matrice `B` — O(m·n).
     *          Aggiungere molti archi sequenzialmente è costoso; preferire
     *          la costruzione in batch tramite il costruttore principale.
     */
    void add_edge(int a, int b, double c, double u_lower, double u_upper);
};