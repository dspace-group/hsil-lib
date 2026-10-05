""" <!-------------------------------------------------------------------------->
*
*   @file __init__.py
*
*   @brief Python ctypes binding for the HSIL CoSim shared library.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Thin ctypes wrapper around libhsil_cosim.so / hsil_cosim.dll.
*       Exposes a Session class with subscribe callbacks for both 
*       GroupedData and StreamingData topics.
*       The shared library must be built before importing this package.
*
*   @copyright
*       Copyright 2026, dSPACE SE & Co. KG. All rights reserved.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->"""

"""
HSIL CoSim Python bindings.

Thin ctypes wrapper around libhsil_cosim (shared library).
Build the library first (a shared library is the default):
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel

Then import with:
    import hsil
    with hsil.Session(domain_id=0) as session:
        session.subscribe_grouped("MyTopic", my_callback)
"""

# ------------------------------------------------------------------------------
# IMPORTS
# ------------------------------------------------------------------------------

import ctypes
import ctypes.util
import os
import platform

# ------------------------------------------------------------------------------
# LIBRARY LOADING
# ------------------------------------------------------------------------------

def _load_library() -> ctypes.CDLL:
    """Locate and load the HSIL CoSim shared library.

    Search order:
    1. HSIL_LIB_PATH environment variable (full path to the .so/.dll).
    2. Build-tree locations relative to this package (for development use).
    3. System library search via ctypes.util.find_library.
    """

    # 1. Explicit override.
    env_path = os.environ.get("HSIL_LIB_PATH")
    if env_path:
        return ctypes.CDLL(env_path)

    # 2. Derive the repository root from this file's location.
    #    examples/python/hsil/__init__.py -> examples/python/hsil -> examples/python
    #    -> examples -> repo_root
    pkg_dir   = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.normpath(os.path.join(pkg_dir, "..", "..", ".."))
    sys_name  = platform.system()

    if sys_name == "Windows":
        candidates = [
            os.path.join(repo_root, "build", "bin", "hsil_cosim.dll"),
            os.path.join(repo_root, "build", "bin", "Release", "hsil_cosim.dll"),
            os.path.join(repo_root, "build", "bin", "Debug", "hsil_cosim.dll"),
        ]
    elif sys_name == "Darwin":
        candidates = [
            os.path.join(repo_root, "build", "lib", "libhsil_cosim.dylib"),
        ]
    else:
        candidates = [
            os.path.join(repo_root, "build", "lib", "libhsil_cosim.so"),
        ]

    for path in candidates:
        if os.path.isfile(path):
            return ctypes.CDLL(path)

    # 3. System-wide search.
    name = ctypes.util.find_library("hsil_cosim")
    if name:
        return ctypes.CDLL(name)

    raise OSError(
        "libhsil_cosim not found.\n"
        "Build the library first:\n"
        "  cmake -S lib -B lib/build -DHSIL_BUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release\n"
        "  cmake --build lib/build --parallel\n"
        "Or set HSIL_LIB_PATH to the full path of the shared library."
    )


_lib = _load_library()

# ------------------------------------------------------------------------------
# ERROR CODES
# ------------------------------------------------------------------------------

HSIL_OK                = 0
HSIL_ERR_GENERIC       = -1
HSIL_ERR_INVALID_ARG   = -2
HSIL_ERR_CONFIG        = -3
HSIL_ERR_DDS           = -4
HSIL_ERR_UNKNOWN_TOPIC = -5
HSIL_ERR_OUT_OF_MEMORY = -6
HSIL_ERR_UNKNOWN_SIGNAL     = -7
HSIL_ERR_TOPIC_TYPE_MISMATCH = -8
HSIL_ERR_PUB_NOT_CREATED    = -9
HSIL_ERR_SIGNAL_CONVERSION  = -10
HSIL_ERR_SUB_NOT_CREATED    = -11

HSIL_ALL_IDS = 0xFFFFFFFF

# QoS policy reported in HsilEndpointStatus.lastMismatchedQosPolicy.
HSIL_QOS_MISMATCH_NONE        = 0
HSIL_QOS_MISMATCH_RELIABILITY = 1
HSIL_QOS_MISMATCH_OTHER       = 2

# ------------------------------------------------------------------------------
# C STRUCTURE DEFINITIONS
# ------------------------------------------------------------------------------

class HsilGroupedData(ctypes.Structure):
    """Mirror of the HsilGroupedData C struct (must match the C layout exactly)."""
    _fields_ = [
        ("id",             ctypes.c_uint32),
        ("sequenceNumber", ctypes.c_uint32),
        ("timestamp",      ctypes.c_uint64),
        ("data",           ctypes.POINTER(ctypes.c_uint8)),
        ("dataSize",       ctypes.c_size_t),
    ]


