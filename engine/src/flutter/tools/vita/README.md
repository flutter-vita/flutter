# The Vita port's patches to the dependencies

This branch, `vita-3.44.8`, is Flutter 3.44.8 (`058e0af2`) plus the
PlayStation Vita port. The engine's own changes are the commits above the
tag. Four dependencies change too, and they are not files of this
repository: `DEPS` names their revisions and `gclient sync` checks them
out. Their changes live here as patch files and go in through the
`vita_patches` hook in `DEPS`, which runs `apply_patches.py` after every
sync.

| Directory | Tree | What the patches are |
|---|---|---|
| `patches/dart/` | `engine/src/flutter/third_party/dart` | A Vita OS layer for the Dart VM: the platform dispatch, `runtime/bin` (sockets, files, the event handler), emulated TLS, the `vm:entry-point` hooks an embedder needs, the SSL filter on the isolate |
| `patches/zlib/` | `engine/src/flutter/third_party/zlib` | No ARMv8 paths on this ARMv7 |
| `patches/skia/` | `engine/src/flutter/third_party/skia` | Platform detection, and no `mmap` |
| `patches/boringssl/` | `engine/src/flutter/third_party/boringssl/src` | An entropy backend on the console's RNG |

`apply_patches.py` is idempotent: a patch that reverse-applies is already
in and is skipped; one that neither reverse-applies nor applies stops the
sync and names itself. `--check` reports without writing.

Why patches and not forks: the Tizen port needs none of this, because
Tizen is Linux and the VM has a Linux layer. The Vita is not Linux. A
fork per dependency is the cleaner shape for a large series; fifteen
patches is not yet that, and turning them into forks later is `git am`
and one line in `DEPS`.

The numbering has gaps (`dart/0005` is the zlib patch, filed by the
component it fixes). `dart/0004` is the `runtime/` half of the port's
GN patch: its `build/` half is not needed here, because the engine
resolves `//build/...` to `engine/src/build` and carries its own Vita
toolchain in the first commit of this branch.

The tool that builds apps against this engine, and the documentation,
are in [flutter-vita/flutter-vita](https://github.com/flutter-vita/flutter-vita).
