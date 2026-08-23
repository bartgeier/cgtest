### Build `cgtest.c`

To merge all C source files into `cgtest.c`, the Makefile uses the Amalgamate CLI tool (v0.99.0):

> [Amalgamate v0.99.0 on GitHub](https://github.com/rindeal/Amalgamate/releases) 

The repository root contains a `Makefile`. Run the appropriate command for your platform:

### Linux

```
make
```

### Windows

Install MinGW-w64 (for example from WinLibs):

> [WinLibs](https://winlibs.com/) 

If you're unsure which version to choose, use the latest 64-bit release.

```
mingw32-make
```

### macOS

```
make
```

macOS has **not been tested**. I develop on Linux and can test on Windows, but I don't have access to macOS.  
see [macOs build issues](macos-clang-issues.md)  

> Note: If you only want to use `cgtest`, you don't need to build it. Just download `cgtest.c`- that's all you need.
