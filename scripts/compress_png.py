#!/usr/bin/env python3
"""Re-compresses the PNGs written by encodePng (stored deflate blocks) with
zlib level 9. Lossless: the pixel data is decompressed and compressed again.
usage: compress_png.py <dir>   (all *.png below <dir>, in place)"""
import pathlib
import struct
import sys
import zlib


def chunks(data):
    pos = 8
    while pos < len(data):
        n = struct.unpack(">I", data[pos:pos + 4])[0]
        yield data[pos + 4:pos + 8], data[pos + 8:pos + 8 + n]
        pos += 12 + n


def chunk(kind, body):
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)


def main(root):
    total_before = total_after = 0
    for p in sorted(pathlib.Path(root).rglob("*.png")):
        data = p.read_bytes()
        if data[:8] != b"\x89PNG\r\n\x1a\n":
            raise SystemExit(f"{p}: not a PNG")
        parts = list(chunks(data))
        raw = zlib.decompress(b"".join(body for kind, body in parts if kind == b"IDAT"))
        out = data[:8]
        wrote_idat = False
        for kind, body in parts:
            if kind == b"IDAT":
                if not wrote_idat:
                    out += chunk(b"IDAT", zlib.compress(raw, 9))
                    wrote_idat = True
                continue
            out += chunk(kind, body)
        # verify before replacing
        check = zlib.decompress(b"".join(body for kind, body in chunks(out) if kind == b"IDAT"))
        if check != raw:
            raise SystemExit(f"{p}: verification failed")
        p.write_bytes(out)
        total_before += len(data)
        total_after += len(out)
        print(f"{p}: {len(data)} -> {len(out)} bytes")
    print(f"total {total_before} -> {total_after} bytes")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else ".")
