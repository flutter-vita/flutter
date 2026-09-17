#!/usr/bin/env python3
"""Apply the Vita port's patches to the DEPS-managed trees.

The engine's own Vita changes are commits on this branch. The Dart SDK, zlib,
Skia, abseil and BoringSSL are not files of this repository: DEPS names their
revisions and gclient checks them out. The port changes them too (the Dart VM
has no Vita OS layer of its own), and those changes live here as patch files,
one directory per tree, applied by the DEPS hook that runs this script after
every `gclient sync`.

Idempotent: a patch that reverse-applies is already in, and is skipped. A
patch that neither reverse-applies nor applies stops the sync with its name,
which is what a Flutter upgrade looks like from here.

    python3 engine/src/flutter/tools/vita/apply_patches.py            apply
    python3 engine/src/flutter/tools/vita/apply_patches.py --check    report only
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# The repository root: engine/src/flutter/tools/vita -> five levels up.
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))

# Patch directory -> the tree it applies to, relative to the repository root.
TREES = {
    "dart": "engine/src/flutter/third_party/dart",
    "zlib": "engine/src/flutter/third_party/zlib",
    "skia": "engine/src/flutter/third_party/skia",
    "abseil": "engine/src/third_party/abseil-cpp",
    "boringssl": "engine/src/flutter/third_party/boringssl/src",
}


def git_apply(tree, patch, *flags):
    return subprocess.run(
        ["git", "-C", tree, "apply", *flags, patch],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0


def main(argv):
    check = "--check" in argv
    failed = 0
    for name, rel in TREES.items():
        tree = os.path.join(ROOT, rel)
        patch_dir = os.path.join(HERE, "patches", name)
        patches = sorted(p for p in os.listdir(patch_dir) if p.endswith(".patch"))
        if not os.path.isdir(os.path.join(tree, ".git")) and not os.path.isfile(os.path.join(tree, ".git")):
            print("vita patches: %s is not checked out (%s); %d patches skipped" % (name, rel, len(patches)))
            continue
        for p in patches:
            path = os.path.join(patch_dir, p)
            if git_apply(tree, path, "--check", "--reverse"):
                print("vita patches: %s/%s already applied" % (name, p))
                continue
            if not git_apply(tree, path, "--check"):
                print("vita patches: %s/%s does not apply to %s" % (name, p, rel))
                failed += 1
                continue
            if check:
                print("vita patches: %s/%s would apply" % (name, p))
                continue
            if git_apply(tree, path):
                print("vita patches: %s/%s applied" % (name, p))
            else:
                print("vita patches: %s/%s failed to apply to %s" % (name, p, rel))
                failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
