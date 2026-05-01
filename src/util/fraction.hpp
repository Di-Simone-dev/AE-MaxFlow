#pragma once
#include <stdexcept>
#include <numeric>
#include <string>

class Fraction {
public:
    int num;  // numeratore
    int den;  // denominatore > 0

    Fraction(int n = 0, int d = 1) {
        if (d == 0)
            throw std::runtime_error("Denominatore zero in Fraction");
        if (d < 0) { n = -n; d = -d; }
        int g = std::gcd(n, d);
        num = n / g;
        den = d / g;
    }

    double to_double() const {
        return static_cast<double>(num) / static_cast<double>(den);
    }

    // operatori aritmetici
    Fraction operator+(const Fraction& other) const {
        return Fraction(num * other.den + other.num * den,
                        den * other.den);
    }

    Fraction operator-(const Fraction& other) const {
        return Fraction(num * other.den - other.num * den,
                        den * other.den);
    }

    Fraction operator*(const Fraction& other) const {
        return Fraction(num * other.num, den * other.den);
    }

    Fraction operator/(const Fraction& other) const {
        if (other.num == 0)
            throw std::runtime_error("Divisione per zero in Fraction");
        return Fraction(num * other.den, den * other.num);
    }

    std::string str() const {
        return std::to_string(num) + "/" + std::to_string(den);
    }
};
