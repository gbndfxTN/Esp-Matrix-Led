#!/usr/bin/env python3
"""
BLE OTA client for MatrixLed-ESP – sends firmware binary to ESP32-S3 via BLE.
Usage: python ble_ota.py <firmware.bin>
"""

import asyncio
import sys
import struct
import argparse
from bleak import BleakScanner, BleakClient

SERVICE_UUID = "000000ff-0000-1000-8000-00805f9b34fb"
CHAR_RX_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"
CHAR_TX_UUID = "0000ff02-0000-1000-8000-00805f9b34fb"
DEVICE_NAME  = "MatrixLed-OTA"
CHUNK_SIZE   = 488


def chunked(data, size):
    for i in range(0, len(data), size):
        yield data[i:i + size]


async def ota_firmware(bin_path: str):
    with open(bin_path, "rb") as f:
        firmware = f.read()

    total_size = len(firmware)
    print(f"[*] Firmware: {total_size} bytes ({total_size // 1024} KiB)")
    print(f"[*] Scanning for '{DEVICE_NAME}'...")

    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    if device is None:
        print(f"[!] Device '{DEVICE_NAME}' not found")
        return 1

    print(f"[+] Found: {device.name} ({device.address})")
    print(f"[*] Connecting...")

    async with BleakClient(device) as client:
        if not client.is_connected:
            print("[!] Connection failed")
            return 1

        print(f"[+] Connected, MTU={client.mtu_size}")

        svc = client.services.get_service(SERVICE_UUID)
        if svc is None:
            print("[!] OTA service not found")
            return 1

        rx_char = svc.get_characteristic(CHAR_RX_UUID)
        if rx_char is None:
            print("[!] RX characteristic not found")
            return 1

        header = struct.pack("<I", total_size)
        print(f"[*] Sending header ({total_size} bytes total)...")
        await client.write_gatt_char(rx_char, header, response=True)

        sent = 0
        for chunk in chunked(firmware, CHUNK_SIZE):
            await client.write_gatt_char(rx_char, chunk, response=True)
            sent += len(chunk)
            pct = (sent * 100) // total_size
            print(f"\r[*] Sending: {sent}/{total_size} ({pct}%)", end="", flush=True)

        print()

    print("[+] OTA transfer complete. ESP32 will reboot.")
    return 0


def main():
    parser = argparse.ArgumentParser(description="BLE OTA for MatrixLed-ESP")
    parser.add_argument("firmware", help="Firmware .bin path")
    args = parser.parse_args()

    try:
        ret = asyncio.run(ota_firmware(args.firmware))
        sys.exit(ret)
    except KeyboardInterrupt:
        print("\n[!] Interrupted")
        sys.exit(1)


if __name__ == "__main__":
    main()
