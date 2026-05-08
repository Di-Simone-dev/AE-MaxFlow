#include "min_cost_flow_instance.hpp"
#include <stdexcept>
#include <numeric>

static const double _LOG2 = 0.6931471805599453;

// ---------------------------------------------------------------------------
// Costruttore principale
// ---------------------------------------------------------------------------
MinCostFlow::MinCostFlow(
    const std::vector<std::pair<int,int>>& edges_,
    const Eigen::VectorXd& c_,
    const Eigen::VectorXd& u_lower_,
    const Eigen::VectorXd& u_upper_,
    long long optimal_cost_
) {
    m = static_cast<int>(edges_.size());

    // n = max indice di nodo + 1
    int max_node = 0;
    for (auto& [a, b] : edges_)
        max_node = std::max(max_node, std::max(a, b));
    n = max_node + 1;

    edges    = edges_;
    c        = c_;
    c_org    = c_;
    u_lower  = u_lower_;
    u_upper  = u_upper_;
    optimal_cost = optimal_cost_;

    // U = max capacità in valore assoluto
    U = static_cast<long long>(std::max(u_upper.cwiseAbs().maxCoeff(),
                                  u_lower.cwiseAbs().maxCoeff()));

    alpha = 1.0 / std::log2(1000.0 * m * static_cast<double>(U));

    // Verifiche di consistenza
    assert(static_cast<int>(edges.size())    == m);
    assert(static_cast<int>(c.size())        == m);
    assert(static_cast<int>(u_lower.size())  == m);
    assert(static_cast<int>(u_upper.size())  == m);

    // Matrice di incidenza B (m x n)
    B = Eigen::MatrixXd::Zero(m, n);
    for (int e = 0; e < m; ++e) {
        auto [a, b] = edges[e];
        B(e, a) =  1.0;
        B(e, b) = -1.0;
    }

    // Mappa archi non orientata
    for (int e = 0; e < m; ++e) {
        auto [a, b] = edges[e];
        undirected_edge_to_indices[{a, b}].push_back(e);
        undirected_edge_to_indices[{b, a}].push_back(e);
    }

    // Lista di adiacenza
    adj.resize(n);
    for (int e = 0; e < m; ++e) {
        auto [a, b] = edges[e];
        adj[a].push_back(e);
        adj[b].push_back(e);
    }
}

// ---------------------------------------------------------------------------
// clone()
// ---------------------------------------------------------------------------
MinCostFlow MinCostFlow::clone() const {
    MinCostFlow obj;
    obj.m            = m;
    obj.n            = n;
    obj.edges        = edges;
    obj.c            = c;
    obj.c_org        = c_org;
    obj.u_lower      = u_lower;
    obj.u_upper      = u_upper;
    obj.optimal_cost = optimal_cost;
    obj.U            = U;
    obj.alpha        = alpha;
    obj.B            = B;
    obj.undirected_edge_to_indices = undirected_edge_to_indices;
    obj.adj          = adj;
    return obj;
}

// ---------------------------------------------------------------------------
// from_max_flow_instance()
// ---------------------------------------------------------------------------
MinCostFlow MinCostFlow::from_max_flow_instance(
    const std::vector<std::pair<int,int>>& edges,
    int s, int t,
    long long optimal_flow,
    const std::vector<long long>& capacities,
    const std::vector<long long>* lower_capacities
) {
    // Aggiunge arco di ritorno (t -> s)
    auto new_edges = edges;
    new_edges.push_back({t, s});
    int total = static_cast<int>(new_edges.size());

    // Costi: 0 su tutti gli archi, -1 sull'arco di ritorno
    Eigen::VectorXd c = Eigen::VectorXd::Zero(total);
    c[total - 1] = -1.0;

    // Capacità inferiori
    Eigen::VectorXd u_lower = Eigen::VectorXd::Zero(total);
    if (lower_capacities) {
        for (int i = 0; i < static_cast<int>(lower_capacities->size()); ++i)
            u_lower[i] = (*lower_capacities)[i];
        // u_lower[total-1] resta 0
    }

    // Capacità superiori
    Eigen::VectorXd u_upper(total);
    long long cap_sum = 0;
    for (int i = 0; i < static_cast<int>(capacities.size()); ++i) {
        u_upper[i] = capacities[i];
        cap_sum += capacities[i];
    }
    u_upper[total - 1] = cap_sum;

    return MinCostFlow(new_edges, c, u_lower, u_upper, -optimal_flow);
}

