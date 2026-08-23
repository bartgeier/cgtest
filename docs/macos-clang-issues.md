# macOS `make` build fails under clang (unconfirmed, needs verification)

## Status

Not yet reproduced or fixed. Reasoned from code inspection only — nobody
on the team has an actual macOS build log to confirm the exact compiler
error yet. Verify against real output before implementing a fix.

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

???
