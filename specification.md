
# cgtest — Full Project Specification

## 1. Introduction

**cgtest** is a lightweight command-line unit testing framework and DSL compiler written in C (C89). It discovers test files, generates a test runner, compiles it, and executes it automatically.

The system is designed to be:

* Portable (Windows, Linux, macOS)
* Dependency-free (except optional single-header utilities)
* Compiler-agnostic (gcc, clang, cl.exe, etc.)
* Minimal and self-contained

---

## 2. System Overview

cgtest operates as a pipeline tool:

1. Read configuration from `cgtest-config.json`
2. Scan project directories for test files
3. Parse test functions inside files
4. Generate `cgtest-runner.c`
5. Compile runner using configured compiler
6. Execute resulting binary

### Primary Binary

**cgtest.exe**

Responsibilities:

* Configuration management
* Code generation orchestration
* Compilation invocation
* Test execution

---

## 3. CLI Specification

### 3.1 Commands

```bash
cgtest.exe --create <config-path>
cgtest.exe --config <config-path>
cgtest.exe --version
cgtest.exe --help
```

---

### 3.2 --create

Creates a default configuration file.

Behavior:

* If path exists → error and exit
* If path is invalid → error and exit
* Otherwise → generate template `cgtest-config.json`

---

### 3.3 --config

Runs the full test pipeline.

Behavior:

* If config file does not exist → error and exit
* If output runner already exists → error and exit (prevents overwrite unless explicitly changed in future)
* Loads configuration
* Generates runner source
* Compiles runner
* Executes runner

---

### 3.4 --version

Prints version information of cgtest.

---

### 3.5 --help

Displays usage information.

---

## 4. Configuration System

### 4.1 File Format

Configuration is defined in JSON using `cgtest-config.json`.

All paths are **relative to the configuration file location**.

---

### 4.2 Schema

```json
{
  "cgtest_h": ".",
  "cgtestrunner_output_dir": "..",
  "compiler": "gcc",
  "compiler_arguments": [
    "-std=c99",
    "-O3"
  ],
  "include_directories": [],
  "source_files": [],
  "test_directories": []
}
```

---

### 4.3 Field Definitions

#### cgtest_h

Path where `cgtest.h` is generated or updated.

---

#### cgtestrunner_output_dir

Directory where generated runner source and binary are written.

---

#### compiler

Compiler executable name or full path.

Examples:

* gcc
* clang
* cl.exe

---

#### compiler_arguments

List of compiler flags.

Example:

* `-std=c99`
* `-O3`

---

#### include_directories

List of include search paths.

---

#### source_files

Explicit C source files to include in compilation.

---

#### test_directories

Directories scanned for test files.

---

## 5. Test File Specification

### 5.1 Discovery Rules

cgtest scans all directories listed in `test_directories`.

It identifies files matching:

```
test_*.c
```

Examples:

* test_math.c
* test_strview.c

---

### 5.2 Test Function Rules

Inside test files, cgtest extracts functions matching:

```c
bool test_xxx(void);
```

Example implementations:

```c
bool test_math_add(void) { return true; }
bool test_goal_sub(void) { return true; }
bool test_div(void) { return true; }
```

---

### 5.3 Execution Order

Test functions are executed in **source order**.

This enables structured setup/teardown patterns:

* First function in file → setup
* Middle functions → tests
* Last function → teardown

---

## 6. Code Generation Pipeline

### 6.1 Runner Generation

cgtest generates a single file:

```
cgtest-runner.c
```

This file contains:

* Discovered test registrations
* Execution harness
* Result aggregation

---

### 6.2 Compilation Step

The runner is compiled using the configured compiler:

Example:

```bash
gcc -std=c99 -O3 cgtest-runner.c -o cgtest-runner.exe
```

---

### 6.3 Execution Step

After compilation, the binary is executed automatically.

---

## 7. cgtest.h System

### 7.1 Purpose

`cgtest.h` is a generated header containing:

* Assertion macros
* Test registration macros
* Utility helpers

---

### 7.2 Distribution Model

Instead of file-based distribution, `cgtest.h` is embedded as a string inside the cgtest executable.

This ensures:

* Cross-platform compatibility
* No external dependency requirement
* Single-binary distribution

---

### 7.3 Installation Behavior

* If missing → generated automatically
* If outdated → overwritten based on timestamp comparison with `cgtest.exe`

---

## 8. Architecture

### 8.1 Modules

| Module          | Responsibility                      |
| --------------- | ----------------------------------- |
| cgtest_main.c   | Entry point                         |
| cgtest_config.c | JSON parsing (jsmn)                 |
| cgtest_arq.c    | CLI parsing (arq)                   |
| cgtest_lexer.c  | C source parsing for test discovery |
| cgtest_header.c | cgtest.h generation                 |

---

## 9. Dependencies

cgtest uses minimal external components:

* arq (CLI parsing)
* jsmn (JSON parsing)

Both are single-header style dependencies.

They are fetched via `curl` during the build process.

---

## 10. Build System

Uses **Tsoding nob.h** build system.

Build process is designed to be:

* Single-file bootstrap friendly
* Scriptable
* Dependency-light

---

## 11. Design Constraints

* Must be written in C (C89)
* Must not require runtime dependencies
* Must be portable across major OS platforms
* Must avoid platform-specific APIs where possible

---

## 12. Future Extensions (Planned)

* Parallel test execution
* Colored output logs
* Test filtering by name
* Watch mode (auto-run on file change)
* Benchmark mode
* Improved assertion macros

---

## 13. Summary

cgtest is designed as a minimal, portable, and deterministic C test execution system that emphasizes:

* Simplicity over abstraction
* Explicit configuration
* Transparent code generation
* Cross-platform consistency
