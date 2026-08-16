# cgtest-project.json
## Linux gcc Compiler
```json
{
    "compiler_command": "gcc -std=c89 -O0 -Wall -Wextra -pedantic-errors",
    "msvc": false,
    "single_translation_unit": false,
    "include_paths": [],
    "source_files": [],
    "output_path": "../build",
    "test_directories": [
        "."
    ]
}
```

## Windows gcc (WinLibs) Compiler
[https://winlibs.com/](https://winlibs.com/)  
*// if you don't know what to take, go with latest Windows 64-bit*
```json
{
    "compiler_command": "gcc -std=c89 -O0 -Wall -Wextra -pedantic-errors",
    "msvc": false,
    "single_translation_unit": false,
    "include_paths": [],
    "source_files": [],
    "output_path": "../build",
    "test_directories": [
        "."
    ]
}
```

## Windows cl Compiler
MSVC does not have a C89 mode switch equivalent to GCC/Clang's -std=c89
```json
{
    "compiler_command": "cl /W4 /Od /permissive-",
    "msvc": true,
    "single_translation_unit": false,
    "include_paths": [],
    "source_files": [],
    "output_path": "../build",
    "test_directories": [
        "."
    ]
}
```

## macOs clang Compiler
```json
{
    "compiler_command": "clang -std=c89 -O0 -Wall -Wextra -pedantic-errors",
    "msvc": false,
    "single_translation_unit": false,
    "include_paths": [],
    "source_files": [],
    "output_path": "../build",
    "test_directories": [
        "."
    ]
}
```

