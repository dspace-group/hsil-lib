# AGENTS.md — Quick Reference for AI Agents

## What This Project Is
A lightweight **C library** (`libhsil_cosim.so`) that abstracts DDS communication behind a plain C API for HSIL Co-Simulation. The entire public API is exposed through a single header (`lib/include/hsil/hsil.h`), easy to use — no DDS expertise required.

### HSIL Co-Simulation Background
HSIL Co-Simulation is an open interface for coupling simulation agents over Ethernet-based networks. It defines a data-centric communication protocol built around DDS (Data Distribution Service) to exchange signal data (engineering data with fixed byte layout), bus data (CAN, Ethernet), and generic byte streams between simulators.

---

## Repository Layout
```
CMakeLists.txt              ← Top-level build entry point
lib/
  include/hsil/hsil.h       ← Only public header (the entire C API)
  src/
    hsil.cpp                ← All public API implementations
    hsilConfig.cpp          ← JSON config parser
    ddsAbstraction.h        ← Vtable struct (HsilDdsOps) for DDS abstraction
    vendors/cycloneDDS/     ← CycloneDDS backend (OBJECT library)
      cycloneDdsProvider.cpp/h
      idl/HSIL-CoSim.idl
  tests/
    api/                    ← Layer 2: stub-vtable tests (no live DDS)
      test_hsilApi.cpp      ← All API unit tests
      stub_dds_ops.cpp/h    ← Stub that replaces CycloneDDS at link time
    config/                 ← Layer 1: config parser tests
    dds/                    ← Layer 3: real CycloneDDS loopback tests (opt-in)
third_party/
  cyclonedds/               ← git submodule
  googletest/               ← git submodule
  nlohmann_json/            ← git submodule
examples/cpp/               ← C++ usage examples
docs/developer/codingConventions.md ← SPDX and source-header conventions
```

---

## Build & Test Commands
```bash
# Configure (from repo root)
cmake -S . -B build

# Build everything
cmake --build build --parallel

# Run all tests
ctest --test-dir build --output-on-failure

# Build + test a single target fast
cmake --build build --target hsilApiTests && ctest --test-dir build -R HsilApiTests --output-on-failure
```

Default build type is **Release**. Pass `-DCMAKE_BUILD_TYPE=Debug` for debug symbols.

---

## Key Architecture Decisions

### DDS Abstraction via Vtable
`hsil.cpp` never calls CycloneDDS directly. It calls through `HsilDdsOps*` (function pointer struct in `ddsAbstraction.h`). The vendor provides `hsil_getDdsOps()`. Tests replace this with a stub at link time — no DDS daemon needed.

### Exception Safety
All public API functions in `hsil.cpp` use **function-try-blocks** to catch any C++ exception and map it to `HSIL_ERR_GENERIC`. No exception must ever cross the C API boundary.

### Symbol Visibility
The shared library uses `CXX_VISIBILITY_PRESET hidden` / `C_VISIBILITY_PRESET hidden`. Only symbols decorated with `HSIL_API` are exported. `HSIL_API` is defined in `hsil.h`.

### OBJECT Library for Vendor Backend
`hsilDdsVendorObject` is an OBJECT library (not STATIC). Its `.o` files are merged directly into `libhsil_cosim.so`. This is what hides CycloneDDS and IDL symbols from the `.so`'s exported symbol table.

---

## CMake Options
| Option | Default | Purpose |
|---|---|---|
| `HSIL_DDS_VENDOR` | `cycloneDDS` | Vendor backend subdirectory name |
| `HSIL_BUILD_STATIC_LIB` | `OFF` | Build static instead of shared library |
| `HSIL_BUILD_TESTING` | `ON` | Build test suite |
| `HSIL_BUILD_TESTING_DDS` | `ON` | Build real DDS loopback tests (needs network) |
| `HSIL_BUILD_EXAMPLES` | `ON` | Build examples |

---

## Coding Conventions
- **C++17 / C11**; 4-space indent; opening braces on their own line
- `camelCase` for variables/functions/filenames, `PascalCase` for classes, capitalized namespaces
- Public C APIs must **never throw** — use error codes (`HSIL_ERR_*`)
- First-party source and build files must start with an SPDX comment block using the file's comment syntax.
- C/C++ files retain the Doxygen `@file`, `@brief`, `@author`, and `@description` block after the SPDX lines.
  Keep copyright and license metadata in the SPDX block, not in Doxygen.
- Python scripts keep the shebang on line 1, followed by the SPDX comment block and module docstring.
- CMake files keep `cmake_minimum_required()` as the first command, after the SPDX comments.
- See `docs/developer/codingConventions.md` for examples.

Example C/C++ file opening:

```cpp
// SPDX-FileCopyrightText: <Your Copyright>
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file <filename>
*
*   @brief <short description>
*
*   @author
*       <author name(s)>
*
*   @description
*       <what the file does>
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/
```
- Line length limit: 120 characters

---

## Error Codes (defined in hsil.h)
`HSIL_OK (0)`, `HSIL_WARN_SIGNAL_CONVERSION (1)`, `HSIL_ERR_GENERIC (-1)`, `HSIL_ERR_INVALID_ARG (-2)`, `HSIL_ERR_CONFIG (-3)`, `HSIL_ERR_DDS (-4)`, `HSIL_ERR_UNKNOWN_TOPIC (-5)`, `HSIL_ERR_OUT_OF_MEMORY (-6)`, `HSIL_ERR_UNKNOWN_SIGNAL (-7)`, `HSIL_ERR_TOPIC_TYPE_MISMATCH (-8)`, `HSIL_ERR_PUB_NOT_CREATED (-9)`, `HSIL_ERR_SIGNAL_CONVERSION (-10)`, `HSIL_ERR_SUB_NOT_CREATED (-11)`

---

## Adding a New Vendor Backend
1. Create `lib/src/vendors/<name>/CMakeLists.txt`
2. Define an OBJECT library named `hsilDdsVendorObject`
3. Implement `hsil_getDdsOps()` returning a filled `HsilDdsOps` vtable
4. Build with `-DHSIL_DDS_VENDOR=<name>`
