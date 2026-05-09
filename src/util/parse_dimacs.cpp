/**
 * @file parse_dimacs.cpp
 * @brief Implementazione del parser DIMACS Max-Flow.
 *
 * Il file implementa:
 * - `trim_left()`: funzione di utilità statica per rimuovere spazi iniziali.
 * - `parse_dimacs()`: parser principale che legge riga per riga un file DIMACS
 *   e popola un `DimacsResult`.
 *
 * @see parse_dimacs.hpp
 *
 * @author  [inserire autore]
 * @date    [inserire data]
 */

#include "parse_dimacs.hpp"
#include "capacity.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// ---------------------------------------------------------------------------
// Funzioni di utilità (solo uso interno)
// ---------------------------------------------------------------------------

/**
 * @brief Rimuove gli spazi e i tab iniziali da una stringa.
 *
 * Usata internamente da `parse_dimacs()` per pulire la sottostringa della
 * capacità estratta con `std::getline` dopo aver già letto `u` e `v`.
 *
 * @param s Stringa di input (non modificata).
 * @return Copia della stringa senza caratteri di spaziatura iniziali
 *         (`' '` e `'\t'`). Se la stringa è composta di soli spazi,
 *         restituisce una stringa vuota.
 */
static std::string trim_left(const std::string& s) {
    size_t pos = s.find_first_not_of(" \t");
    return (pos == std::string::npos) ? "" : s.substr(pos);
}

// ---------------------------------------------------------------------------
// parse_dimacs
// ---------------------------------------------------------------------------

/**
 * @brief Legge un file DIMACS Max-Flow e costruisce il grafo corrispondente.
 *
 * ### Logica di parsing riga per riga
 *
 * La funzione legge il file una riga alla volta e dispaccio sul primo
 * carattere non-spazio (`tag`):
 *
 * | Tag | Azione                                                              |
 * |-----|---------------------------------------------------------------------|
 * | `c` | Riga di commento → ignorata.                                        |
 * | *(vuota)* | Riga vuota → ignorata.                                     |
 * | `p` | Intestazione: legge tipo (deve essere `"max"`), `n`, `m_actual`.   |
 * | `n` | Nodo speciale: associa l'id a `source` (se `s`) o `sink` (se `t`).|
 * | `a` | Arco: legge `u`, `v`, poi la capacità come stringa grezza.         |
 *
 * #### Gestione degli archi paralleli
 * Se la coppia `(u, v)` è già presente in `res.graph`, la nuova capacità
 * viene **sommata** a quella esistente tramite `add_capacity()`, preservando
 * il tipo più preciso (es. `Fraction + Fraction → Fraction`).
 *
 * #### Lettura della capacità
 * La capacità viene letta come stringa grezza con `std::getline` (anziché
 * `iss >> val`) per supportare espressioni simboliche come `"sqrt(2)"` o
 * `"3/4"`, che vengono poi delegate a `parse_capacity()`.
 *
 * ### Validazioni post-parsing
 * Al termine della lettura, la funzione verifica che:
 * - `res.source >= 0` (la riga `n <id> s` era presente).
 * - `res.sink >= 0`   (la riga `n <id> t` era presente).
 *
 * @param path Percorso del file DIMACS da leggere.
 * @return `DimacsResult` completamente popolato.
 *
 * @throws std::runtime_error Se il file non è apribile, se un tag non è
 *         riconosciuto, se il tipo dichiarato non è `"max"`, se un ruolo
 *         nodo non è `s` o `t`, se mancano sorgente o pozzo, o se
 *         `parse_capacity()` fallisce su una capacità malformata.
 */
DimacsResult parse_dimacs(const std::string& path) {
    std::ifstream fh(path);
    if (!fh)
        throw std::runtime_error("Impossibile aprire file: " + path);

    DimacsResult res;   // source e sink inizializzati a -1 dal costruttore di default

    std::string line;
    while (std::getline(fh, line)) {

        // Righe vuote e commenti → ignorate
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

            // La capacità è letta come stringa grezza per supportare
            // espressioni simboliche (es. "3/4", "sqrt(2)", "pi").
            std::string cap_str;
            std::getline(iss, cap_str);
            cap_str = trim_left(cap_str);

            Capacity cap = parse_capacity(cap_str);
            auto key = std::make_pair(u, v);

            if (res.graph.count(key) == 0) {
                // Primo arco (u,v): inserimento diretto.
                res.graph[key] = cap;
            } else {
                // Arco parallelo: accorpamento per somma delle capacità.
                res.graph[key] = add_capacity(res.graph[key], cap);
            }
        }

        else {
            throw std::runtime_error("Tag non riconosciuto nel file DIMACS");
        }
    }

    // Validazioni post-parsing: sorgente e pozzo devono essere stati dichiarati.
    if (res.source < 0)
        throw std::runtime_error("Nessun nodo sorgente (n <id> s) nel file DIMACS");

    if (res.sink < 0)
        throw std::runtime_error("Nessun nodo pozzo (n <id> t) nel file DIMACS");

    return res;
}