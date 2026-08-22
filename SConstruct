#!/usr/bin/env python
import os
import sys

from methods import print_error
from build_slang import slang
from gdextension import generate as generate_gdextension, library_filename, verify_shlib_affixes

localEnv = Environment(tools=["default"], PLATFORM="")

# Build profiles can be used to decrease compile times.
# You can either specify "disabled_classes", OR
# explicitly specify "enabled_classes" which disables all other classes.
# Modify the example file as needed and uncomment the line below or
# manually specify the build_profile parameter when running SCons.

# localEnv["build_profile"] = "build_profile.json"

customs = ["custom.py"]
customs = [os.path.abspath(path) for path in customs]

opts = Variables(customs, ARGUMENTS)
opts.Update(localEnv)

Help(opts.GenerateHelpText(localEnv))

env = localEnv.Clone()

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

env = SConscript("godot-cpp/SConstruct", {"env": env, "customs": customs})

if env.get("is_msvc", False):
    env["CXXFLAGS"].remove("/std:c++17")
    env["CXXFLAGS"].insert(0, "/std:c++20")
    env["CXXFLAGS"].insert(0, "/Zc:preprocessor")
else:
    env["CXXFLAGS"].remove("-std=c++17")
    env["CXXFLAGS"].insert(0, "-std=c++20")

env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

actions = []

libname = "shader-slang"
addondir = "addons"
plugindir = f"{addondir}/{libname}"
libdir = f"{plugindir}/bin"
projectdir = "demo"

platformdir = f"{libdir}/{env['platform']}" if env["arch"] == "universal" else f"{libdir}/{env['platform']}-{env["arch"]}"

# TODO: Make this a separate flag
slang_import_enabled = env["target"] == "editor"

if slang_import_enabled:
    env.Append(
        CPPPATH=[
            "src/editor",
            "slang/include",
        ],
        LIBPATH=["slang/build/RelWithDebInfo/lib"],
        LIBS=["slang-compiler"],
        CPPDEFINES=["SLANG_IMPORT_ENABLED"],
    )
    if env["platform"] == "macos":
        env.Append(LINKFLAGS=["-Wl,-rpath,@loader_path"])

    build_preset = "vs2022" if env["platform"] == "windows" else "default"
    slang_build = slang(env=env, output_dir=f"{projectdir}/{platformdir}", build_preset=build_preset, build_type="releaseWithDebugInfo")
    actions += slang_build

    editor_sources = Glob("src/editor/*.cpp")
    for src in editor_sources + Glob("src/editor/*.h"):
        env.Depends(src, slang_build)
    sources += editor_sources

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

verify_shlib_affixes(env)
lib_filename = library_filename(env["platform"], env["target"], env["precision"], env["threads"], libname)
build_plugin_action = env.SharedLibrary(
    f"{projectdir}/{platformdir}/{lib_filename}",
    source=sources,
)

# The libraries section is derived from the binaries present in bin/
# Always run, since a previous invocation may have left a library for another target behind
def write_gdextension(target, source, env):
    generate_gdextension(f"{projectdir}/{plugindir}", str(source[0]))

gdextension_action = env.Command(
    f"{projectdir}/{plugindir}/{libname}.gdextension",
    f"{libname}.gdextension.in",
    env.Action(write_gdextension, f"Generating {libname}.gdextension..."),
)
env.AlwaysBuild(gdextension_action)
env.Depends(gdextension_action, build_plugin_action)

copy_output_action = env.Install(addondir, f"{projectdir}/{plugindir}")
copy_license_action = env.Install(plugindir, "LICENSE.md")

env.Depends([copy_output_action, copy_license_action], build_plugin_action)
env.Depends(copy_output_action, gdextension_action)

actions += [
    build_plugin_action,
    gdextension_action,
    copy_output_action,
    copy_license_action,
]
Default(*actions)
