#!/usr/bin/env python3
"""
mk-a56-boot.py: Build a standalone Android Boot Image v4 for Samsung Galaxy A56 5G (SM-A566E / Exynos 1580).

Layout (Boot Image Header Version 4):
  - Page 0 (4096 bytes): boot_img_hdr_v4
  - Kernel pages: ARM64 kernel / shim with appended XNUP payload
  - Ramdisk: empty (or standalone ramdisk if specified)
  - Padded to partition block size (64 MiB = 67,108,864 bytes)
  - Output: boot.img and boot.tar (for Odin / Download Mode)
"""
import os
import sys
import struct
import tarfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
A56_DIR = os.path.join(ROOT, 'devices', 'a56')
FIRMWARE_DIR = r'C:\Users\Usuario\a56-firmware'

STOCK_KERNEL = os.path.join(FIRMWARE_DIR, 'unpacked', 'kernel')
PAYLOAD_BIN = os.path.join(A56_DIR, 'payload-a56.bin')
OUT_DIR = os.path.join(A56_DIR, 'build')
OUT_BOOT_IMG = os.path.join(OUT_DIR, 'boot.img')
OUT_BOOT_TAR = os.path.join(OUT_DIR, 'hakios-a56-boot.tar')

PARTITION_SIZE = 64 * 1024 * 1024 # 64 MiB = 16384 blocks * 4096 bytes
PAGE_SIZE = 4096

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    print(f'[*] Building standalone HakiOS boot image for Galaxy A56...')

    if not os.path.isfile(PAYLOAD_BIN):
        sys.exit(f'[-] Missing payload: {PAYLOAD_BIN}')
    
    # Load kernel base
    if os.path.isfile(STOCK_KERNEL):
        print(f'[+] Using base ARM64 GKI kernel from {STOCK_KERNEL}')
        kernel_data = open(STOCK_KERNEL, 'rb').read()
    else:
        # Fallback to prebuilt kernel
        prebuilt = os.path.join(ROOT, 'prebuilt', 'hakios-kernel')
        print(f'[+] Using prebuilt XNU kernel from {prebuilt}')
        kernel_data = open(prebuilt, 'rb').read()

    payload_data = open(PAYLOAD_BIN, 'rb').read()
    print(f'[+] Loaded payload: {len(payload_data)} bytes')

    # Append payload to kernel with alignment
    combined_kernel = kernel_data + b'\0' * (-len(kernel_data) % PAGE_SIZE) + payload_data
    kernel_size = len(combined_kernel)
    kernel_pages = (kernel_size + PAGE_SIZE - 1) // PAGE_SIZE
    padded_kernel_size = kernel_pages * PAGE_SIZE

    print(f'[+] Total kernel + payload size: {kernel_size} bytes ({kernel_pages} pages)')

    # Build Header v4
    # magic: ANDROID! (8 bytes)
    # kernel_size (uint32)
    # ramdisk_size (uint32) = 0
    # os_version (uint32) = 0
    # header_size (uint32) = 1584
    # reserved[4] (uint32) = 0
    # header_version (uint32) = 4
    # cmdline[1536 + 512] = 2048 bytes
    # signature_size (uint32) = 0

    hdr = bytearray(PAGE_SIZE)
    hdr[:8] = b'ANDROID!'
    struct.pack_into('<IIII', hdr, 8, kernel_size, 0, 0, 1584)
    struct.pack_into('<I', hdr, 40, 4) # header_version = 4

    cmdline = b'loop.max_part=7'
    hdr[64:64+len(cmdline)] = cmdline

    # Assemble boot image
    with open(OUT_BOOT_IMG, 'wb') as f:
        f.write(hdr)
        f.write(combined_kernel)
        f.write(b'\0' * (padded_kernel_size - kernel_size))
        # Pad up to partition size (64 MiB)
        current_len = PAGE_SIZE + padded_kernel_size
        if current_len < PARTITION_SIZE:
            f.write(b'\0' * (PARTITION_SIZE - current_len))

    print(f'[+] Written {OUT_BOOT_IMG}: {os.path.getsize(OUT_BOOT_IMG)} bytes')

    # Create Odin-flashable TAR
    print(f'[*] Creating Odin-flashable TAR archive...')
    with tarfile.open(OUT_BOOT_TAR, 'w') as tar:
        tar.add(OUT_BOOT_IMG, arcname='boot.img')

    print(f'[+] Created Odin package: {OUT_BOOT_TAR} ({os.path.getsize(OUT_BOOT_TAR)} bytes)')
    print(f'[+] Standalone build complete!')

if __name__ == '__main__':
    main()
