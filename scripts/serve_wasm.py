#!/usr/bin/env python3
import http.server
import socketserver
import os
import sys
import subprocess
import platform
from pathlib import Path


def auto_detect_build_type(project_root):
    debug_dir = project_root / "build" / "wasm-debug" / "webgpu_app"
    release_dir = project_root / "build" / "wasm-release" / "webgpu_app"

    if debug_dir.exists() and release_dir.exists():
        debug_time = debug_dir.stat().st_mtime
        release_time = release_dir.stat().st_mtime
        return "release" if release_time > debug_time else "debug"
    elif debug_dir.exists():
        return "debug"
    elif release_dir.exists():
        return "release"
    else:
        return None


def serve_wasm(port=8000):
    project_root = Path(__file__).parent.parent

    build_type = auto_detect_build_type(project_root)
    if build_type is None:
        print("Error: No WebAssembly build found.")
        print("\nBuild the project first:")
        print("  cmake --preset wasm-debug")
        print("  cmake --build --preset wasm-debug")
        sys.exit(1)

    print(f"Auto-detected build type: {build_type}")
    build_dir = project_root / "build" / f"wasm-{build_type}" / "webgpu_app"
    os.chdir(build_dir)

    class WasmHandler(http.server.SimpleHTTPRequestHandler):
        def end_headers(self):
            self.send_header("Cross-Origin-Opener-Policy", "same-origin")
            self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
            super().end_headers()

        extensions_map = {
            **http.server.SimpleHTTPRequestHandler.extensions_map,
            '.wasm': 'application/wasm',
            '.js': 'application/javascript',
        }

    with socketserver.TCPServer(("", port), WasmHandler) as httpd:
        url = f"http://localhost:{port}/webgpu_app.html"
        print(f"Serving: {build_dir}")
        print(f"Opening: {url}\n")

        try:
            if platform.system() == "Windows":
                subprocess.Popen(["cmd", "/c", "start", url], shell=True)
            elif platform.system() == "Darwin":
                subprocess.Popen(["open", url])
            else:
                subprocess.Popen(["xdg-open", url])
        except Exception as e:
            print(f"Could not open browser: {e}")
            print(f"Open manually: {url}\n")

        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped.")


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    serve_wasm(port)
