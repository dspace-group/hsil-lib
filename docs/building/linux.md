# Building on Linux

Perform the steps mentioned below to configure, compile, and install the library on Linux systems.

## 1. Standalone Build and Installation

Run the following commands from the **repository root** to configure, build, and install the static or shared library as a standalone project:

### Configure and Build

```bash
# Initialize submodules (if not already done)
git submodule update --init --recursive

# Configure the build directory
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build the project in parallel
cmake --build build --parallel
```

### Artifacts in the Build tree
After a successful build, you will find:
- **`build/bin`**: Example applications (if enabled) with hsil config json files, Unit tests applications (if enabled) and idl compiler
- **`build/lib`**: `libhsil_cosim.so` or `libhsil_cosim.a`, DDS vendor as shared library (by default `libddsc.so` for CycloneDDS) and its dependent shared libraries.

---

### Installation

To install the built headers and libraries onto your system, run:

```bash
cmake --install build --prefix /path/to/install/dir
```

### Artifacts in the Install tree
After a successful installation, you will find:
- **`<install root>/bin`**: Example applications (if enabled) with hsil config json files, Unit tests applications (if enabled) and idl compiler
- **`<install root>/lib`**: `libhsil_cosim.so` or `libhsil_cosim.a`, DDS vendor as shared library (by default `libddsc.so` for CycloneDDS) and its dependent shared libraries.
- **`<install root>/include`**: `hsil/hsil.h` public header of hsil_cosim library, dds vendor headers
- **`<install root>/lib/cmake/HsilCoSim`**: CMake target designators (`HsilCoSimConfig.cmake` etc.) for packaging.
---

## 2. Building and Integrating as a Subproject (via add_subdirectory)

If you are developing a parent application that uses CMake, you can integrate the HSIL CoSim library directly into your project's build tree using `add_subdirectory`. This compiles the library from source as part of your overall application ([Example CMake file](../../lib/tests/integration/subproject_consumer/CMakeLists.txt)).

### Recommended CMake Configuration
To integrate the library, add the subproject folder in your parent `CMakeLists.txt`. The test suite is already off by default, so you only need to disable the examples if you do not want them. You can also configure whether to build it as a static or shared library:

```cmake
# Optional: Force HSIL library to build as a static library
set(HSIL_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)

# Optional: skip the bundled examples (the test suite is OFF by default)
set(HSIL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Add the HSIL CoSim directory
add_subdirectory("path/to/hsil-cosim" "project-build-dir/hsil-cosim")

# Link against the HSIL library target
target_link_libraries(my_app PRIVATE HsilCoSim::hsil_cosim)
```

By linking against the alias `HsilCoSim::hsil_cosim` (or directly against `hsil_cosim`), you can transparently switch between using the library found via `find_package` or built via `add_subdirectory` without making changes to your target definitions.

### Output Directories and Library Discovery

When included as a subproject, HSIL CoSim always generates its build artifacts (libraries, DDS vendor runtime, examples, and tests) in its own subproject build folder (`${CMAKE_CURRENT_BINARY_DIR}/lib` and `${CMAKE_CURRENT_BINARY_DIR}/bin`), without polluting the parent scope.

Inside the build tree CMake sets a build RPATH automatically, so `my_app` runs without `LD_LIBRARY_PATH`. Once you install your own application, give it an RPATH relative to its own location so it can still find `libhsil_cosim.so` and the DDS vendor library:

```cmake
include(GNUInstallDirs)

set_target_properties(my_app PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
)
```

Adjust the relative path if your executables and libraries are not installed into `bin/` and `lib/` respectively. On macOS use `@loader_path` in place of `$ORIGIN`.

---

## 3. Linking Against the Standalone Library

If you built and installed the library standalone, you can link against it in one of the following ways.

### Option A: Using CMake (Recommended)
If your application uses CMake, simply locate and link the installed package ([Example CMake file](../../lib/tests/integration/installed_consumer/CMakeLists.txt)):

```cmake
# Required when install directory is not in PATH. Can also be supplied as argument (-DCMAKE_PREFIX_PATH="path") during CMake configuration
set(CMAKE_PREFIX_PATH "/path/to/install/root/dir")

find_package(HsilCoSim REQUIRED)

target_link_libraries(my_app PRIVATE HsilCoSim::hsil_cosim)
```

### Option B: Manual Linking (No CMake)
You can directly link the static libraries using GCC or Clang:

```bash
gcc my_app.c \
    -I/usr/local/include \
    -L/usr/local/lib \
    -lhsil_cosim \
    -lddsc \
    -lpthread \
    -o my_app
```
*(Note: `-lpthread` enables multi-threading support,`-lddsc` links against the shared CycloneDDS library dependencies. Use different library if the dds vendor is not cyclonedds).*
