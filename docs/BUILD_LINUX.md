# Guida di build ed esecuzione — Linux

Questa guida descrive come configurare l'ambiente, generare le istanze di test,
compilare l'eseguibile `maxflow` ed eseguire i benchmark su Linux.

## 1. Prerequisiti

- **Compilatore C++** con supporto C++17 (GCC ≥ 9 o Clang ≥ 10)
- **CMake** ≥ 3.16
- **Python** ≥ 3.11 (necessario per il modulo `tomllib`, usato dagli script di generazione/aggregazione)
- `git`

Su distribuzioni basate su Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake python3 python3-venv python3-pip git
```

Verifica le versioni installate:

```bash
g++ --version
cmake --version
python3 --version   # deve essere >= 3.11
```

## 2. Clonazione del repository

```bash
git clone https://github.com/Di-Simone-dev/AE-MaxFlow.git
cd AE-MaxFlow
```

Tutti i comandi successivi vanno eseguiti dalla **root del repository**: sia
l'eseguibile C++ che gli script Python leggono i file di configurazione
(`configs/configmain.toml`, `configs/config.toml`) con percorsi relativi alla
directory corrente.

## 3. Setup dell'ambiente Python

```bash
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
```

> **Nota — encoding del file `requirements.txt`**
> Il file presente nel repository è salvato in **UTF-16** (con terminatori
> `CRLF`), un retaggio dell'ambiente Windows in cui è stato generato. Su
> Linux questo può causare un errore di `pip` simile a:
> ```
> ERROR: Invalid requirement: ...
> ```
> Se accade, ricodifica il file in UTF-8 prima di installare:
> ```bash
> iconv -f utf-16 -t utf-8 requirements.txt -o requirements_utf8.txt
> pip install -r requirements_utf8.txt
> ```
> oppure installa le dipendenze direttamente:
> ```bash
> pip install numpy pandas matplotlib pillow
> ```

## 4. Generazione dei grafi di test

Lo script `graphgenerator.py` genera tutte le istanze sintetiche (`layered`,
`grid`, Erdős–Rényi DAG) configurate in `configs/config.toml`, organizzate per
algoritmo (`graphs/pr`, `graphs/cs`, `graphs/al`) e tipo di capacità
(`int`, `unit`, `rational`, `irrational`):

```bash
python3 graphgenerator.py
```

L'esecuzione stampa a video l'avanzamento per ciascun batch (PR, CS, AL).

> Per i dataset reali (BVZ-tsukuba, KZ2-venus) assicurati che le relative
> directory esistano al percorso configurato in `configs/configmain.toml`
> (sezione `[dataset_reali.-BVZ]` / `[dataset_reali.-KZ2]`, di default
> `graphs/BVZ-tsukuba` e `graphs/KZ2-venus`).

## 5. Compilazione con CMake

```bash
cmake -S . -B build -G "Unix Makefiles"
cmake --build build -j"$(nproc)"
```

Al termine della build, l'eseguibile `maxflow` viene copiato automaticamente
nella **root del progetto** (non in `build/`), perché `CMakeLists.txt` imposta
`CMAKE_RUNTIME_OUTPUT_DIRECTORY` sulla directory del codice sorgente. Verifica:

```bash
ls -la ./maxflow
```

## 6. Esecuzione dei benchmark

L'eseguibile va lanciato dalla root del progetto (per la risoluzione dei
percorsi relativi nel TOML):

```bash
# Push-Relabel su un singolo file
./maxflow -pr graphs/pr/int/layered_n250_d6/layered_n250_d6_seed0.max

# Capacity Scaling sul dataset reale KZ2-venus
./maxflow -cs -KZ2

# Almost Linear Time sulle istanze sintetiche configurate per "al"
./maxflow -al -SINTH

# Tutti gli algoritmi su tutte le directory di istanze sintetiche configurate
./maxflow -fullsuite
```

I CSV con i risultati vengono scritti in `benchmarksingoli/`,
`benchmarksintetici/`, `benchmarkBVZ/` o `benchmarkKZ2/` a seconda della
modalità (directory create automaticamente se assenti).

## 7. Aggregazione dei risultati e generazione dei grafici

Una volta raccolti i CSV grezzi (`benchmarksintetici/*.csv`):

```bash
python3 aggregate_and_plot.py
```

Lo script (configurabile anche con `--config <percorso.toml>`) produce i CSV
aggregati (media/mediana dei tempi) e i grafici di scalabilità in `plots/`,
secondo le voci `[[runs]]` definite in `configs/config.toml`.

## 8. Build di tipo Debug (opzionale)

Per una build con simboli di debug (utile con `gdb`/`valgrind`):

```bash
cmake -S . -B build-debug -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j"$(nproc)"
```

## 9. Risoluzione dei problemi più comuni

| Problema | Causa probabile | Soluzione |
|----------|------------------|-----------|
| `tomllib` non trovato in Python | Python < 3.11 | Aggiorna Python o usa `tomli` come backport (`pip install tomli`) e adatta l'import |
| Errore di parsing su `requirements.txt` | File codificato in UTF-16 | Vedi nota al punto 3 |
| `maxflow` non trova `configs/configmain.toml` | Eseguibile lanciato da una directory diversa dalla root del progetto | Esegui sempre `./maxflow ...` dalla root del repo |
| File `.max` non trovato durante `-SINTH`/`-fullsuite` | Grafi non ancora generati | Esegui `python3 graphgenerator.py` prima del benchmark |
| Errori di compilazione legati a Eigen | Sottocartella `Eigen/` mancante o spostata | Verifica che `Eigen/` sia presente nella root (è una libreria header-only già inclusa nel repository) |
