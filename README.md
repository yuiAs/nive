# nive

Lightweight image viewer for Windows.

## Features

- Three-pane UI: directory tree, file list, and thumbnail grid
- Image viewer with zoom and pan (Direct2D rendering)
- Archive browsing via 7-Zip (`7z.dll`)
- Network share (UNC path) support
- Thumbnail caching with SQLite and zstd compression
- Drag & drop file operations with conflict resolution
- Natural / lexicographic / date / size sorting
- AVIF image support via plugin (libavif)
- Plugin API for extending supported formats
- TOML-based persistent settings

## Requirements

- Windows 10 or later (x64)
- [7-Zip](https://www.7-zip.org/) (optional, for archive support)

### Archive Support

Archive browsing requires `7z.dll` from 7-Zip. The DLL is searched in the following order:

1. Application directory (next to `nive.exe`)
2. `%ProgramFiles%\7-Zip`
3. `%ProgramFiles(x86)%\7-Zip`
4. System `PATH`

If 7-Zip is installed in the default location, no additional setup is needed. For portable use, place `7z.dll` alongside `nive.exe`.

## Build

### Prerequisites

- [Visual Studio 2026](https://visualstudio.microsoft.com/) (MSVC with C++ workload)
- [CMake](https://cmake.org/) 3.28+
- [NASM](https://www.nasm.us/) (for libaom SIMD optimizations)
- Git (for submodules)

### Steps

```powershell
git submodule update --init --recursive
cmake -B build
cmake --build build --config Release
```

The output binary is located at `build/bin/Release/nive.exe`.

### Dependencies

All dependencies are vendored as git submodules under `externals/`:

| Library | Purpose |
|---------|---------|
| [spdlog](https://github.com/gabime/spdlog) | Logging |
| [SQLite](https://www.sqlite.org/) | Thumbnail cache database |
| [bit7z](https://github.com/rikyoz/bit7z) | 7-Zip archive support |
| [zstd](https://github.com/facebook/zstd) | Cache compression |
| [toml++](https://github.com/marzer/tomlplusplus) | Settings file parsing |
| [libavif](https://github.com/AOMediaCodec/libavif) | AVIF image decoding (with libaom) |

## License

[MIT](LICENSE)
