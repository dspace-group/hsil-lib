# Running Tests

Follow these steps to compile and verify the test coverage in your local sandbox.

---

## 1. Setup and Build

Tests are built conditionally and are turned off by default to minimize compile times. Activate them using `-DHSIL_BUILD_TESTING=ON` inside CMake:

```bash
# Ensure dependency libraries are fetched
git submodule update --init --recursive

# Configure project with testing turned ON
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DHSIL_BUILD_TESTING=ON

# Compile the targets
cmake --build build --parallel
```

---

## 2. Running the Complete Test Suite

To run all three layers via **`ctest`**:

On Linux:
```bash
ctest --test-dir build --output-on-failure
```

On Windows:
```cmd
ctest --test-dir build -C Debug --output-on-failure
```
*Note: Modify the -C option as per Debug or Release build on Windows.*

---

## 3. Running and Filtering Individual Layers

For faster feedback loops, call compiled test binaries directly.

### Running Individual Tests Directly:

On Linux:
- **Layer 1:** `./build/bin/hsilConfigTests`
- **Layer 2:** `./build/bin/hsilApiTests`
- **Layer 3:** `./build/lib/tests/hsilDdsTests`

On Windows:
- **Layer 1:** `.\build\bin\Relase\hsilConfigTests`
- **Layer 2:** `.\build\bin\Relase\hsilApiTests`
- **Layer 3:** `.\build\bin\Relase\hsilDdsTests`

### List All Available Tests:
To view available test suites without executing them, use:

On Linux
```bash
./build/bin/hsilApiTests --gtest_list_tests
```

On Windows:
```bash
.\build\bin\Release\hsilApiTests --gtest_list_tests
```

### Apply GTest Filters:
To isolate and run a single test, pass the filter command:

On Linux
```bash
./build/bin/hsilApiTests --gtest_filter="*UnknownTopicReturnsUnknownTopic*"
```

On Windows
```bash
.\build\bin\Release\hsilApiTests --gtest_filter="*UnknownTopicReturnsUnknownTopic*"
```