@echo off
setlocal
cd /d "%~dp0" || exit /b 1

if not exist "runtime" mkdir runtime
if not exist "dist" mkdir dist

cd /d "%~dp0compiler" || exit /b 1
clang++ -std=c++17 -O2 -o ..\runtime\runtime_x64.exe src\runtime.cpp || exit /b 1
clang++ -std=c++17 -O2 -m32 -o ..\runtime\runtime_x86.exe src\runtime.cpp || exit /b 1
clang++ -std=c++17 -O2 -o tools\embed_runtimes.exe tools\embed_runtimes.cpp || exit /b 1
tools\embed_runtimes.exe ..\runtime\runtime_x64.exe ..\runtime\runtime_x86.exe src\embedded_runtime_x64.h src\embedded_runtime_x86.h || exit /b 1
clang++ -std=c++17 -O2 -o scc.exe src\main.cpp || exit /b 1
cd /d .. || exit /b 1
clang++ -std=c++17 -O2 -o repl.exe repl\repl.cpp || exit /b 1

copy /y compiler\scc.exe dist\scc.exe >nul
copy /y repl.exe dist\repl.exe >nul

powershell -NoProfile -Command "Compress-Archive -Path 'dist\*' -DestinationPath 'dist.zip' -Force" || exit /b 1
echo Done.
