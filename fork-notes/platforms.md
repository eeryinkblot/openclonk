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
| Windows x64 | verified in CI | **verified here** | **verified here** | **yes**, launched |

Everything in the "derived" column is an open question, not a formality. On
macOS the full build took eight fixes to reach; there is no reason to expect the
untried combinations to be free.

Windows was the cheap one, against expectation: one `CMakeLists.txt` fix and no
source change at all. See the Windows section for what that does and does not
prove.

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
  not compiled *on this machine*. It is compiled and run on Windows, where vcpkg
  still has Qt5 — so editor code paths are no longer untested outright, but
  anything macOS-specific in them still is. #8 left the editor branch of the
  mouse handler alone for exactly that reason, and a Windows build says nothing
  about it.
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

**Verified here**, on Windows 11 Pro 26100 / x64, up to and including a launched
and playable `openclonk.exe`. The headline result: **the engine needed no source
change at all**. The Win32 layer that had not been compiled since VS 2017 —
`C4WindowWin32.cpp`, `C4CrashHandlerWin32.cpp`, `StdSchedulerWin32.cpp`,
`C4AppWin32Impl.h`, `C4windowswrapper.h` — built clean on MSVC 19.44 the first
time it was asked to. The hunt the macOS side needed did not materialise.

One `CMakeLists.txt` defect had to be fixed, in the packing step rather than the
engine (divergence.md 18), on top of the two the `C4GROUP_TOOL_ONLY` CI job had
already paid for (divergence.md 14 and 15).

Note that `C4GROUP_TOOL_ONLY` still *defines* `libc4script` and `libopenclonk`,
so its default `all` target tries to compile sources needing PNG and JPEG that
the configuration does not look for. Build the `c4group` target explicitly, or
use `HEADLESS_ONLY` instead.

### Toolchain

Nothing was installed on this machine beforehand — no Visual Studio, no Windows
SDK, no CMake, no vcpkg.

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools `
  --override "--quiet --wait --norestart `
    --add Microsoft.VisualStudio.Workload.VCTools `
    --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    --add Microsoft.VisualStudio.Component.Windows11SDK.22621 `
    --add Microsoft.VisualStudio.Component.VC.CMake.Project"

git clone --depth 1 https://github.com/microsoft/vcpkg C:\Development\vcpkg
C:\Development\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

Verified versions: MSVC 14.44.35207 (VS 2022 17.14), Windows SDK 10.0.22621.0.
**`cmake` is not on `PATH`** — the Build Tools ship it at
`…\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`.
The Visual Studio generator locates MSBuild by itself, so no `vcvars64.bat` is
needed.

All twelve ports build from source. Budget for it: **38 minutes** on four cores.

```powershell
C:\Development\vcpkg\vcpkg.exe install --triplet x64-windows `
  zlib libpng libjpeg-turbo curl freetype libepoxy `
  openal-soft freealut libogg libvorbis miniupnpc sdl2
```

`--only-downloads` fetches every source archive without compiling anything and
needs no compiler, so it is worth starting in parallel with the Build Tools
install.

### Headless — verified here

`HEADLESS_ONLY` needs only zlib, libpng, libjpeg-turbo and curl. Freetype and
Epoxy are excluded by the option, and SDL2 is optional — it is `REQUIRED` only
under `USE_SDL_MAINLOOP`, which loses to `USE_WIN32_WINDOWS` on Windows.

```powershell
cmake -B build -A x64 -DHEADLESS_ONLY=ON `
  -DCMAKE_TOOLCHAIN_FILE=C:/Development/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DGTEST_ROOT=<gtest>/googletest -DGMOCK_ROOT=<gtest>/googlemock .
cmake --build build --config RelWithDebInfo
```

Result: `c4group.exe`, `c4script.exe`, `openclonk-server.exe`, **zero errors**.
Only C4267/C4244/C4068 warnings, all from the C sources — `/wd4244` and
`/wd4267` are set on `CMAKE_CXX_FLAGS` only, so `gzio.c` is not covered.

Do **not** add `--parallel`: `/MP` is already on `CMAKE_CXX_FLAGS`, so MSBuild
project parallelism multiplies with it.

### Unit tests — verified here

**74 of 74 pass**, two more than macOS. `tests` runs 17 rather than 15; the
extra pair is Windows-only registry coverage in `UnicodeHandlingTest`.

```powershell
cmake --build build --config RelWithDebInfo --target tests aul_test StdMeshMath
build\tests\RelWithDebInfo\tests.exe        # 17
build\tests\RelWithDebInfo\aul_test.exe     # 52
build\tests\RelWithDebInfo\StdMeshMath.exe  #  5
```

The test binaries land in `build\tests\<Config>\`, not in the build root:
`oc_set_target_names()` only covers the four shipped executables. That is the
*safe* layout, and the shipped binaries are the anomaly — see the DLL collision
below.

**`ctest` needs `-C` here, and the manifest guard does not catch it.** The
command the CI uses on Linux and macOS fails outright on a multi-config
generator:

```
ctest --test-dir build/tests --output-on-failure
    Start 1: StdMeshMath
