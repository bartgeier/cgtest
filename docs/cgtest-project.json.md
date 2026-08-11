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

## Windows gcc Compiler
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

