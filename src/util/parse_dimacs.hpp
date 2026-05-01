#pragma once
#include <string>
#include <unordered_map>
#include <utility>
#include "capacity.hpp"

struct DimacsResult {
    std::unordered_map<std::pair<int,int>, Capacity> graph;
    int source;
    int sink;
    int n;
    int m_actual;
};

DimacsResult parse_dimacs(const std::string& path);
