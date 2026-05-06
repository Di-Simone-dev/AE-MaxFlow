#include "parse_dimacs.hpp"
#include "capacity.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

static std::string trim_left(const std::string& s) {
    size_t pos = s.find_first_not_of(" \t");
    return (pos == std::string::npos) ? "" : s.substr(pos);
}

DimacsResult parse_dimacs(const std::string& path) {
    std::ifstream fh(path);
    if (!fh)
        throw std::runtime_error("Impossibile aprire file: " + path);

    DimacsResult res;   // ora il costruttore di default è valido

    std::string line;
    while (std::getline(fh, line)) {

        if (line.empty() || line[0] == 'c')
            continue;

        std::istringstream iss(line);
        char tag;
        iss >> tag;

        // -------------------------
        // p <type> <n> <m>
        // -------------------------
        if (tag == 'p') {
            std::string type;
            iss >> type >> res.n >> res.m_actual;

            if (type != "max")
                throw std::runtime_error("Atteso 'p max <n> <m>' nel file DIMACS");
        }

        // -------------------------
        // n <id> <role>
        // -------------------------
        else if (tag == 'n') {
            int id;
            std::string role;
            iss >> id >> role;

            if (role == "s")      res.source = id;
            else if (role == "t") res.sink   = id;
            else
                throw std::runtime_error("Ruolo nodo sconosciuto: " + role);
        }

        // -------------------------
        // a <u> <v> <cap>
        // -------------------------
        else if (tag == 'a') {
            int u, v;
            iss >> u >> v;

            std::string cap_str;
            std::getline(iss, cap_str);
            cap_str = trim_left(cap_str);

            Capacity cap = parse_capacity(cap_str);
            auto key = std::make_pair(u, v);

            if (res.graph.count(key) == 0) {
                res.graph[key] = cap;
            } else {
                res.graph[key] = add_capacity(res.graph[key], cap);
            }
        }

        else {
            throw std::runtime_error("Tag non riconosciuto nel file DIMACS");
        }
    }

    if (res.source < 0)
        throw std::runtime_error("Nessun nodo sorgente (n <id> s) nel file DIMACS");

    if (res.sink < 0)
        throw std::runtime_error("Nessun nodo pozzo (n <id> t) nel file DIMACS");

    

    return res;
}
