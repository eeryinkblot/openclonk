# Platforms

What is known to work where, what is only assumed, and the traps each platform
has already cost time on.

**Read the status markers literally.** The difference between "verified" and
"derived" is the whole point of this file — most of the time lost on this fork
went into things that looked verified and were not.

| Marker | Meaning |
| --- | --- |
| **verified here** | Actually run on this machine |
| **verified in CI** | Green in `.github/workflows/build.yml` |
| **derived** | Read out of `CMakeLists.txt` or inferred. Never executed. |

## Status

| Platform | `C4GROUP_TOOL_ONLY` | `HEADLESS_ONLY` | Full build | GUI actually launched |
| --- | --- | --- | --- | --- |
| macOS arm64 | verified here | verified here + CI | verified here + CI | **yes**, played |
| Linux x86_64 | derived | verified in CI | derived | no |
| Windows x64 | verified in CI | derived | derived | no |

Everything in the "derived" column is an open question, not a formality. On
macOS the full build took eight fixes to reach; there is no reason to expect the
untried combinations to be free.

---

## macOS / Apple Silicon

The reference platform for this fork. Everything below is **verified here**.

```sh
brew install cmake libepoxy openal-soft miniupnpc freealut \
             libpng jpeg-turbo freetype libogg libvorbis sdl2 curl

# headless: openclonk-server, c4group, c4script
/opt/homebrew/bin/cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=ON .

# playable: openclonk.app with graphics and sound
/opt/homebrew/bin/cmake -B build-gui -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DHEADLESS_ONLY=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/openal-soft .
```

- `cmake` is **not on PATH** — use `/opt/homebrew/bin/cmake`.
- `CMAKE_PREFIX_PATH` is not optional. openal-soft is keg-only; without it
  `FindAudio` falls back to Apple's deprecated framework. Check for
  `Using Audio toolkit: OpenAL` at configure time and
  `OpenAL extensions loaded. Available: AL_EFFECT_REVERB, …` at runtime.
- **Qt5 is gone from Homebrew**, so `WITH_QT_EDITOR` is off and `src/editor/` is
  not compiled by anything here. Editor code paths are untested by this fork.
- Unit tests need googletest **sources**, unpacked at
  `/Users/tk/Repositories/clonk/deps/googletest-release-1.10.0`. See CLAUDE.md.

Runtime paths:

| What | Where |
| --- | --- |
| Log | `~/Library/Application Support/OpenClonk/OpenClonk.log` (not stdout, when bundled) |
| Config | `~/Library/Preferences/org.openclonk.openclonk.config` |

Known-good log lines to check against: `GL 4.1 Metal - … on Apple M1 Pro`,
`OpenAL extensions loaded. … Unavailable: (none).`,
`C4AulScriptEngine linked - 91026 lines, 0 warnings, 0 errors`.

---

## Linux x86_64

**Verified in CI** for `HEADLESS_ONLY` on `ubuntu-latest`, including unit tests,
packing all 13 groups and running a scenario. Everything else is open.

```sh
sudo apt-get install -y --no-install-recommends \
  libpng-dev libjpeg-dev libfreetype-dev zlib1g-dev \
  libcurl4-openssl-dev libminiupnpc-dev

cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=ON .
cmake --build build --parallel "$(getconf _NPROCESSORS_ONLN)"
```

On Arch/EndeavourOS the package names differ (`libpng`, `libjpeg-turbo`,
`freetype2`, `zlib`, `curl`, `miniupnpc`) — **derived**, not tried.

### What this machine can do that CI cannot

- **Run the GUI.** `HEADLESS_ONLY=OFF` on Linux is completely untested in this
  fork; the runner has no display. That covers mouse input and sound, which is
  where three of the eight macOS defects were. Needs epoxy, OpenAL, freealut,
  Ogg/Vorbis and SDL2 on top of the list above — **derived** from the macOS
  requirements, since `FindAudio` and the windowing code branch differently
  there (`USE_SDL_MAINLOOP` instead of `USE_COCOA`).
- **A much newer toolchain.** Arch carries current GCC and glibc where
  `ubuntu-latest` is conservative. That matters: `gzio.c` broke on modern clang
  for exactly this reason (see divergence.md 3), and a newer GCC is a good
  canary for the next such case.

