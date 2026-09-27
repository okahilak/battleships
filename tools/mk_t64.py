#!/usr/bin/env python3
"""Wrap .prg files into a .t64 tape image.

Usage: mk_t64.py out.t64 "TAPE NAME" file.prg[=NAME] ...
"""
import os
import struct
import sys


def petscii_name(s, size):
    return s.upper().encode("ascii")[:size].ljust(size, b" ")


def main():
    out, tape_name, files = sys.argv[1], sys.argv[2], sys.argv[3:]
    entries = []
    for f in files:
        path, _, name = f.partition("=")
        data = open(path, "rb").read()
        name = name or os.path.splitext(os.path.basename(path))[0]
        entries.append((name, data[:2], data[2:]))

    header = b"C64S tape image file".ljust(32, b"\0")
    header += struct.pack("<HHHH", 0x0101, len(entries), len(entries), 0)
    header += petscii_name(tape_name, 24)

    offset = 64 + 32 * len(entries)
    directory = b""
    payload = b""
    for name, load, body in entries:
        start = struct.unpack("<H", load)[0]
        directory += struct.pack("<BBHHHII", 1, 0x82, start, start + len(body), 0, offset, 0)
        directory += petscii_name(name, 16)
        payload += body
        offset += len(body)

    with open(out, "wb") as f:
        f.write(header + directory + payload)


if __name__ == "__main__":
    main()
