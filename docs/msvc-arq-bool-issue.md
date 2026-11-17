# MSVC `cl cgtest.c` fails: `arq_bool_t` undefined

## Status

Root cause confirmed, fix agreed, **not yet applied**. The fix belongs
upstream in the arq library; `third_party/arq/arq.h` and `cgtest.c` are
inlined copies of it and would be overwritten on the next re-vendor.

## Observed symptoms

`docs/compile_cgtest.c.md` says `cl cgtest.c` works, but it currently fails
with about 100 errors, starting with:

```
cgtest.c(6666): error C2061: Syntaxfehler: Bezeichner "arq_bool_t"
cgtest.c(6668): error C2059: Syntaxfehler: "}"
...
fatal error C1003: Mehr als 100 Fehler gefunden; Kompilierung wird abgebrochen.
```

| Compiler | `main` (56e0e04) |
|---|---|
| gcc (default std and `-std=c89`) | compiles |
| cl 14.39 (VS 2022) | fails |
| cl 14.29 (older toolset, also in VS 2022) | fails |
| clang-cl (defines `_MSC_VER`) | fails |
| cl 14.39 on `0de4572` (the commit before `e00dd89`) | compiles and links |

The failure doesn't depend on the compiler version. It is caused by a source change.

## Cause

Commit `e00dd89` ("update arq.h has now its own types like arq_uint32_t…")
added this `arq_bool.h` section (`third_party/arq/arq.h:356`,
`cgtest.c:6643`):

```c
#if defined(_MSC_VER)
    #include <stdbool.h>
                                /* <-- no typedef: arq_bool_t never defined */
#elif defined(__cplusplus) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    #include <stdbool.h>
    typedef bool arq_bool_t;
#else
    typedef int arq_bool_t;     /* C89 */
#endif
```

Any compiler that defines `_MSC_VER` (cl, clang-cl) takes the first branch.
That branch never declares `arq_bool_t`, so every later use of it is a syntax
error. gcc, MinGW and macOS clang don't define `_MSC_VER`, so they take the
other branches, which is why only MSVC breaks.

The `_MSC_VER`-only branch seems to have been copied from the
`arq_inttypes.h` sections. That pattern is correct there, because
`<inttypes.h>` only has to supply the `PRI*` macros. For bool, the branch
also needs a typedef. The fixed-width integer section (`arq.h:24`) gets
this right: it puts `_MSC_VER` in the same `#if` as C99/C++.

## Relation to the macOS clang C89 fix

`e00dd89` fixed the macOS `-std=c89` problem, where arq's C89 shim
typedef'd `uint32_t` etc. itself and collided with the macOS SDK's own
typedefs (which `<sys/stat.h>` pulls in even under C89). The fix was to
stop claiming standard names and use arq-prefixed ones (`arq_uint32_t`, …).

`arq_bool_t` follows the same idea, but `<stdbool.h>` was never part of that
fix: the C89 branch, which macOS `-std=c89` takes, never included it.

## Why `<stdbool.h>` isn't needed

`<stdbool.h>` is included only so that `typedef bool arq_bool_t;` can be
written. Nothing in `src/`, `arq.h` or `cgtest.c` uses `bool`, `true` or
`false` directly, apart from strings and comments. All code uses
`arq_bool_t`, `ARQ_TRUE` and `ARQ_FALSE`.

Making the type depend on `-std` also has a downside: `sizeof(arq_bool_t)`
then differs between C89 and C99 builds. That would be an ABI mismatch if
arq were ever built as a separate library and linked with code compiled
under a different standard.

## Fix (agreed)

Use one definition for all compilers and standards. It needs no `#if` and
no system header:

```c
#ifndef ARQ_BOOL_H
#define ARQ_BOOL_H

typedef int arq_bool_t;

#define ARQ_TRUE  ((arq_bool_t)1)
#define ARQ_FALSE ((arq_bool_t)0)

#endif
```

Why this is safe:

- It can't bring back the macOS collision, because it uses arq's own name
  and includes no system header.
- It is exactly what the C89 build (gcc `-std=c89`, macOS `make`) already
  uses, so this path is already tested.
- Nothing relies on the 0/1 normalization that `bool` provides. Every value
  stored or returned as `arq_bool_t` is a comparison (`==`, `!=`, `!`), a
  `||`/`&&` expression, or `ARQ_TRUE`/`ARQ_FALSE`. The only `== ARQ_FALSE`
  comparisons are `assert`s on `.error` fields, which only ever hold those
  literals.

## Side note found while investigating

`arq.h` lines 1694, 1721 and 1746:

```c
return num.error; /* return ARQ_TRUE if successful */
```

This returns the error flag, while the comment promises true on success.
Either the comment or the logic is inverted. Check this upstream as well.
