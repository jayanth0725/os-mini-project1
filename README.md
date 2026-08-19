# Custom C-Shell

A custom, POSIX-compliant Unix shell written entirely in C from the ground up. This project implements a custom lexer and right-linear grammar parser to handle user input, alongside several complex built-in commands implemented natively without relying on `exec` system calls.

## Features

* **Strict Grammar Parsing:** Custom lexical analysis and parsing to handle quoted strings, escaped characters, and multi-argument commands.
* **`hop` (Directory Jumper):** A persistent directory navigation tool featuring a Zoxide-style time-decaying frecency algorithm. It remembers your most visited directories across sessions and allows you to jump to them using substring matching.
* **`reveal` (Directory Scanner):** A recursive directory lister that sorts entries lexicographically and supports flags for showing hidden files (`-a`) and recursive tree viewing (`-t`).
* **`peek` (File Viewer):** A `cat`-like tool that can number lines (`-n`) and print files in reverse (`-r`). It optimizes reverse reading by using `lseek` to read files backward in fixed-size chunks, avoiding full-file memory loading.
* **`locate` (Path Resolver):** Dynamically scans the current working directory and the system's `$PATH` to find and verify executable binaries.

## How to Build and Run

This shell requires a POSIX-compliant environment (like Linux or macOS) to run.

1. Clone the repository and navigate into the `c-shell` directory.
2. Compile the source code using the provided Makefile:
  ```bash
  make all
  ```
3. Execute the compiled shell:
  ```bash
  ./shell.out
  ```
