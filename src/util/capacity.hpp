#pragma once
#include <variant>
#include <string>
#include "fraction.hpp"

// Capacità: può essere intera, frazionaria o floating
using Capacity = std::variant<int, Fraction, double>;

// Parsing da stringa (DIMACS)
Capacity parse_capacity(const std::string& s_raw);

// Conversione generica a double
double to_double(const Capacity& c);

// Somma robusta di due capacità
Capacity add_capacity(const Capacity& a, const Capacity& b);
