#include "capacity.hpp"
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <numbers>

static bool contains_alpha(const std::string& s) {
    for (char c : s)
        if (std::isalpha(static_cast<unsigned char>(c)))
            return true;
    return false;
}

Capacity parse_capacity(const std::string& s_raw) {
    std::string s;
    for (char c : s_raw)
        if (!std::isspace(static_cast<unsigned char>(c)))
            s.push_back(c);

    // Caso FRAZIONE: "7/4"
    if (s.find('/') != std::string::npos && !contains_alpha(s)) {
        auto pos = s.find('/');
        int num = std::stoi(s.substr(0, pos));
        int den = std::stoi(s.substr(pos + 1));
        if (den == 0)
            throw std::runtime_error("Divisione per zero in frazione");
        return Fraction(num, den);
    }

    // Caso ESPRESSIONE IRRAZIONALE: contiene lettere
    if (contains_alpha(s)) {
        std::string expr = s;

        auto replace = [&](const std::string& a, const std::string& b) {
            size_t pos = 0;
            while ((pos = expr.find(a, pos)) != std::string::npos) {
                expr.replace(pos, a.size(), b);
                pos += b.size();
            }
        };

        replace("pi", std::to_string(std::numbers::pi));
        replace("e",  std::to_string(std::numbers::e));


        // Supporto minimo: sqrt(...)
        if (expr.rfind("sqrt(", 0) == 0 && expr.back() == ')') {
            double x = std::stod(expr.substr(5, expr.size() - 6));
            return std::sqrt(x);
        }

        // fallback: prova a convertire come double
        try {
            return std::stod(expr);
        } catch (...) {
            throw std::runtime_error("Espressione irrazionale non supportata: " + s);
        }
    }

    // Caso INTERO
    try {
        size_t idx;
        int val = std::stoi(s, &idx);
        if (idx == s.size())
            return val;
    } catch (...) {}

    // Caso FLOAT
    try {
        return std::stod(s);
    } catch (...) {
        throw std::runtime_error("Formato capacità non riconosciuto: " + s);
    }
}
