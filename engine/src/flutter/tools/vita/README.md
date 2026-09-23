# The Vita port's patches to the dependencies

This branch, `vita-3.44.8`, is Flutter 3.44.8 (`058e0af2`) plus the
PlayStation Vita port. The engine's own changes are the commits above the
tag.

Five dependencies change too. They are not files of this repository: `DEPS`
names a revision of each and `gclient sync` checks them out.

Four of the five now come from forks, so their changes are commits as well.
`DEPS` names the fork and the commit:

| Dependency | Fork | The change |
|---|---|---|
| abseil | `flutter-vita/abseil-cpp` | No ELF introspection, no `tm_gmtoff`, `memalign` in place of `mmap` |
| zlib | `flutter-vita/zlib` | No ARMv8 code paths on this ARMv7 |
| Skia | `flutter-vita/skia` | Platform detection, and a file reader in place of `mmap` |
| BoringSSL | `flutter-vita/boringssl` | An entropy source on the console's kernel RNG |

The Dart SDK is the one that is left, and it is the largest: a Vita OS layer
for the Dart VM, 4 834 lines. Its changes are patch files here, in
`patches/dart/`, applied to `engine/src/flutter/third_party/dart` by the
`vita_patches` hook in `DEPS`. A fork replaces them, and then this directory
goes away.

`apply_patches.py` is idempotent: a patch that reverse-applies is already
in and is skipped; one that neither reverse-applies nor applies stops the
sync and names itself. `--check` reports without writing.

Why a fork and not a patch file: a patch file is not readable work. A
developer cannot use `git log` or `git blame` on it, cannot open a pull
request against it, and cannot see the change beside the code it changes.
The Tizen port needs none of this, because Tizen is Linux and the Dart VM
has a Linux layer. The Vita is not Linux.

Each fork is the upstream tree at the revision `DEPS` named before, as one
root commit, plus one commit with the port's change. The root commit is
there because gclient checks a dependency out without history, and GitHub
refuses a push from a shallow clone. The root commit's tree is identical to
upstream's: clone the upstream address at that revision and compare.

The numbering of what is left has a gap. `dart/0004` is the `runtime/` half
of the port's GN patch. Its `build/` half is not needed here, because the
engine resolves `//build/...` to `engine/src/build` and carries its own Vita
toolchain in the first commit of this branch.

The tool that builds apps against this engine, and the documentation,
are in [flutter-vita/flutter-vita](https://github.com/flutter-vita/flutter-vita).
