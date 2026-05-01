#pragma once
#include <vector>
#include <utility>
#include "fraction.hpp"

// Restituisce: (vector<int> scaled, int scaling_factor)
std::pair<std::vector<int>, int>
scale_rationals(const std::vector<Fraction>& values);
