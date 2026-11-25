# `cgtest --init` doesn't update mathlib's `cgtest-project.json`

## Status

Cause confirmed, **not fixed yet**. The trailing comma in
`examples/mathlib/cgtest/cgtest-project.json` needs to be removed. Whether
`--run` should also reject such files is still open (see "Fix").

## Observed symptoms

```
cgtest --init examples/mathlib
  cgtest-project.json: already exists, left unchanged (could not parse it to check for missing fields)
```

`cgtest --run examples/mathlib/cgtest` reads the same file without any
complaint. `--init` is supposed to update an existing `cgtest-project.json`,
so it looks as if the update is broken.

## Cause

The file isn't valid JSON. It has a trailing comma after the last field:

```json
    "test_directories": [
        ".",
        "../tests"
    ],      <-- trailing comma before the closing "}"
}
```

`--run` and `--init` read the file with two different functions:

| | Function | Trailing comma |
|---|---|---|
| `--run` | `cgtest_project_parse()` (`src/cgtest_project.c`) | accepted silently (jsmn's tokenizer is lenient) |
| `--init` | `cgtest_project_scan_optional_fields()` (`src/cgtest_project.c`) | rejected |

`--init` is strict on purpose. It doesn't rewrite the file. It inserts the
missing fields as text before the closing `}`, and that is only safe when
every separator is exactly where valid JSON puts it. The check after the
last field expects `}` and finds `,`
(`cgtest_project_next_nonspace_is(..., '}')` in
`cgtest_project_scan_optional_fields()`). The function returns 0,
`cgtest_create_run()` sets `project_could_not_be_checked`, and
`cgtest_main.c` prints "could not parse it to check for missing fields".
Leaving the file unchanged is the correct reaction to invalid JSON. The
update logic itself works.

## What `--init` updates

`--init` only adds the **optional** fields `msvc` and
`single_translation_unit`, with their default `false`, when they are
missing. It never adds or changes the five required fields and never
touches existing values.

mathlib's file already contains both optional fields. So even with the
comma removed, `--init` has nothing to add and correctly reports
`already exists, left unchanged`.

## Verified

`cgtest --init` on copies of the mathlib file:

| Copy | `--init` output |
|---|---|
| as committed (trailing comma) | `left unchanged (could not parse it to check for missing fields)` |
| trailing comma removed | `left unchanged` (nothing missing) |
| trailing comma and `"msvc"` removed | `added missing field(s)`: `"msvc": false` appended before `}` |

## History

Commit `6d06ec5` ("--init patches missing optional fields into an existing
cgtest-project.json") introduced the comma. Its message says it *removed* a
trailing comma from the mathlib example, but its diff for that file does
the opposite: it changes the last `]` to `],`. The file has been invalid
JSON since then. The mathlib example is the only `cgtest-project.json` in
`examples/` that has the problem.

## Fix

1. **Required:** remove the trailing comma in
   `examples/mathlib/cgtest/cgtest-project.json`.
2. **Open decision:** `--run` accepts a file that `--init` rejects, so the
   problem stays hidden until someone runs `--init`. Options:
   - `--run` also rejects invalid JSON (consistent, but breaks existing
     projects with such typos), or
   - `--run` prints a warning and keeps going.
3. **Optional:** make the `--init` message more specific, e.g. say that the
   file isn't valid JSON and where (`unexpected ',' before '}'`), instead
   of "could not parse it".
