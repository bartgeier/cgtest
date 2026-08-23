# macOS `make` build fails under clang (unconfirmed, needs verification)

## Status

Hypothesis below is reasoned from code inspection plus the one failure
log we do have (see "Observed symptoms"). The proposed fix has been
applied to the Makefile, but nobody on the team has run it on an
actual macOS machine yet. Verify before considering this closed.

## Observed symptoms

- A friend testing on macOS: `clang cgtest.c -o cgtest` (compiling the
  amalgamated single-file source directly, no special flags) **succeeded**.
- Running `make` on the same machine **failed**.
- So the compiler itself is fine — the failure is specific to how `make`
  invokes it.
```
mkdir -p build
cc -std=c89 -Wall -Wextra -pedantic -Werror -fPIC -shared -Wl,--no-undefined -Ithird_party/arq -Ithird_party/jsmn src/*.c -o build/libcgtest_src_check.so
ld: unknown options: --no-undefined
clang: error: linker command failed with exit code 1 (use -v to see invocation)
make: *** [check-c89] Error 1
```

## Leading hypothesis

`Makefile:125` (`check-c89`) hardcodes a GNU-ld-only linker flag:

```
$(CC) -std=c89 -Wall -Wextra -pedantic -Werror -fPIC -shared -Wl,--no-undefined -Ithird_party/arq -Ithird_party/jsmn src/*.c -o $(BUILD_DIR)/libcgtest_src_check.so
```

`--no-undefined` is a **GNU ld (binutils) option**: it makes the linker
error if the shared object being built has unresolved symbols. It's
needed on Linux because GNU ld's *default* behavior for a shared
object is to allow undefined symbols (they're assumed to resolve at
load time), which would silently defeat the whole point of
`check-c89` — catching cross-file link errors.

On macOS, `cc`/`clang` invokes Apple's linker (**ld64**, `ld-prime` on
newer Xcode), not GNU ld. ld64 has no `--no-undefined` flag, so
`-Wl,--no-undefined` gets forwarded straight through and ld64 rejects
it before it even looks at symbols — exactly the observed
`ld: unknown options: --no-undefined`.

This lines up with the friend's report: `clang cgtest.c -o cgtest` is
a plain executable link (no `-shared`, no `-Wl,--no-undefined`), so it
never touches this code path and succeeds. `make` fails immediately
because `test` depends on `check-c89` first (`Makefile:37`), and
`check-c89` is the first target that runs — and the only one using a
GNU-only linker flag.

Bonus fact that matters for the fix: ld64's *default* behavior for a
dylib is already to error on undefined symbols (two-level namespace
resolution) unless you opt out with `-undefined dynamic_lookup`. So
macOS gets `check-c89`'s intended behavior for free, with no
replacement flag needed — the fix is to *not pass* a
`--no-undefined`-equivalent on macOS at all, not to find its ld64
spelling.

## Proposed fix

Make the GNU-only flag conditional on platform instead of hardcoded.
Add a `uname -s` detection (the Makefile currently only distinguishes
`Windows_NT` vs. everything else, which bucket macOS in with Linux):

```make
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    NO_UNDEFINED_LDFLAG :=
else
    NO_UNDEFINED_LDFLAG := -Wl,--no-undefined
endif
```

and change `check-c89`'s recipe to use `$(NO_UNDEFINED_LDFLAG)` in
place of the hardcoded `-Wl,--no-undefined`. On Linux, behavior is
unchanged. On macOS, the flag drops out entirely and ld64's own
default (error on undefined symbols) still gives `check-c89` the
cross-file-link-error check it exists for.

Not proposing to touch `-fPIC -shared` — clang on Darwin has accepted
`-shared` (mapping it to dylib output) for a long time, and nothing in
the observed log complains about it. Flagged as unconfirmed below
since it's still unverified on a real machine.

## Other potential macOS issues in `make` (unconfirmed)

Went through every recipe in the Makefile; `check-c89` is the only
target using a non-portable, GNU-specific flag. Everything else is
`$(CC) $(CFLAGS) ... -o $@` with plain include paths, which is exactly
what the friend's manual `clang` compile already proved works. Two
things worth keeping an eye on but *not* treated as confirmed bugs:

- `-fPIC -shared` in `check-c89` (`Makefile:125`) and nowhere else —
  should be a no-op/accepted on Darwin clang, but unverified live.
- `check-amalgamate`'s dependency on the external `amalgamate` CLI
  being on `PATH` (`Makefile:139`) isn't macOS-specific — it'd fail
  the same way on any platform missing the tool — but it's the next
  thing that could stop a macOS `make all` after `check-c89` is fixed,
  so worth checking it's actually installed before declaring the
  build green.

No other platform-conditional shell syntax, path separators, or
GNU-only flags found elsewhere in the Makefile.
