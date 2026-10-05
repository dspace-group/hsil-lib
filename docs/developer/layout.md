# Source Code Layout

The core HSIL CoSim library code is structured systematically to separate interface definitions from target-dependent implementations.

```
lib/
├── CMakeLists.txt              # Standard system exports and compilation directives
│
├── include/                    # Exported public API headers
│   └── hsil/
│       └── hsil.h              # The only header required by client integrations
│
└── src/                        # Private implementation source files
    ├── ddsAbstraction.h        # Vtable contract declaration (HsilDdsOps)
    ├── hsil.cpp                # Core library source
    ├── hsilConfig.h            # Header structure for simulation schema configurations
    ├── hsilConfig.cpp          # Config file parsing implementation (nlohmann_json)
    │
    └── vendors/                # Interchangeable transport implementation drivers
        └── cycloneDDS/         # Default CycloneDDS provider
            ├── CMakeLists.txt  # IDL compilation rules and sub-target linking
            ├── cycloneDdsProvider.h / .cpp  # Concrete implementations of HsilDdsOps
            └── idl/
                └── HSIL-CoSim.idl  # Vendor-specific interface descriptors
```
