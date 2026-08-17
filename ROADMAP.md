# Roadmap

What to do next, in order, and why that order. Written 2026-08-17 at `16ca9057d`.

**Fork-local. Never include this file in a PR branch** — same rule as `CLAUDE.md`
and everything under `fork-notes/`.

This file exists so that work can be picked up cold. It is a plan, not a record:
the reasoning behind past changes lives in
[`fork-notes/divergence.md`](fork-notes/divergence.md) and
[`fork-notes/decisions.md`](fork-notes/decisions.md), what is verified on which
machine in [`fork-notes/platforms.md`](fork-notes/platforms.md), and the CI
shape in [`fork-notes/ci.md`](fork-notes/ci.md).

## Where things stand

All three platforms build, run and are green in CI. No cell in the platform
matrix is "derived" any more.

| | |
| --- | --- |
| CI jobs | 5, all green: Linux headless, Linux client, macOS headless, macOS app bundle, Windows c4group |
| Unit tests | 96 over four binaries (`tests`, `aul_test`, `StdMeshMath`, `determinism`) |
| Scenario assertions in CI | 906, across the five scenarios that have any |
| Known failing | 15 assertions, pinned per scenario so they cannot silently grow (#35, #51) |

What CI does **not** do: launch anything (#45), build a client on Windows or
macOS (#30), or load the 69 real game scenarios outside `Tests.ocf` (#52).

## The order

### 1. #41 — `C4Group::AddEntryOnDisk` reads a buffer after its scope ends

First because it is the only finding that is certainly a defect and needs no
argument. `temp_filename` is declared inside the `if (DirectoryExists(...))`
block, `filename` is pointed at it, the block ends, and four later statements
read it. Undefined behaviour in the path every `groups` build takes for every
directory it packs.

The fix is moving one declaration up. Do it together with:

- **#33** — the first unit test to cover `C4Group` at all. This fork's first two
  defects lived in that class and nothing tests it.
- **#47** — `MakeTempFilename` hands out a name without claiming it. A collision
  test is three lines: call it twice without creating anything and assert the
  results differ. Fixing the function itself is a bigger decision, recorded as
  ADR-019; the test can land first and will fail until someone takes it on.

### 2. #35 — `Movement.ocs` test 3 expects a rock position no platform produces

Cheap, and it closes a wound rather than adding coverage. Windows, macOS and
Linux all land the rock at `[372, 157]` against an expected `x > 380`,
bit-identical, so the expectation is wrong rather than the engine. Establish
from the history whether `380` was ever right, correct the number, then lower
the pin in `.github/workflows/build.yml` from 1 to 0.

### 3. #52 — put `start_all_scenarios.rs` back in service

Do this **before** #49, not after. It is the only thing that has ever loaded the
69 scenarios in Missions, Worlds, Arena, Parkour, Defense and Tutorials — so if
a language-standard change moves script behaviour anywhere in real game content,
this is the only thing that would see it.

It is not a revival project: it builds on Rust 1.93 with its 2018 dependencies
unchanged, and it runs. What it needs is a decision about gating, and the answer
is the same one the scenario suite already uses — **pinned expected counts per
scenario**, failing on any deviation in either direction. A naive "fail on any
error" would be wrong, because `Tests.ocf/ScriptError1.ocs` contains a
deliberate syntax error and exists to produce two.

Known non-clean today: `ScriptError1` (2 errors, intentional), `SkeletonAppend`
(2 warnings, one of them a `FindObject` call still using the Clonk 4 signature),
`CableLorrys` (2), `Benchmarks` (1). And `CableCars.ocs` and `LiquidSystem.ocs`
cannot run against a packed tree at all, because `Experimental.ocf`, `Tests.ocf`
and `Issues.ocf` are not in `OC_C4GROUPS`.

### 4. #44 — raise `cmake_minimum_required` above 3.5.1

One line. CMake 4 already warns that compatibility below 3.10 is going away, and
the project sits one patch version above the floor that was removed in 4.0. Take
it while you are in `CMakeLists.txt` for #41.

---

### 5. #49 — `CMAKE_CXX_STANDARD` to 17

The pivot. Everything above exists to make this safe, and after steps 1-4 the
ground is as good as it gets without open-ended debugging: the determinism
primitives have bit-exact tests, the scenario suite runs identically on clang
and GCC, the two known failures are pinned, and game content is watched.

C++14 is what caps googletest at 1.14.0 — a window one release wide, since
1.10.0 already stopped compiling on GCC 15 — and it is what blocks Qt6. Expect
the breakage to be loud: C++17's removals are compile errors, not behaviour
changes. Budget one build per platform to find out.

### 6. #50 — port the editor to Qt6, closing #46

Only after #49; Qt6 requires C++17. macOS has no editor because Qt5 is gone from
Homebrew, not because anything here pins an old version, so meeting the platform
where it is now is the fix.

Measured scope in `src/editor/`: one `QRegExp`, four `QLayout::setMargin()`
calls, one `qt5_add_resources`. No `QDesktopWidget`, `QLinkedList`, `QTextCodec`
or `QStringRef` anywhere. That is a grep and not a build, so expect a compiler to
find more.

Worth supporting Qt5 and Qt6 together (`find_package(QT NAMES Qt6 Qt5)`):
Windows and Linux have working Qt5 editor builds today and there is no reason to
break them.

Note what this does *not* fix. Getting the editor to compile on macOS is the
easy half; the Cocoa window and event plumbing has never been exercised, and
that is exactly the half #8 could not test when it left the editor branch of the
mouse handler alone.

---

### 7. CI breadth: #45, then #30

**#45** — launch the engine under `xvfb`. Cheap now that the `linux-client` job
exists: it needs one apt package and a launch step, not a new build. The engine
runs on llvmpipe at GL 4.6 Core, verified, so software rendering is not the
obstacle it was assumed to be for a long time. A menu smoke test would also be
the thing that eventually catches #37.

**#30** — Windows beyond `c4group`. The largest single coverage gap and the most
expensive: all twelve vcpkg ports build from source, about 38 minutes, though it
caches. Everything it would cover is verified by hand on the Windows machine
already, so this buys regression protection rather than knowledge.

### 8. Investigations with no known bottom

None of these blocks anything else. Take them one at a time when someone has the
appetite.

| | |
| --- | --- |
| #51 | `ObjectInteractionMenu.ocs` fails 14 of 328, all in the item-transfer path. Engine regression or stale test is undecided; `Stackable.ocs` covers neighbouring behaviour and passes completely, which points at the menu path |
| #42 | `C4Config` violates the ODR under `WITH_AUTOMATIC_UPDATE`. Real UB; which translation unit disagrees is not established |
| #43 | Four memory-safety warnings from GCC 16, untriaged. The `C4ObjectAction` one is uninitialised reads in game logic, i.e. a desync source |
| #37 | Intermittent segfault drawing the first-run player dialog, roughly one in six, not reproduced |
| #38 | UPnP has never been exercised on any platform. Needs a router with IGD, not a build machine |

### Not a work item

**#48** — the vendored dependency inventory. A list, not a task. Pull
`backward-cpp` from it if the moment arises: a directory swap, and the current
copy demonstrably works. `src/zlib/gzio.c` is the one with real risk — a file
zlib deleted in 2010 — but it deserves its own issue and its own decision when
someone is ready.

## Before starting

Read [`fork-notes/platforms.md`](fork-notes/platforms.md) for the machine you
are on, and its closing note on how these files rot. The short version: **a
claim repeated in several places is more suspect than one stated once.** Four of
them were wrong on 2026-08-17, each written from reading rather than running and
then copied forward until it looked settled.

Verify by running the thing. Nothing here is validated by anyone else and
upstream's own CI is gone.

## Caveat on this file

The issue numbers were taken from the session that produced them, not from a
fresh query — `gh` stopped answering at the end of it (`HTTP 401` on the GraphQL
API, then timeouts). Everything referenced here was created or read during that
session, but if the list has moved since, this file has not.
