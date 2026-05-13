/**
 * @file capacity.cpp
 * @brief Implementazione delle funzioni dichiarate in capacity.hpp.
 *
 * Contiene:
 * - Una funzione di utilità statica (`contains_alpha`) per rilevare lettere
 *   in una stringa.
 * - `to_double()`: conversione generica da `Capacity` a `double`.
 * - `add_capacity()`: somma robusta con promozione minima del tipo.
 * - `parse_capacity()`: parser completo di stringhe DIMACS in `Capacity`.
 *
 */

#include "capacity.hpp"
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <numbers>

// ---------------------------------------------------------------------------
// Funzioni di utilità (solo uso interno)
// ---------------------------------------------------------------------------

/**
 * @brief Controlla se una stringa contiene almeno un carattere alfabetico.
 *
 * Usata internamente da `parse_capacity()` per distinguere stringhe numeriche
 * pure (es. `"3.14"`, `"7/4"`) da espressioni che contengono costanti o
 * funzioni simboliche (es. `"pi"`, `"sqrt(2)"`).
 *
 * @param s Stringa da esaminare.
 * @return `true` se la stringa contiene almeno una lettera, `false` altrimenti.
 */
static bool contains_alpha(const std::string& s) {
    for (char c : s)
        if (std::isalpha(static_cast<unsigned char>(c)))
            return true;
    return false;
}

// ---------------------------------------------------------------------------
// to_double
// ---------------------------------------------------------------------------

/**
 * @brief Converte una `Capacity` in un valore `double`.
 *
 * Utilizza `std::visit` per selezionare a compile-time la conversione
 * appropriata in base al tipo attivo nel variant:
 * - `int`      → `static_cast<double>`
 * - `Fraction` → `Fraction::to_double()`
 * - `double`   → restituzione diretta
 *
 * @param c La capacità da convertire.
 * @return Il valore numerico come `double`.
 *
 * @note Un `static_assert` a compile-time garantisce che nessun tipo non
 *       gestito possa raggiungere il ramo `else`.
 */
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

// ---------------------------------------------------------------------------
// add_capacity
// ---------------------------------------------------------------------------

/**
 * @brief Somma due `Capacity` con promozione minima del tipo.
 *
 * Utilizza un visitor binario (`std::visit` con due argomenti) per scegliere
 * a compile-time la strategia di somma ottimale:
 *
 * - `int + int`           → risultato `int`      (nessuna perdita di precisione)
 * - `Fraction + Fraction` → risultato `Fraction` (aritmetica esatta)
 * - `Fraction + int`      → risultato `Fraction` (l'intero viene promosso a `Fraction(n,1)`)
 * - `int + Fraction`      → risultato `Fraction` (simmetrico al caso precedente)
 * - qualsiasi altra coppia → risultato `double`  (fallback tramite `to_double`)
 *
 * @param a Primo addendo.
 * @param b Secondo addendo.
 * @return La somma come `Capacity` nel tipo più preciso compatibile.
 */
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

// ---------------------------------------------------------------------------
// parse_capacity
// ---------------------------------------------------------------------------

/**
 * @brief Analizza una stringa nel formato DIMACS e la converte in `Capacity`.
 *
 * ### Pipeline di parsing
 * 1. **Pre-processing**: rimuove tutti gli spazi bianchi dalla stringa.
 * 2. **Frazione esatta** (`"7/4"`): se è presente `/` e non ci sono lettere,
 *    viene costruita una `Fraction(num, den)`.
 * 3. **Espressione irrazionale** (stringa con lettere): le costanti `pi` ed `e`
 *    vengono sostituite con i loro valori numerici, poi si prova a valutare
 *    una delle funzioni matematiche supportate. Se nessuna corrisponde,
 *    si tenta una conversione `double` diretta.
 * 4. **Intero**: `std::stoi` con verifica che tutta la stringa sia consumata.
 * 5. **Floating point**: `std::stod` come fallback finale.
 *
 * ### Funzioni matematiche supportate
 * `sqrt(x)`, `log(x)` / `ln(x)`, `log10(x)`, `log2(x)`,
 * `sin(x)`, `cos(x)`, `tan(x)`, `exp(x)`, `abs(x)`.
 *
 * ### Lambda interna: `eval_simple`
 * Valuta espressioni della forma `"a"` o `"a/b"` (con `a`, `b` convertibili
 * in `double`). Usata per valutare l'argomento delle funzioni matematiche.
 *
 * @param s_raw Stringa da analizzare (può contenere spazi).
 * @return La `Capacity` corrispondente al valore riconosciuto.
 *
 * @throws std::runtime_error Con messaggio descrittivo nei seguenti casi:
 *         - denominatore zero in una frazione (es. `"5/0"`),
 *         - espressione irrazionale non riconoscibile (es. `"foo(3)"`),
 *         - formato completamente non riconosciuto.
 *
 * @par Esempio
 * @code
 * auto c1 = parse_capacity("42");       // int(42)
 * auto c2 = parse_capacity("3/4");      // Fraction(3, 4)
 * auto c3 = parse_capacity("3.14");     // double(3.14)
 * auto c4 = parse_capacity("sqrt(2)");  // double(1.41421...)
 * auto c5 = parse_capacity("pi");       // double(3.14159...)
 * @endcode
 */