class HsilEndpointStatus(ctypes.Structure):
    """Mirror of the HsilEndpointStatus C struct (must match the C layout exactly)."""
    _fields_ = [
        ("matchedCount",             ctypes.c_int),
        ("qosMismatchCount",         ctypes.c_int),
        ("lastMismatchedQosPolicy",  ctypes.c_int),
    ]


class _HsilCanMetaData(ctypes.Structure):
    _fields_ = [
        ("messageId",  ctypes.c_uint32),
        ("frameType",  ctypes.c_uint32),  # HsilCanFrameType enum
    ]


class _HsilEthMetaData(ctypes.Structure):
    _fields_ = [
        ("flags", ctypes.c_uint8),
    ]


class _HsilEthJMetaData(ctypes.Structure):
    _fields_ = [
        ("mtu",   ctypes.c_uint32),
        ("flags", ctypes.c_uint8),
    ]


class _HsilGenericMetaData(ctypes.Structure):
    _fields_ = [
        ("size", ctypes.c_size_t),
        ("data", ctypes.POINTER(ctypes.c_uint8)),
    ]


class _HsilStreamingMetaData(ctypes.Union):
    """Mirror of the HsilStreamingMetaData C union."""
    _fields_ = [
        ("generic", _HsilGenericMetaData),
        ("can",     _HsilCanMetaData),
        ("eth",     _HsilEthMetaData),
        ("ethj",    _HsilEthJMetaData),
    ]


class HsilStreamingData(ctypes.Structure):
    """Mirror of the HsilStreamingData C struct."""
    _fields_ = [
        ("protocol",  ctypes.c_char * 8),
        ("timestamp", ctypes.c_uint64),
        ("meta",      _HsilStreamingMetaData),
        ("data",      ctypes.POINTER(ctypes.c_uint8)),
        ("dataSize",  ctypes.c_size_t),
    ]

# ------------------------------------------------------------------------------
# CALLBACK FUNCTION TYPES
# ------------------------------------------------------------------------------

# Native ctypes function types matching the C typedef signatures.
_GroupedCb   = ctypes.CFUNCTYPE(None, ctypes.c_char_p,
                                 ctypes.POINTER(HsilGroupedData), ctypes.c_void_p)
_StreamingCb = ctypes.CFUNCTYPE(None, ctypes.c_char_p,
                                 ctypes.POINTER(HsilStreamingData), ctypes.c_void_p)

# ------------------------------------------------------------------------------
# FUNCTION SIGNATURES
# ------------------------------------------------------------------------------

_lib.hsil_create.restype  = ctypes.c_int
_lib.hsil_create.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]

_lib.hsil_create_from_config.restype  = ctypes.c_int
_lib.hsil_create_from_config.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]

_lib.hsil_destroy.restype  = None
_lib.hsil_destroy.argtypes = [ctypes.c_void_p]

_lib.hsil_subscribe_grouped.restype  = ctypes.c_int
_lib.hsil_subscribe_grouped.argtypes = [
    ctypes.c_void_p,   # handle
    ctypes.c_char_p,   # topicName
    ctypes.c_void_p,   # qos (NULL = use defaults)
    ctypes.c_uint32,   # groupId
    _GroupedCb,
    ctypes.c_void_p,   # userData
]

_lib.hsil_subscribe_streaming.restype  = ctypes.c_int
_lib.hsil_subscribe_streaming.argtypes = [
    ctypes.c_void_p,   # handle
    ctypes.c_char_p,   # topicName
    ctypes.c_void_p,   # qos (NULL = use defaults)
    ctypes.c_char_p,   # protocol (nullable)
    _StreamingCb,
    ctypes.c_void_p,   # userData
]

_lib.hsil_get_subscriber_status.restype  = ctypes.c_int
_lib.hsil_get_subscriber_status.argtypes = [
    ctypes.c_void_p,              # handle
    ctypes.c_char_p,              # topicName
    ctypes.POINTER(HsilEndpointStatus),   # status (out)
]

# ------------------------------------------------------------------------------
# META DATA DECODER
# ------------------------------------------------------------------------------

def _decode_meta(protocol: str, meta: _HsilStreamingMetaData) -> dict:
    """Decode the HsilStreamingMetaData union into a protocol-specific dict."""
    if protocol == "CAN_v1":
        return {"messageId": meta.can.messageId, "frameType": meta.can.frameType}
    if protocol == "Eth_v1":
        return {"flags": meta.eth.flags}
    if protocol == "EthJ_v1":
        return {"mtu": meta.ethj.mtu, "flags": meta.ethj.flags}
    if protocol == "generic":
        raw = meta.generic
        data = bytes(raw.data[:raw.size]) if (raw.size > 0 and raw.data) else b""
        return {"data": data}
    return {}

# ------------------------------------------------------------------------------
# SESSION CLASS
# ------------------------------------------------------------------------------

