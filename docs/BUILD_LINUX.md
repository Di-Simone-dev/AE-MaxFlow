# MaxFlow Project — Build & Execution Guide (Linux)

Questa guida descrive i passaggi per generare i grafi e compilare/eseguire l'algoritmo di Max Flow su sistemi Linux.

## 0. Prerequisiti

Assicurati di avere installati i pacchetti di base per la compilazione e Python con i moduli `venv`:

```bash
sudo apt update
sudo apt install build-essential cmake python3 python3-venv python3-pip
```

## 1. Creazione dell'ambiente virtuale Python

```bash
python3 -m venv venv
```

## 2. Attivazione dell'ambiente virtuale

```bash
source venv/bin/activate
```

## 3. Installazione dei pacchetti Python

Assicurati di avere `requirements.txt` nella root del progetto.

```bash
pip install -r requirements.txt
```

## 4. Generazione dei grafi

Esegui lo script Python dedicato:

```bash
python3 ./graphgenerator.py
```

## 5. Configurazione del progetto C++ con CMake

Genera i file di build (Makefile standard):

```bash
cmake -S . -B build -G "Unix Makefiles"
```

## 6. Compilazione

```bash
cmake --build ./build
```

L'eseguibile risultante sarà disponibile nella directory `build` (a meno che il `CMakeLists.txt` non specifichi diversamente).

## 7. Esecuzione dell'algoritmo MaxFlow

Assicurati che l'eseguibile abbia i permessi di esecuzione, poi avvialo:

```bash
chmod +x ./build/maxflow
./build/maxflow -pr -SINTH
```

## 8. Aggregazione dati e generazione grafici

Esegui lo script Python dedicato:

```bash
python3 ./aggregate_and_plot.py 
```

## 9. Pulizia della build (opzionale)

```bash
rm -rf ./build
```