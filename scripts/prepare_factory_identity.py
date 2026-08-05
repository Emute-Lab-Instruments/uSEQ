#!/usr/bin/env python3
"""Build a CRC-protected uSEQ factory identity sector and provision-only UF2."""

from __future__ import annotations

import argparse
import binascii
import datetime
import json
import struct
from pathlib import Path

SECTOR_SIZE = 4096
RECORD_SIZE = 128
XIP_BASE = 0x10000000
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID = 0x00002000
UF2_FAMILY = {
    "rp2040": 0xE48BFF56,
    "rp2350": 0xE48BFF59,
}
MCU_CODE = {"rp2040": 1, "rp2350": 2}


def fixed_text(value: str, width: int, name: str) -> bytes:
    encoded = value.encode("ascii")
    if not encoded or len(encoded) > width:
        raise ValueError(f"{name} must be 1-{width} ASCII bytes")
    return encoded + bytes(width - len(encoded))


def make_record(args: argparse.Namespace) -> bytes:
    record = bytearray(RECORD_SIZE)
    record[0:8] = b"USEQID1\0"
    record[8] = 1
    record[9] = MCU_CODE[args.mcu]
    record[10] = args.i2c_address
    features = 0x2 | (0x1 if args.usb_update else 0)
    struct.pack_into("<I", record, 12, features)
    record[16:32] = fixed_text(args.product, 16, "product")
    record[32:40] = fixed_text(args.hardware_revision, 8, "hardware revision")
    record[40:56] = fixed_text(args.assembly_variant, 16, "assembly variant")
    record[56:72] = fixed_text(args.batch, 16, "batch")
    record[72:96] = fixed_text(args.serial, 24, "serial")
    record[96:106] = fixed_text(args.manufacture_date, 10, "manufacture date")
    struct.pack_into("<I", record, 106, args.flash_size)
    struct.pack_into("<I", record, 124, binascii.crc32(record[:124]) & 0xFFFFFFFF)
    return bytes(record)


def make_uf2(sector: bytes, flash_offset: int, family_id: int) -> bytes:
    payload_size = 256
    block_count = len(sector) // payload_size
    blocks = []
    for block_number in range(block_count):
        payload = sector[block_number * payload_size:(block_number + 1) * payload_size]
        header = struct.pack(
            "<IIIIIIII",
            UF2_MAGIC_START0,
            UF2_MAGIC_START1,
            UF2_FLAG_FAMILY_ID,
            XIP_BASE + flash_offset + block_number * payload_size,
            payload_size,
            block_number,
            block_count,
            family_id,
        )
        blocks.append(header + payload + bytes(476 - payload_size) + struct.pack("<I", UF2_MAGIC_END))
    return b"".join(blocks)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--product", default="useq-exp-aout08")
    parser.add_argument("--hardware-revision", required=True)
    parser.add_argument("--assembly-variant", default="production")
    parser.add_argument("--batch", required=True)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--manufacture-date", required=True, help="YYYY-MM-DD")
    parser.add_argument("--mcu", choices=sorted(MCU_CODE), default="rp2040")
    parser.add_argument("--flash-size", type=lambda value: int(value, 0), default=2 * 1024 * 1024)
    parser.add_argument("--i2c-address", type=lambda value: int(value, 0), default=0,
                        help="fixed address 1-126; 0 derives it from the MCU ID")
    parser.add_argument("--usb-update", action="store_true")
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    if not 0 <= args.i2c_address <= 126:
        parser.error("--i2c-address must be between 0 and 126")
    if args.flash_size < SECTOR_SIZE or args.flash_size % SECTOR_SIZE:
        parser.error("--flash-size must be a positive multiple of 4096")
    try:
        manufacture_date = datetime.date.fromisoformat(args.manufacture_date)
    except ValueError:
        parser.error("--manufacture-date must be a valid YYYY-MM-DD date")
    if manufacture_date.isoformat() != args.manufacture_date:
        parser.error("--manufacture-date must use canonical YYYY-MM-DD format")
    return args


def main() -> None:
    args = parse_args()
    record = make_record(args)
    sector = record + bytes([0xFF]) * (SECTOR_SIZE - len(record))
    flash_offset = args.flash_size - SECTOR_SIZE
    uf2 = make_uf2(sector, flash_offset, UF2_FAMILY[args.mcu])

    args.output_dir.mkdir(parents=True, exist_ok=True)
    stem = f"{args.product}-{args.serial}-factory-identity"
    bin_path = args.output_dir / f"{stem}.bin"
    uf2_path = args.output_dir / f"{stem}.uf2"
    json_path = args.output_dir / f"{stem}.json"
    bin_path.write_bytes(sector)
    uf2_path.write_bytes(uf2)
    metadata = {
        "schemaVersion": 1,
        "product": args.product,
        "hardwareRevision": args.hardware_revision,
        "assemblyVariant": args.assembly_variant,
        "batch": args.batch,
        "serial": args.serial,
        "manufactureDate": args.manufacture_date,
        "mcu": args.mcu,
        "flashSize": args.flash_size,
        "flashOffset": flash_offset,
        "xipAddress": XIP_BASE + flash_offset,
        "i2cAddress": args.i2c_address,
        "i2cAddressPolicy": "chip-id" if args.i2c_address == 0 else "fixed",
        "features": ["i2c-outputs"] + (["usb-update"] if args.usb_update else []),
        "recordCrc32": f"{struct.unpack_from('<I', record, 124)[0]:08x}",
        "files": {"bin": bin_path.name, "uf2": uf2_path.name},
    }
    json_path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(json_path)


if __name__ == "__main__":
    main()
