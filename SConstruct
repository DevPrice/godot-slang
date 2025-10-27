#!/usr/bin/env python
import os
import sys

from methods import print_error
from build_slang import build_slang

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

env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

actions = []

libname = "shader-slang"
addondir = "addons"
plugindir = "{}/{}".format(addondir, libname)
libdir = "{}/bin".format(plugindir)
projectdir = "demo"

platformdir = f"{libdir}/{env['platform']}-{env["arch"]}" if env["arch"] else f"{libdir}/{env['platform']}"

if env["target"] == "editor":
    slang_sources = [
        "slang/slang-tag-version.h.in",
        "slang/CMakeLists.txt",
    ]

    slang_sources += Glob("slang/*/CMakeLists.txt")
    slang_sources += Glob("slang/include/*.h")
    slang_sources += Glob("slang/source/slang/*.cpp")
    slang_sources += Glob("slang/source/slang/*.h")

    slang_lib_dir = "bin" if env["platform"] == "windows" else "lib"

    slang_lib_file = "slang/build/RelWithDebInfo/{}/{}slang{}".format(slang_lib_dir, env.subst('$SHLIBPREFIX'), env.subst('$SHLIBSUFFIX'))
    slang_outputs = [env.File(slang_lib_file), env.Dir("slang/build/RelWithDebInfo/include/")]

    if env["platform"] == "windows":
        slang_outputs += [env.File("slang/build/RelWithDebInfo/lib/slang.lib")]
    else:
        slang_outputs += [env.File("slang/build/RelWithDebInfo/lib/libslang.a")]

    slang_build = env.Command(slang_outputs, slang_sources, env.Action(build_slang, "Building Slang..."))

    env.Append(
        CPPPATH=[
            "src/editor",
            "slang/build/RelWithDebInfo/include",
        ],
        LIBPATH=["slang/build/RelWithDebInfo/lib"],
        LIBS=["slang"]
    )

    editor_sources = Glob("src/editor/*.cpp")
    for src in editor_sources + Glob("src/editor/*.h"):
        env.Depends(src, slang_build)
    sources += editor_sources

    slang_install_command = env.Install("{}/{}".format(projectdir, platformdir), slang_lib_file)
    env.Depends(slang_install_command, slang_build)
    actions += slang_install_command

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

debug_suffix = "" if env["target"] == "template_release" else ".{}".format(env["target"].replace("template_", ""))
threads_suffix = ".nothreads" if not env["threads"] else ""

lib_filename = "".join([env.subst('$SHLIBPREFIX'), libname, debug_suffix, threads_suffix, env["SHLIBSUFFIX"]])

actions += [
    env.SharedLibrary(
        "{}/{}/{}".format(projectdir, platformdir, lib_filename),
        source=sources,
    ),
    env.Install(addondir, "{}/{}".format(projectdir, plugindir)),
]
Default(*actions)
