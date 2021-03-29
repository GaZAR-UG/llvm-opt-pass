# llvm-opt-pass
This is a minimal LLVM optimization pass that allows one to experiment with the LLVM infrastructure for analysis and optimization.

## How to Build the Project?

Build the project using `$ make`.

Delete the auto-generated files using `$ make clean`.

## Compiling a C/C++ Compilation Unit to LLVM IR

Use `$ clang++ -std=c++17 -Wall -Wextra -emit-llvm -S -fno-discard-value-names some-file.cpp` to compile the file `some-file.cpp` to LLVM IR.

## Running the Program and Analyzing LLVM IR

Use `./transformation some-file.ll` to run the small LLVM-IR-Analyzer program on the `some-file.ll` LLVM IR file.