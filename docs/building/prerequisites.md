# Prerequisites

Before compiling HSIL CoSim library, ensure that your environment meets the following specifications.

## 1. Supported Compilers and Build Tools

A modern toolchain with support for **C11** and **C++17** is required.

| Tool | Version | Notes / Recommendations |
|---|---|---|
| **CMake** | `3.21+` | Required for code generation and project configuration |
| **Git** | `2.13+` | Required to initialize and manage vendor submodules |
| **C/C++ Compiler** | GCC <= 13.2.0, Clang <= 16.0.6, MSVC <= 2026 | Platforms must support standard C++17 library types |

---

## 2. Dependencies and Git Submodules

All third-party dependencies are housed inside the `third_party/` directory at the repository root. No active internet access is needed during CMake configuration if the submodules are already checked out.

| Dependency | Version | Submodule Location | Purpose |
|---|---|---|---|
| **nlohmann/json** | `3.11.3` | `third_party/nlohmann_json` | JSON simulation profile configuration parser |
| **CycloneDDS** | `0.10.5` | `third_party/cyclonedds` | Default underlying DDS backend transport |

---

## 3. Cloning the Code and Submodules

To clone the repository complete with its submodules in a single command, run:

```bash
git clone --recurse-submodules <repository-url>
```

If you have already cloned the repository without submodules, fetch and initialize them by running:

```bash
git submodule update --init --recursive
```
