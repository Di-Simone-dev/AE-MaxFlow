# MaxFlow Project

Implementazione dell'algoritmo di Max Flow in C++, con generazione dei grafi di test tramite script Python.

## Struttura del progetto

- `graphgenerator.py` — script Python per la generazione dei grafi usati come input dell'algoritmo.
- `requirements.txt` — dipendenze Python necessarie per eseguire lo script di generazione.
- `CMakeLists.txt` — configurazione CMake per la compilazione del codice C++.
- Codice sorgente C++ dell'algoritmo di Max Flow.

## Requisiti

- Python 3.x con `venv`
- CMake
- Un compilatore C++ (MinGW su Windows, GCC/Clang su Linux)

## Guide di build ed esecuzione

Le istruzioni dettagliate per la generazione dei grafi, la compilazione e l'esecuzione dell'algoritmo sono disponibili nelle guide dedicate per ciascun sistema operativo:

- [Guida per Windows](./BUILD_WINDOWS.md)
- [Guida per Linux](./BUILD_LINUX.md)

## Esecuzione rapida

In sintesi, il flusso di lavoro tipico prevede: creazione e attivazione di un ambiente virtuale Python, installazione delle dipendenze, generazione dei grafi, configurazione e compilazione del progetto C++ con CMake, ed esecuzione dell'eseguibile risultante con i parametri desiderati (es. `-pr -SINTH`).
