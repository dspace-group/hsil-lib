# API Reference

---

## Concepts

### Simulation Agents and Topics
An agent is any executable participating in the co-simulation. Agents exchange information using named **topics** (plain strings) through a publish-subscribe architecture. All agents in a co-simulation must share the same **DDS domain** — an integer in the range `0`–`232` (default is 42).

### Opaque C API
- Include: `#include <hsil/hsil.h>`
- All symbols prefixed with `hsil_` (e.g. `hsil_create`, `hsil_destroy`)
- The library never throws — all functions return integer error codes
- Internal implementation uses C++17 but is completely hidden from the consumer

### Supported Data Types

| Data Type | Purpose | Use Case |
|---|---|---|
| `GroupedData` | Packed binary array of engineering signals | High-frequency, bulk transmission of fixed-layout engineering values |
| `StreamingData` | Raw byte buffers with protocol markings | Bus frames (CAN, Ethernet) or custom byte streams |

---

## Data Types

### `HsilGroupedData`

| Field | C Type | Description |
|---|---|---|
| `id` | `uint32_t` | Unique data-group identifier |
| `sequenceNumber` | `uint32_t` | Monotonically increasing counter |
| `timestamp` | `uint64_t` | Simulation time in nanoseconds |
| `data` | `uint8_t*` | Packed binary payload (valid only inside the callback) |
| `dataSize` | `size_t` | Byte count of `data` |

### `HsilStreamingData`

| Field | C Type | Description |
|---|---|---|
| `protocol` | `char[8]` | Protocol identifier: `"generic"`, `"CAN_v1"`, `"Eth_v1"`, `"EthJ_v1"` |
| `timestamp` | `uint64_t` | Simulation time in nanoseconds |
| `meta` | `HsilStreamingMetaData` | Protocol-specific metadata union |
| `data` | `const uint8_t*` | Raw payload (valid only inside callback) |
| `dataSize` | `size_t` | Byte count of `data` |

### Callback Rules
- Callbacks execute on internal DDS receiver threads — implementations must be thread-safe.
- Keep callbacks short and non-blocking; defer heavy work to a separate thread.
- **`data` pointers inside `HsilGroupedData` and `HsilStreamingData` are only valid for the duration of the callback.** Copy the data if you need it beyond the callback.

### `HsilEndpointStatus`

Filled in by `hsil_get_publisher_status()` and `hsil_get_subscriber_status()`. Both calls return immediately; poll them to await discovery.

| Field | C Type | Description |
|---|---|---|
| `matchedCount` | `int` | Number of compatible remote endpoints currently matched. For publisher `0` means no subscriber is matched and for subscriber `0` means no publisher is matched. |
| `qosMismatchCount` | `int` | Cumulative count of remote endpoints rejected because their QoS was incompatible |
| `lastMismatchedQosPolicy` | `HsilQosMismatchPolicy` | Which policy caused the most recent rejection |

`matchedCount` is a live count that rises and falls as peers come and go. `qosMismatchCount` only ever increases. A topic that stays at `matchedCount == 0` while `qosMismatchCount > 0` indicates a QoS misconfiguration rather than a missing peer.

### `HsilQosMismatchPolicy`

| Value | Numeric | Meaning |
|---|---|---|
| `HSIL_QOS_MISMATCH_NONE` | `0` | No incompatibility has been recorded |
| `HSIL_QOS_MISMATCH_RELIABILITY` | `1` | A `FAST` publisher was paired with a `RELIABLE` subscriber |
| `HSIL_QOS_MISMATCH_OTHER` | `2` | Another, non-HSIL QoS policy was incompatible |

---

## Error Codes

| Code | Value | Meaning |
|---|---|---|
| `HSIL_OK` | `0` | Success |
| `HSIL_WARN_SIGNAL_CONVERSION` | `1` | Write succeeded with lossy type conversion |
| `HSIL_ERR_GENERIC` | `-1` | Unspecified internal error |
| `HSIL_ERR_INVALID_ARG` | `-2` | NULL pointer or invalid parameter |
| `HSIL_ERR_CONFIG` | `-3` | Config file missing, unreadable, or malformed |
| `HSIL_ERR_DDS` | `-4` | DDS vendor backend failure |
| `HSIL_ERR_UNKNOWN_TOPIC` | `-5` | Topic not declared in the configuration |
| `HSIL_ERR_OUT_OF_MEMORY` | `-6` | Memory allocation failure |
| `HSIL_ERR_UNKNOWN_SIGNAL` | `-7` | Signal name not found for the given topic/group |
| `HSIL_ERR_TOPIC_TYPE_MISMATCH` | `-8` | Topic used with the wrong data type |
| `HSIL_ERR_PUB_NOT_CREATED` | `-9` | Publisher not created for the topic |
| `HSIL_ERR_SIGNAL_CONVERSION` | `-10` | Type combination is not convertible |
| `HSIL_ERR_SUB_NOT_CREATED` | `-11` | Subscriber not created for the topic |

Diagnostic messages are also written to `stderr` alongside error codes.
