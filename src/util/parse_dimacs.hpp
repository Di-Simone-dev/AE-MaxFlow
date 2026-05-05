#pragma once
#include <map>
#include <utility>
#include "capacity.hpp"   // contiene using Capacity = std::variant<int, Fraction, double>;

struct DimacsResult {
    int n;                       // numero nodi
    int m_actual;                // numero archi letti
    int source;                  // nodo sorgente
    int sink;                    // nodo pozzo

    // grafo: (u,v) -> capacità
    std::map<std::pair<int,int>, Capacity> graph;

    // Costruttore di default robusto
    DimacsResult()
        : n(0),
          m_actual(0),
          source(-1),
          sink(-1),
          graph()
    {}

    // Costruttore comodo opzionale
    DimacsResult(int n_, int m_, int s_, int t_)
        : n(n_),
          m_actual(m_),
          source(s_),
          sink(t_),
          graph()
    {}
};
