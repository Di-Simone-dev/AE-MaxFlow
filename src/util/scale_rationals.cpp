/**
 * @file scale_rationals.cpp
 * @brief Implementazione di `scale_rationals()`.
 *
 * @see scale_rationals.hpp
 *
 * @author  [inserire autore]
 * @date    [inserire data]
 */

#include "scale_rationals.hpp"
#include <numeric>   // std::lcm, std::gcd
#include <iostream>  // TODO: solo per debug, rimuovere in produzione
#include <limits>    // std::numeric_limits

/**
 * @brief Converte un vettore di frazioni in interi scalati per il fattore MCM
 *        dei denominatori, con controllo di overflow a ogni moltiplicazione.
 *
 * ### Pipeline in tre fasi
 *
 * **Fase 1 — Raccolta dei denominatori**
 * Estrae il denominatore di ciascuna `Fraction` e lo accumula in un vettore
 * temporaneo. I denominatori sono già positivi per invariante di `Fraction`.
 *
 * **Fase 2 — Calcolo dell'MCM con guard overflow**
 * L'MCM viene calcolato in modo iterativo:
 * @code
 * k = 1
 * per ogni denominatore d:
 *     g      = gcd(k, d)
 *     safe_d = d / g          // parte "nuova" da integrare in k
 *     k      = k * safe_d     // equivalente a lcm(k, d)
 * @endcode
 * Prima di ogni moltiplicazione viene verificato che `k * safe_d` non superi
 * `std::numeric_limits<unsigned long long>::max()`. In caso di overflow viene
 * lanciata `std::overflow_error` con un messaggio che suggerisce CapacityScaling.
 *
 * **Fase 3 — Scalatura delle frazioni con guard overflow**
 * Per ogni frazione `p/q`, il valore scalato è `p * (k/q)`. Anche qui viene
 * eseguito un controllo pre-moltiplicazione per evitare overflow silenzioso.
 *
 * @param values Vettore di frazioni da scalare.
 * @return Coppia `(scaled, k)`:
 *         - `scaled`: valori interi proporzionali alle frazioni originali.
 *         - `k`:      fattore di scala (MCM dei denominatori).
 *
 * @throws std::overflow_error Se l'MCM o un prodotto `num * (k/den)` supera
 *         il massimo di `unsigned long long`.
 *
 * @warning Il `#include <iostream>` è presente solo a scopo di debug e
 *          dovrebbe essere rimosso prima del deploy in produzione.
 */
std::pair<std::vector<unsigned long long>, unsigned long long>
scale_rationals(const std::vector<Fraction>& values)
{
    // ------------------------------------------------------------------
    // Fase 1: estrazione dei denominatori
    // ------------------------------------------------------------------
    std::vector<unsigned long long> denominators;
    denominators.reserve(values.size());

    for (const auto& f : values)
        denominators.push_back(f.den);

    // ------------------------------------------------------------------
    // Fase 2: calcolo dell'MCM iterativo con controllo overflow
    // ------------------------------------------------------------------
    unsigned long long k = 1;
    for (unsigned long long d : denominators) {
        unsigned long long g      = std::gcd(k, d);
        unsigned long long safe_d = d / g;  // contributo "nuovo" di d rispetto a k

        // Verifica: k * safe_d <= ULLONG_MAX  ⟺  safe_d <= ULLONG_MAX / k
        if (safe_d != 0 && k > std::numeric_limits<unsigned long long>::max() / safe_d)
            throw std::overflow_error(
                "MCM overflow: denominatori incompatibili con unsigned long long. "
                "Usa CapacityScaling per grafi con capacità razionali complesse.");

        k = k * safe_d;
    }

    // ------------------------------------------------------------------
    // Fase 3: scalatura di ogni frazione con controllo overflow
    // ------------------------------------------------------------------
    std::vector<unsigned long long> scaled;
    scaled.reserve(values.size());

    for (const auto& f : values) {
        unsigned long long multiplier = k / f.den;             // k/q, sempre esatto (k è multiplo di q)
        unsigned long long num        = static_cast<unsigned long long>(f.num);

        // Verifica: num * multiplier <= ULLONG_MAX  ⟺  multiplier <= ULLONG_MAX / num
        if (num != 0 && multiplier > std::numeric_limits<unsigned long long>::max() / num)
            throw std::overflow_error(
                "Overflow nel prodotto num * (k/den): num=" + std::to_string(f.num) +
                " k/den=" + std::to_string(multiplier));

        scaled.push_back(num * multiplier);
    }

    return {scaled, k};
}