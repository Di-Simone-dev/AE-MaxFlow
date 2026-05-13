/**
 * @file scale_rationals.hpp
 * @brief Dichiarazione di `scale_rationals()`, funzione per la conversione
 *        di un insieme di frazioni in interi scalati con fattore comune.
 *
 * La tecnica consiste nel moltiplicare tutte le frazioni per il loro
 * **minimo comune multiplo dei denominatori** (MCM), ottenendo valori interi
 * proporzionali a quelli originali. Questo permette di utilizzare algoritmi
 * di flusso massimo che operano solo su capacità intere (es. Ford-Fulkerson,
 * Dinic) anche quando il grafo DIMACS contiene capacità frazionarie.
 *
 * @see scale_rationals.cpp
 * @see fraction.hpp
 *
 */

#pragma once
#include <vector>
#include <utility>
#include "fraction.hpp"

/**
 * @brief Converte un vettore di frazioni in interi scalati e restituisce
 *        il fattore di scala utilizzato.
 *
 * Data una lista di frazioni \f$ \frac{p_i}{q_i} \f$, la funzione calcola
 * \f$ k = \mathrm{lcm}(q_1, q_2, \ldots, q_n) \f$ e restituisce la coppia:
 * \f[
 *   \left( \left\{ p_i \cdot \frac{k}{q_i} \right\}_{i=1}^{n},\ k \right)
 * \f]
 *
 * Il vettore risultante contiene valori interi esatti e proporzionali alle
 * frazioni originali; il fattore `k` permette di risalire ai valori originali
 * dividendo per esso.
 *
 * ### Esempio
 * @code
 * // Input: [1/2, 1/3, 3/4]
 * // k = lcm(2, 3, 4) = 12
 * // scaled = [6, 4, 9]   (1/2*12=6, 1/3*12=4, 3/4*12=9)
 * auto [scaled, k] = scale_rationals({Fraction(1,2), Fraction(1,3), Fraction(3,4)});
 * @endcode
 *
 * @param values Vettore di frazioni da scalare. I numeratori devono essere
 *               non negativi (le capacità degli archi sono sempre ≥ 0).
 *
 * @return Una coppia `(scaled, k)` dove:
 *         - `scaled` è il vettore degli interi scalati (`p_i * k / q_i`),
 *         - `k` è il fattore di scala (MCM dei denominatori).
 *
 * @throws std::overflow_error Se il calcolo dell'MCM dei denominatori o il
 *         prodotto `num * (k/den)` supera il limite di `unsigned long long`.
 *         In tal caso si consiglia di usare un algoritmo come CapacityScaling
 *         che opera direttamente su capacità razionali.
 *
 * @note La funzione non gestisce frazioni con numeratore negativo.
 *       Assicurarsi che tutte le frazioni in input rappresentino capacità
 *       non negative prima di chiamare questa funzione.
 */
std::pair<std::vector<unsigned long long>, unsigned long long>
scale_rationals(const std::vector<Fraction>& values);