#!/usr/bin/env python3

import argparse
import sys
import subprocess
import tempfile
import shutil
import os

def run(cmd, cwd=None):
    print(f"---- Running: {' '.join(cmd)} (cwd={cwd})", flush=True)
    subprocess.check_call(cmd, cwd=cwd)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--install-prefix", required=True)
    parser.add_argument("--cmake-path", default="cmake")
    parser.add_argument("--ninja-path", default="ninja")
    parser.add_argument("--git-path", default="git")
    args = parser.parse_args()

    install_prefix = os.path.abspath(args.install_prefix)
    tmp_dir = tempfile.mkdtemp(prefix="installsdl_")
    clone_dir = os.path.join(tmp_dir, "SDL")
    repo_url = "https://github.com/libsdl-org/SDL.git"

    try:
        run([args.git_path, "clone", repo_url, clone_dir])
        run([args.git_path, "checkout", "SDL2"], cwd=clone_dir)

        for build_type in ["Release"]:
            build_dir = os.path.join(clone_dir, "build", build_type)
            os.makedirs(build_dir, exist_ok=True)
            run([args.cmake_path, clone_dir,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=" + build_type,
                "-DCMAKE_INSTALL_PREFIX=" + install_prefix],
                cwd=build_dir)
            run([args.ninja_path], cwd=build_dir)
            run([args.ninja_path, "install"], cwd=build_dir)


        shutil.rmtree(tmp_dir, ignore_errors=True)
        print(f"---- Successfully installed SDL to {install_prefix}", flush=True)
        return 0

    except Exception as exc:
        print("---- ERROR:", exc, flush=True)
        if os.path.exists(tmp_dir):
            shutil.rmtree(tmp_dir, ignore_errors=True)
        sys.exit(1)

if __name__ == "__main__":
    sys.exit(main())
