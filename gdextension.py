#!/usr/bin/env python
"""Generates the `.gdextension` file from the libraries that were actually built.

The `[libraries]` section is derived by walking `<addon>/bin/<platform>[-<arch>]/` and
testing, for every combination of target/precision/threads, whether the library SCons
would have named for that combination exists. Nothing is declared unless it is on disk.

The same naming logic is used by `SConstruct` to name the library it builds, so the two
cannot drift. It also means CI can simply union every job's `bin/` subtree and re-run
this script to get a `.gdextension` describing exactly what the matrix produced.
"""
import os
import sys

LIBNAME = "shader-slang"

# platform -> (SHLIBPREFIX, SHLIBSUFFIX), matching what godot-cpp's platform tools set on
# the SCons environment. Verified against the environment by `verify_shlib_affixes`.
SHLIB_AFFIXES = {
    "windows": ("", ".dll"),
    "linux": ("lib", ".so"),
    "android": ("lib", ".so"),
    "macos": ("lib", ".dylib"),
    "ios": ("lib", ".dylib"),
    "web": ("lib", ".wasm"),
}

# scons target -> the Godot feature tag (and filename infix) for it
TARGET_TAGS = {
    "editor": "editor",
    "template_debug": "debug",
    "template_release": "release",
}

PRECISIONS = ["single", "double"]


def library_filename(platform, target, precision="single", threads=True, libname=LIBNAME):
    prefix, suffix = SHLIB_AFFIXES[platform]
    parts = [prefix, libname]
    if target != "template_release":
        parts.append(f".{TARGET_TAGS[target]}")
    if precision == "double":
        parts.append(".double")
    if not threads:
        parts.append(".nothreads")
    parts.append(suffix)
    return "".join(parts)


def feature_key(platform, arch, target, precision="single", threads=True):
    tags = [platform]
    if arch != "universal":
        tags.append(arch)
    tags.append(precision)
    if not threads:
        tags.append("nothreads")
    tags.append(TARGET_TAGS[target])
    return ".".join(tags)


def verify_shlib_affixes(env):
    """Fails the build if godot-cpp no longer names shared libraries as we assume."""
    platform = env["platform"]
    if platform not in SHLIB_AFFIXES:
        raise ValueError(f"Unknown platform '{platform}', add it to SHLIB_AFFIXES in gdextension.py")
    expected = SHLIB_AFFIXES[platform]
    actual = (env.subst("$SHLIBPREFIX"), env.subst("$SHLIBSUFFIX"))
    if expected != actual:
        raise ValueError(
            f"godot-cpp names shared libraries {actual} on {platform}, but gdextension.py "
            f"assumes {expected}; update SHLIB_AFFIXES so the generated .gdextension stays correct"
        )


def scan(bindir):
    """Returns the libraries present in `bindir`, grouped by platform directory.

    Each group is a list of `(feature key, path relative to the addon directory)` pairs.
    """
    groups = []
    if not os.path.isdir(bindir):
        return groups

    for dirname in sorted(os.listdir(bindir)):
        if not os.path.isdir(os.path.join(bindir, dirname)):
            continue

        # "windows-x86_64" -> ("windows", "x86_64"), "macos" -> ("macos", "universal")
        platform, _, arch = dirname.partition("-")
        if platform not in SHLIB_AFFIXES:
            continue
        arch = arch or "universal"

        group = []
        for target in TARGET_TAGS:
            for precision in PRECISIONS:
                for threads in [True, False]:
                    filename = library_filename(platform, target, precision, threads)
                    if not os.path.isfile(os.path.join(bindir, dirname, filename)):
                        continue
                    group.append((
                        feature_key(platform, arch, target, precision, threads),
                        f"./bin/{dirname}/{filename}",
                    ))

        if group:
            groups.append(group)

    return groups


def generate(addon_dir, template_path):
    """Writes `<addon_dir>/<LIBNAME>.gdextension` from the template plus a scan of `bin/`."""
    with open(template_path, "r", encoding="utf-8") as template_file:
        template = template_file.read()

    groups = scan(os.path.join(addon_dir, "bin"))
    if not groups:
        print(f"Warning: no libraries found under {addon_dir}/bin, [libraries] will be empty")

    lines = []
    for group in groups:
        if lines:
            lines.append("")
        lines += [f'{key} = "{path}"' for key, path in group]

    contents = "".join([template.rstrip("\n"), "\n", "\n".join(lines), "\n" if lines else ""])

    output_path = os.path.join(addon_dir, f"{LIBNAME}.gdextension")
    # Avoid touching the file when nothing changed, so Godot doesn't see a spurious edit.
    if os.path.isfile(output_path):
        with open(output_path, "r", encoding="utf-8") as existing:
            if existing.read() == contents:
                return output_path

    with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
        output_file.write(contents)
    return output_path


def main(argv):
    import argparse

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("addon_dir", help=f"directory containing bin/, e.g. addons/{LIBNAME}")
    parser.add_argument(
        "--template",
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)), f"{LIBNAME}.gdextension.in"),
        help="the .gdextension template to prepend to the generated [libraries] section",
    )
    args = parser.parse_args(argv)

    if not os.path.isdir(args.addon_dir):
        parser.error(f"'{args.addon_dir}' is not a directory")

    output_path = generate(args.addon_dir, args.template)
    print(f"Generated {output_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
