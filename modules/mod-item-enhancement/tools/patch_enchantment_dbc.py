#!/usr/bin/env python3
"""Append the ten visual enhancement records to a WotLK SpellItemEnchantment.dbc."""

import argparse
import pathlib
import struct


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--base-id", type=int, default=50000)
    parser.add_argument("--levels", type=int, default=10)
    args = parser.parse_args()

    data = args.source.read_bytes()
    magic, count, fields, record_size, string_size = struct.unpack_from("<4s4I", data, 0)
    if magic != b"WDBC" or fields != 38 or record_size != 152:
        raise SystemExit(
            f"Unsupported DBC: magic={magic!r}, fields={fields}, record_size={record_size}; "
            "expected WotLK SpellItemEnchantment.dbc (38 fields, 152 bytes)."
        )

    records_end = 20 + count * record_size
    records = bytearray(data[20:records_end])
    strings = bytearray(data[records_end:records_end + string_size])
    existing = {struct.unpack_from("<I", records, offset)[0] for offset in range(0, len(records), record_size)}

    added = 0
    for level in range(1, args.levels + 1):
        enchant_id = args.base_id + level
        if enchant_id in existing:
            continue
        label = f"Улучшение +{level}".encode("utf-8") + b"\0"
        string_offset = len(strings)
        strings.extend(label)
        row = [0] * fields
        row[0] = enchant_id
        for locale_index in range(16):
            row[14 + locale_index] = string_offset
        # No enchant effects: the server module supplies the percentage bonus.
        # Flags=0 avoids socket/gem behaviour; the entry is only a visible marker.
        records.extend(struct.pack("<38I", *row))
        added += 1

    header = struct.pack("<4s4I", b"WDBC", count + added, fields, record_size, len(strings))
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    args.destination.write_bytes(header + records + strings)
    print(f"created {args.destination}: appended {added} records")


if __name__ == "__main__":
    main()
