cmake -S . -B build -G "MinGW Makefiles"
cmake --build ./build


doxygen -g          # crea il Doxyfile
doxygen Doxyfile    # genera la documentazione
start html/index.html
