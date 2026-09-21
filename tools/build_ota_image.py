#!/usr/bin/env python3
"""Wraps a built .bin in a Zigbee ZCL OTA upgrade file header.

Header layout matches ezb_zcl_ota_file_header_t in
managed_components/espressif__esp-zigbee-lib/include/ezbee/zcl/cluster/ota_file.h
(standard Zigbee Alliance OTA file format, no optional header fields).

Usage:
    build_ota_image.py <input.bin> <output.ota> --manufacturer 0x131B --image-type 0x1011 \
        --file-version 0x01010001 [--header-string "Grille Fan"]
"""
import argparse
import struct
import sys

OTA_FILE_IDENTIFIER = 0x0BEEF11E
OTA_HEADER_VERSION = 0x0100
OTA_HEADER_LENGTH = 56  # mandatory header only, no optional fields
STACK_VERSION_PRO = 0x0002
TAG_UPGRADE_IMAGE = 0x0000


def build_ota_image(fw_bytes: bytes, manufacturer_code: int, image_type: int, file_version: int, header_string: str) -> bytes:
    header_string_bytes = header_string.encode("ascii")[:32].ljust(32, b"\x00")

    element_header = struct.pack("<HI", TAG_UPGRADE_IMAGE, len(fw_bytes))
    total_image_size = OTA_HEADER_LENGTH + len(element_header) + len(fw_bytes)

    header = struct.pack(
        "<IHHHHHIH32sI",
        OTA_FILE_IDENTIFIER,
        OTA_HEADER_VERSION,
        OTA_HEADER_LENGTH,
        0,  # field control: no optional fields present
        manufacturer_code,
        image_type,
        file_version,
        STACK_VERSION_PRO,
        header_string_bytes,
        total_image_size,
    )
    assert len(header) == OTA_HEADER_LENGTH, f"header packed to {len(header)} bytes, expected {OTA_HEADER_LENGTH}"

    return header + element_header + fw_bytes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input_bin")
    parser.add_argument("output_ota")
    parser.add_argument("--manufacturer", required=True, help="Manufacturer code, e.g. 0x131B")
    parser.add_argument("--image-type", required=True, help="Image type, e.g. 0x1011")
    parser.add_argument("--file-version", required=True, help="File version, e.g. 0x01010001")
    parser.add_argument("--header-string", default="Grille Fan OTA", help="Cosmetic header string (max 32 chars)")
    args = parser.parse_args()

    with open(args.input_bin, "rb") as f:
        fw_bytes = f.read()

    image = build_ota_image(
        fw_bytes,
        manufacturer_code=int(args.manufacturer, 0),
        image_type=int(args.image_type, 0),
        file_version=int(args.file_version, 0),
        header_string=args.header_string,
    )

    with open(args.output_ota, "wb") as f:
        f.write(image)

    print(f"Wrote {args.output_ota}: {len(image)} bytes (firmware payload {len(fw_bytes)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
