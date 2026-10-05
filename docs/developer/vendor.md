# Adding a New DDS Vendor Backend

You can easily swap the default CycloneDDS carrier for another vendor (for example, FastDDS). Follow the step-by-step pipeline below to build and expose a generic endpoint:

---

## 1. Directory and Submodule Creation
1. Link your desired DDS SDK source repository under `third_party/` as a Git submodule.
2. Create a folder under `lib/src/vendors/`, e.g., `lib/src/vendors/fastdds/`.

---

## 2. CMake Integration
Inside your new subdirectory, create a `CMakeLists.txt` that:
1. Generates compiler types from the project IDL interface (`idl/HSIL-CoSim.idl`) using the target vendor's stub-compiler.
2. Formulates a **object library target** named explicitly as **`hsilDdsVendorObject`**.
3. Link dynamically against the target vendor library.
4. Finally, add find_dependency() to the vendor target in lib/cmake/HsilCoSimConfig.cmake.in file. This allows cmake to automatically locate the dds vendor library in the install tree avoiding the need to manually link it from user applications. 
5. hsilDdsVendorObject will now automatically be added and compiled as a part of the main library. The dependency to the internal target vendor is also exported. 

---

## 3. Operations Vtable Implementation
Implement `hsil_getDdsOps()` inside your code. This function must return a pointer to a populated `HsilDdsOps` struct declared in `src/ddsAbstraction.h`:

| Function Slot | Signature | Purpose |
|---|---|---|
| `init` | `void*(int domainId, const char* ddsConfigPath)` | Instantiates a DDS DomainParticipant |
| `destroy` | `void(void* participant)` | Deletes a participant |
| `createGroupedWriter` | `void*(void* participant, const char* topic, const HsilQos* qos)`| Creates a DataWriter for GroupedData |
| `createStreamingWriter`| `void*(void* participant, const char* topic, const HsilQos* qos)`| Creates a DataWriter for StreamingData |
| `destroyWriter` | `void(void* participant, void* writer)` | Explicitly deletes a writer |
| `writeGrouped` | `int(void* part, void* writer, const HsilGroupedData* data)` | Publishes an single GroupedData sample |
| `writeStreaming` | `int(void* part, void* writer, const HsilStreamingData* data)`| Publishes a single StreamingData sample |
| `createGroupedReader` | `void*(void* part, const char* topic, const HsilQos* qos, HsilGroupedDataCallback cb, void* user)`| Creates reader, registers listener callback |
| `createStreamingReader`| `void*(void* part, const char* topic, const HsilQos* qos, HsilStreamingDataCallback cb, void* user)`| Creates reader, registers listener callback |
| `destroyReader` | `void(void* participant, void* reader)` | Explicitly deletes a reader |
| `getEndpointStatus` | `int(void* part, void* endpoint, int isWriter, HsilEndpointStatus* status)` | Reports matched count and QoS-mismatch info for one endpoint |

> **Note:** `HsilDdsOps` is populated using positional aggregate initialization. Always append new slots at the **end** of the struct and the **end** of the initializer, otherwise existing slots silently bind to the wrong functions.

---

## 4. Activation
To compile the library with your custom backend, specify its directory name when configuring the project:

```bash
cmake -S . -B build -DHSIL_DDS_VENDOR=fastdds
```
**Note:The name of the vendor should match the folder name of the vendor source eg. lib/src/vendors/fastdds**
No modifications to the core library wrapper files are needed.
