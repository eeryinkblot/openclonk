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
| Linux x86_64 | **verified here** | verified here + CI | **verified here** | **yes**, launched |
| Windows x64 | verified in CI | **verified here** | **verified here** | **yes**, launched |

**No cell in that table is "derived" any more.** Every configuration has been
run on the machine it claims, and the engine launches on all three. Reaching it
took eight fixes on macOS, one `CMakeLists.txt` fix on Windows and no source
change, and four fixes on Linux — none of them in the engine either.

What it does *not* mean, since the table is easy to over-read:

- **The Qt editor is built and run on two of the three.** macOS has no Qt5 from
  Homebrew, so `src/editor/` is compiled by nothing there (#46).
- **`mape` is built on one and run on none.** It needs GTK3, so it does not
  exist on macOS or Windows; CI compiles it on Linux since 3f0b9374d.
- **CI covers less than the machines do.** `HEADLESS_ONLY` on two runners,
  `C4GROUP_TOOL_ONLY` on Windows, and a Linux client build that compiles the
  editor and `mape` but launches nothing (#45). Everything else in this table is
  one machine, by hand, once.
- **A configuration that builds is not a configuration that is exercised.** See
  the `C4GROUP_TOOL_ONLY` note below, where Linux succeeds by accident of where
  its headers live.

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
  not compiled *on this machine* — and macOS is now the only platform where that
  is true. It is built and run on Windows and Linux and compiled by CI, so editor
  code paths are no longer untested outright, but anything macOS-specific in them
  still is. #8 left the editor branch of the mouse handler alone for exactly that
  reason, and neither of the other two platforms says anything about it. #46.
- Unit tests need googletest **sources**, unpacked at
  `/Users/tk/Repositories/clonk/deps/googletest-1.14.0`. See CLAUDE.md.

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

**Verified here** on EndeavourOS / Arch, kernel 7.1.8, up to and including a
launched and playing `openclonk` with **sound and music** — headless, unit
tests, packing, a scenario run, the full GL client, and an installed tree. The
toolchain is far ahead of what CI uses: **GCC 16.2.1 and CMake 4.4.2**, against
`ubuntu-latest`'s conservative pair.

One thing named in this file is still an assumption rather than a result: UPnP
is verified as far as the right source file being compiled, not as a forwarded
port (#38).

Also **verified in CI** for `HEADLESS_ONLY` on `ubuntu-latest`.

```sh
# Arch package names. All were already installed here except the two below.
sudo pacman -S --needed base-devel cmake libpng libjpeg-turbo freetype2 zlib \
                        curl libepoxy openal libogg libvorbis sdl2-compat \
                        qt5-base glu mesa
sudo pacman -S --needed freealut miniupnpc      # see "audio and UPnP" below

cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=ON .
cmake --build build --parallel "$(getconf _NPROCESSORS_ONLN)"

cmake -B build-gui -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=OFF .
cmake --build build-gui --parallel "$(getconf _NPROCESSORS_ONLN)"
```

No `CMAKE_PREFIX_PATH` is needed — nothing is keg-only the way Homebrew's
openal-soft is, and `pkg_check_modules` finds every audio component directly.

### What the modern toolchain cost

The engine itself needed **no source change**, as on Windows. GCC 16 compiles
all of `libmisc`, `libc4script`, `libopenclonk`, `openclonk` and
`openclonk-server` with warnings only. Three things did break, none of them in
the engine:

- **`mape` does not compile.** `segv_handler()` is declared with an empty
  parameter list, which in C23 — GCC 15 and later default to it — means
  `(void)` rather than "unspecified", so it no longer converts to
  `__sighandler_t` and the `signal()` call is a hard error rather than a
  warning. Fixed by giving the handler its `int`. This is the `gzio.c` pattern
  from divergence.md 3 repeating: an old C source that only ever compiled by
  the grace of a lenient default.
- **googletest 1.10.0 does not compile.** `gtest-death-test.cc` and
  `gtest-port.cc` use `uintptr_t` and `Int32` without including `<cstdint>`,
  which GCC 15 stopped supplying transitively. Not fixable in this repository;
  the pin moved to **1.14.0**, the newest release that still builds under
  `CMAKE_CXX_STANDARD 14` (1.15+ wants C++17). The nine `MOCK_METHOD1`/`2`
  uses in `tests/` were rewritten to the variadic `MOCK_METHOD`, which exists
  in 1.10.0 as well, so no other machine has to move in lockstep.
- **`cmake --build --target groups --parallel N` fails intermittently.** See
  the trap below; fixed in `CMakeLists.txt`.

### `C4GROUP_TOOL_ONLY` — verified here, and it hides #19

```sh
cmake -B build-tool -DCMAKE_BUILD_TYPE=RelWithDebInfo -DC4GROUP_TOOL_ONLY=ON .
cmake --build build-tool --target c4group --parallel 24
```

`c4group` builds and works — it packed `System.ocg` to 124544 bytes, the same
size the full build produces.

The interesting part is the **default target**, which is what #19 is about:
`C4GROUP_TOOL_ONLY` still defines `libc4script` and `libopenclonk`, so `all`
compiles sources needing PNG and JPEG that the configuration never looked for.
On Windows that fails. **Here it succeeds** — `cmake --build build-tool` exits
0 and builds seven targets:

    blake2  c4group  c4script  libc4script  libmisc  libopenclonk
    oc-licenses-into-code

`PNG_LIBRARY` and `JPEG_INCLUDE_DIR` are absent from `CMakeCache.txt`, so CMake
genuinely never searched — and `StdPNG.cpp.o` compiled anyway, because
`/usr/include/png.h` is on the compiler's default search path on a Linux system.

So the defect is present and asymptomatic. That is worse than a failure, not
better: the option claims to build only `c4group` and quietly builds six more
targets, and the platform where that is obvious is the one nobody uses to check
build options.

### What the modern toolchain found without breaking anything

The more valuable half, and the reason this machine is worth keeping. None of
these stops a build; all of them are things older compilers never said, and none
had been looked at before. Filed rather than fixed, because each needs a
judgement this fork has no test to support.

| Finding | Where | Issue |
| --- | --- | --- |
| Pointer to a block-scoped buffer used after the block ends | `C4Group.cpp:1598-1609` | #41 |
| `C4Config`/`C4ConfigNetwork` have two layouts in one binary | `C4Config.h:165` | #42 |
| `free()` on a stack buffer, three call sites | `StdBuf.h:192` | #43 |
| One byte written before a buffer | `C4MusicSystem.cpp:212` | #43 |
| `memcpy` with a bound of `SIZE_MAX` | `StdBuf.h:156` | #43 |
| Four uninitialised locals in `C4Object::ExecAction` | `C4ObjectAction.cpp:957` | #43 |
| `cmake_minimum_required(3.5.1)`, one release from removal | `CMakeLists.txt:14` | #44 |

Two of them are worth singling out. The `C4Group` one is a genuine use of a
buffer after its scope ends, on the path every `groups` build takes — and #33
already notes that nothing tests `C4Group`. The `C4ObjectAction` one is
uninitialised reads in *game logic*, which on a lockstep-synchronised engine is
a desync source rather than a mere crash risk (#34).

**Two conditions had to hold before any of this was visible**, which is worth
remembering when reading a clean build elsewhere: LTO, which is what lets GCC
compare types across translation units at all — `CheckIPOSupported` turns it on
at `CMakeLists.txt:97-101` — and, for #42, `WITH_AUTOMATIC_UPDATE=ON`, since the
ODR mismatch is between the two sides of an `#ifdef`. The default build reports
zero `-Wodr` warnings; the snapshot configuration reports eighteen.

### Audio — verified here

`freealut` and `miniupnpc` are the only dependencies nothing else pulls in, and
both had to be installed by hand. With them, `FindAudio` reports
`Using Audio toolkit: OpenAL` and the engine gets full EFX — the same
known-good line as macOS and Windows:

    OpenAL extensions loaded. Available: AL_EFFECT_REVERB, AL_EFFECT_ECHO,
        AL_EFFECT_EQUALIZER. Unavailable: (none).

Sound effects and music both play, **confirmed by ear** on `build-play` and not
only by instrument. The supporting evidence, in case it is ever needed without
a person present: the process holds an uncorked `Stream/Output/Audio` node in
PipeWire (`pw-cli ls Node`, `node.name = "openclonk"`), and the sink monitor
measures well above the noise floor — see the reference table below. `libopenal`, `libalut`, `libvorbisfile`,
`libvorbis` and `libogg` are all in `ldd build-gui/openclonk`.

`miniupnpc` gets `C4Network2UPnPLinux.cpp` compiled in place of
`C4Network2UPnPDummy.cpp` — verified by the object file, **not** by forwarding
a port. No platform in this fork has ever exercised UPnP; see #38, and note
that Windows builds a third implementation again (`C4Network2UPnPWin32.cpp`).

What it looks like *without* them, since that is the state a fresh machine is
in: the configure prints `Package 'freealut' not found` among a hundred other
lines and then `Not enabling audio output.`, and the build succeeds. The engine
then logs `Music not available.` and `Error at sound file.` and is silent.
`FindAudio` wants OpenAL *and* ALUT *and* Ogg/Vorbis together; two out of three
is the same as none.

### Music needs an installed tree here, and `Music.ocg` beside the binary does nothing

This is where Linux differs from both other platforms, and the difference is
not in `C4MusicSystem` but in what `SystemDataPath` means:

| Platform | `SystemDataPath` | So music goes |
| --- | --- | --- |
| macOS | `Application.GetGameDataPath()` | in the bundle |
| Windows | `ExePath` | next to the executable |
| Linux, `WITH_AUTOMATIC_UPDATE=ON` | `ExePath` | next to the executable |
| **Linux, default** | **`${CMAKE_INSTALL_PREFIX}/share/games/openclonk/`** | **only into an install** |

`C4Config.cpp:520-538` picks these with `#if`. The default here is the last
row, so the advice that works on Windows — junction `Music.ocg` next to the
binary — is a no-op on Linux, and an uninstalled build tree can never have
music no matter how it is staged.

**The cheap way out is `WITH_AUTOMATIC_UPDATE=ON`**, which moves Linux to the
third row of that table — `SystemDataPath` becomes `ExePath`, and music can then
be staged next to the binary exactly as on Windows. That is what the option is
*for*: it marks the tarball and development-snapshot layout, where the game data
sits with the executable. Verified here:

```sh
cmake -B build-play -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=OFF \
    -DWITH_AUTOMATIC_UPDATE=ON .
cmake --build build-play --parallel 24
mkdir -p build-play/planet
for f in build/*.oc*; do ln -sf "$PWD/$f" "build-play/planet/$(basename "$f")"; done
ln -sfn "$PWD/planet/Music.ocg" build-play/Music.ocg
./build-play/openclonk
```

    SystemDataPath: "/home/eeryinkblot/projects/openclonk/build-play/"
    Music: UrbanBolero.ogg

Note what it costs: `C4UpdateDlg.cpp` joins the build, `install` is deliberately
blocked in that configuration (`CMakeLists.txt:1522-1530`), and the engine is
then willing to look for updates. Fine for a machine you play on, wrong for
anything you would ship.

Installing into a prefix you own is the other way, and it works too:

```sh
cmake -B build-install -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=OFF \
    -DCMAKE_INSTALL_PREFIX=<prefix> .
cmake --build build-install --parallel 24
cmake --build build-install --target groups
cmake --install build-install
<prefix>/games/openclonk          # binary in games/, data in share/games/openclonk/
```

Result: `Music: UrbanBolero.ogg`. The install rules put all twelve packed
groups plus the **unpacked** `Music.ocg` directory into
`share/games/openclonk/`, which is the layout `e324289f8` intended.

`--config=<file>` runs against a config of your choosing, and the
`UserDataPath` key inside it relocates the log and player data too — the way to
test without disturbing an existing `~/.clonk`.

### Runtime paths, and the config trap

| What | Where |
| --- | --- |
| Log | `~/.clonk/openclonk/OpenClonk.log`, and stdout as well |
| Config | `~/.clonk/openclonk/config` — a plain file, unlike Windows |

The `Sound=0` trap is here too, and was confirmed rather than assumed: one
headless run wrote `Sound`, `Music`, `MenuMusic` and `MenuSound` to `0` in
`[Sound]`, permanently. Also set `[Graphics] Windowed=1` before the first GUI
run unless you want the engine to take the display.

Known-good log lines to check against:

    GL 4.6 (Core Profile) Mesa 26.1.7-arch1.1 on AMD Radeon RX 9070 XT (radeonsi, gfx1201, ACO, DRM 3.64) (AMD)
    C4AulScriptEngine linked - 90857 lines, 0 warnings, 0 errors

The client runs under a Wayland session (KDE, `XDG_SESSION_TYPE=wayland`)
through SDL2 — `sdl2-compat` here, not the original SDL2 — and renders a full
game at GL 4.6 Core.

**It also runs with no GPU at all.** `LIBGL_ALWAYS_SOFTWARE=1` gives
`GL 4.6 (Core Profile) Mesa 26.1.7 on llvmpipe (LLVM 22.1.8, 256 bits)`, and the
main menu renders completely — animated background, buttons, version string.
Slow, but the engine asks for nothing llvmpipe cannot provide. That matters
beyond this machine: it is the half of "CI cannot run the GUI" that was worth
checking, and it is false. The other half, `xvfb-run`, is one package. See #45,
and note that `xvfb` is *not* installed here, so only the renderer side is
verified.

### How to tell whether there is actually sound

The log is not enough. `OpenAL extensions loaded.` says the toolkit came up,
`Music: <file>` says a song was handed to it — neither says a sample reached a
speaker. Record the sink's monitor and measure it:

```sh
SINK=$(pactl info | sed -n 's/^Default Sink: //p')
timeout 30 parec --device=$SINK.monitor --file-format=wav cap.wav
ffmpeg -hide_banner -i cap.wav -af volumedetect -f null - 2>&1 | grep volume
```

Reference points measured here, all on the same sink:

| What | mean | max |
| --- | --- | --- |
| Nothing running | −91.0 dB | −91.0 dB |
| Installed tree, main menu, music on | −23.8 dB | −10.9 dB |
| `build-play`, main menu, music on | −25.9 dB | −9.5 dB |
| `build-gui`, `Movement.ocs`, **music off** | −47.1 dB | −6.3 dB |

Silence is −91, not −inf, so that is the number to compare against. The third
row is the one that matters for sound *effects*: music was switched off in the
config, so nothing but the effect path could have produced it.

**Start recording before the engine, not after.** Two captures here read −91.0
and nearly produced a "sound effects are dead on Linux" conclusion; both had
started at t+16 s. A scenario with a clonk standing still makes no noise
whatsoever, and `Movement.ocs` fires everything it has in the first twelve
seconds.

### One segfault, seen once and not reproduced

Recorded because an unexplained crash is worth more written down than
remembered. The installed build died on startup with a null dereference while
drawing the startup dialog:

    Segmentation fault (Address not mapped to object [(nil)])
    #0  C4Facet::Draw            src/graphics/C4Facet.cpp:68
    #1  C4GUI::Window::Draw      src/gui/C4GuiContainers.cpp:298
    #2  C4GUI::Dialog::Draw      src/gui/C4GuiDialogs.cpp:481
    ...
    #13 C4Startup::DoStartup     src/gui/C4Startup.cpp:246

backward-cpp caught it, which is itself a first for this fork — the handler has
never fired on any platform before, and it works.

**Not reproduced.** Five further runs, all reached the menu and stayed. Roughly
one in six. Frame #0 lands on the line that is only a null *guard*, which under
`RelWithDebInfo` means the faulting instruction is in the inlined neighbourhood
rather than exactly there — most plausibly the `pDraw->Blit(...)` below it.

The middle of the trace is the useful part, and it was missed at first.
`C4StartupMainDlg::OnShown` (`C4StartupMainDlg.cpp:344`) opens a **modal
first-run player dialog** when the user path holds no `.ocp`:

```cpp
if (!fHasPlayer)
{
    // no player created yet: Create one
    GetScreen()->ShowModalDlg(pDlg=new C4StartupPlrPropertiesDlg(nullptr, nullptr), true);
}
```

Everything below that frame is *that* dialog drawing itself through the nested
scheduler loop `ShowModalDlg` runs — not the main menu. The crashing run used a
`UserDataPath` with no player in it; the `Neuling.ocp` that directory holds now
is stamped 22 minutes later.

Which also suggests why an intermittent fault there could go unnoticed
indefinitely: every developer machine has had a player file since its first
launch, so the path runs once per user path ever created.

Do not read it as Linux-specific on this evidence. `C4Facet`, the GUI containers
and `C4StartupMainDlg` are all platform-independent. Filed as #37 with a
reproduction recipe.

### The Qt editor runs here — verified here

Arch still ships `qt5-base` (5.15.19), so `WITH_QT_EDITOR` turns itself on and
all 17 sources in `src/editor/` compile, with `libQt5Widgets`, `libQt5Gui` and
`libQt5Core` linked into `openclonk`. That makes Linux the **second** platform
where the editor exists at all, after Windows — and the first non-Windows one,
which is what issue #22 was really about.

It was started, and it works:

```sh
./build-play/openclonk --editor build-play/planet/Missions.ocf/DeadlyGrotto.ocs
```

Window titled `OpenClonk Editor`, and genuinely the Qt console rather than a
successful link: properties dock with a live selection (`Object(307)`, a Gondor
clonk, `KI-Auswahl` and `Skin` editable), the
`Objektliste`/`Eigenschaften`/`Objekte erstellen` tabs, the play/pause/step and
landscape-tool toolbars, a log dock mirroring the engine log, and a viewport
running the scenario at `Frame: 888`, `36 FPS`. All three Qt5 libraries mapped
into the process. Sound and music play in editor mode too.

So what is left is macOS, where Qt5 is still gone from Homebrew (#46). CI
compiles the editor on Linux now, but does not launch it (#45).

Note the consequence recorded in the Windows section: enabling the editor drops
`C4ConsoleWin32.cpp` — and its non-Qt equivalents — from the build, so the two
configurations cover different code and neither is a superset.

### Unit tests — verified here

**72 of 72 pass**, the same set as macOS. Sources unpacked at
`/home/eeryinkblot/projects/deps/googletest-1.14.0`.

```sh
GT=/home/eeryinkblot/projects/deps/googletest-1.14.0
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=ON \
    -DGTEST_ROOT=$GT/googletest -DGMOCK_ROOT=$GT/googlemock .
cmake --build build --target tests aul_test StdMeshMath --parallel 24
./build/tests/tests        # 15
./build/tests/aul_test     # 52
./build/tests/StdMeshMath  #  5
```

### Scenario run — verified here

Identical to the other two platforms, **including the failure**: `Movement.ocs`
reports 3 tests, 1 failed, 0 skipped, and the failing one is test 3, the rock
position of issue #35. Linux puts the rock at `[372, 157]` where the assertion
wants X > 380 — so it is not a platform disagreement, it is an assertion no
platform satisfies.

### Traps paid for here

- **The `groups` target is not parallel-safe, and only Ninja was protected.**
  `cmake --build build --target groups --parallel 24` fails with `Pack failed`,
  intermittently, and by preference on `Objects.ocd` — the largest group, and
  therefore the one still running when the others start. The `USES_TERMINAL`
  on the custom command was the whole defence and it only does anything for
  Ninja, whose console pool takes one job at a time; Makefiles ignore it. The
  failure is loud in the log but easy to miss in a long build, and what it
  leaves behind is a *complete-looking* set of eleven groups out of twelve. The
  engine then dies at
  `FATAL ERROR: Required object file Objects.ocd not available.` — which reads
  like a staging mistake, not a packing one.

  The root cause is `MakeTempFilename()` (`src/platform/StdFile.cpp:320`): it
  scans for the lowest unused `<name>.NNN` and returns it **without claiming
  it**, so two c4group processes in the same directory pick the same temporary
  file. Fixed in `CMakeLists.txt` by chaining each group onto the previous one,
  which serialises packing under every generator. Why not at the source: see
  decisions.md, and #47 for the defect itself.

- **A stale `GTEST_INCLUDE_DIR` survives being given the sources later.** On any
  machine with a googletest *package* installed — Arch has `gtest` 1.17 in
  `/usr/include` — the first configure, the one CLAUDE.md tells you to run
  without `GTEST_ROOT`, finds no sources but still caches the system headers.
  `find_path` never revisits a cached value, so adding `-DGTEST_ROOT=` to that
  same build directory afterwards changes the *sources* and leaves the
  *headers* pointing at the installed copy. gtest-all.cc 1.10.0 then compiles
  against gtest 1.17 headers and dies on a page of names that no longer exist —
  `Int32`, `StrDup`, `kDeathTestStyleFlag` — with nothing in the output
  connecting it to the include path. Deleting the build directory is the only
  cure. `tests/CMakeLists.txt` now looks for the headers only once the sources
  have turned up, and only where they are.

- **`ln -sf ../<name>` for staging is fine here**, unlike Windows: the
  `build/planet` symlink farm the CI uses works unprivileged.

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
the original obstacle and unchanged, and CI, which builds no client (#45).

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
- **Pass the player file by absolute path.** `planet/Test.ocp` exists now, but a
  relative `.ocp` argument is made absolute against the *working directory* and
  never consults `C4Reloc` (`C4Application.cpp:413`), so the bare `Test.ocp` in
  the older command lines only resolves when the working directory happens to
  contain it. Without a player that loads, no `Test<N>` assertion runs at all
  and the scenario still reports `Game started` — the state the whole suite was
  in until then.
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
  onto `SystemDataPath`. So music goes wherever `SystemDataPath` points — and
  that is **not the same place on every platform**. On Windows and macOS it is
  the executable's own directory; on Linux, unless `WITH_AUTOMATIC_UPDATE` is
  on, it is `${CMAKE_INSTALL_PREFIX}/share/games/openclonk/`. Staging music next
  to the binary is the fix on the first two and a no-op on the third. Either
  way it is never the `planet` folder every other group is staged into.
- **Whether the absence is reported depends on the containing directory, not on
  the music.** `C4MusicSystem::LoadDir` (`C4MusicSystem.cpp:204-250`) tries to
  open `<dir>/Music.ocg`, then falls back to opening `<dir>` itself and
  searching it for that name. Only if *both* fail does it log

      Music File not found: <path>

  So on Windows the directory is the exe directory, which always exists: the
  fallback succeeds, the search comes up empty, and nothing is said. On Linux
  the default `/usr/local/share/games/openclonk` usually does not exist at all,
  and the engine names the exact missing path. Both were observed here — the
  uninstalled build tree reports it, and an installed tree with `Music.ocg`
  removed from an existing `share/games/openclonk/` says nothing at all
  (reproduced three times, eleven log lines, no music line of any kind).
- `Init()` still returns `true` whether or not it found a single song, so the
  `IDS_PRC_NOMUSIC` branch in `C4Application` (`C4Application.cpp:561`) reports
  only a failure to initialise the audio *toolkit*, never an empty playlist.
  The reliable signal is the positive one — `C4MusicFile::Play()` logs
  `IDS_PRC_PLAYMUSIC` (`Music: <file>` / `Musik: <file>`) per song. **Absence of
  that line is the check**, since `Music not available.` means something else.

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

**The engine exits 2 when you quit before the scenario ends.** `ClonkMain.cpp`
returns `C4XRV_Aborted` unless `Game.GameOver` is set — `C4XRV_Completed = 0`,
`C4XRV_Failure = 1`, `C4XRV_Aborted = 2` (`C4Constants.h:70`). Same logic in the
Windows and the Unix entry point. Intended, and easy to misread as a crash: any
automation that starts the engine, does something and closes it again gets a
non-zero status from a completely successful run. Existing CI never noticed
because it kills the process and discards the status.

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

### Repetition is not corroboration

The specific way these notes decay, learned the hard way in a single day. Four
claims were wrong, and every one of them was wrong in the same shape: written
once from reading rather than running, then **copied forward** until the number
of places it appeared made it look settled.

| Claim | Repeated in | Actually |
| --- | --- | --- |
| "No display on the runner", so CI cannot launch anything | 4 places, plus #22 and the workflow comments | `xvfb` is one package and the engine runs on llvmpipe at GL 4.6 (#45) |
| Music must sit next to the executable | here and #28 | True on Windows and macOS, a no-op on Linux, where `SystemDataPath` is the install prefix |
| The 496 assertions are spread over 29 unrun scenarios | #36, and repeated from it here | They live in **five** files; the four that never ran held 899 of them |
| `start_all_scenarios.rs` is the weaker harness, fit for retirement | written here without reading it | It is a content lint over all 99 scenarios, it still builds on Rust 1.93, and it finds real defects (#52) |

Each cost real work to unlearn, and two of them had made *other* gaps look
permanent — the display claim alone was load-bearing for the editor and the GUI
smoke test.

So: **a claim that appears in several places is more suspect than one that
appears once**, not less. It has had more opportunities to be copied and fewer
to be checked. When something here reads as settled and nothing says who ran it,
that is the entry to test first.

The cheap check is usually cheaper than it looks. llvmpipe was one environment
variable. Reading `start_all_scenarios.rs` was ten minutes. Counting the
`doTest()` calls was one `grep -c`.
