#!/usr/bin/env python3
import os
import sys
import time
import urllib.request

URL = "https://updates.cdn-apple.com/2023SpringFCS/fullrestores/032-85130/6AB0D10A-8AF9-4F6C-8CF5-A1B315CABE88/iPhone10,3,iPhone10,6_16.5_20F66_Restore.ipsw"
DEST = r"C:\Users\Usuario\Downloads\iPhone10,3,iPhone10,6_16.5_20F66_Restore.ipsw"
EXPECTED_SIZE = 6165698198

def main():
    print(f"[*] Starting download of iOS 16.5 (20F66) IPSW...")
    print(f"[*] Destination: {DEST}")
    print(f"[*] Expected Size: {EXPECTED_SIZE / (1024**3):.2f} GB ({EXPECTED_SIZE} bytes)")

    downloaded = 0
    if os.path.isfile(DEST):
        downloaded = os.path.getsize(DEST)
        if downloaded == EXPECTED_SIZE:
            print("[+] IPSW already completely downloaded!")
            return
        elif downloaded > EXPECTED_SIZE:
            print("[!] File corrupted or oversized, restarting...")
            os.remove(DEST)
            downloaded = 0
        else:
            print(f"[*] Resuming from byte {downloaded} ({downloaded / (1024**2):.1f} MB)...")

    headers = {"User-Agent": "Mozilla/5.0"}
    if downloaded > 0:
        headers["Range"] = f"bytes={downloaded}-"

    req = urllib.request.Request(URL, headers=headers)
    t0 = time.time()
    last_log = t0
    last_bytes = downloaded

    try:
        with urllib.request.urlopen(req) as resp, open(DEST, "ab" if downloaded > 0 else "wb") as f:
            chunk_size = 1024 * 1024 # 1 MB chunks
            while True:
                chunk = resp.read(chunk_size)
                if not chunk:
                    break
                f.write(chunk)
                downloaded += len(chunk)
                now = time.time()
                if now - last_log >= 5.0 or downloaded == EXPECTED_SIZE:
                    speed = (downloaded - last_bytes) / (now - last_log) / (1024 * 1024)
                    pct = (downloaded / EXPECTED_SIZE) * 100
                    print(f"[{pct:5.1f}%] {downloaded / (1024**3):.2f} / {EXPECTED_SIZE / (1024**3):.2f} GB | Speed: {speed:5.1f} MB/s")
                    last_log = now
                    last_bytes = downloaded

        total_time = time.time() - t0
        print(f"[+] Download finished successfully in {total_time:.1f}s!")
        print(f"[+] Final file size: {os.path.getsize(DEST)} bytes")
    except Exception as e:
        print(f"[-] Download error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
