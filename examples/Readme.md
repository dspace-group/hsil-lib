# HSIL CoSim API – Examples

This directory contains standalone usage examples showing how to integrate and use the HSIL CoSim API library across multiple languages (C++ and Python).

---

## 1. Prerequisites

To build and run the examples, your machine must meet the following minimum requirements:

| Tool | Minimum Version | Notes |
|---|---|---|
| **CMake** | `3.21` | Required to build C++ targets |
| **C++ Compiler** | C++17 support | GCC 7+, Clang 6+, MSVC 2019+ |
| **Git** | `2.13+` | To fetch vendor dependencies in `third_party/` |
| **Python** | `3.6+` | To run the Python ctypes example. |

---

## 2. Building the C++ Examples

The C++ examples are integrated directly with the main CMake workspace.

### Together with the Library (Linux / macOS)

From the repository root, build the main project with `HSIL_BUILD_EXAMPLES=ON`:

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHSIL_BUILD_EXAMPLES=ON

# Compile everything
cmake --build build --parallel
```

### Together with the Library (Windows)

Run inside an **x64 Native Tools Command Prompt for VS 2022/2019**:

```cmd
# Create VS Solution
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DHSIL_BUILD_EXAMPLES=ON

# Build Release targets
cmake --build build --config Release --parallel
```

*(For Ninja on Windows: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DHSIL_BUILD_EXAMPLES=ON` followed by `cmake --build build`)*

### Standalone Against an Installed Library

If you have already compiled and installed the library globally to `/usr/local`:

```bash
cmake -S examples -B examples/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local
cmake --build examples/build --parallel
```

---

## 3. C++ Examples Guide

All compiled C++ example binaries are located under the `build/bin/` directory.

### Example 1: BMS Scenario with HSIL Config (`cpp/bmsScenarioWithHsilConfig`)

This scenario simulates a multi-agent battery temperature monitoring network with modular profile setups.

- **`hsilTempSensor`**: Publishes battery temperature signals periodically. You can type `o` (over) or `n` (normal) in the terminal to alter temperatures interactively.
- **`hsilBmsEcu`**: Subscribes to temperatures. If any battery cell exceeds 55°C, it transmits a cooling command over a streaming CAN bus.
- **`hsilCoolingEcu`**: Subscribes to the cooling CAN topic and prints incoming instructions.

**Running the Scenario:**
Open three separate terminals from the repository root:

```bash
# Terminal 1: Sensor Simulation
./build/bin/hsilTempSensor

# Terminal 2: ECU Logic
./build/bin/hsilBmsEcu

# Terminal 3: Cooling Actuator
./build/bin/hsilCoolingEcu
```

---

### Example 2: Pub/Sub without hsil Configuration file (`cpp/withoutHsilConfig`)

Shows how to publish and subscribe entirely without any hsil configuration JSON file.

- **`hsilGroupedDataPubNoConfig`** & **`hsilGroupedDataSubNoConfig`**: Demonstrate low-level `GroupedData` publishing and packet-callback subscription.
- **`hsilStreamingDataPubGenericNoConfig`** & **`hsilStreamingDataSubGenericNoConfig`**: Demonstrate raw unstructured binary packet delivery via `StreamingData`.

**Running:**
```bash
# To test signals loopback:
./build/bin/hsilGroupedDataSubNoConfig &
./build/bin/hsilGroupedDataPubNoConfig
```

---

### Example 3: Latency Benchmark (`cpp/latencyBenchmark`)

Measures the round-trip latency of `GroupedData` payloads through the full DDS serialization / network delivery loop.

- **Single-Machine Mode** (Pinger and Ponger threads in a single process):
  ```bash
  ./build/bin/hsilLatencyBenchmark --count 100 --size 64
  ```
- **Two-Machine Mode** (Communicating over separate physical systems on the same network domain):
  ```bash
  # Machine B (Start Ponger first)
  ./build/bin/hsilLatencyBenchmark --role ponger --count 100 --size 64
  
  # Machine A (Run Pinger)
  ./build/bin/hsilLatencyBenchmark --role pinger --count 100 --size 64
  ```
  *Note: The example application can simply be run without any arguments (uses defaults internally).*
  Checkout help:
  ```bash
  ./build/bin/hsilLatencyBenchmark --help
  ```

---

## 4. Python Example (`python/`)

A simple utility script that uses Python's `ctypes` bindings to load the underlying compiled `libhsil_cosim.so` or `hsil_cosim.dll` shared library from the build tree

- **Source**: [python/topicDump.py](python/topicDump.py)

**Running**:
1. Compile the shared library first (ensure `HSIL_BUILD_STATIC_LIB=OFF`).
2. Set the environment variable `HSIL_LIB_PATH` to `path/to/hsil_cosim/library`, if the built library is not in the project root.
3. Run the dumper against any active topic on the network:
   ```bash
   python python/topicDump.py --type grouped --domain 42 SensorTopic
   ```

---

## 5. Directory Structure

```
examples/
├── CMakeLists.txt                    - Top level examples CMake configure
├── cpp/                              - C++ implementation directories
│   ├── bmsScenarioWithHsilConfig/    - Grouped & Streaming config-mode case study
│   ├── withoutHsilConfig/            - pub/sub without hsil configuration file
│   └── latencyBenchmark/             - Single & multi-machine latency tool
└── python/                           - ctypes Python bindings wrapper & dumper
```
