@echo off

:: Build Runtime

clang++ -std=c++17 -O2 -o runtime_x64.exe src/runtime.cpp
clang++ -std=c++17 -O2 -m32 -o runtime_x86.exe src/runtime.cpp

:: Embed Runtime

clang++ -std=c++17 -O2 -o tools\\embed_runtimes.exe tools\\embed_runtimes.cpp
.\tools\\embed_runtimes.exe runtime_x64.exe runtime_x86.exe src\\embedded_runtime_x64.h src\\embedded_runtime_x86.h

:: Build

clang++ -std=c++17 -O2 -o scc.exe src/main.cpp

:: Clean up

del runtime_x64.exe
del runtime_x86.exe
