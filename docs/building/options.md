# CMake Options

The library features several CMake options to customize compilation. Pass these variables to the `cmake` configure command with the `-D` flag (e.g. `-DHSIL_BUILD_EXAMPLES=ON`).

## Core Build Configuration Options

| CMake Option | Allowed/Typical Values | Default | Description |
|---|---|---|---|
| `CMAKE_BUILD_TYPE` | `Release` / `Debug` / `RelWithDebInfo` | `Release` | Sets the compiler optimization and debug information levels. **Note: Only works for the single-config generators on Linux or Ninja on Windows.** |
| `HSIL_BUILD_STATIC_LIB` | `ON` / `OFF` | `OFF` | Build hsil_cosim as a static library. By default shared library is built. If enabled, only static library will be built.  | 
| `HSIL_DDS_VENDOR` | Any folder under `lib/src/vendors/` | `cycloneDDS` | Selects which DDS backend implementation is linked. |
| `HSIL_BUILD_EXAMPLES` | `ON` / `OFF` | `ON` for standalone build. `OFF` for sub project build. | Enables or disables compiling the C++ executable usage examples under `examples/`. |
| `HSIL_BUILD_TESTING` | `ON` / `OFF` | `OFF` | Enables or disables compiling the GTest unit test layers. Opt-in, so that consumers integrating this library via `add_subdirectory` do not build the test suite or its Google Test dependency. |
| `HSIL_BUILD_TESTING_DDS`| `ON` / `OFF` | `ON` | Enables or disables DDS layer tests. Disable on headless CI pipelines lacking multicast loopback interfaces. |
| `ENABLE_SSL` | `ON` / `OFF` | `ON` | Control CycloneDDS library compilation with OpenSSL security plugins. |
| `HSIL_INSTALL_DOCS` | `ON` / `OFF` | `OFF` | Controls the installation of docs. | 


### Example Usage:

```bash
# Build a static library with debug build and GTest validation but without compiling loopback DDS tests or examples
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DHSIL_BUILD_STATIC_LIB=ON \
  -DHSIL_BUILD_TESTING_DDS=OFF \
  -DHSIL_BUILD_EXAMPLES=OFF
```
