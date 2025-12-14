#!/usr/bin/env python
import os
import subprocess
from SCons.Defaults import Copy

def slang(env, output_dir, build_preset = "default", build_type = "releaseWithDebugInfo"):
    def build_slang(env, target, source):
        slang_dir = "slang"
        build_dir = os.path.join(slang_dir, "build")

        cmake_env = os.environ.copy()

        configure_file = os.path.join(build_dir, "CMakeCache.txt")
        if not os.path.exists(configure_file):
            os.makedirs(build_dir, exist_ok=True)

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

            if env["platform"] == "macos":
                if env["arch"] == "universal":
                    configure_cmd.append("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
                else:
                    configure_cmd.append(f"-DCMAKE_OSX_ARCHITECTURES={env["arch"]}")

            result = subprocess.run(configure_cmd, cwd=slang_dir, env=cmake_env)
            if result.returncode != 0:
                print("Error: CMake configuration failed!")
                return result.returncode

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

    slang_sources = [
        "slang/slang-tag-version.h.in",
        "slang/CMakeLists.txt",
    ]

    slang_sources += env.Glob("slang/*/CMakeLists.txt")
    slang_sources += env.Glob("slang/include/*.h")
    slang_sources += env.Glob("slang/source/slang/*.cpp")
    slang_sources += env.Glob("slang/source/slang/*.h")

    slang_lib_dir = "bin" if env["platform"] == "windows" else "lib"

    base_lib_name = f"slang/build/RelWithDebInfo/{slang_lib_dir}/{env.subst('$SHLIBPREFIX')}slang-compiler"
    slang_lib_files = [env.File(f"{base_lib_name}{env["SHLIBSUFFIX"]}")]
    if env["platform"] != "windows":
        slang_lib_files += [file for file in env.Glob(f"{base_lib_name}{env["SHLIBSUFFIX"]}.0.*") if not str(file).endswith(".dwarf")]

    slang_outputs = [env.File(slang_lib_files), env.Dir("slang/build/RelWithDebInfo/include/")]

    if env["platform"] == "windows":
        slang_outputs += [env.File("slang/build/RelWithDebInfo/lib/slang-compiler.lib")]
    else:
        slang_outputs += [env.File("slang/build/RelWithDebInfo/lib/libslang-compiler.a")]

    slang_build = env.Command(slang_outputs, slang_sources, env.Action(build_slang, "Building Slang..."))

    slang_install_command = env.Install(output_dir, [file for file in slang_lib_files if not os.path.islink(str(file))])
    env.Depends(slang_install_command, slang_build)

    return slang_install_command
