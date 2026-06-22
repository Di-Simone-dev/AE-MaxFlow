# Guida di build ed esecuzione — Windows

Questa guida descrive come configurare l'ambiente, generare le istanze di
test, compilare l'eseguibile `maxflow.exe` ed eseguire i benchmark su Windows.

Sono descritti due percorsi alternativi per la build C++:
**MinGW + CMake da terminale** (consigliato, più semplice) oppure
**Visual Studio** (se preferisci un IDE completo).

## 1. Prerequisiti

- **MinGW-w64** (compilatore GCC per Windows) oppure **Visual Studio 2022**
  con il workload "Desktop development with C++"
- **CMake** ≥ 3.16 ([cmake.org](https://cmake.org/download/), spunta
  "Add CMake to system PATH" durante l'installazione)
- **Python** ≥ 3.11 ([python.org](https://www.python.org/downloads/),
  necessario per `tomllib`)
- `git` (es. [Git for Windows](https://git-scm.com/download/win))

Verifica le installazioni da PowerShell:

```powershell
g++ --version
cmake --version
python --version    # deve essere >= 3.11
```

Se usi MinGW, assicurati che `g++.exe` sia nel `PATH` (es. `C:\MinGW\bin`).

## 2. Clonazione del repository

```powershell
git clone https://github.com/Di-Simone-dev/AE-MaxFlow.git
cd AE-MaxFlow
```

Tutti i comandi successivi vanno eseguiti dalla **root del repository**: sia
l'eseguibile C++ che gli script Python leggono i file di configurazione
(`configs\configmain.toml`, `configs\config.toml`) con percorsi relativi alla
directory corrente.

## 3. Setup dell'ambiente Python

```powershell
python -m venv venv
.\venv\Scripts\Activate.ps1
pip install --upgrade pip
pip install -r requirements.txt
```

> **Nota — esecuzione script PowerShell**
> Se `Activate.ps1` viene bloccato dalla policy di esecuzione, esegui una
> volta (come utente corrente, non richiede privilegi di amministratore):
> ```powershell
> Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
> ```

> **Nota — encoding del file `requirements.txt`**
> Il file è salvato in **UTF-16** (con terminatori `CRLF`). Su molte
> installazioni di `pip` per Windows questo viene gestito correttamente; se
> invece ottieni un errore di parsing del tipo:
> ```
> ERROR: Invalid requirement: ...
> ```
> ricodificalo in UTF-8 prima di installare:
> ```powershell
> Get-Content requirements.txt -Encoding Unicode | Set-Content requirements_utf8.txt -Encoding UTF8
> pip install -r requirements_utf8.txt
> ```

## 4. Generazione dei grafi di test

```powershell
python graphgenerator.py
```

Lo script genera tutte le istanze sintetiche (`layered`, `grid`, Erdős–Rényi
DAG) configurate in `configs\config.toml`, organizzate per algoritmo
(`graphs\pr`, `graphs\cs`, `graphs\al`) e tipo di capacità
(`int`, `unit`, `rational`, `irrational`).

> Per i dataset reali (BVZ-tsukuba, KZ2-venus) assicurati che le relative
> directory esistano al percorso configurato in `configs\configmain.toml`
> (di default `graphs\BVZ-tsukuba` e `graphs\KZ2-venus`).

## 5. Compilazione — Opzione A: MinGW da terminale (consigliata)

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

Al termine, `maxflow.exe` viene copiato automaticamente nella **root del
progetto** (non in `build\`), perché `CMakeLists.txt` imposta
`CMAKE_RUNTIME_OUTPUT_DIRECTORY` sulla directory del codice sorgente. Verifica:

```powershell
dir .\maxflow.exe
```

## 5bis. Compilazione — Opzione B: Visual Studio

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

> **Attenzione (generatore multi-config):** con Visual Studio, CMake usa un
> generatore *multi-config* e per default colloca l'eseguibile in
> `build\Debug\maxflow.exe` (o `build\Release\maxflow.exe`), **non** nella
> root del progetto come avviene con MinGW. Poiché `main.cpp` risolve
> `configs\configmain.toml` rispetto alla cartella dell'eseguibile, lanciare
> `maxflow.exe` da `build\Debug\` farebbe fallire la lettura della
> configurazione. Per eseguirlo correttamente:
> ```powershell
> Copy-Item build\Debug\maxflow.exe . -Force
> .\maxflow.exe -fullsuite
> ```
> oppure copia anche le cartelle `configs\`, `graphs\` ecc. dentro
> `build\Debug\` prima di eseguire da lì. Per evitare questa complicazione è
> consigliabile usare l'Opzione A (MinGW).

Il repository include una configurazione `.vscode/tasks.json` già pronta per
compilare ed eseguire con Visual Studio direttamente da Visual Studio Code
(task "Build with CMake (Debug)", "Compile (CMake build)", "Run maxflow").

## 6. Esecuzione dei benchmark

L'eseguibile va lanciato dalla root del progetto (per la risoluzione dei
percorsi relativi nel TOML):

```powershell
# Push-Relabel su un singolo file
.\maxflow.exe -pr graphs\pr\int\layered_n250_d6\layered_n250_d6_seed0.max

# Capacity Scaling sul dataset reale KZ2-venus
.\maxflow.exe -cs -KZ2

# Almost Linear Time sulle istanze sintetiche configurate per "al"
.\maxflow.exe -al -SINTH

# Tutti gli algoritmi su tutte le directory di istanze sintetiche configurate
.\maxflow.exe -fullsuite
```

I CSV con i risultati vengono scritti in `benchmarksingoli\`,
`benchmarksintetici\`, `benchmarkBVZ\` o `benchmarkKZ2\` a seconda della
modalità (directory create automaticamente se assenti).

## 7. Aggregazione dei risultati e generazione dei grafici

```powershell
python aggregate_and_plot.py
```

Lo script (configurabile anche con `--config <percorso.toml>`) produce i CSV
aggregati (media/mediana dei tempi) e i grafici di scalabilità in `plots\`,
secondo le voci `[[runs]]` definite in `configs\config.toml`.

## 8. Risoluzione dei problemi più comuni

| Problema | Causa probabile | Soluzione |
|----------|------------------|-----------|
| `tomllib` non trovato in Python | Python < 3.11 | Aggiorna Python o usa `tomli` come backport (`pip install tomli`) e adatta l'import |
| Errore di parsing su `requirements.txt` | File codificato in UTF-16 | Vedi nota al punto 3 |
| `cmake` non riconosciuto come comando | CMake non aggiunto al `PATH` | Reinstalla CMake spuntando "Add CMake to system PATH", oppure aggiungilo manualmente |
| `g++` non riconosciuto come comando | MinGW non nel `PATH` | Aggiungi `C:\MinGW\bin` (o il percorso della tua installazione) alle variabili d'ambiente `PATH` |
| `maxflow.exe` non trova `configs\configmain.toml` | Eseguibile lanciato da `build\Debug\` (Visual Studio) o da una directory diversa dalla root | Esegui sempre dalla root del repo, vedi nota al punto 5bis |
| File `.max` non trovato durante `-SINTH`/`-fullsuite` | Grafi non ancora generati | Esegui `python graphgenerator.py` prima del benchmark |
| Caratteri accentati/simboli illeggibili nel terminale | Code page della console non UTF-8 | Esegui `chcp 65001` prima di lanciare `maxflow.exe`, oppure usa Windows Terminal |
