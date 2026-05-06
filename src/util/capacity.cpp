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


    auto eval_simple = [&](const std::string& t) -> double {
    // caso a/b
    size_t pos = t.find('/');
    if (pos != std::string::npos) {
        double a = std::stod(t.substr(0, pos));
        double b = std::stod(t.substr(pos + 1));
        return a / b;
    }
    // numero semplice
    return std::stod(t);
    };

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

        // MinGW non supporta <numbers>
        replace("pi", "3.14159265358979323846");
        replace("e",  "2.71828182845904523536");

        // --- Funzioni supportate ---

        // sqrt(x)
        if (expr.rfind("sqrt(", 0) == 0 && expr.back() == ')') {
            double x = std::stod(expr.substr(5, expr.size() - 6));
            return std::sqrt(x);
        }

        // log(x) = ln(x)
        if ((expr.rfind("log(", 0) == 0 || expr.rfind("ln(", 0) == 0) && expr.back() == ')') {
            size_t off = expr[1] == 'o' ? 4 : 3; // log( → 4, ln( → 3
            double x = std::stod(expr.substr(off, expr.size() - off - 1));
            return std::log(x);
        }

        // log10(x)
        if (expr.rfind("log10(", 0) == 0 && expr.back() == ')') {
            double x = std::stod(expr.substr(6, expr.size() - 7));
            return std::log10(x);
        }

        // log2(x)
        if (expr.rfind("log2(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(5, expr.size() - 6));
            return std::log2(x);
        }

        // sin(x)
        if (expr.rfind("sin(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(4, expr.size() - 5));
            return std::sin(x);
        }

        // cos(x)
        if (expr.rfind("cos(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(4, expr.size() - 5));
            return std::cos(x);
        }

        // tan(x)
        if (expr.rfind("tan(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(4, expr.size() - 5));
            return std::tan(x);
        }

        // exp(x)
        if (expr.rfind("exp(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(4, expr.size() - 5));
            return std::exp(x);
        }

        // abs(x)
        if (expr.rfind("abs(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(4, expr.size() - 5));
            return std::abs(x);
        }

        // fallback: prova a convertire come double
        try {
            return eval_simple(expr);
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
