#!/usr/bin/env python3

import argparse
import sys
import os
import urllib.request
import zipfile
import shutil
import tempfile


def fail(msg):
    print(f"---- ERROR (in fetchDawnPort.py): {msg}", file=sys.stderr)
    sys.exit(1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--extern-dir", required=True)
    parser.add_argument("--dawn-version", required=True)
    args = parser.parse_args()

    extern_dir = os.path.abspath(args.extern_dir)
    pkg_dir = os.path.join(extern_dir, "emdawnwebgpu_pkg")
    port_file = os.path.join(pkg_dir, "emdawnwebgpu.port.py")

    tmp = tempfile.mkdtemp(prefix="fetchdawn_")

    zipname = f"emdawnwebgpu_pkg-v{args.dawn_version}.zip"
    url = f"https://github.com/google/dawn/releases/download/v{args.dawn_version}/{zipname}"
    zippath = os.path.join(tmp, zipname)

    try:
        print("---- Downloading: ", url)
        with urllib.request.urlopen(url) as r, open(zippath, "wb") as f:
            shutil.copyfileobj(r, f)

        print("---- Extracting zip file: ", zippath)
        with zipfile.ZipFile(zippath, "r") as z:
            z.extractall(tmp)

        # find emdawnwebgpu_pkg folder
        found_pkg = None
        for root, dirs, _ in os.walk(tmp):
            if "emdawnwebgpu_pkg" in dirs:
                found_pkg = os.path.join(root, "emdawnwebgpu_pkg")
                break

        if not found_pkg:
            fail("emdawnwebgpu_pkg not found in the extracted Dawn package")

        if os.path.exists(pkg_dir):
            shutil.rmtree(pkg_dir)

        shutil.move(found_pkg, pkg_dir)

        if not os.path.exists(port_file):
            fail("emdawnwebgpu.port.py missing after installation")

        print("---- Successfully fetched Dawn port:", pkg_dir)

        return 0

    except Exception as e:
        fail(str(e))

    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