Capacity parse_capacity(const std::string& s_raw) {
    // Rimuove tutti gli spazi
    std::string s;
    for (char c : s_raw)
        if (!std::isspace(static_cast<unsigned char>(c)))
            s.push_back(c);

    // -------------------------------------------------------------------
    // Caso FRAZIONE: "7/4"
    // -------------------------------------------------------------------
    if (s.find('/') != std::string::npos && !contains_alpha(s)) {
        auto pos = s.find('/');
        int num = std::stoi(s.substr(0, pos));
        int den = std::stoi(s.substr(pos + 1));
        if (den == 0)
            throw std::runtime_error("Divisione per zero in frazione");
        return Fraction(num, den);
    }

    /**
     * @brief Lambda di supporto: valuta una sottoespressione numerica semplice.
     *
     * Accetta stringhe della forma `"a"` oppure `"a/b"` dove `a` e `b` sono
     * convertibili in `double`. Usata per estrarre l'argomento delle funzioni
     * matematiche dopo la sostituzione delle costanti simboliche.
     *
     * @param t Sottostringa da valutare.
     * @return Il valore numerico come `double`.
     * @throws std::invalid_argument / std::out_of_range (da `std::stod`)
     *         se la sottostringa non è convertibile.
     */
    auto eval_simple = [&](const std::string& t) -> double {
        size_t pos = t.find('/');
        if (pos != std::string::npos) {
            double a = std::stod(t.substr(0, pos));
            double b = std::stod(t.substr(pos + 1));
            return a / b;
        }
        return std::stod(t);
    };

    // -------------------------------------------------------------------
    // Caso ESPRESSIONE IRRAZIONALE: contiene lettere
    // -------------------------------------------------------------------
    if (contains_alpha(s)) {
        std::string expr = s;

        /**
         * @brief Lambda di supporto: sostituisce tutte le occorrenze di `a`
         *        con `b` all'interno di `expr`.
         *
         * @param a Sottostringa da cercare.
         * @param b Stringa sostitutiva.
         */
        auto replace = [&](const std::string& a, const std::string& b) {
            size_t pos = 0;
            while ((pos = expr.find(a, pos)) != std::string::npos) {
                expr.replace(pos, a.size(), b);
                pos += b.size();
            }
        };

        // Sostituzione costanti simboliche
        // Nota: MinGW non supporta <numbers>, quindi si usano valori letterali.
        replace("pi", "3.14159265358979323846");
        replace("e",  "2.71828182845904523536");

        // --- Dispatch sulle funzioni matematiche supportate ---

        /** @note `sqrt(x)` — radice quadrata */
        if (expr.rfind("sqrt(", 0) == 0 && expr.back() == ')') {
            double x = std::stod(expr.substr(5, expr.size() - 6));
            return std::sqrt(x);
        }

        /** @note `log(x)` / `ln(x)` — logaritmo naturale */
        if ((expr.rfind("log(", 0) == 0 || expr.rfind("ln(", 0) == 0) && expr.back() == ')') {
            size_t off = expr[1] == 'o' ? 4 : 3; // "log(" → 4 char, "ln(" → 3 char
            double x = std::stod(expr.substr(off, expr.size() - off - 1));
            return std::log(x);
        }

        /** @note `log10(x)` — logaritmo in base 10 */
        if (expr.rfind("log10(", 0) == 0 && expr.back() == ')') {
            double x = std::stod(expr.substr(6, expr.size() - 7));
            return std::log10(x);
        }

        /** @note `log2(x)` — logaritmo in base 2 */
        if (expr.rfind("log2(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(5, expr.size() - 6));
            return std::log2(x);
        }

        /** @note `sin(x)` — seno (argomento in radianti) */
        if (expr.rfind("sin(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(4, expr.size() - 5));
            return std::sin(x);
        }

        /** @note `cos(x)` — coseno (argomento in radianti) */
        if (expr.rfind("cos(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(4, expr.size() - 5));
            return std::cos(x);
        }

        /** @note `tan(x)` — tangente (argomento in radianti) */
        if (expr.rfind("tan(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(4, expr.size() - 5));
            return std::tan(x);
        }

        /** @note `exp(x)` — esponenziale naturale (e^x) */
        if (expr.rfind("exp(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(4, expr.size() - 5));
            return std::exp(x);
        }

        /** @note `abs(x)` — valore assoluto */
        if (expr.rfind("abs(", 0) == 0 && expr.back() == ')') {
            double x = eval_simple(expr.substr(4, expr.size() - 5));
            return std::abs(x);
        }

        // Fallback: tentativo di conversione diretta dopo espansione delle costanti
        try {
            return eval_simple(expr);
        } catch (...) {
            throw std::runtime_error("Espressione irrazionale non supportata: " + s);
        }
    }

    // -------------------------------------------------------------------
    // Caso INTERO
    // -------------------------------------------------------------------
    try {
        size_t idx;
        int val = std::stoi(s, &idx);
        if (idx == s.size())   // tutta la stringa è stata consumata → è un intero puro
            return val;
    } catch (...) {}

    // -------------------------------------------------------------------
    // Caso FLOAT (fallback finale)
    // -------------------------------------------------------------------
    try {
        return std::stod(s);
    } catch (...) {
        throw std::runtime_error("Formato capacità non riconosciuto: " + s);
    }
}