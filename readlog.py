#!/usr/bin/env python3
import struct
import sys

HDR = struct.Struct("<6I")          # magic, size, head, total, state, seq
MAGIC = 0x31474F4C                  # "LOG1"
STATES = {1: "running (never marked done)", 2: "left cleanly"}


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    raw = open(sys.argv[1], "rb").read()
    if len(raw) < HDR.size:
        sys.exit("file too short")

    magic, size, head, total, state, seq = HDR.unpack_from(raw, 0)
    if magic != MAGIC:
        sys.exit("no log header found - the program did not run, or crashed "
                 "before log_init() (magic = 0x%08X)" % magic)
    if size > len(raw) - HDR.size or head >= max(size, 1):
        sys.exit("header looks corrupt (size=%d head=%d)" % (size, head))

    data = raw[HDR.size:HDR.size + size]
    wrapped = total > size
    text = data[head:] + data[:head] if wrapped else data[:total]
    text = text.decode("ascii", errors="replace")

    if wrapped:
        text = text.split("\n", 1)[-1]

    print("# state: %s, %d lines written, %d bytes%s"
          % (STATES.get(state, state), seq, total,
             ", older lines overwritten" if wrapped else ""))
    print(text, end="" if text.endswith("\n") else "\n")


if __name__ == "__main__":
    main()
