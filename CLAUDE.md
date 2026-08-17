# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## This is a fork

`eeryinkblot/openclonk`, carrying macOS / Apple Silicon fixes on top of upstream
`openclonk/openclonk`. Remotes: `origin` is the fork, `upstream` is the project.

**Start with [`ROADMAP.md`](ROADMAP.md)** if you are picking work up rather than
answering a specific question. It carries the ordered plan and what each step is
waiting on.

Reference material under `fork-notes/`, worth consulting when it is relevant rather
than up front:

| File | What it answers |
| --- | --- |
| `divergence.md` | What every deviation from upstream does, and why upstream's version was wrong |
| `decisions.md` | Why each change has the shape it does, and which alternatives were rejected |
| `ci.md` | What the GitHub Actions workflow checks, and the environment constraints behind it |
| `platforms.md` | What is known to work on which machine, and what is only assumed |
| `pr-plan.md` | How the commits group into eventual pull requests |

`decisions.md` is the one to reach for before reworking something that already looks
odd — several of the alternatives rejected there are the obvious first idea.

`CLAUDE.md`, `ROADMAP.md` and `fork-notes/` are fork-local and must never end up in
a PR branch.
Build PR branches by cherry-picking onto `upstream/master`, never by branching off
this `master`.

**This fork is the primary line of development.** Upstream is dormant — roughly one
merged PR a year, outside contributions sitting for years — so submission is
opportunistic and never a reason to hold anything back here. `fork-notes/pr-plan.md`
keeps the commits separable so a PR stays cheap if one is ever worth opening.

Nothing here is validated by anyone else, and upstream's own CI is gone. Verify
changes by running the thing.

## What this is

OpenClonk is a 2D multiplayer action game engine (C++17, CMake). The repository holds two
very different kinds of code:

