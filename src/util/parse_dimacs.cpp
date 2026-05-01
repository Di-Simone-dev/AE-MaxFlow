#include "parse_dimacs.hpp"
#include "capacity.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

DimacsResult parse_dimacs(const std::string& path) {
    std::ifstream fh(path);
    if (!fh)
        throw std::runtime_error("Impossibile aprire file: " + path);

    DimacsResult res;
    res.source = -1;
    res.sink   = -1;

    std::string line;
    while (std::getline(fh, line)) {
        if (line.empty() || line[0] == 'c')
            continue;

        std::istringstream iss(line);
        char tag;
        iss >> tag;

        if (tag == 'p') {
            std::string type;
            iss >> type >> res.n >> res.m_actual;
            if (type != "max")
                throw std::runtime_error("Atteso 'p max ...'");
        }
        else if (tag == 'n') {
            int id;
            char role;
            iss >> id >> role;
            if (role == 's') res.source = id;
            else if (role == 't') res.sink = id;
            else throw std::runtime_error("Ruolo nodo sconosciuto");
        }
        else if (tag == 'a') {
            int u, v;
            iss >> u >> v;
            std::string cap_str;
            std::getline(iss, cap_str);
            Capacity cap = parse_capacity(cap_str);

            auto key = std::make_pair(u, v);

            if (res.graph.count(key) == 0)
                res.graph[key] = cap;
            else {
                // somma: variant + variant
                std::visit([&](auto& old_val) {
                    using T = std::decay_t<decltype(old_val)>;

                    // Caso: old_val è int
                    if constexpr (std::is_same_v<T, int>) {
                        if (std::holds_alternative<int>(cap)) {
                            old_val += std::get<int>(cap);
                        }
                        else if (std::holds_alternative<Fraction>(cap)) {
                            res.graph[key] = Fraction(old_val, 1) + std::get<Fraction>(cap);
                        }
                        else { // double
                            res.graph[key] = static_cast<double>(old_val) + std::get<double>(cap);
                        }
                    }

                    // Caso: old_val è Fraction
                    else if constexpr (std::is_same_v<T, Fraction>) {
                        if (std::holds_alternative<int>(cap)) {
                            old_val = old_val + Fraction(std::get<int>(cap), 1);
                        }
                        else if (std::holds_alternative<Fraction>(cap)) {
                            old_val = old_val + std::get<Fraction>(cap);
                        }
                        else { // double
                            res.graph[key] = old_val.to_double() + std::get<double>(cap);
                        }
                    }

                    // Caso: old_val è double
                    else if constexpr (std::is_same_v<T, double>) {
                        if (std::holds_alternative<int>(cap)) {
                            old_val += std::get<int>(cap);
                        }
                        else if (std::holds_alternative<Fraction>(cap)) {
                            old_val += std::get<Fraction>(cap).to_double();
                        }
                        else {
                            old_val += std::get<double>(cap);
                        }
                    }
                }, res.graph[key]);
            }
        }
        else {
            throw std::runtime_error("Tag non riconosciuto");
        }
    }

    if (res.source < 0) throw std::runtime_error("Nessun nodo sorgente");
    if (res.sink   < 0) throw std::runtime_error("Nessun nodo pozzo");

    return res;
}