---

## Windows x64

**Verified in CI** for `C4GROUP_TOOL_ONLY` only. That is a real result — **MSVC
2022 compiles all of `libmisc` and `c4group` without a single error**, the first
successful Windows build of this codebase in roughly five years, and it needed
no source changes. Two `CMakeLists.txt` defects had to be fixed to get there
(divergence.md 14 and 15).

```powershell
vcpkg install zlib --triplet x64-windows

cmake -B build -A x64 -DC4GROUP_TOOL_ONLY=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows .
cmake --build build --config RelWithDebInfo --target c4group --parallel
```

### Next step: HEADLESS_ONLY — derived

The dependency set is smaller than it looks. Going through the `REQUIRED`
`find_package` calls: Freetype and Epoxy are excluded by `HEADLESS_ONLY`, and
SDL2 is only required under `USE_SDL_MAINLOOP`, which is off on Windows because
`USE_WIN32_WINDOWS` wins. What remains:

```powershell
vcpkg install zlib libpng libjpeg-turbo curl --triplet x64-windows

cmake -B build -A x64 -DHEADLESS_ONLY=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows .
cmake --build build --config RelWithDebInfo --parallel
```

The unknown is the Win32 layer, untouched since VS 2017:
`C4WindowWin32.cpp`, `C4ConsoleWin32.cpp`, `C4CrashHandlerWin32.cpp`,
`StdSchedulerWin32.cpp`, `C4AppWin32Impl.h`, `C4windowswrapper.h`. Expect the
same kind of hunt the macOS side needed.

Note also that `C4GROUP_TOOL_ONLY` still *defines* `libc4script` and
`libopenclonk`, so the default `all` target tries to compile sources needing PNG
and JPEG that the configuration does not look for. Build the `c4group` target
explicitly, or use `HEADLESS_ONLY` instead.

### Windows traps already paid for

- **Binaries land in the build root**, not `build/<Config>/`.
  `oc_set_target_names()` sets `RUNTIME_OUTPUT_DIRECTORY` per configuration to
  `CMAKE_CURRENT_BINARY_DIR`, and non-`RelWithDebInfo` configurations get a
  suffixed name (`c4group-debug.exe`). Search for the file rather than assuming
  a layout.
- **In PowerShell, reset `$LASTEXITCODE`** after deliberately failing a command,
  or the enclosing script inherits the non-zero status.

---

## Traps that apply everywhere

Each of these cost at least one debugging session. All are documented in detail
in [divergence.md](divergence.md) and [ci.md](ci.md).

**`c4group <name>` creates the group** when it does not exist —
`ProcessGroup()` opens with `do_create` set. A missing file is therefore not an
error, and is useless as a negative test. Use a usage error such as `-t` with no
destination.

**The engine finds its data next to the executable, or not at all.**
`C4Reloc::Init()` prefers a `planet` folder beside the binary and otherwise
falls back to the install prefix. An uninstalled out-of-source build has
neither, and fails with `Error opening system group file (System.ocg)!` — on
everything except macOS, where `SystemDataPath` is the executable's own
directory. Symlink the packed groups into `<build>/planet`, or build in-source
the way Travis did.

**Closing stdin quits the engine.** `C4StdInProc::Execute()` treats a failed
read as a shutdown request. That is the intended mechanism, but it means any
context where stdin is `/dev/null` — a CI step, some launchers — makes the
engine exit before it initialises, silently and with status 0. Not observed on
macOS, where it keeps running.

**The engine writes `Sound=0` permanently** whenever it starts without working
audio; a single headless run is enough. Total silence with correct volume
values and no error anywhere. Check the `[Sound]` section of the config before
suspecting the audio libraries.

**`cmake --build … -j` with no number means unlimited parallelism** to GNU Make.
About 500 translation units will exhaust a machine's memory. Always pass a
count.

---

## Recording findings

When something here turns out to be wrong, or a "derived" entry gets exercised
for the first time, change the marker and say what was actually run. An entry
that claims more confidence than it has is worse than no entry: three of the six
failed Windows CI attempts were assumptions of exactly that kind.

New defects go in [divergence.md](divergence.md); the reasoning behind a fix,
and the alternatives rejected, go in [decisions.md](decisions.md).
