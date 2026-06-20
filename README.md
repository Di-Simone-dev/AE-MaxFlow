# AE-MaxFlow

Implementazione e benchmarking sperimentale di tre algoritmi di **flusso massimo** su grafi in formato DIMACS (`.max`):

| Algoritmo                          | Flag CLI |
|-------------------------------------|----------|
| Push-Relabel                        | `-pr`    |
| Capacity Scaling                    | `-cs`    |
| Almost Linear Time (min-cost-flow based) | `-al`    |

Il progetto è composto da un motore C++ ad alte prestazioni per l'esecuzione degli algoritmi e da una pipeline Python per la generazione dei grafi di test, l'aggregazione dei risultati e la produzione di grafici/tabelle LaTeX per l'analisi sperimentale.

## Struttura del progetto

```
AE-MaxFlow/
├── CMakeLists.txt              # build system (eseguibile "maxflow")
├── requirements.txt            # dipendenze Python
├── graphgenerator.py           # generatore di grafi di test (DIMACS .max)
├── aggregate_and_plot.py       # aggregazione CSV + generazione grafici
├── Report_sperimentale_Algorithm_Engineering.pdf
│
├── configs/
│   ├── config.toml             # config per gli script Python (graphgenerator / aggregate_and_plot)
│   └── configmain.toml         # config per l'eseguibile C++ (flag, percorsi CSV, dataset reali)
│
├── docs/
│   ├── BUILD_LINUX.md          # guida di build/esecuzione (Linux)
│   ├── BUILD_WINDOWS.md        # guida di build/esecuzione (Windows, MinGW)
│   ├── doxygen.txt             # configurazione Doxygen
│   └── html/                   # documentazione generata da Doxygen
│
├── Eigen/                      # libreria header-only di terze parti (dipendenza)
├── build/                      # output di CMake
│
├── src/
│   ├── main.cpp                # entry point, parsing CLI, orchestrazione benchmark
│   ├── push_relabel/           # algoritmo Push-Relabel
│   ├── capacity_scaling/       # algoritmo Capacity Scaling
│   ├── almost_linear/          # algoritmo Almost Linear Time (+ min-cost-flow, Howard, feasible flow)
│   └── util/                   # parser DIMACS, capacità razionali (Fraction), scalatura, hashing
│
└── Dati_PAPER/                  # dataset e risultati usati nel report sperimentale
    ├── graphs_PAPER/             # istanze .max usate per generare i risultati del paper
    ├── benchmarksintetici_PAPER/ # CSV grezzi/aggregati + tabelle .tex (tables.py)
    └── plots_PAPER/              # grafici .png per algoritmo/topologia/tipo di capacità

# Directory generate a runtime (non versionate, vedi .gitignore):
#   graphs/, benchmarksingoli/, benchmarksintetici/, benchmarkBVZ/, benchmarkKZ2/, plots/
```

## Funzionalità

- **Tre algoritmi di flusso massimo** intercambiabili da riga di comando: Push-Relabel, Capacity Scaling, Almost Linear Time.
- **Gestione di capacità eterogenee**: intere, razionali (`Fraction`, scalate tramite MCM dei denominatori) e irrazionali/double (gestite con eps numerico). L'algoritmo Almost Linear Time supporta solo capacità intere.
- **Quattro modalità di esecuzione**:
  - singolo file `.max`;
  - dataset reali DIMACS (BVZ-tsukuba, KZ2-venus), con verifica di correttezza rispetto al file `.sol` quando disponibile;
  - dataset sintetici (grafi `layered`, `grid`, Erdős–Rényi DAG `erdag`), con misurazione su più run (scarto del warmup, calcolo della mediana);
  - `-fullsuite`: esegue automaticamente tutti gli algoritmi su tutte le directory di istanze sintetiche configurate.
