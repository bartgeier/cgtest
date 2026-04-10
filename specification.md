# Project Name cgtest

## 1. Overview
cgtest is a unit test framework.
cgtest.exe  generates cgtest-runner.c compiles it and runes it.

Example:
This project is a command-line C application that manages a simple to-do list stored in a local file. It supports adding, listing, editing, and deleting tasks.

---

## 2. Goals
List clear goals.

- Must be written in C (C99 standard)
- Must compile with gcc or clang
- Must run on Linux/macOS
- Must not use external libraries (unless specified)

---

## 3. Non-Goals (Important)
What the project should NOT do.

- No GUI
- No network features
- No external database systems

---

## 4. Features

### Core Features
- Add a new item
- Remove an item by ID
- List all items
- Save/load from file

### Optional Features (nice to have)
- Search items
- Sort items
- Colorized terminal output

---

## 5. CLI Interface

Define exactly how the program is used:

```bash
./app add "Buy milk"
./app list
./app remove 3