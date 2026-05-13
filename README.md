# MaxFlow Project — Build & Execution Guide

Questo repository contiene gli script Python per la generazione dei grafi e il codice C++ per la compilazione ed esecuzione dell'algoritmo di Max Flow.

## 1. Creazione dell'ambiente virtuale Python

```powershell
python -m venv venv
```

## 2. Attivazione dell'ambiente virtuale

```powershell
.\venv\Scripts\activate
```

## 3. Installazione dei pacchetti Python

Assicurati di avere `requirements.txt` nella root del progetto.

```powershell
pip install -r requirements.txt
```

## 4. Generazione dei grafi

Esegui lo script Python dedicato:

```powershell
python .\graphgenerator.py
```

## 5. Configurazione del progetto C++ con CMake (MinGW)

Genera i file di build:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
```

## 6. Compilazione

```powershell
cmake --build ./build
```

L'eseguibile risultante sarà disponibile nella directory principale del progetto.

## 7. Esecuzione dell'algoritmo MaxFlow

Esempio di esecuzione con parametri:

```powershell
.\maxflow.exe -pr -SINTH
```

## 8. Pulizia della build (opzionale)

```powershell
Remove-Item -Recurse -Force .\build
```
