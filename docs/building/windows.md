# Building on Windows

The HSIL CoSim library on Windows compiles using Microsoft Visual Studio or LLVM-Ninja. All platform-dependent code uses C++17 standard helper types.

---

## 1. Prerequisites and Terminal Setup

- **Compiler:** Install Visual Studio 2019 or 2022 with the **Desktop development with C++** workload.
- **CMake:** Install CMake 3.21+ via the Visual Studio installer or download it directly from [cmake.org](https://cmake.org/).
- **Shell:** Always execute build commands inside the **x64 Native Tools Command Prompt for VS 2022/2019**.

---

## 2. Standalone Build and Configuration

Choose one of the two build generation options below to configure and build the library standalone:

### Option A: Build with Visual Studio (MSVC) Generator

To generate a multi-configuration Visual Studio solution and build in Release mode:

```cmd
# Configure the VS 2022 project
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Compile the Release target
cmake --build build --config Release --parallel
```
*(For VS 2019, specify `-G "Visual Studio 16 2019"`).*

#### Visual Studio Build Artifacts:
- **`build\lib\Release\hsil_cosim.lib`** (import/static library)
- **`build\bin\Release\hsil_cosim.dll`** (shared DLL library) and its dependent vendor DLLs if built as shared.

---

### Option B: Build with Ninja (Recommended for Incremental Builds)

Ninja produces single-configuration builds and compiles significantly faster:

```cmd
# Configure with Ninja in Release mode
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile
cmake --build build --parallel
```

#### Ninja Build Artifacts:
- **`build\lib\hsil_cosim.lib`** (import/static library)
- **`build\bin\hsil_cosim.dll`** (shared DLL library) and its dependent vendor DLLs if built as shared.

---

## 3. Installation

To install the built headers, libraries, and binaries onto your system, run the installation command pointing to your chosen install directory.

### For Ninja (Single-Configuration)
```cmd
cmake --install build --prefix "C:\path\to\install"
```

### For Visual Studio (Multi-Configuration)
You must specify the build configuration used for compiling:
```cmd
cmake --install build --config Release --prefix "C:\path\to\install"
```

### Artifacts in the Install tree
After a successful installation, you will find:
- **`<install root>\bin`**: Example applications (if enabled) with hsil config json files, Unit tests applications (if enabled), DLL files like `hsil_cosim.dll`, and shared CycloneDDS DLL dependencies.
- **`<install root>\lib`**: Import library `hsil_cosim.lib` (for DLL linking) or static library `hsil_cosim.lib` (if `HSIL_BUILD_STATIC_LIB` is ON).
- **`<install root>\lib\cmake\HsilCoSim`**: CMake target designators (`HsilCoSimConfig.cmake` etc.) for packaging.
- **`<install root>\include`**: `hsil/hsil.h` public header of the hsil_cosim library, along with DDS vendor headers.

---

## 4. Building and Integrating as a Subproject (via add_subdirectory)

If you are developing a parent application on Windows that uses CMake, you can integrate the HSIL CoSim library directly into your project's build tree using `add_subdirectory`. This compiles the library from source alongside your application ([Example CMake file](../../lib/tests/integration/subproject_consumer/CMakeLists.txt)).

HSIL CoSim always generates its build artifacts (libraries, DDS vendor runtime DLLs, examples, and tests) in its own subproject build folder (`${CMAKE_CURRENT_BINARY_DIR}/bin` and `lib`), keeping the parent scope completely untouched.

### Recommended CMake Configuration

Windows has no RPATH: at startup the loader looks for DLLs next to the executable or along `PATH`. To make `hsil_cosim.dll` and the DDS runtime (`ddsc.dll`) discoverable, copy the runtime dependencies next to your executable post-build using CMake 3.21's `$<TARGET_RUNTIME_DLLS:...>`:

```cmake
# Optional: Force HSIL library to build as a static library
set(HSIL_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)

# Optional: skip the bundled examples (the test suite is OFF by default)
set(HSIL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory("path/to/hsil-cosim" "project-build-dir/hsil-cosim")

add_executable(my_app main.cpp)

# Link against the HSIL library target
target_link_libraries(my_app PRIVATE HsilCoSim::hsil_cosim)

# Copy all required runtime DLLs next to my_app.exe
if(WIN32)
    add_custom_command(TARGET my_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_RUNTIME_DLLS:my_app>" "$<TARGET_FILE_DIR:my_app>"
        COMMAND_EXPAND_LISTS
    )
endif()
```

> **Notes:**
> - `$<TARGET_RUNTIME_DLLS:...>` automatically resolves the entire transitive runtime DLL closure (including `ddsc.dll`).
> - If your target links only static libraries, `$<TARGET_RUNTIME_DLLS:...>` expands to empty; guard it accordingly to prevent `copy_if_different` from failing with missing arguments. (Even if `HSIL_BUILD_STATIC_LIB=ON`, `ddsc.dll` is still a shared DLL and must remain reachable).
> - Linking against the alias `HsilCoSim::hsil_cosim` lets you seamlessly transition between `add_subdirectory` and `find_package(HsilCoSim)`.

---

## 5. Linking Against the Standalone Library

If you built and installed the library standalone, you can link against it in one of the following ways.

### Using CMake (Recommended)
If your application uses CMake, simply locate and link the installed package ([Example CMake file](../../lib/tests/integration/installed_consumer/CMakeLists.txt)):

```cmake
# Required when install directory is not in PATH. Can also be supplied as argument (-DCMAKE_PREFIX_PATH="path") during CMake configuration
set(CMAKE_PREFIX_PATH "C:/path/to/install/root/dir")

find_package(HsilCoSim REQUIRED)

target_link_libraries(my_app PRIVATE HsilCoSim::hsil_cosim)
```

---

## 6. OpenSSL Configuration

CycloneDDS features security mechanics dependent on OpenSSL. If building gets interrupted by OpenSSL-related configuration errors, use one of the two solutions below:

### Feed OpenSSL Directory to CMake
If you have OpenSSL installed (e.g., via `winget install ShiningLight.OpenSSL`), point CMake to the installation folder:

```cmd
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL-Win64"
```

### Disable Security Plugin
If security capabilities are not required in your simulation environment, simply disable SSL support to eliminate the OpenSSL dependency:

```cmd
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_SSL=OFF
```
