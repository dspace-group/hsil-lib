# Test Suite Layers

The test framework enforces strict behavior checks at four distinct boundaries.

---

## 1. Test Layer Overview

| Executable | Layer | DDS Requirement | Scope of Coverage |
|---|---|---|---|
| **`hsilConfigTests`** | **Layer 1:** Configuration Parser | **None** | Checks simulation JSON parser. Validates correct fields, malformed lines, signal mappings, and default parameters. |
| **`hsilApiTests`** | **Layer 2:** Core API Logic | **None** | Validates API arguments, object destructions, error emissions, and dispatch filters using dummy mock structures instead of real carriers. |
| **`hsilDdsTests`** | **Layer 3:** DDS Transport Backend | **Yes** (UDP Loopback) | Triggers `hsilDdsVendor` functions: launches domain partitions, registers real topic endpoints, and asserts real loopback payload delivery. |
| **`subprojectConsumer`** / **`installedConsumer`** | **Layer 4:** Packaging & Integration | **Yes** (Participant lifecycle) | Downstream consumer applications validating library consumption via CMake `add_subdirectory()` and installed `find_package(HsilCoSim)`. |

---
