# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## This is a fork

`eeryinkblot/openclonk`, carrying macOS / Apple Silicon fixes on top of upstream
`openclonk/openclonk`. Remotes: `origin` is the fork, `upstream` is the project.

- `fork-notes/divergence.md` — what every deviation does and why. **Read it before
  touching the macOS platform layer**; several changes exist because the upstream code
  fails silently rather than loudly.
- `fork-notes/pr-plan.md` — how the commits group into eventual pull requests.

`CLAUDE.md` and `fork-notes/` are fork-local and must never end up in a PR branch.
Build PR branches by cherry-picking onto `upstream/master`, never by branching off
this `master`.

Upstream has been near-dormant since 2021 and its CI (Travis, AppVeyor) is dead, so
nothing here is validated by anyone else. Verify changes by running the thing.

## What this is

OpenClonk is a 2D multiplayer action game engine (C++14, CMake). The repository holds two
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

### Building on this machine (macOS / Apple Silicon)

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

- **Qt5** is gone from Homebrew, so `WITH_QT_EDITOR` is off and the editor is not built.
  Editor code paths in `src/editor/` are therefore untested by anything done in this fork.
- **gtest/gmock sources** are not installed, so the `tests` and `aul_test` targets do not
  exist. Pass `-DGTEST_ROOT=` / `-DGMOCK_ROOT=` pointing at *sources* to get them.

### Running

```sh
open build-gui/openclonk.app                                    # play
./build/openclonk-server --language=US planet/Tests.ocf/Movement.ocs Test.ocp
```

The engine log goes to `~/Library/Application Support/OpenClonk/OpenClonk.log`, **not**
to stdout when launched as a bundle. Config lives in
`~/Library/Preferences/org.openclonk.openclonk.config`.

That config file is a trap worth knowing: the engine writes `Sound=0` / `Music=0`
permanently whenever it starts without working audio — a headless run is enough — and
never revisits it. Symptom is total silence with no error anywhere and correct volume
values. Check `[Sound]` there before debugging audio libraries.

Two harmless log lines: `Error loading player "…/Test.ocp"` (the file does not exist,
the engine substitutes a stand-in) and, in headless builds, `Error at sound file.`

## Tests

Two independent test layers.

**C++ unit tests (GoogleTest/GMock).** Both targets are `EXCLUDE_FROM_ALL`, so name them explicitly.
CMake must find gtest/gmock *sources* (`gtest-all.cc`, `gmock-all.cc`), not just headers — pass
`-DGTEST_ROOT=` / `-DGMOCK_ROOT=` if they are not in `/usr/src/gtest`. **Not currently available
on this machine**, so neither target exists in `build/` or `build-gui/`; nothing in this fork has
been checked against them.

```sh
cmake --build build --target tests aul_test StdMeshMath
./build/tests/tests                       # C4NetIO, C4Value, StdFile, string table, unicode, DirectExec
./build/tests/aul_test                    # C4Script (Aul) language tests
./build/tests/tests --gtest_filter='C4NetIOTest.*'   # single test / suite
SKIP_IPV6_TEST=1 ./build/tests/tests      # when the host has no ::1
ctest --test-dir build                    # runs the create_test() targets
```

`tests/aul/*` execute C4Script snippets through the standalone script engine
(`AulTest::RunCode/RunScript/RunExpr`) and assert on the resulting `C4Value` — this is the
fastest way to test language or built-in-function behaviour without a running game.

**Script/scenario tests.** `planet/Tests.ocf/*.ocs` are real scenarios that self-check at runtime.
The convention: `InitializePlayer` installs an `IntTestControl` effect that iterates
`Test<N>_OnStart(plr)` → `Test<N>_Completed()` → `Test<N>_OnFinished()`, with a local `doTest()`
helper that logs pass/fail per assertion. Copy the pattern from an existing scenario when adding one.
Run a scenario by passing it to the headless engine:

```sh
./openclonk-server --language=US planet/Tests.ocf/Movement.ocs Test.ocp
```

`tests/start_all_scenarios.rs` (cargo-script) walks every `.ocs` under a planet directory, starts it
headless, and greps the log for `C4AulScriptEngine linked - N lines, N warnings, N errors`.

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
