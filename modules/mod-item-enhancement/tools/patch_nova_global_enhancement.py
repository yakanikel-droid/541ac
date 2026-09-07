#!/usr/bin/env python3
"""Patch the exact supplied WoW 3.3.5a build 12340 client.

The stock item-link formatter hardcodes gem4 to zero.  This patch makes the
central formatter copy a validated enhancement marker (50001..50010) from the
fourth value supplied by every item-link source.  For live Item objects it also
loads PRISMATIC_ENCHANTMENT_SLOT (6) into that fourth value before calling the
same formatter.  No per-window Lua interception is involved.
"""

from hashlib import sha256
from pathlib import Path
import shutil
import struct
import sys

EXPECTED_SHA256 = "8149c26659957b0d648867ed4795d16d851158c4f6293a2727194abab942b797"

TEXT_FILE_DELTA = 0x400C00  # VA - file offset for .text
ZDATA_VA = 0x00DD1000
ZDATA_FILE = 0x0072CE00


def rel32(source_va: int, target_va: int) -> bytes:
    return struct.pack("<i", target_va - (source_va + 5))


def jmp(source_va: int, target_va: int) -> bytes:
    return b"\xE9" + rel32(source_va, target_va)


def require(data: bytearray, offset: int, expected: bytes, label: str) -> None:
    actual = bytes(data[offset:offset + len(expected)])
    if actual != expected:
        raise SystemExit(
            f"{label}: unexpected bytes at 0x{offset:X}: "
            f"{actual.hex()} != {expected.hex()}"
        )


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: patch_nova_global_enhancement.py INPUT.exe OUTPUT.exe")

    source = Path(sys.argv[1])
    output = Path(sys.argv[2])
    digest = sha256(source.read_bytes()).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"unsupported client SHA-256: {digest}")

    shutil.copyfile(source, output)
    data = bytearray(output.read_bytes())

    formatter_hook_va = 0x0061E311
    formatter_hook_file = formatter_hook_va - TEXT_FILE_DELTA
    object_hook_va = 0x0061E48D
    object_hook_file = object_hook_va - TEXT_FILE_DELTA

    require(data, formatter_hook_file, bytes.fromhex("8B450C8B55FC"), "item-link formatter")
    require(data, object_hook_file, bytes.fromhex("8B46088B7508"), "live item-link source")
    require(data, ZDATA_FILE, b"\x00" * 0x80, "empty executable code cave")

    # Central formatter cave. It is assembled for VA 0xDD1080 from the
    # accompanying nova_global_link_formatter.s source.
    formatter_cave_va = ZDATA_VA + 0x80
    formatter_cave = bytes.fromhex(
        "8b450c8b55fc508b45148b400c3d51c30000724b3d5ac30000774489c181e950c30000"
        "518d8dfcfeffff5152ff751cff7518508b4d14ff7108ff7104ff31ff75105753680012"
        "dd0068000400006850cfc500e899df99ff83c450e973d284ff8d8dfcfeffff5152ff75"
        "1cff75186a008b4d14ff7108ff7104ff31ff7510575368044fa20068000400006850cf"
        "c500e85ddf99ff83c44ce937d284ff"
    )
    enhanced_format = b"%s|Hitem:%d:%d:%d:%d:%d:%d:%d:%d:%d|h[%s +%d]|h%s\x00"

    # Cave 2: expose PRISMATIC_ENCHANTMENT_SLOT (object value offset 0x88)
    # as the fourth value consumed by the central formatter.
    cave2_va = ZDATA_VA + 0x40
    cave2 = bytearray.fromhex(
        "8B86D4000000"      # mov eax,[esi+0D4h]  (object values)
        "85C0"              # test eax,eax
        "7408"              # je zero
        "8B8088000000"      # mov eax,[eax+88h]   (slot 6 enchant id)
        "EB02"              # jmp store
        "33C0"              # zero: xor eax,eax
        "8945F0"            # store: mov [ebp-10h],eax (gem4)
        "8B4608"            # original: mov eax,[esi+8]
        "8B7508"            # original: mov esi,[ebp+8]
    )
    cave2 += jmp(cave2_va + len(cave2), 0x0061E493)

    data[ZDATA_FILE + 0x40:ZDATA_FILE + 0x40 + len(cave2)] = cave2
    data[ZDATA_FILE + 0x80:ZDATA_FILE + 0x80 + len(formatter_cave)] = formatter_cave
    data[ZDATA_FILE + 0x200:ZDATA_FILE + 0x200 + len(enhanced_format)] = enhanced_format
    data[formatter_hook_file:formatter_hook_file + 6] = jmp(formatter_hook_va, formatter_cave_va) + b"\x90"
    data[object_hook_file:object_hook_file + 6] = jmp(object_hook_va, cave2_va) + b"\x90"

    # A modified executable no longer has the original PE checksum. Windows
    # does not require it for a normal user-mode executable; zero is explicit.
    pe_checksum_offset = 0xD8
    data[pe_checksum_offset:pe_checksum_offset + 4] = b"\x00\x00\x00\x00"
    output.write_bytes(data)

    print(f"input_sha256={digest}")
    print(f"output_sha256={sha256(data).hexdigest()}")
    print(f"output={output}")


if __name__ == "__main__":
    main()
