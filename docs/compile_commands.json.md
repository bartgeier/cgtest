## compile_commands.json

Every `cgtest -r` run writes a `compile_commands.json` (JSON compilation
database) into the project's `output_path`, next to `cgtest-runner.c`.
Editors and language servers such as clangd (nvim LSP) read it to learn how
each file is compiled, so includes resolve and you can jump around the
sources.

### Content

One entry per source file:

- every file in `source_files`
- every discovered `test_*.c` file (also in `single_translation_unit` mode,
  where the runner only `#include`s them)
- the generated `cgtest-runner.c`

All entries share one `command`: the `compiler_command`, followed by one
include flag for every `include_paths` and `test_directories` entry
(`-I"..."`, or `/I"..."` with `"msvc": true`). The source list and `-o` are
not part of it. All paths are absolute. `directory` is the `output_path`.

```json
[
  {
    "directory": "/home/user/mathlib/build",
    "command": "gcc -std=c89 -Wall -I\"/home/user/mathlib/src\" -I\"/home/user/mathlib/tests\"",
    "file": "/home/user/mathlib/src/mathlib.c"
  }
]
```

### Using it with clangd / nvim

clangd looks for the file in the project root or in `build/`. If your
`output_path` is elsewhere, point clangd at it:

```
clangd --compile-commands-dir=<output_path>
```

### Notes

- The file is regenerated on every run.
- Writing it is a convenience: if it fails, cgtest prints a warning to
  stderr and carries on with the run.
- It is written by `gen_compile_commands.h` (`src/`), a single-header C89
  library.