- **Output CSV** con intestazioni dedicate per ciascuna modalità (numero nodi/archi, tempo in secondi con 17 cifre decimali, flusso calcolato, file grafo, correttezza).
- **Configurazione esterna via TOML** (`configs/configmain.toml`): flag CLI, percorsi dei CSV di output, directory delle istanze sintetiche e metadati dei dataset reali (con fallback hardcoded se assenti).
- **Pipeline Python complementare**:
  - `graphgenerator.py`: genera le famiglie di grafi sintetici (layered, grid, Erdős–Rényi DAG) con capacità intere/unitarie/razionali/irrazionali, configurabile tramite `configs/config.toml`;
  - `aggregate_and_plot.py`: aggrega i CSV di output (media/mediana dei tempi) e produce i grafici di scalabilità, con logica differenziata per Capacity Scaling rispetto a Push-Relabel/Almost Linear Time;
  - `Dati_PAPER/benchmarksintetici_PAPER/tables.py`: genera le tabelle LaTeX dei risultati a partire dai CSV aggregati.
- **Documentazione tecnica** generata con Doxygen (`docs/html/`).

## Argomenti da riga di comando

L'eseguibile `maxflow` accetta un flag dell'algoritmo e, salvo per `-fullsuite`, un secondo argomento che indica la modalità/il target:

```
./maxflow <flag_algoritmo> <dataset|file.max>
./maxflow -fullsuite
```

**Flag dell'algoritmo** (definiti in `configs/configmain.toml`, sezione `[algs]`):

| Flag   | Algoritmo selezionato |
|--------|------------------------|
| `-pr`  | Push-Relabel           |
| `-cs`  | Capacity Scaling       |
| `-al`  | Almost Linear Time     |

**Secondo argomento** (modalità di esecuzione):

| Argomento        | Effetto |
|-------------------|---------|
| `-BVZ`            | Benchmark sul dataset reale BVZ-tsukuba (16 istanze) |
| `-KZ2`            | Benchmark sul dataset reale KZ2-venus (22 istanze) |
| `-SINTH`          | Benchmark sulle istanze sintetiche configurate per l'algoritmo selezionato (`layered`, `grid`, `erdag`) |
| `<percorso.max>`  | Esegue l'algoritmo su un singolo file DIMACS `.max` |

**Modalità speciale** (nessun secondo argomento):

| Argomento      | Effetto |
|----------------|---------|
| `-fullsuite`   | Esegue tutti gli algoritmi configurati su tutte le directory di istanze sintetiche (`[instances_dir]` in `configmain.toml`) |

### Esempi

```bash
# Push-Relabel su un singolo file
./maxflow -pr graphs/pr/int/layered_n1000_d6/layered_n1000_d6_seed0.max

# Capacity Scaling sul dataset reale KZ2-venus
./maxflow -cs -KZ2

# Almost Linear Time sulle istanze sintetiche configurate per "al"
./maxflow -al -SINTH

# Tutti gli algoritmi su tutti i dataset sintetici
./maxflow -fullsuite
```

## Requisiti

- Python 3.x con `venv` (lo script di generazione usa `tomllib`, quindi richiede Python ≥ 3.11)
- CMake ≥ 3.16
- Un compilatore C++17 (MinGW su Windows, GCC/Clang su Linux)

Dipendenze Python (`requirements.txt`): `numpy`, `pandas`, `matplotlib`, `pillow`, e relative dipendenze indirette (`contourpy`, `cycler`, `fonttools`, `kiwisolver`, `packaging`, `pyparsing`, `python-dateutil`, `six`, `tzdata`).

## Guide di build ed esecuzione

Istruzioni dettagliate per generazione dei grafi, compilazione ed esecuzione:

- [Guida per Linux](docs/BUILD_LINUX.md)
- [Guida per Windows](docs/BUILD_WINDOWS.md)

## Esecuzione rapida

```bash
# 1. Ambiente Python
python3 -m venv venv
source venv/bin/activate          # Windows: .\venv\Scripts\activate
pip install -r requirements.txt

# 2. Generazione dei grafi di test
python3 graphgenerator.py

# 3. Build C++ (CMake)
cmake -S . -B build -G "Unix Makefiles"   # Windows: -G "MinGW Makefiles"
cmake --build ./build

# 4. Esecuzione del benchmark
./maxflow -fullsuite                       # oppure -pr/-cs/-al con -BVZ, -KZ2, -SINTH o un file .max

# 5. Aggregazione dei risultati e generazione dei grafici
python3 aggregate_and_plot.py
```

## Documentazione tecnica

La documentazione del codice C++ (classi, funzioni, formati dei file) è generata con Doxygen e disponibile in `docs/html/index.html`.