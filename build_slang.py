#!/usr/bin/env python
import os
import subprocess

def build_slang(target, source, env):
    slang_dir = "slang"
    build_dir = os.path.join(slang_dir, "build")

    cmake_env = os.environ.copy()

    configure_file = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.exists(configure_file):
        os.makedirs(build_dir, exist_ok=True)

        build_preset = "default"

        if env["platform"] == "windows":
            build_preset = "vs2022"

        configure_cmd = [
            "cmake",
            "--preset", build_preset,
            "-DSLANG_ENABLE_GFX=FALSE",
            "-DSLANG_ENABLE_SLANGI=FALSE",
            "-DSLANG_ENABLE_SLANGRT=FALSE",
            "-DSLANG_ENABLE_TESTS=FALSE",
            "-DSLANG_ENABLE_EXAMPLES=FALSE",
            "-DSLANG_LIB_TYPE=SHARED",
        ]

        result = subprocess.run(configure_cmd, cwd=slang_dir, env=cmake_env)
        if result.returncode != 0:
            print("Error: CMake configuration failed!")
            return result.returncode

    build_type = "releaseWithDebugInfo"
    build_cmd = [
        "cmake",
        "--build",
        "--preset", build_type,
    ]

    result = subprocess.run(build_cmd, cwd=slang_dir, env=cmake_env)
    if result.returncode != 0:
        print("Error: CMake build failed!")
        return result.returncode

    return None
