/**
 * @file fraction.hpp
 * @brief Definizione della classe `Fraction` per l'aritmetica razionale esatta.
 *
 * La classe rappresenta un numero razionale p/q con numeratore e denominatore
 * di tipo `long long`. La frazione è mantenuta sempre in **forma ridotta ai
 * minimi termini** e con **denominatore positivo**, grazie alla normalizzazione
 * eseguita nel costruttore tramite il massimo comun divisore (MCD).
 *
 * Tutta l'implementazione è inline nel file `.hpp`; il file `.cpp` è vuoto
 * per convenzione (classi piccole senza dipendenze esterne).
 *
 * @note Questa classe è usata da `Capacity` (capacity.hpp) per rappresentare
 *       capacità frazionarie esatte nei grafi di flusso DIMACS.
 *
 */

#pragma once
#include <stdexcept>
#include <numeric>
#include <string>

/**
 * @brief Numero razionale esatto in forma ridotta ai minimi termini.
 *
 * Invarianti garantite dopo ogni costruzione:
 * - `den > 0` (il segno è sempre portato dal numeratore),
 * - `gcd(|num|, den) == 1` (la frazione è ridotta).
 *
 * Tutti gli operatori aritmetici restituiscono una nuova `Fraction`
 * anch'essa normalizzata.
 */
class Fraction {
public:
    long long num;  ///< Numeratore (può essere negativo o zero).
    long long den;  ///< Denominatore (sempre strettamente positivo).

    /**
     * @brief Costruisce una frazione normalizzata `n/d`.
     *
     * La normalizzazione comprende:
     * 1. Lancio di eccezione se `d == 0`.
     * 2. Spostamento del segno al numeratore se `d < 0`.
     * 3. Divisione di entrambi i termini per `gcd(|n|, d)`.
     *
     * @param n Numeratore (default `0`).
     * @param d Denominatore (default `1`, deve essere ≠ 0).
     *
     * @throws std::runtime_error Se `d == 0`.
     *
     * @par Esempi
     * @code
     * Fraction f1(3, 4);    // → 3/4
     * Fraction f2(6, 4);    // → 3/2  (riduzione automatica)
     * Fraction f3(1, -2);   // → -1/2 (segno normalizzato)
     * Fraction f4;          // → 0/1  (default)
     * @endcode
     */
    Fraction(long long n = 0, long long d = 1) {
        if (d == 0)
            throw std::runtime_error("Denominatore zero in Fraction");
        if (d < 0) { n = -n; d = -d; }
        long long g = std::gcd(n, d);
        num = n / g;
        den = d / g;
    }

    /**
     * @brief Converte la frazione in un valore `double`.
     *
     * Esegue la divisione in virgola mobile `(double)num / (double)den`.
     * Per frazioni con denominatori grandi può perdere precisione.
     *
     * @return Il valore approssimato come `double`.
     */
    double to_double() const {
        return static_cast<double>(num) / static_cast<double>(den);
    }

    // -----------------------------------------------------------------------
    // Operatori aritmetici
    // -----------------------------------------------------------------------

    /**
     * @brief Addizione fra frazioni: `this + other`.
     *
     * Calcola `(num * other.den + other.num * den) / (den * other.den)`,
     * poi normalizza il risultato tramite il costruttore.
     *
     * @param other Secondo addendo.
     * @return Somma in forma ridotta.
     *
     * @warning Possibile overflow di `long long` con denominatori molto grandi.
     */
    Fraction operator+(const Fraction& other) const {
        return Fraction(num * other.den + other.num * den,
                        den * other.den);
    }

    /**
     * @brief Sottrazione fra frazioni: `this - other`.
     *
     * Calcola `(num * other.den - other.num * den) / (den * other.den)`,
     * poi normalizza il risultato tramite il costruttore.
     *
     * @param other Sottraendo.
     * @return Differenza in forma ridotta.
     *
     * @warning Possibile overflow di `long long` con denominatori molto grandi.
     */
    Fraction operator-(const Fraction& other) const {
        return Fraction(num * other.den - other.num * den,
                        den * other.den);
    }

    /**
     * @brief Moltiplicazione fra frazioni: `this * other`.
     *
     * Calcola `(num * other.num) / (den * other.den)`,
     * poi normalizza il risultato tramite il costruttore.
     *
     * @param other Secondo fattore.
     * @return Prodotto in forma ridotta.
     *
     * @warning Possibile overflow di `long long` con valori grandi.
     */
    Fraction operator*(const Fraction& other) const {
        return Fraction(num * other.num, den * other.den);
    }

    /**
     * @brief Divisione fra frazioni: `this / other`.
     *
     * Calcola `(num * other.den) / (den * other.num)` (moltiplicazione per
     * il reciproco), poi normalizza tramite il costruttore.
     *
     * @param other Divisore.
     * @return Quoziente in forma ridotta.
     *
     * @throws std::runtime_error Se `other.num == 0` (divisione per zero).
     *
     * @warning Possibile overflow di `long long` con valori grandi.
     */
    Fraction operator/(const Fraction& other) const {
        if (other.num == 0)
            throw std::runtime_error("Divisione per zero in Fraction");
        return Fraction(num * other.den, den * other.num);
    }

    /**
     * @brief Restituisce la rappresentazione testuale della frazione.
     *
     * Il formato prodotto è `"numeratore/denominatore"`, ad esempio `"3/4"`
     * o `"-1/2"`. Non viene inserito alcuno spazio attorno alla barra.
     *
     * @return Stringa nel formato `"num/den"`.
     */
    std::string str() const {
        return std::to_string(num) + "/" + std::to_string(den);
    }
};