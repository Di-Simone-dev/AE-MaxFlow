#include "scale_rationals.hpp"
#include <numeric>   // std::lcm

std::pair<std::vector<int>, int>
scale_rationals(const std::vector<Fraction>& values)
{
    // 1) Estrai tutti i denominatori
    std::vector<int> denominators;
    denominators.reserve(values.size());

    for (const auto& f : values)
        denominators.push_back(f.den);

    // 2) Calcola l'MCM di tutti i denominatori
    int k = 1;
    for (int d : denominators)
        k = std::lcm(k, d);

    // 3) Scala tutte le frazioni
    std::vector<int> scaled;
    scaled.reserve(values.size());

    for (const auto& f : values) {
        // (num/den) * k = num * (k/den)
        int scaled_val = f.num * (k / f.den);
        scaled.push_back(scaled_val);
    }

    return {scaled, k};
}
