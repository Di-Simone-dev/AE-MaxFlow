/**
 * @file parse_dimacs.hpp
 * @brief Definizione della struttura `DimacsResult` e della funzione
 *        `parse_dimacs()` per la lettura di grafi in formato DIMACS Max-Flow.
 *
 * Il formato DIMACS per il flusso massimo prevede le seguenti righe:
 * | Tag | Sintassi              | Significato                          |
 * |-----|-----------------------|--------------------------------------|
 * | `c` | `c <testo>`           | Commento (ignorato)                  |
 * | `p` | `p max <n> <m>`       | Intestazione: n nodi, m archi        |
 * | `n` | `n <id> s` / `n <id> t` | Nodo sorgente / pozzo              |
 * | `a` | `a <u> <v> <cap>`     | Arco da u a v con capacità cap       |
 *
 * Le capacità sono gestite tramite il tipo `Capacity` (capacity.hpp) e
 * possono essere intere, frazionarie o floating point.
 *
 * @see capacity.hpp
 * @see parse_dimacs.cpp
 *
 * @author  [inserire autore]
 * @date    [inserire data]
 */

#pragma once
#include <map>
#include <utility>
#include "capacity.hpp"   // contiene using Capacity = std::variant<int, Fraction, double>;

/**
 * @brief Struttura che raccoglie tutti i dati estratti da un file DIMACS
 *        nel formato Max-Flow.
 *
 * Viene popolata da `parse_dimacs()` e contiene la topologia del grafo
 * (numero di nodi e archi, sorgente, pozzo) e la mappa delle capacità
 * degli archi.
 *
 * Archi paralleli (stessa coppia `(u,v)` definita più volte nel file) vengono
 * automaticamente **accorpati**: le loro capacità vengono sommate tramite
 * `add_capacity()`, preservando il tipo più preciso possibile.
 */
struct DimacsResult {
    int n;          ///< Numero totale di nodi nel grafo.
    int m_actual;   ///< Numero di righe `a` lette (archi dichiarati nel file, prima dell'accorpamento).
    int source;     ///< Identificatore del nodo sorgente (`s`). Inizializzato a -1 (non impostato).
    int sink;       ///< Identificatore del nodo pozzo (`t`). Inizializzato a -1 (non impostato).

    /**
     * @brief Mappa delle capacità degli archi del grafo.
     *
     * La chiave è la coppia `(u, v)` di nodi estremi; il valore è la
     * `Capacity` associata all'arco. Se nel file DIMACS compaiono più righe
     * `a u v cap` con la stessa coppia `(u,v)`, le capacità vengono sommate.
     *
     * @note Viene usata `std::map` (ordinata) anziché `std::unordered_map`
     *       per evitare la necessità di un funtore hash esterno per
     *       `std::pair<int,int>`. Se le prestazioni di lookup diventassero
     *       critiche, si può sostituire con `std::unordered_map` e `PairHash`.
     */
    std::map<std::pair<int,int>, Capacity> graph;

    /**
     * @brief Costruttore di default: inizializza il grafo in stato "vuoto/invalido".
     *
     * I campi `source` e `sink` sono posti a `-1` per segnalare che non sono
     * ancora stati impostati. `parse_dimacs()` controlla questa condizione
     * al termine del parsing e lancia eccezione se rimangono a `-1`.
     */
    DimacsResult()
        : n(0),
          m_actual(0),
          source(-1),
          sink(-1),
          graph()
    {}

    /**
     * @brief Costruttore parametrico per inizializzazione diretta dei metadati.
     *
     * Utile in test o contesti in cui i valori sono già noti senza dover
     * passare per il parser.
     *
     * @param n_ Numero di nodi.
     * @param m_ Numero di archi.
     * @param s_ Identificatore del nodo sorgente.
     * @param t_ Identificatore del nodo pozzo.
     */
    DimacsResult(int n_, int m_, int s_, int t_)
        : n(n_),
          m_actual(m_),
          source(s_),
          sink(t_),
          graph()
    {}
};

/**
 * @brief Legge un file in formato DIMACS Max-Flow e restituisce un `DimacsResult`.
 *
 * @param path Percorso assoluto o relativo del file DIMACS da leggere.
 * @return Un `DimacsResult` completamente popolato.
 *
 * @throws std::runtime_error Nei seguenti casi:
 *         - Il file non può essere aperto.
 *         - La riga `p` non ha tipo `max`.
 *         - Un nodo `n` ha un ruolo diverso da `s` o `t`.
 *         - Una capacità non è parsabile (propagato da `parse_capacity()`).
 *         - Un tag di riga non è riconosciuto.
 *         - Al termine del parsing manca il nodo sorgente o il nodo pozzo.
 *
 * @see parse_capacity()
 * @see add_capacity()
 */
DimacsResult parse_dimacs(const std::string& path);