Test not available without configuration.  (Missing "-C <config>"?)
0% tests passed, 3 tests failed out of 3        # exit 8
```

`ctest --test-dir build/tests -N` still lists all three and exits 0, so
[ADR-016](decisions.md#adr-016--register-tests-by-hand-and-keep-checking-the-ctest-manifest)'s
registration check passes while nothing runs. Loud rather than silent — exit 8 —
but the guard is no help. `ctest --test-dir build/tests -C RelWithDebInfo`
passes 3/3.

### Full GUI build — verified here

The same configure with `-DHEADLESS_ONLY=OFF` in a second build directory.
Builds clean, again with no source change. vcpkg's applocal deployment drops all
sixteen DLLs next to the executable, so nothing needs staging by hand.

### The Qt editor — verified here

**This is the only platform where the editor can be built at all.** Qt5 is gone
from Homebrew, which is what made `src/editor/` dead code everywhere; vcpkg has
`qt5-base` 5.15.19, comfortably above the `5.4` the project asks for.

All 17 editor sources compile, and the editor **runs** — window titled
`OpenClonk Editor`, with `Qt5Widgets.dll`, `Qt5Core.dll`, `Qt5Gui.dll`,
`qwindows.dll` and `qwindowsvistastyle.dll` loaded into the process, so it is
genuinely the Qt console and not merely a successful link. AUTOMOC, AUTOUIC over
the six `.ui` files and `qt5_add_resources` all work untouched.

```powershell
C:\Development\vcpkg\vcpkg.exe install qt5-base:x64-windows     # 2.7 h
cmake -B build-qt -A x64 -DHEADLESS_ONLY=OFF `
  -DCMAKE_TOOLCHAIN_FILE=C:/Development/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows .
cmake --build build-qt --config RelWithDebInfo
.\build-qt\openclonk.exe --editor <scenario>
```

Three things to know before repeating it:

- **Leave `DEPLOY_QT_LIBRARIES` off.** `cmake/DeployQt.cmake:32` aborts the
  configure with `windeployqt not found`; vcpkg's qt5-base ships only
  `windeployqt.prf`, not the program. Not a defect — the option predates vcpkg,
  whose own applocal step deploys the Qt DLLs *and* the plugins, writing an
  empty `[Paths]` `qt.conf` so `plugins/platforms/qwindows.dll` is found beside
  the executable.
- **Enabling the editor removes `C4ConsoleWin32.cpp` from the build.** It lives
  in the `else()` of `WITH_QT_EDITOR` (`CMakeLists.txt:982`), so the two
  configurations cover *different* code and neither is a superset. Both are
  worth building; the non-Qt console is verified in `build-gui`, the Qt one
  here.
- **Installing Qt breaks audio** until `FindAudio.cmake` is fixed — see the
  traps below. That one is a real defect and cost the first build.

Not covered even now: the editor on macOS or Linux, where Qt5 availability is
the original obstacle and unchanged, and CI, which has no display.

Known-good log lines to check against:

    GL 3.2.0 - Build 31.0.101.2140 on Intel(R) Iris(R) Plus Graphics (Intel)
    OpenAL extensions loaded. Available: AL_EFFECT_REVERB, ... Unavailable: (none).
    C4AulScriptEngine linked - 90857 lines, 0 warnings, 0 errors

Runtime paths — note that **only the log lives on disk**:

| What | Where |
| --- | --- |
| Log | `%APPDATA%\OpenClonk\OpenClonk.log` |
| Config | `HKCU\Software\OpenClonk Project\OpenClonk` — the registry, not a file |

### What Windows still does not prove

- **`C4StdInProc` does not exist here.** `STDSCHEDULER_USE_EVENTS` is set, so
  `C4AbstractApp` never adds it; the comment in `C4AppT.cpp` says outright that
  it is broken on Windows. The closing-stdin trap below therefore does not
  apply — and neither does the way out, so `openclonk-server` cannot be stopped
  by closing its stdin and has to be killed.
- **The scenario run proves less than the macOS one.** `Movement.ocs` reaches
  `Game started` with 0 script errors, but not one `Test<N>` assertion executes:
  `Test.ocp` does not exist, only the script player joins, and `LaunchTest(1)`
  runs for non-script players only. CI has the same hole on every platform.
- **CI still covers `C4GROUP_TOOL_ONLY` only.** Everything above is one machine,
  once, by hand.

### Windows traps already paid for

- **The `Sound=0` trap is here too, in the registry.** One headless run set
  `Sound`, `Music`, `MenuMusic` and `MenuSound` to `0` under
  `HKCU\Software\OpenClonk Project\OpenClonk\Sound`, permanently and silently,
  exactly as the macOS config file does. Looking for a config *file* to check
  will not find one. Reset with `Set-ItemProperty`, and set `Graphics\Windowed`
  to 1 while you are there if you would rather the engine not take the display.
