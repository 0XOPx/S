# S REPL

Simple line-by-line REPL that runs statements through `scc.exe --run`.

## Build

```powershell
clang++ -std=c++17 -O2 -o repl.exe repl/repl.cpp
```

## Run

```powershell
.\repl.exe
```

## Notes

- One line = one statement.
- Use `:quit` to exit.
- Requires `compiler\\scc.exe` next to the `compiler` folder.
