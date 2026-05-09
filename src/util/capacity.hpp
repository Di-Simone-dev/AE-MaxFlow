#pragma once
#include <variant>
#include <string>
#include "fraction.hpp"

/**
 * @file capacity.hpp
 * @brief Definizione del tipo `Capacity` e delle funzioni associate per il parsing,
 *        la conversione e la somma di capacità nei grafi di flusso (formato DIMACS).
 *
 * Il tipo `Capacity` è un `std::variant` che può contenere:
 * - un valore intero (`int`),
 * - una frazione esatta (`Fraction`),
 * - un valore in virgola mobile (`double`).
 *
 * Questa distinzione permette di preservare la precisione aritmetica il più
 * a lungo possibile (es. somme fra frazioni) cadendo su `double` solo quando
 * necessario (es. valori irrazionali).
 *
 * @author  [inserire autore]
 * @date    [inserire data]
 */

// ---------------------------------------------------------------------------
// Tipo principale
// ---------------------------------------------------------------------------

/**
 * @brief Tipo variante che rappresenta la capacità di un arco in un grafo di flusso.
 *
 * Può contenere:
 * - `int`      — valore intero esatto
 * - `Fraction` — valore frazionario esatto (numeratore/denominatore interi)
 * - `double`   — valore in virgola mobile (usato per irrazionali o espressioni miste)
 *
 * @note Definito come alias di `std::variant<int, Fraction, double>`.
 */
using Capacity = std::variant<int, Fraction, double>;

// ---------------------------------------------------------------------------
// Funzioni pubbliche
// ---------------------------------------------------------------------------

/**
 * @brief Effettua il parsing di una stringa nel formato DIMACS e restituisce
 *        la `Capacity` corrispondente.
 *
 * La funzione gestisce i seguenti formati (gli spazi vengono ignorati):
 * | Formato              | Esempio          | Tipo restituito |
 * |----------------------|------------------|-----------------|
 * | Intero               | `"42"`           | `int`           |
 * | Frazione             | `"7/4"`          | `Fraction`      |
 * | Floating point       | `"3.14"`         | `double`        |
 * | Costante simbolica   | `"pi"`, `"e"`    | `double`        |
 * | Funzione matematica  | `"sqrt(2)"`, `"log(3)"`, `"sin(pi/2)"` | `double` |
 *
 * Funzioni matematiche supportate: `sqrt`, `log`/`ln`, `log10`, `log2`,
 * `sin`, `cos`, `tan`, `exp`, `abs`.
 *
 * @param s_raw Stringa da analizzare (può contenere spazi).
 * @return La `Capacity` corrispondente al valore nella stringa.
 * @throws std::runtime_error Se la stringa non è riconoscibile o contiene
 *         una divisione per zero.
 */
Capacity parse_capacity(const std::string& s_raw);

/**
 * @brief Converte una `Capacity` in un valore `double`.
 *
 * - `int`      → cast diretto a `double`
 * - `Fraction` → chiama `Fraction::to_double()`
 * - `double`   → restituzione diretta
 *
 * @param c La capacità da convertire.
 * @return Il valore numerico approssimato come `double`.
 */
double to_double(const Capacity& c);

/**
 * @brief Somma due `Capacity` preservando il tipo più preciso possibile.
 *
 * La strategia di promozione del tipo è la seguente:
 * | Operandi          | Risultato  |
 * |-------------------|------------|
 * | `int` + `int`     | `int`      |
 * | `Fraction` + `Fraction` | `Fraction` |
 * | `Fraction` + `int` o viceversa | `Fraction` |
 * | qualsiasi altro   | `double`   |
 *
 * @param a Primo operando.
 * @param b Secondo operando.
 * @return La somma come `Capacity` nel tipo più preciso compatibile.
 */
Capacity add_capacity(const Capacity& a, const Capacity& b);