#pragma once
#include <vector>
#include <utility>
#include "fraction.hpp"

// Restituisce: (vector<int> scaled, int scaling_factor)
std::pair<std::vector<unsigned long long>, unsigned long long>
scale_rationals(const std::vector<Fraction>& values);
