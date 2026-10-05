# CMake Test Options

The test targets are managed by two user-facing variables. You can override these defaults during project configuration.

## Variable Reference

### `HSIL_BUILD_TESTING`
- **Default:** `OFF`
- **Description:** Global toggle controls whether the test suites are constructed. If set to `ON`, it configures directories under `lib/tests/`.

### `HSIL_BUILD_TESTING_DDS`
- **Default:** `ON` (if global testing is activated)
- **Description:** Toggles Layer 3 DDS UDP network testing.
- **Use Case:** Set this to `OFF` inside headless Cloud CI systems where multicast loopback network addresses are blocked or unavailable:

```bash
cmake -S . -B build \
  -DHSIL_BUILD_TESTING=ON \
  -DHSIL_BUILD_TESTING_DDS=OFF
```
