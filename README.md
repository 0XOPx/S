# S Language ("S")

A small C-like language with its own compiler front-end and bytecode VM. The compiler parses S source into bytecode, and the VM executes it.

## Bootstrap Build (one-time)

This project produces standalone Windows `.exe` files **without** invoking any external compiler at compile time.
To keep a single compiler exe with no runtime files, the runtimes are embedded as byte arrays.

1) Build the runtime templates once:

```powershell
clang++ -std=c++17 -O2 -o runtime_x64.exe src/runtime.cpp
clang++ -std=c++17 -O2 -m32 -o runtime_x86.exe src/runtime.cpp
```

2) Generate embedded headers:

```powershell
clang++ -std=c++17 -O2 -o tools\\embed_runtimes.exe tools\\embed_runtimes.cpp
.\tools\\embed_runtimes.exe runtime_x64.exe runtime_x86.exe src\\embedded_runtime_x64.h src\\embedded_runtime_x86.h
```

3) Build the compiler:

```powershell
clang++ -std=c++17 -O2 -o scc.exe src/main.cpp
```

## Compile

```powershell
.\scc.exe path/to/your/file -o hello.exe
.\hello.exe
```

```powershell
.\scc.exe path/to/your/file -o hello_x86.exe --arch x86
.\hello_x86.exe
```

## Run (interpreter mode)

```powershell
.\scc.exe --run examples\hello.s
```

## Language Summary

- `int` variables and functions
- `if`, `else`, `while`, `return`
- arithmetic: `+ - * /`
- comparisons: `== != < <= > >=`
- builtin `print(expr)`
- function calls with integer arguments

## Notes / Limitations

- Only `int` type
- No block scoping (locals are function-scoped)
- No `for`, `break`, `continue`, or logical `&&` / `||`
- No strings yet

## Example

```c
int fib(int n) {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int main() {
    int i = 0;
    while (i < 8) {
        print(fib(i));
        i = i + 1;
    }
    return 0;
}
```

## You can use Pre-Compiled binaries!
