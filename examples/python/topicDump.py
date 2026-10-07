#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
# SPDX-License-Identifier: Apache-2.0

""" <!-------------------------------------------------------------------------->

   @file topicDump.py

   @brief Subscribe to an HSIL CoSim topic and print every received sample
          as a hexadecimal / ASCII dump to stdout.

   @author
       dSPACE SE & Co. KG

   @description
       Command-line tool that uses the hsil Python binding to subscribe to a
       single GroupedData or StreamingData topic and print hex dumps of each
       received sample.  Mirrors the C++ hsilTopicDump example.

       Usage:
           python topicDump.py --type grouped|streaming [--domain <id>] <topicName>

   <hr><br>
<!-------------------------------------------------------------------------->"""

# ------------------------------------------------------------------------------
# IMPORTS
# ------------------------------------------------------------------------------

import argparse
import sys
import time

import hsil

# ------------------------------------------------------------------------------
# HEX DUMP
# ------------------------------------------------------------------------------

_BYTES_PER_ROW = 16


def hexDump(label: str, data: bytes) -> None:
    """Print a labelled hex + ASCII dump of a byte sequence."""

    if not data:
        print(f"  {label}: (empty)")
        return

    print(f"  {label}: {len(data)} byte(s)")

    for rowStart in range(0, len(data), _BYTES_PER_ROW):
        chunk  = data[rowStart : rowStart + _BYTES_PER_ROW]
        offset = rowStart

        # -- Offset column
        line = f"  {offset:04x}: "

        # -- Hex section (with extra space after byte 8)
        for i, byte in enumerate(chunk):
            if i == 8:
                line += " "
            line += f"{byte:02X} "

        # -- Padding for short final row
        pad = _BYTES_PER_ROW - len(chunk)
        for i in range(pad):
            if i + len(chunk) == 8:
                line += " "
            line += "   "

        # -- ASCII section
        ascii_part = "".join(chr(b) if 0x20 <= b < 0x7F else "." for b in chunk)
        line += f" |{ascii_part}|"

        print(line)

# ------------------------------------------------------------------------------
# CALLBACKS
# ------------------------------------------------------------------------------

def _printMeta(protocol: str, meta: dict) -> None:
    """Print the protocol-specific meta fields."""
    if protocol == "CAN_v1":
        print(f"  meta (CAN_v1): id=0x{meta.get('messageId', 0):08X}  frameType={meta.get('frameType', 0)}")
    elif protocol == "Eth_v1":
        print(f"  meta (Eth_v1): flags=0x{meta.get('flags', 0):02X}")
    elif protocol == "EthJ_v1":
        print(f"  meta (EthJ_v1): mtu={meta.get('mtu', 0)}  flags=0x{meta.get('flags', 0):02X}")
    elif protocol == "generic":
        hexDump("meta (generic)", meta.get("data", b""))
    else:
        print(f"  meta: (unknown protocol '{protocol}')")


def onGroupedData(topic: str, groupId: int, seq: int, timestamp: int, data: bytes) -> None:
    """Print a GroupedData sample."""
    print(f"[GROUPED] topic='{topic}'  id={groupId}  seq={seq}  ts={timestamp}ns  size={len(data)}")
    hexDump("payload", data)
    print()
    sys.stdout.flush()


def onStreamingData(topic: str, protocol: str, timestamp: int, meta: dict, data: bytes) -> None:
    """Print a StreamingData sample."""
    print(f"[STREAMING] topic='{topic}'  protocol='{protocol}'  ts={timestamp}ns  data={len(data)}")
    _printMeta(protocol, meta)
    hexDump("payload", data)
    print()
    sys.stdout.flush()

# ------------------------------------------------------------------------------
# ENTRY POINT
# ------------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        prog="topicDump.py",
        description="Subscribe to an HSIL CoSim topic and dump received samples as hex.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Examples:\n"
            "  python topicDump.py --type grouped SensorTopic\n"
            "  python topicDump.py --type streaming --domain 10 CanBus0"
        ),
    )
    parser.add_argument("--type",   required=True, choices=["grouped", "streaming"],
                        dest="topicType",
                        help="Data type of the topic.")
    parser.add_argument("--domain", type=int, default=0, metavar="<id>",
                        help="DDS domain id (default: 0).")
    parser.add_argument("topicName", help="DDS topic name to subscribe to.")

    args = parser.parse_args()

    print(f"hsilTopicDump – domain {args.domain}, topic '{args.topicName}', type {args.topicType}")
    print("Press Ctrl+C to stop.\n")

    try:
        with hsil.Session(domain_id=args.domain) as session:
            if args.topicType == "grouped":
                session.subscribe_grouped(args.topicName, onGroupedData)
            else:
                session.subscribe_streaming(args.topicName, onStreamingData)

            # Block until Ctrl+C. The HSIL DDS receive thread delivers samples
            # to the callbacks while this thread sleeps. Report publisher
            # match changes so "nothing is printed" can be diagnosed: no
            # publisher discovered vs. one rejected for incompatible QoS.
            lastMatched = -1
            lastMismatch = 0
            while True:
                status = session.get_subscriber_status(args.topicName)

                if status.matchedCount != lastMatched:
                    print(f"-- matched publishers: {status.matchedCount}")
                    lastMatched = status.matchedCount

                if status.qosMismatchCount != lastMismatch:
                    reason = (" (reliability: this subscriber requires RELIABLE, "
                              "the publisher is FAST)"
                              if status.lastMismatchedQosPolicy == hsil.HSIL_QOS_MISMATCH_RELIABILITY
                              else "")
                    print(f"-- warning: {status.qosMismatchCount} publisher(s) "
                          f"rejected due to incompatible QoS{reason}", file=sys.stderr)
                    lastMismatch = status.qosMismatchCount

                time.sleep(1)

    except KeyboardInterrupt:
        pass   # clean exit on Ctrl+C
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