// ---------------------------------------------------------------------------
// phi()
// ---------------------------------------------------------------------------
double MinCostFlow::phi(const Eigen::VectorXd& f) const {
    double cur_cost = c.dot(f);

    double objective = 20.0 * m * std::log(cur_cost - optimal_cost) / _LOG2;

    // Barriere: (u_upper - f)^(-alpha) + (f - u_lower)^(-alpha)
    Eigen::VectorXd upper_barriers = (u_upper - f).array().pow(-alpha);
    Eigen::VectorXd lower_barriers = (f - u_lower).array().pow(-alpha);

    double barrier = (upper_barriers + lower_barriers).sum();

    return objective + barrier;
}

// ---------------------------------------------------------------------------
// calc_gradients()
// ---------------------------------------------------------------------------
Eigen::VectorXd MinCostFlow::calc_gradients(const Eigen::VectorXd& f) const {
    double cur_cost = c.dot(f);

    Eigen::VectorXd objective = (20.0 * m / (cur_cost - optimal_cost)) * c;

    Eigen::VectorXd left  =  alpha * (u_upper - f).array().pow(-1.0 - alpha).matrix();
    Eigen::VectorXd right =  alpha * (f - u_lower).array().pow(-1.0 - alpha).matrix();

    return objective + left - right;
}

// ---------------------------------------------------------------------------
// calc_lengths()
// ---------------------------------------------------------------------------
Eigen::VectorXd MinCostFlow::calc_lengths(const Eigen::VectorXd& f) const {
    Eigen::VectorXd left  = (u_upper - f).array().pow(-1.0 - alpha).matrix();
    Eigen::VectorXd right = (f - u_lower).array().pow(-1.0 - alpha).matrix();
    return left + right;
}

// ---------------------------------------------------------------------------
// edges_between()
// ---------------------------------------------------------------------------
std::vector<int> MinCostFlow::edges_between(int a, int b) const {
    auto it = undirected_edge_to_indices.find({a, b});
    if (it == undirected_edge_to_indices.end())
        return {};
    return it->second;
}

// ---------------------------------------------------------------------------
// add_vertex()
// ---------------------------------------------------------------------------
int MinCostFlow::add_vertex() {
    n += 1;
    // Aggiunge una colonna a B (nodo non ancora connesso)
    Eigen::MatrixXd new_B(m, n);
    new_B.leftCols(n - 1) = B;
    new_B.col(n - 1).setZero();
    B = std::move(new_B);
    adj.push_back({});
    return n - 1;
}

// ---------------------------------------------------------------------------
// add_edge()
// ---------------------------------------------------------------------------
void MinCostFlow::add_edge(int a, int b, double c_val, double ul, double uu) {
    assert(a < n);
    assert(b < n);

    edges.push_back({a, b});
    int e = static_cast<int>(edges.size()) - 1;

    undirected_edge_to_indices[{a, b}].push_back(e);
    undirected_edge_to_indices[{b, a}].push_back(e);

    adj[a].push_back(e);
    adj[b].push_back(e);

    m += 1;

    // Estendi i vettori numpy → conservativeResize in Eigen
    c.conservativeResize(m);       c[m - 1] = c_val;
    u_lower.conservativeResize(m); u_lower[m - 1] = ul;
    u_upper.conservativeResize(m); u_upper[m - 1] = uu;

    // Aggiunge una riga a B per il nuovo arco
    Eigen::MatrixXd new_B(m, n);
    new_B.topRows(m - 1) = B;
    new_B.row(m - 1).setZero();
    new_B(m - 1, a) =  1.0;
    new_B(m - 1, b) = -1.0;
    B = std::move(new_B);
}