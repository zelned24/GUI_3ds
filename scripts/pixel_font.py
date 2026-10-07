"""Remove rasterizer antialiasing from mkbcfnt A4 texture sheets.

Layout follows libctru font.h TGLP_s. Glyph maps, metrics and sheet layout
are preserved; this is an explicit presentation conversion for Old 3DS.
"""
import struct


def crisp_font(data: bytes) -> bytes:
    if len(data) < 20 or data[:4] not in (b"CFNT", b"CFNU"):
        raise ValueError("Invalid BCFNT header")
    if struct.unpack_from("<H", data, 4)[0] != 0xFEFF:
        raise ValueError("Only little-endian BCFNT is supported")
    position = struct.unpack_from("<H", data, 6)[0]
    if struct.unpack_from("<I", data, 12)[0] != len(data):
        raise ValueError("BCFNT size mismatch")
    result = bytearray(data)
    found = False
    for _ in range(struct.unpack_from("<I", data, 16)[0]):
        if position + 8 > len(data):
            raise ValueError("Truncated BCFNT block")
        size = struct.unpack_from("<I", data, position + 4)[0]
        if size < 8 or position + size > len(data):
            raise ValueError("Invalid BCFNT block size")
        if data[position:position + 4] == b"TGLP":
            if size < 32 or found:
                raise ValueError("Invalid TGLP block")
            sheet_size, sheets, texture_format = struct.unpack_from("<IHH", data, position + 12)
            width, height, offset = struct.unpack_from("<HHI", data, position + 24)
            if texture_format != 11:  # libctru GPU_A4
                raise ValueError("Expected A4 font sheets")
            end = offset + sheet_size * sheets
            if not sheets or sheet_size != width * height // 2 or offset < position + 32 or end > len(data):
                raise ValueError("Invalid A4 sheet bounds")
            for i in range(offset, end):
                value = data[i]
                result[i] = (0xF0 if value >> 4 >= 8 else 0) | (0x0F if value & 15 >= 8 else 0)
            found = True
            # mkbcfnt declares a 32-byte TGLP header; sheet bytes follow separately.
            position = max(position + size, end)
        else:
            position += size
    if not found:
        raise ValueError("Missing TGLP")
    return bytes(result)
