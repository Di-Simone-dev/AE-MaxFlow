#include "capacity.hpp"
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <numbers>

// ---------------------------------------------------------
// Funzione di utilità: controlla se la stringa contiene lettere
// ---------------------------------------------------------
static bool contains_alpha(const std::string& s) {
    for (char c : s)
        if (std::isalpha(static_cast<unsigned char>(c)))
            return true;
    return false;
}

// ---------------------------------------------------------
// Conversione generica a double
// ---------------------------------------------------------
double to_double(const Capacity& c) {
    return std::visit([](auto&& x) -> double {
        using T = std::decay_t<decltype(x)>;

        if constexpr (std::is_same_v<T, int>)
            return static_cast<double>(x);

        else if constexpr (std::is_same_v<T, Fraction>)
            return x.to_double();

        else if constexpr (std::is_same_v<T, double>)
            return x;

        else
            static_assert(!sizeof(T*), "Tipo non supportato in Capacity");
    }, c);
}

// ---------------------------------------------------------
// Somma robusta delle capacità
// ---------------------------------------------------------
Capacity add_capacity(const Capacity& a, const Capacity& b) {
    return std::visit([](auto&& x, auto&& y) -> Capacity {
        using X = std::decay_t<decltype(x)>;
        using Y = std::decay_t<decltype(y)>;

        // Caso 1: int + int → int
        if constexpr (std::is_same_v<X,int> && std::is_same_v<Y,int>) {
            return x + y;
        }

        // Caso 2: Fraction + Fraction → Fraction
        else if constexpr (std::is_same_v<X,Fraction> && std::is_same_v<Y,Fraction>) {
            return x + y;
        }

        // Caso 3: Fraction + int → Fraction
        else if constexpr (std::is_same_v<X,Fraction> && std::is_same_v<Y,int>) {
            return x + Fraction(y, 1);
        }

        // Caso 4: int + Fraction → Fraction
        else if constexpr (std::is_same_v<X,int> && std::is_same_v<Y,Fraction>) {
            return Fraction(x, 1) + y;
        }

        // Caso 5: tutto il resto → double
        else {
            return to_double(x) + to_double(y);
        }

    }, a, b);
}

// ---------------------------------------------------------
// Parsing capacità da stringa DIMACS
// ---------------------------------------------------------
Capacity parse_capacity(const std::string& s_raw) {
    // Rimuove tutti gli spazi
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

        //sostituiti MinGW 13.2 NON implementa <numbers>
        replace("pi", "3.14159265358979323846");
        replace("e",  "2.71828182845904523536");


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