class Session:
    """A HSIL CoSim session without hsil configuration file.

    Creates a DDS participant on the given domain and provides methods for
    subscribing to GroupedData and StreamingData topics.

    Usage::

        with hsil.Session(domain_id=42) as session:
            session.subscribe_grouped("SensorTopic", on_sensor)
            session.subscribe_streaming("CanBus0", on_can)
            while True:
                time.sleep(1)

    Grouped callback signature::
        def on_sensor(topic: str, id: int, seq: int, timestamp: int, data: bytes) -> None

    Streaming callback signature::
        def on_can(topic: str, protocol: str, timestamp: int, meta: dict, data: bytes) -> None

    The ``meta`` dict contains protocol-specific fields:

    * ``CAN_v1``  – ``{"messageId": int, "frameType": int}``
    * ``Eth_v1``  – ``{"flags": int}``
    * ``EthJ_v1`` – ``{"mtu": int, "flags": int}``
    * ``generic`` – ``{"data": bytes}``
    """

    def __init__(self, domain_id: int = 0):
        handle = ctypes.c_void_p()
        rc = _lib.hsil_create(domain_id, None, ctypes.byref(handle))
        if rc != HSIL_OK:
            raise RuntimeError(f"hsil_create() failed on domain {domain_id} (rc={rc})")
        self._handle    = handle
        # Keep strong references to ctypes callback wrappers to prevent GC.
        self._callbacks = []

    # --------------------------------------------------------------------------
    # SUBSCRIBE
    # --------------------------------------------------------------------------

    def subscribe_grouped(self, topic_name: str, callback) -> None:
        """Subscribe to a GroupedData topic.

        Callback receives ``(topic: str, id: int, seq: int, timestamp: int, data: bytes)``.
        Data bytes are copied before the callback returns; no pointer aliasing issues.
        """

        def _native_cb(c_topic, c_data_ptr, _user_data):
            d = c_data_ptr.contents
            payload = bytes(d.data[:d.dataSize]) if (d.dataSize > 0 and d.data) else b""
            callback(c_topic.decode(), d.id, d.sequenceNumber, d.timestamp, payload)

        wrapped = _GroupedCb(_native_cb)
        self._callbacks.append(wrapped)          # prevent garbage collection

        rc = _lib.hsil_subscribe_grouped(self._handle, topic_name.encode(), None, HSIL_ALL_IDS, wrapped, None)
        if rc != HSIL_OK:
            raise RuntimeError(f"hsil_subscribe_grouped('{topic_name}') failed (rc={rc})")

    def subscribe_streaming(self, topic_name: str, callback, protocol: str = None) -> None:
        """Subscribe to a StreamingData topic.

        Callback receives ``(topic: str, protocol: str, timestamp: int, meta: bytes, data: bytes)``.
        Pass ``protocol`` to filter by protocol tag; ``None`` receives all protocols.
        """

        def _native_cb(c_topic, c_data_ptr, _user_data):
            d = c_data_ptr.contents
            proto   = d.protocol.decode("ascii", errors="replace").rstrip("\x00")
            meta_d  = _decode_meta(proto, d.meta)
            payload = bytes(d.data[:d.dataSize]) if (d.dataSize > 0 and d.data) else b""
            callback(c_topic.decode(), proto, d.timestamp, meta_d, payload)

        wrapped = _StreamingCb(_native_cb)
        self._callbacks.append(wrapped)

        c_proto = protocol.encode() if protocol else None
        rc = _lib.hsil_subscribe_streaming(self._handle, topic_name.encode(), None, c_proto, wrapped, None)
        if rc != HSIL_OK:
            raise RuntimeError(f"hsil_subscribe_streaming('{topic_name}') failed (rc={rc})")

    # --------------------------------------------------------------------------
    # STATUS
    # --------------------------------------------------------------------------

    def get_subscriber_status(self, topic_name: str) -> HsilEndpointStatus:
        """Return the current status of the subscriber on ``topic_name``.

        Useful to tell "no publisher discovered yet" apart from "a publisher was
        found but rejected because its QoS is incompatible"::

            status = session.get_subscriber_status("SensorTopic")
            if status.matchedCount == 0 and status.qosMismatchCount > 0:
                print("publisher rejected due to incompatible QoS")

        The call does not block; poll it to await discovery.
        """
        status = HsilEndpointStatus()
        rc = _lib.hsil_get_subscriber_status(self._handle, topic_name.encode(), ctypes.byref(status))
        if rc != HSIL_OK:
            raise RuntimeError(f"hsil_get_subscriber_status('{topic_name}') failed (rc={rc})")
        return status

    # --------------------------------------------------------------------------
    # LIFECYCLE
    # --------------------------------------------------------------------------

    def close(self) -> None:
        """Destroy the session and release all DDS resources."""
        if self._handle:
            _lib.hsil_destroy(self._handle)
            self._handle = None
            self._callbacks.clear()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
