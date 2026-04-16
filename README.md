# cgtest

A lightweight C unit testing framework and DSL compiler.

## Build

```sh
# 1. Download nob.h (one-time bootstrap)
curl -sSfL -o nob.h https://raw.githubusercontent.com/tsoding/nob.h/main/nob.h

# 2. Compile the build script
gcc -o nob nob.c

# 3. Build cgtest (downloads arq.h and jsmn.h automatically)
./nob
```

On Windows with MSVC replace step 2–3 with:
```bat
cl nob.c /Fenob.exe
nob.exe
```

## Usage

```sh
cgtest --create cgtest-config.json   # create default config
cgtest --config cgtest-config.json   # run the full test pipeline
cgtest --version
cgtest --help
```

## Config (`cgtest-config.json`)

```json
{
  "cgtest_h": ".",
  "cgtestrunner_output_dir": "..",
  "compiler": "gcc",
  "compiler_arguments": ["-std=c99", "-O2"],
  "include_directories": [],
  "source_files": [],
  "test_directories": ["tests"]
}
```

## Writing tests

Name files `test_*.c` and functions `bool test_xxx(void)`:

```c
#include "cgtest.h"

bool test_addition(void)
{
    CGTEST_ASSERT_EQ(1 + 1, 2);
    return true;
}
```
