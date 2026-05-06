#include "scale_rationals.hpp"
#include <numeric>   // std::lcm
#include <iostream> //SOLO DI DEBUG
#include <limits>
std::pair<std::vector<unsigned long long>, unsigned long long>
scale_rationals(const std::vector<Fraction>& values)
{
    // 1) Estrai tutti i denominatori
    std::vector<unsigned long long> denominators;
    denominators.reserve(values.size());

    for (const auto& f : values)
        denominators.push_back(f.den);

    // 2) Calcola l'MCM di tutti i denominatori con controllo overflow
    unsigned long long k = 1;
    for (unsigned long long d : denominators) {
        unsigned long long g = std::gcd(k, d);
        unsigned long long safe_d = d / g;  // la parte "nuova" da moltiplicare

        // Controlla overflow PRIMA di moltiplicare
        if (safe_d != 0 && k > std::numeric_limits<unsigned long long>::max() / safe_d)
            throw std::overflow_error(
                "MCM overflow: denominatori incompatibili con unsigned long long. "
                "Usa CapacityScaling per grafi con capacità razionali complesse.");

        k = k * safe_d;
    }

    // 3) Scala tutte le frazioni con controllo overflow sul prodotto
    std::vector<unsigned long long> scaled;
    scaled.reserve(values.size());

    for (const auto& f : values) {
        unsigned long long multiplier = k / f.den;
        unsigned long long num = static_cast<unsigned long long>(f.num);

        // Controlla overflow PRIMA di moltiplicare
        if (num != 0 && multiplier > std::numeric_limits<unsigned long long>::max() / num)
            throw std::overflow_error(
                "Overflow nel prodotto num * (k/den): num=" + std::to_string(f.num) +
                " k/den=" + std::to_string(multiplier));

        scaled.push_back(num * multiplier);
    }

    return {scaled, k};
}