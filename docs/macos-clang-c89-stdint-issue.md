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

## Leading hypothesis

The Makefile compiles several targets with `-std=c89` (`check-c89`,
`check-amalgamate`, and the `cgtest` binary itself — see `Makefile`),
and `check-c89`/`check-amalgamate` additionally use `-Werror`.

`third_party/arq/arq.h` has its own stdint shim near the top of the file:

```c
#ifndef ARQ_STDINT_H
#define ARQ_STDINT_H
#include <stddef.h>
#if defined(_MSC_VER)
#include <stdint.h>
#elif defined(__cplusplus) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdint.h>
...
#else /* C89 */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
...
#endif
#endif /* ARQ_STDINT_H */
```

`__STDC_VERSION__` is a C99+ macro. C89 doesn't define it at all, so when
clang is invoked with `-std=c89`, the `#elif` test is false and the code
falls into the `/* C89 */` branch, manually typedef'ing `int8_t`,
`uint8_t`, `int16_t`, `uint16_t`, `int32_t`, `uint32_t` itself.

Separately, several `src/*.c` files include OS headers that, on macOS,
transitively pull in Apple's own fixed-width typedefs regardless of the
requested language standard:

- `src/cgtest_create.c` → `<sys/stat.h>`
- `src/cgtest_project.c` → `<sys/stat.h>`
- `src/cgtest_runner.c` → `<sys/stat.h>`, `<sys/wait.h>`

On macOS, these drag in Apple's `<sys/_types/_int8_t.h>` etc. chain,
which typedefs `int8_t`/`uint16_t`/`uint32_t`/etc. unconditionally — this
is **not** gated behind `__STDC_VERSION__` the way arq.h's own check is.
glibc on Linux does not expose these same typedefs via `<sys/stat.h>` /
`<sys/wait.h>`, so the same `-std=c89` build works fine there.

Net effect on macOS + `-std=c89`: by the time arq.h's C89 fallback tries
to typedef `int8_t` etc., the SDK has already defined them via a
different header included earlier in the same translation unit. Clang
treats a duplicate typedef of these names as an error in strict C89 mode
(the diagnostic is essentially "redefinition of typedef ... is a C11
feature") — and since `check-c89` / `check-amalgamate` build with
`-Werror`, this turns into a hard build failure. `int8_t` and friends are
**not** builtin to clang or to the C89 standard itself — C89 has no
standard header that defines them at all. They only exist here because
Apple's SDK headers put them in scope ahead of arq.h's own definitions.

## Why the friend's manual compile worked

`clang cgtest.c -o cgtest` with no `-std=` flag defaults to a newer C
standard (clang's default, e.g. gnu17), so `__STDC_VERSION__` **is**
defined and `>= 199901L`. arq.h's shim takes the `#include <stdint.h>`
branch instead of the manual-typedef branch, so the manual typedefs — and
the collision with Apple's SDK headers — never happen. This is why
plain-clang works but `make` (which forces `-std=c89`) doesn't.

## What would need to happen to confirm this

- Get the actual `make` error output from macOS (exact file/line and
  diagnostic text). This has not been captured yet.
- Confirm on a real Mac (or via an osxcross-style cross toolchain — plain
  `clang` on Linux won't reproduce this, since the issue is Apple's SDK
  headers, not clang itself) that the error is indeed a duplicate-typedef
  diagnostic pointing at arq.h's stdint shim.

## Recommended fix

Widen the `#elif` that gates the `#include <stdint.h>` branch to also
trust `__clang__`:

```c
#elif defined(__cplusplus) || defined(__clang__) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#include <stdint.h>
```

Why this works: Clang always ships its own `<stdint.h>` as a
**freestanding** header (`lib/clang/<ver>/include/stdint.h` in its
resource directory). Freestanding headers must work under any `-std=`
mode, including `-std=c89` — unlike library headers, they aren't gated by
`__STDC_VERSION__` internally. So `#include <stdint.h>` succeeds under
clang + `-std=c89` on macOS and yields the SDK's own canonical
`int8_t`/`uint32_t`/etc. — the exact same ones `<sys/stat.h>` already put
in scope. No manual retypedef happens, so no collision, so no
`-Werror` failure. This sidesteps the bug rather than working around its
symptom.

Two refinements worth considering while making this change (not required
to fix the reported case, but consistent with the reasoning above):

- **Also add `__GNUC__`.** GCC has the identical property: it always
  ships a working `<stdint.h>` even under `-std=c89`/`-ansi`. Not needed
  to fix the current macOS bug (gcc-on-Linux/Windows isn't hitting this
  collision), but makes the shim's logic consistent instead of
  clang-special-cased for no principled reason.
- **Or go more general: `__has_include(<stdint.h>)`.** Both clang and
  gcc support `__has_include` as a preprocessor builtin regardless of
  `-std=`, so this covers any future compiler that ships a real
  `<stdint.h>` without hand-naming compilers, while still falling through
  to the manual-typedef branch for genuinely ancient C89-only compilers
  that lack `<stdint.h>` entirely.

Either way, the fix belongs in `third_party/arq/arq.h`'s stdint shim, not
in project-local code — `int8_t`/`uint32_t` are typedef'd there, not in
`src/`.

## Still needed before implementing

- The actual `make` error output from macOS, to confirm the diagnostic
  really is the duplicate-typedef error this doc predicts, before
  spending time on the fix above.
