/**
 * @file pair_hash.hpp
 * @brief Funzione hash per `std::pair<int,int>`, utilizzabile come funtore
 *        nei contenitori non ordinati della STL.
 *
 * La STL non fornisce una specializzazione di `std::hash` per `std::pair`,
 * rendendo impossibile usare direttamente `std::unordered_map` o
 * `std::unordered_set` con chiavi di tipo `std::pair<int,int>`.
 *
 * Questo file definisce il funtore `PairHash` come soluzione leggera e
 * header-only a tale limitazione.
 *
 * ### Utilizzo tipico
 * @code
 * #include "pair_hash.hpp"
 * #include <unordered_map>
 *
 * std::unordered_map<std::pair<int,int>, double, PairHash> edge_capacity;
 * edge_capacity[{1, 2}] = 3.5;
 * @endcode
 *
 */

#pragma once

#include <functional>
#include <utility>

/**
 * @brief Funtore hash per coppie di interi `std::pair<int,int>`.
 *
 * Implementa `operator()` compatibile con il requisito `Hash` della STL,
 * consentendo l'uso di `std::pair<int,int>` come chiave in:
 * - `std::unordered_map<std::pair<int,int>, V, PairHash>`
 * - `std::unordered_set<std::pair<int,int>, PairHash>`
 *
 * ### Algoritmo di combinazione
 * I due hash individuali `h1` e `h2` vengono combinati con la formula:
 * @code
 * h1 ^ (h2 * 0x9e3779b97f4a7c15 + (h1 << 6) + (h1 >> 2))
 * @endcode
 * La costante `0x9e3779b97f4a7c15` è la versione a 64 bit della
 * *golden ratio* di Knuth, scelta per distribuire uniformemente i bit e
 * ridurre le collisioni. Lo shift e la rotazione di `h1` introducono
 * dipendenza fra i due hash, evitando che coppie simmetriche come
 * `(a,b)` e `(b,a)` producano lo stesso valore.
 *
 * @note La struttura è stateless e tutti i metodi sono `noexcept`:
 *       può essere usata in contesti che richiedono garanzie di eccezione forte.
 */
struct PairHash {
    /**
     * @brief Calcola l'hash di una coppia `(int, int)`.
     *
     * @param p La coppia da hashare.
     * @return Un valore `std::size_t` che rappresenta l'hash della coppia.
     *
     * @note Marcato `noexcept` perché `std::hash<int>` non lancia eccezioni.
     */
    std::size_t operator()(const std::pair<int,int>& p) const noexcept {
        std::size_t h1 = std::hash<int>{}(p.first);
        std::size_t h2 = std::hash<int>{}(p.second);
        // Combinazione con costante di Knuth (golden ratio a 64 bit)
        // per minimizzare collisioni e garantire avalanche effect.
        return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};