- **`cmake --build --target <t>` does not re-run CMake.** `ZERO_CHECK` is not a
  dependency of an explicitly named target, so an edited `CMakeLists.txt` is
  ignored and the stale rule runs again — with no hint that it did. Costs one
  confusing round per edit. Reconfigure explicitly.
- **Git's `tar` shadows the system one** and reads `C:\...` as a remote host
  (`tar (child): Cannot connect to C: resolve failed`). Use
  `C:\Windows\System32\tar.exe` for anything with a drive letter in it.
- **Binaries land in the build root**, not `build/<Config>/`.
  `oc_set_target_names()` sets `RUNTIME_OUTPUT_DIRECTORY` per configuration to
  `CMAKE_CURRENT_BINARY_DIR`, and non-`RelWithDebInfo` configurations get a
  suffixed name (`c4group-debug.exe`). Search for the file rather than assuming
  a layout. This is also what triggers divergence.md 18.
- **Building a second configuration silently replaces the first one's DLLs.**
  Consequence of the point above: every configuration shares one output
  directory, the per-configuration *names* only disambiguate the executables,
  and vcpkg deploys each configuration's dependencies into that same directory.
  Both steps are visible in the generated project file:

      [Debug]          vcpkg z-applocal --target-binary=…/build-gui/openclonk-debug.exe
                                        --installed-bin-dir=…/installed/x64-windows/debug/bin
      [RelWithDebInfo] vcpkg z-applocal --target-binary=…/build-gui/openclonk.exe
                                        --installed-bin-dir=…/installed/x64-windows/bin

  Some debug DLLs carry a `d` suffix and are harmless — `zd.dll`,
  `libpng16d.dll`, `freetyped.dll`, `bz2d.dll`, `fmtd.dll`, `libcurl-d.dll`,
  `SDL2d.dll`. About a dozen do not: `jpeg62.dll`, `OpenAL32.dll`, `alut.dll`,
  `epoxy-0.dll`, `ogg.dll`, `vorbis*.dll`, `miniupnpc.dll`, `brotli*.dll`.
  Whichever configuration was built last owns those for *all* of them.

  Verified by swapping the debug `jpeg62.dll` (1461248 bytes) over the release
  one (694272) and running `openclonk-server`: it started and reached
  `Game started` with no complaint. It only works because Visual Studio put
  `ucrtbased.dll` and `vcruntime140d.dll` in System32 — **anything copied off
  such a build tree fails to start on a machine without the debug CRT.** That
  makes it a packaging concern, not just a developer annoyance. Rebuilding the
  configuration you want puts its own DLLs back.
- **In PowerShell, reset `$LASTEXITCODE`** after deliberately failing a command,
  or the enclosing script inherits the non-zero status.

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
the way Travis did. Confirmed on Windows, where symlinks need a privilege the
build does not have — `New-Item -ItemType HardLink` does the same job.

**Music is the exception to that, twice over, and fails silently.** Symptom:
sound effects play, music never does, and the log says nothing at all.

- It is **not packed** anywhere except macOS. `Music.ocg` is appended to
  `OC_C4GROUPS` only under `if(APPLE)`; everywhere else `e324289f8` (2017,
  "Do not pack music on installation") ships it unpacked via
  `install(DIRECTORY …)`. An uninstalled build tree therefore has no music at
  all — that is also why `groups` produces twelve files and not thirteen.
- It is **not looked up through `C4Reloc`**. `C4MusicSystem::Init()` calls
  `Config.AtSystemDataPath(C4CFN_Music)`, which is plain string concatenation
  onto `SystemDataPath`. So music has to sit *directly next to the executable*,
  not in the `planet` folder every other group is staged into.
- Nothing reports the absence. `Init()` returns `true` whether or not it found
  a single song, so the `IDS_PRC_NOMUSIC` branch in `C4Application` never fires;
  only a failure to initialise the audio *toolkit* logs anything. What does
  exist is the positive signal — `C4MusicFile::Play()` logs
  `IDS_PRC_PLAYMUSIC` (`Music: <file>` / `Musik: <file>`) per song. **Absence of
  that line is the check**, since `Music not available.` will never appear.

For a build tree, junction the source directory next to the binary — and note
that the music system initialises once at startup, so this needs a restart, not
a return to the main menu:

```powershell
cmd /c mklink /J <build>\Music.ocg <repo>\planet\Music.ocg
```

**Closing stdin quits the engine.** `C4StdInProc::Execute()` treats a failed
read as a shutdown request. That is the intended mechanism, but it means any
context where stdin is `/dev/null` — a CI step, some launchers — makes the
engine exit before it initialises, silently and with status 0. Not observed on
macOS, where it keeps running. Does not apply on Windows at all:
`STDSCHEDULER_USE_EVENTS` is defined there and `C4AbstractApp` never registers
the proc.

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
