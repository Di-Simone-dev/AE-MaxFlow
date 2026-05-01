#pragma once
#include <string>
#include <variant>
#include "fraction.hpp"

using Capacity = std::variant<int, Fraction, double>;

Capacity parse_capacity(const std::string& s);