- `src/` — the C++ engine (executables `openclonk`, `openclonk-server`, `c4group`, `c4script`, `mape`, `netpuncher`)
- `planet/` — the game content, written in **C4Script** (the engine's own scripting language, `.c` files),
  plus assets and text-based config files. Most gameplay lives here, not in C++.

## Build

CMake, in-source or out-of-source. There is no configure script.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo .
cmake --build build              # engine + c4group + c4script
cmake --build build --target groups   # pack planet/ into .ocg/.ocd/.ocf group files
```

Relevant options (all in the top-level `CMakeLists.txt`):

| Option | Effect |
| --- | --- |
| `HEADLESS_ONLY=ON` | skip graphics/audio deps; builds `openclonk-server`, not `openclonk` |
| `C4GROUP_TOOL_ONLY=ON` | build only the `c4group` packer |
| `WITH_AUTOMATIC_UPDATE`, `WITH_APPDIR_INSTALLATION` | packaging-related; installation is blocked unless auto-update is off (or AppDir is on) |
| `DEPLOY_QT_LIBRARIES=ON` | run windeployqt/macdeployqt after linking |

The Qt5 editor (`WITH_QT_EDITOR`) is auto-enabled when Qt5Widgets is found; without it the engine
builds without editor dialogs. macOS builds produce `openclonk.app` and pack game data into the
bundle via `tools/osx_pack_gamedata.sh`.

Cross-compiling requires a prior native build: it exports `c4group` and `oc-licenses-into-code`
to `NativeToolsExport.cmake`, which you point at with `-DIMPORT_NATIVE_TOOLS=`.

`tools/default.nix` builds the whole thing under Nix (`nix-build tools -A ...`, `withEditor` toggles Qt).

### Building on macOS / Apple Silicon

The fork is developed on three machines — this section, the Linux one and the
Windows one below each describe one of them, so check which you are on before
following any of them.

Two build directories are kept side by side; both are gitignored via `/build*`.

```sh
# headless: engine-less server, c4group, c4script. Few dependencies.
/opt/homebrew/bin/cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=ON .
/opt/homebrew/bin/cmake --build build -j8
/opt/homebrew/bin/cmake --build build --target groups     # pack planet/ (13 groups, ~115 MB)

# playable: openclonk.app with graphics and sound
/opt/homebrew/bin/cmake -B build-gui -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DHEADLESS_ONLY=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/openal-soft .
/opt/homebrew/bin/cmake --build build-gui -j8
```

**`cmake` is not on `PATH`** — use `/opt/homebrew/bin/cmake`.

**`CMAKE_PREFIX_PATH` matters.** openal-soft is keg-only; without that flag `FindAudio`
finds no real OpenAL and falls back to Apple's deprecated framework (no EFX). Check the
configure output for `Using Audio toolkit: OpenAL`, and the runtime log for
`OpenAL extensions loaded. Available: AL_EFFECT_REVERB, ...` — if it says
`ALExt: No efx extensions available`, the wrong OpenAL got linked.

Dependencies (`brew install`): `cmake libepoxy openal-soft miniupnpc freealut`, plus the
usual `libpng jpeg-turbo freetype libogg libvorbis sdl2 curl`. **All four of OpenAL,
ALUT, and Ogg/Vorbis must be found** or `FindAudio` silently selects no audio at all.

Not available here:

- **Qt5** is gone from Homebrew, so `WITH_QT_EDITOR` is off and the editor is not built
  here. It *is* built and run on the Windows machine, where vcpkg still carries Qt5 — so
  `src/editor/` is no longer untested outright, but anything macOS-specific in it is.
- **gtest/gmock sources** are not installed, so the `tests` and `aul_test` targets do not
  exist. Pass `-DGTEST_ROOT=` / `-DGMOCK_ROOT=` pointing at *sources* to get them.

### Building on Linux (Arch / EndeavourOS)

Verified end to end: headless, unit tests, packing, a scenario run, and a launched and
playing `openclonk`. The engine needed no source change. `fork-notes/platforms.md` has the
full account, including what the modern toolchain broke outside the engine.

`cmake` **is** on `PATH` here, unlike the other two machines, and it is CMake 4.4 with
GCC 16 — well ahead of what CI uses, which is the point of this machine.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=ON .
cmake --build build --parallel 24

cmake -B build-gui -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=OFF .
cmake --build build-gui --parallel 24
```

Dependencies (`pacman -S`): `base-devel cmake libpng libjpeg-turbo freetype2 zlib curl
libepoxy openal libogg libvorbis sdl2-compat qt5-base glu mesa`, plus **`freealut` and
`miniupnpc`**, which are the two nothing else pulls in. Without freealut there is no audio
at all — `FindAudio` needs OpenAL *and* ALUT *and* Ogg/Vorbis, and says only
`Not enabling audio output.` in the middle of a long configure.

`qt5-base` is still packaged here, so `WITH_QT_EDITOR` turns itself on by default and
`src/editor/` is compiled — the only platform besides Windows where that happens. Remember
that enabling it removes the non-Qt console sources from the build, so the two
configurations are not nested.

Three things to know:

- **Do not pack groups in parallel on a Makefile generator.** Fixed in `CMakeLists.txt`
  now, but if you see `Pack failed` and eleven groups where twelve belong, that is what it
  was. The engine reports it later as `Required object file Objects.ocd not available.`
- **The config file is `~/.clonk/openclonk/config`** and the `Sound=0` trap applies to it:
  a single headless run silently disables sound and music for good. Set `[Graphics]
  Windowed=1` there too before the first GUI run.
- **Stage the packed groups next to the binary**, as CI does — plain symlinks work here:

```sh
mkdir -p build/planet
for f in build/*.oc*; do ln -sf "../$(basename "$f")" "build/planet/$(basename "$f")"; done
```

**Music cannot be staged that way on Linux, and putting `Music.ocg` next to the
binary does nothing.** `C4MusicSystem` looks it up under `SystemDataPath`, which is
`${CMAKE_INSTALL_PREFIX}/share/games/openclonk/` here — not the executable's directory,
the way it is on macOS and Windows. An uninstalled build tree therefore has no music,
and says so: `Music File not found: /usr/local/share/games/openclonk/Music.ocg`.

To hear music while still playing out of the build tree, configure with
`WITH_AUTOMATIC_UPDATE=ON` — that alone makes `SystemDataPath` the executable's
directory, the way it already is on Windows:

```sh
cmake -B build-play -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=OFF \
    -DWITH_AUTOMATIC_UPDATE=ON .
cmake --build build-play --parallel 24
mkdir -p build-play/planet
for f in build/*.oc*; do ln -sf "$PWD/$f" "build-play/planet/$(basename "$f")"; done
ln -sfn "$PWD/planet/Music.ocg" build-play/Music.ocg
./build-play/openclonk                       # logs "Music: UrbanBolero.ogg"
```

It also pulls `C4UpdateDlg.cpp` into the build and blocks `install`, so it is a
configuration to play in, not to ship. The alternative is a real install:

```sh
cmake -B build-install -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=OFF \
    -DCMAKE_INSTALL_PREFIX=/tmp/ocinstall .
cmake --build build-install --parallel 24
cmake --build build-install --target groups
cmake --install build-install
/tmp/ocinstall/games/openclonk        # binary in games/, data in share/games/openclonk/
```

`--config=<file>` gives a run its own config, and the `UserDataPath` key inside that
file moves the log and player data with it — useful for a throwaway run that must not
disturb an existing `~/.clonk`.

**To check that audio is actually audible, measure it — the log cannot tell you.**
`OpenAL extensions loaded.` only means the toolkit started:

```sh
SINK=$(pactl info | sed -n 's/^Default Sink: //p')
timeout 30 parec --device=$SINK.monitor --file-format=wav cap.wav
ffmpeg -hide_banner -i cap.wav -af volumedetect -f null - 2>&1 | grep volume
```

Silence reads −91 dB, not −inf. The main menu with music is around −24 dB mean.
**Start the recording before the engine**: a scenario left alone makes no sound at
all, and measuring from t+16 s produces a convincing −91 dB on a perfectly working
build. See `fork-notes/platforms.md` for the reference table.

### Building on Windows (x64 / MSVC)

Verified end to end: headless, unit tests, packing, and a launched `openclonk.exe`.
The engine needed no source change. `fork-notes/platforms.md` has the full account —
toolchain install, timings, and what the result does *not* prove.

Dependencies come from vcpkg at `C:\Development\vcpkg`; googletest sources are unpacked
at `C:\Development\deps\googletest-1.14.0`.

```powershell
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

& $cmake -B build -A x64 -DHEADLESS_ONLY=ON `
  -DCMAKE_TOOLCHAIN_FILE=C:/Development/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows .
& $cmake --build build --config RelWithDebInfo          # no --parallel: /MP is already set

& $cmake -B build-gui -A x64 -DHEADLESS_ONLY=OFF `
  -DCMAKE_TOOLCHAIN_FILE=C:/Development/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows .
& $cmake --build build-gui --config RelWithDebInfo
```

**`cmake` is not on `PATH`** here either — it ships inside the Build Tools. No
`vcvars64.bat` is needed; the Visual Studio generator finds MSBuild itself.

Three things that cost time and will again:

- **The config lives in the registry**, under `HKCU\Software\OpenClonk Project\OpenClonk`,
  not in a file. The `Sound=0` trap described below applies unchanged — look in
  `…\OpenClonk\Sound`, and set `…\OpenClonk\Graphics\Windowed` to 1 unless you want the
  engine to take over the display.
- **`cmake --build --target <t>` does not re-run CMake.** `ZERO_CHECK` is not a
  dependency of a named target, so an edited `CMakeLists.txt` is ignored without a word.
- **Git's `tar` shadows the system one** and treats `C:\...` as a remote host. Use
  `C:\Windows\System32\tar.exe` when a drive letter is involved.

### Running

```sh
open build-gui/openclonk.app                                    # play
./build/openclonk-server --language=US planet/Tests.ocf/Movement.ocs "$PWD/planet/Test.ocp"
```

The engine log goes to `~/Library/Application Support/OpenClonk/OpenClonk.log`, **not**
to stdout when launched as a bundle. Config lives in
`~/Library/Preferences/org.openclonk.openclonk.config`.

That config file is a trap worth knowing: the engine writes `Sound=0` / `Music=0`
permanently whenever it starts without working audio — a headless run is enough — and
never revisits it. Symptom is total silence with no error anywhere and correct volume
values. Check `[Sound]` there before debugging audio libraries.

`Error at sound file.` in headless builds is harmless.

`Error loading player "…/Test.ocp"` is **not** harmless, despite looking it, and if
you see it the run tested nothing. No stand-in is substituted — `C4ClientPlayerInfos`
deletes the info and moves on (`src/control/C4PlayerInfo.cpp:412`). No human player
joins, so `InitializePlayer` only runs for the script player, `LaunchTest(1)` is never
reached, and the scenario reports `Game started` having executed zero assertions.

**Pass `planet/Test.ocp` by absolute path.** A relative `.ocp` argument is made
absolute against the *working directory* and never consults `C4Reloc`
(`C4Application.cpp:413`), so a bare `Test.ocp` resolves only if the working
directory happens to hold it. A good run says `Player join: Test` and ends with a
summary:

```
* 3 tests total
0 tests failed
0 tests skipped
```

`Movement.ocs` is green since #35 was settled. CI runs five scenarios and gates on a
pinned failure count per scenario; only `ObjectInteractionMenu.ocs` is non-zero (14,
issue #51).

**A scenario run is not reproducible across runs.** `RandomSeed = time(nullptr)`
for a local game (`src/game/C4Game.cpp:341`), and results do depend on it: the
rock in `Movement.ocs` test 3 lands at either `[372, 157]` or `[374, 158]`, ten
of each over twenty runs. Same seed, same result — the engine is deterministic,
the seed is not. Assertions in these scenarios therefore need ranges, and a
position that reproduces twice is not evidence that it always will.

On Windows the log is `%APPDATA%\OpenClonk\OpenClonk.log`. The engine finds its data
next to the executable, so pack and stage first — symlinks need a privilege the build
does not have, hardlinks do the same job:

```powershell
& $cmake --build build --config RelWithDebInfo --target groups
New-Item -ItemType Directory -Force build\planet | Out-Null
Get-ChildItem build\* -Include *.ocf,*.ocg,*.ocd,*.ocm | ForEach-Object {
  New-Item -ItemType HardLink -Path "build\planet\$($_.Name)" -Target $_.FullName -Force }

# Music is neither packed nor found via C4Reloc — it has to sit next to the
# executable, unpacked. Without this you get sound effects and no music, and
# no log line anywhere says so. See fork-notes/platforms.md.
cmd /c mklink /J "$PWD\build-gui\Music.ocg" "$PWD\planet\Music.ocg"

.\build-gui\openclonk.exe
.\build\openclonk-server.exe --language=US planet\Tests.ocf\Movement.ocs "$PWD\planet\Test.ocp"
```

`openclonk-server` has to be killed to stop it: `C4StdInProc` is compiled out on
Windows, so closing stdin does nothing.

## Tests

Two independent test layers.

**C++ unit tests (GoogleTest/GMock).** All three targets are `EXCLUDE_FROM_ALL`, so name them
explicitly. CMake needs gtest/gmock **sources** (`gtest-all.cc`, `gmock-all.cc`), not headers or
libraries — it compiles them into the project. A Homebrew `googletest` install does not work;
point at an unpacked release instead.

Unpacked outside the repo — `/Users/tk/Repositories/clonk/deps/googletest-1.14.0` on the
macOS machine, `C:\Development\deps\googletest-1.14.0` on the Windows one,
`/home/eeryinkblot/projects/deps/googletest-1.14.0` on the Linux one:

```sh
curl -sSL -o gtest.tar.gz \
  https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
tar xzf gtest.tar.gz            # yields googletest-1.14.0/{googletest,googlemock}
```

**Version matters, and the floor is real.** Below 1.14.0 the library itself will not
compile on a current toolchain: 1.10.0, which this used to pin, omits `#include <cstdint>` in
`gtest-death-test.cc` and `gtest-port.cc`, and GCC 15 and later stopped supplying it
transitively. There is no upper bound any more: it used to be `CMAKE_CXX_STANDARD 14`,
which ruled out 1.15+, and the project is on **C++17** since `fb3ec7b9c`. Moving past
1.14.0 is now a choice and nothing needs it, so all three machines stay on the copy they
have unpacked. Note the tag naming changes at 1.11 — `release-1.10.0` but `v1.14.0`.

The tests used the arity-based `MOCK_METHOD1`/`MOCK_METHOD2` macros, which later googletest
releases dropped; they now use the variadic `MOCK_METHOD`, which 1.10.0 understands too, so an
older checkout still works where the compiler tolerates it.

**If a build directory was ever configured without `GTEST_ROOT`, delete it.** On a machine with
a googletest *package* installed the include path gets cached from the system before the sources
are known, and adding `-DGTEST_ROOT=` later does not dislodge it. `tests/CMakeLists.txt` guards
against this now; a build directory configured before that fix still carries the bad value.

```sh
GT=/Users/tk/Repositories/clonk/deps/googletest-1.14.0
/opt/homebrew/bin/cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHEADLESS_ONLY=ON \
    -DGTEST_ROOT=$GT/googletest -DGMOCK_ROOT=$GT/googlemock .
/opt/homebrew/bin/cmake --build build --target tests aul_test StdMeshMath -j8

./build/tests/tests          # 15 tests: C4NetIO, C4Value, StdFile, string table, unicode, DirectExec
./build/tests/aul_test       # 52 tests: C4Script (Aul) language
./build/tests/StdMeshMath    # 5 tests: vector/quaternion math
./build/tests/tests --gtest_filter='C4NetIOTest.*'   # single test / suite
SKIP_IPV6_TEST=1 ./build/tests/tests                 # when the host has no ::1 (not needed here)
```

All 72 pass as of the last check. On Windows it is **74**: `tests` runs 17 there, because
`UnicodeHandlingTest` has two registry cases that only exist on that platform. The binaries
also land in `build\tests\<Config>\` rather than `build/tests/` — `oc_set_target_names()`
only redirects the four shipped executables.

Two quirks worth knowing: `enable_testing()` lives in `tests/CMakeLists.txt`, so the ctest
manifest is in `build/tests`, not `build` — use `ctest --test-dir build/tests`. And that only
covers `StdMeshMath` and `aul_test`, because `add_test()` is called from the `create_test()`
helper and the `tests` target is declared without it. **Run `./build/tests/tests` by hand**;
a green ctest run does not mean it passed.

`tests/aul/*` execute C4Script snippets through the standalone script engine
(`AulTest::RunCode/RunScript/RunExpr`) and assert on the resulting `C4Value` — this is the
fastest way to test language or built-in-function behaviour without a running game.

**Script/scenario tests.** `planet/Tests.ocf/*.ocs` are real scenarios that self-check at runtime.
The convention: `InitializePlayer` installs an `IntTestControl` effect that iterates
`Test<N>_OnStart(plr)` → `Test<N>_Completed()` → `Test<N>_OnFinished()`, with a local `doTest()`
helper that logs pass/fail per assertion. Copy the pattern from an existing scenario when adding one.
Run a scenario by passing it to the headless engine:

```sh
./openclonk-server --language=US planet/Tests.ocf/Movement.ocs "$PWD/planet/Test.ocp"
```

`tests/start_all_scenarios.rs` (cargo-script) walks every `.ocs` under a planet directory, starts it
headless, and greps the log for `C4AulScriptEngine linked - N lines, N warnings, N errors`. Note what
that does *not* do: it passes a bare `Test.ocp` and only checks that the script engine linked, so it
reports success for scenarios whose assertions never ran. CI runs `Movement.ocs` alone with a real
player file — the other 29 scenarios, holding most of the 496 `doTest()` calls in the suite, are still
started by nothing.

## Engine architecture

Source is grouped by subsystem under `src/`; static libraries define the layering:

- **`libmisc`** — platform abstraction, `C4Group`/`CStdFile` (the archive format), `StdBuf`,
  `StdCompiler`, `C4NetIO`, logging. No game knowledge; also the only library `c4group` links.
- **`libc4script`** — the C4Script engine plus the value/proplist model. Links `libmisc` + blake2.
  Usable standalone (`c4script` binary, `include/c4script/c4script.h`, `C4ScriptStandalone*.cpp`).
- **`libopenclonk`** — game-object/landscape pieces shared between engine binaries.
- Executables add `OC_SYSTEM_SOURCES` / `OC_GUI_SOURCES` / `OC_CLONK_SOURCES` on top;
  `openclonk-server` is the same code with `USE_CONSOLE` and a stub app layer (`C4AppT.cpp`).

`src/C4Include.h` is included **first by every translation unit** and is the precompiled header. Other
headers assume it was already included and deliberately do not include it or its contents themselves.

Key flows worth knowing before editing:

- **Game loop**: `C4Application` (`src/game/`) owns the app, timers, sound/music, and the interactive
  thread; `C4Application::GameTick()` drives `C4Game::Execute()`, which advances one frame of
  landscape, objects, effects, and script execution.
- **Determinism / networking**: gameplay is lockstep-synchronised — clients exchange *inputs*, not
  state. `C4GameControl` (`src/control/`) queues control packets with a delivery type
  (`CDT_Queue`/`Sync`/`Direct`/`Private`) and periodically runs sync checks (`C4SyncCheckRate`;
  every frame in debug builds). Consequences for any engine change: **no floating point in game
  logic** — use `C4Real` fixed-point (`src/lib/C4Real.h`) — and use `C4Random` (`src/lib/C4Random.h`),
  never `rand()`. Anything that diverges between clients or platforms causes a desync.
  `C4Record`/replays and `DEBUGREC_*` flags in `C4Include.h` exist to hunt those down.
- **Script engine** (`src/script/`): `C4AulParse` → AST (`C4AulAST.h`) → `C4AulCompiler`
  (preparse, constant resolution, codegen) → bytecode run by `C4AulExec`. Scripts live in
  `C4ScriptHost` subclasses — `C4GameScriptHost` (scenario `Script.c`), `C4DefScriptHost` (a
  definition's `Script.c`), `C4ExtraScriptHost` (`System.ocg` scripts). Everything script-visible is
  a `C4PropList`/`C4Value`; definitions (`C4Def`) and objects (`C4Object`) are proplists.
- **C++ functions exposed to C4Script** are registered with `::AddFunc(p, "Name", FnName)` in
  `src/script/C4Script.cpp` (engine-independent), `src/game/C4GameScript.cpp` (game), and
  `src/object/C4ObjectScript.cpp` (object). `scriptdefinitionsources.txt` lists these files for the
  docs tooling; keep it in sync if functions move.
- **Data loading**: everything the game reads goes through `C4Group`, which transparently handles
  both a directory and a packed group file, so content can be edited unpacked and shipped packed.

## Game content (`planet/`)

Directory suffixes are the group types and matter to the engine:
`.ocg` (generic group), `.ocd` (object definition), `.ocf` (scenario folder), `.ocs` (scenario),
`.ocm` (material). They are plain directories in git and packed into single files at build time by
`c4group`; the packed list is `OC_C4GROUPS` in `CMakeLists.txt` (release scripts keep a second copy
of that list — see the comment there).

- `planet/System.ocg/*.c` — global C4Script library loaded for every game (`Assert.c`, `Object.c`,
  `Math.c`, `FindObject.c`, …). Changes here affect all content.
- `planet/Objects.ocd/**` — definitions. A definition is a directory with `DefCore.txt`
  (id, category, physics), `Script.c`, graphics (`Graphics.png`/`.mesh`), and `StringTblUS.txt` /
  `StringTblDE.txt` for localisation. Nested `.ocd` directories are sub-definitions.
- `planet/*.ocf` — scenario folders (Missions, Worlds, Arena, Parkour, Defense, Tutorials, Tests).
  A scenario is `Scenario.txt` + `Script.c` + map/landscape files.
- Localisation is US/DE pairs (`StringTbl??.txt`, `Language??.txt`); `tools/create_missing_language_entries.py`
  and `tools/remove_unused_strings.py` help maintain them.

Pack/unpack content by hand with the built tool:

```sh
./build/c4group planet/Objects.ocd -p     # pack
./build/c4group Objects.ocd -u            # unpack
```

## Docs (`docs/`)

The C4Script reference is XML under `docs/sdk/`, built with a Makefile (xsltproc + node):
`make all` (online HTML, EN+DE), `make check` (XML syntax check), `make chm HHC=...` (offline).
German pages are generated from `de.po`. When adding or changing an engine script function, add the
matching `docs/sdk/script/fn/*.xml` entry.

## Conventions

- Tabs for indentation in C++ and CMake; Allman braces; engine classes are prefixed `C4`,
  platform/utility classes `Std`.
- Every source file carries the ISC licence header with the Clonk trademark note — copy it into new files.
- `Version.txt` holds the version (`C4XVER1`/`C4XVER2`/`C4VERSIONEXTRA`); `src/C4Version.h` is generated.
