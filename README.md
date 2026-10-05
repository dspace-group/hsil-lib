# HSIL CoSim Library

![Library Version](https://img.shields.io/badge/Version-v1.0.0-blue)
![License](https://img.shields.io/badge/License-Apache%202.0-blue)

![C API](https://img.shields.io/badge/C%20API-blue)
![Linux x64](https://img.shields.io/badge/Linux%20x64-blue)
![ARM64 (aarch64)](https://img.shields.io/badge/ARM64%20(aarch64)-blue)
![Windows x64](https://img.shields.io/badge/Windows%20x64-blue)

[![Build/Test](https://github.com/dspace-group/hsil-lib/actions/workflows/buildAndTest.yml/badge.svg)](https://github.com/dspace-group/hsil-lib/actions/workflows/buildAndTest.yml)

A lightweight C library for coupling simulation agents using DDS (Data Distribution Service) middleware, to perform Co-Simulation. It abstracts all DDS communication behind a simple, opaque C API — no DDS expertise required.

HSIL CoSim couples simulation agents over a DDS network using two data types:

| Type | Purpose |
|---|---|
| `GroupedData` | Bulk transmission of engineering signals (with fixed-layout) |
| `StreamingData` | Bus frames (CAN, Ethernet) and arbitrary binary streams |

---

## Repository Layout

```
docs/         - Documentation
examples/     - Usage examples in C++ and Python
lib/          - hsil_cosim library
  include/    - Single public header file <hsil/hsil.h> 
  src/        - Source Files
  tests/      - Unit tests
third_party/  - Third party projects used by this library
```

---

## Getting Started

### 1. Clone the Codebase (Including Submodules)
To fetch the code and all third-party dependencies automatically:

```bash
git clone --recurse-submodules <repository-url>
```

If you cloned without submodules, fetch and initialize them by running:

```bash
git submodule update --init --recursive
```

---

## Build and Install

Choose the platform instructions matching your system:

### Linux
Compile and build the library in Release mode:

```bash
# Configure the project
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build targets in parallel
cmake --build build --parallel

# Install library artifacts
cmake --install build --prefix /path/to/install/dir
```

### Windows (Visual Studio / MSVC)
Run the commands below from inside an **x64 Native Tools Command Prompt for VS 2022/2019**:<br>
**Note: For multi-config generators, build type (Debug/ Release..) must be specified during build and install**
```cmd
# Configure solution
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Compile Release configuration
cmake --build build --config Release --parallel

# Install library artifacts
cmake --install build --config Release --prefix /path/to/install/dir
```

*(For Ninja-based compilation (single config generator) on Windows, run `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` followed by `cmake --build build` and `cmake --install build --prefix /path/to/install/dir`)*

---

## Documentation

For full details, please refer the documentation from sections mention below:

- [API Reference](docs/api/reference.md) - Concepts, data types, error codes, and usage
- [Building & Installing](docs/building/index.md) - Building, installing and linking the library
- [Architecture & Design](docs/api/architecture.md) - Architecture layers and key design decisions
- [Developer Guidelines](docs/developer/index.md) - Library internals (adding a new DDS vendor)
- [Testing Guide](docs/testing/index.md) - Unit test design and execution
- [Examples Guide](examples/Readme.md) - Simple examples in C++ and Python demonstrating library usage

---

## License

Copyright 2026 dSPACE SE & Co. KG. Licensed under the [Apache License 2.0](LICENSE).
