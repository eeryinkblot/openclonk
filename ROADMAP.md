# Roadmap

What to do next, in order, and why that order. Written 2026-08-17 at `16ca9057d`,
updated the same day at `592b7b754`: steps 1 to 4 are done, and #49 is next.

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
| CI jobs | 8, all green. The Linux client is now **launched**, not just built |
| Unit tests | 101 over four binaries (`tests`, `aul_test`, `StdMeshMath`, `determinism`), plus 2 disabled that state a known defect (#47) |
| Scenario assertions in CI | 912, on **all three** platforms, agreeing assertion for assertion |
| Known failing | 14 assertions, all in `ObjectInteractionMenu.ocs`, pinned so they cannot silently grow (#51) |
| Scenarios script-linted | 99 of 99, 92 of them clean; the other 7 pinned and undiagnosed |

What CI does **not** do: build a *client* on Windows, or launch the macOS
bundle. The Linux client is built, launched under Xvfb and asserted on. The 69 real game scenarios are loaded
now, but only linted — nothing asserts what they *do*, since they carry no
`doTest()`.

**Steps 1 to 5 and 7 are done.** What is left is #50, the Qt6 editor port, and
the investigations in step 8 — plus the two lists this work produced: #56 (the
twelve undefined-behaviour sites the sanitizer job pins) and #55 (the seven
scenarios that do not link cleanly).

## The order

### ~~1. #41 — `C4Group::AddEntryOnDisk` reads a buffer after its scope ends~~ — done

`9ce177d99` moves the declaration up. `c0c6d8639` adds the first five tests the
group format has ever had (#33), and `0776ab86f` two disabled ones stating what
`MakeTempFilename` does not promise (#47) — disabled so CI stays a signal, and
so the count of known-broken things stays where gtest prints it every run.

The tests paid for themselves before they were finished: driving C4Group as a
library rather than as part of a program that configures it surfaced a third
defect, fixed in `5a34e6eef`. `AppendEntry2StdFile` read "no sort list was set"
as a write failure, so any embedder that never called `C4Group_SetSortList()`
could not write a group holding a renamed child group — and the failure arrived
with an empty error string. Filed afterwards as **#53**.

Still open in this area: #47 itself, and the fact that the lifetime bug in #41
was found by reading and could not have been found by these tests. That is now
**#54**, with the measurement attached: ASan is clean over all 101 tests, and
UBSan reports ten sites — four of them the script engine's `+`, `-`, `++` and
`--` overflowing signed integers, which `aul_test` *asserts* the wraparound of.
A specified language semantic resting on undefined behaviour, in an engine whose
correctness model is bit-identical results across compilers.

### ~~2. #35 — `Movement.ocs` test 3 expects a rock position no platform produces~~ — done

`013d76873`. The history settles it: the test was written 2019-03-13 and the
movement rewrite that changed the result landed 2019-06-22, saying in its own
commit message that it "may break scenarios that rely on this specific
behaviour". Confirmed by reverting only `C4Movement.cpp` on today's tree, which
makes test 3 pass and test 2 fail. The pin is down to 0.

**The premise of the old entry was wrong, and the correction matters more than
the fix.** `[372, 157]` is not bit-identical across platforms — it is one of
two values *this* machine produces. `RandomSeed = time(nullptr)` for a local
game, so a scenario run is not repeatable: twenty runs gave `[372, 157]` and
`[374, 158]` ten times each. The new assertion is a range for that reason, and
any future scenario assertion needs to be one too.

### ~~3. #52 — put `start_all_scenarios.rs` back in service~~ — done

Three commits: `bc55e8f42` gives it a `tests/Cargo.toml`, since the cargo-script
it was written for will not install on a current toolchain — which is much of
why nothing had started it in eight years. `ed28194a1` adds `--expect` and
`tests/scenario-lint-expected.txt`, a pinned warning and error count per
scenario, failing on deviation in either direction. `592b7b754` adds the
`scenario-lint` job: 99 scenarios in 1m23s, six minutes of wall clock in
parallel with the rest.

It runs against the **unpacked** tree, which is what took the two missing
scenarios from 97 to 99. `Experimental.ocd`, `Experimental.ocf`, `Tests.ocf` and
`Issues.ocf` are deliberately not in `OC_C4GROUPS`, and `CableCars.ocs` wants a
definition nested inside another scenario in `Experimental.ocf` — so the packed
tree cannot host it, and packing turns out to change nothing about script
linking anyway.

Seven scenarios are not clean, one more than this entry listed:
`ColorfulLights.ocs` reports **10** warnings, nine of them globals declared
twice. It was missed by the earlier survey because `grep -v "0 warnings, 0
errors"` also swallows `10 warnings, 0 errors`. None of the seven is diagnosed;
they are pinned, which makes each one a tripwire rather than a fix, and they are
tracked as **#55**. The job was green on its first run, 5m25s.

### ~~4. #44 — raise `cmake_minimum_required` above 3.5.1~~ — done

`443690c41`, at 3.10 rather than higher (ADR-022). It came with three deletions
rather than being one line: the pre-3.8 `try_compile` wrapper, the
`if(POLICY CMP0069)` guard around the IPO check, and a pre-3.6 warning in
`cmake/DeployQt.cmake`. The floor sets both policies to NEW by itself.

---

### ~~5. #49 — `CMAKE_CXX_STANDARD` to 17~~ — done

`fb3ec7b9c`, and it cost nothing. The entry below budgeted "one build per
platform to find out"; the finding is that on Linux/GCC 16 with Qt5 there is
nothing to find. Headless, all four test binaries (101 passing) and the full
client including the Qt5 editor and `mape` all build, and `Movement.ocs` runs
3 of 3 with the same positions and the same script-linking counts the C++14
build produces.

Nothing in `src/` uses what C++17 removed — the grep for `auto_ptr`,
`bind1st`, `random_shuffle`, `unary_function` and `std::iterator` comes back
empty, and every `register` is a word in a comment.

**Settled, including the part that was still open when this was written.** All
jobs went green on the first run, so clang builds the engine, the app bundle and
a client under C++17. The MSVC gap — only `libmisc` compiled, through
`c4group` — closed the next day with `windows-headless` (#30): MSVC now builds
the whole headless engine under C++17, passes all four unit-test binaries and
runs a scenario. What remains unbuilt under MSVC is the editor and the GUI
client, which no CI job builds on any platform except Linux.

Consequences: googletest has no upper bound any more (nothing needs one), and
#50 becomes possible. Why not C++20 is ADR-023 — `throw()` is removed there,
and there are four.

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

### ~~7. CI breadth~~ — done

All three are in, and each cost less than this page predicted.

~~**#45** — launch the engine under `xvfb`~~ — `808f9cddd`. One apt package, one
second to install, and the runner renders on llvmpipe at `GL 4.5 (Core
Profile)`. The gate asserts a GL context, graphics loading and the process
surviving; a second step starts the engine five times against an empty user
directory to hunt #37 and reports without gating, because a one-in-six crash
makes a poor gate. First run: 0 of 5.

~~**#54** — a sanitizer job~~ — `240293816`. ASan gates outright and reports
nothing across the test binaries, packing and a scenario; UBSan has twelve
pinned sites, now tracked as #56. Two of them are reported by the scenario and
by nothing else.

~~**#30** — Windows beyond `c4group`~~ — `5198b01c6`. Billed here as "the
largest single coverage gap and the most expensive: about 38 minutes". The gap
was real; the cost was not — the job takes **7 minutes**, vcpkg two of them on a
cache *miss*. What had blocked it was #29, fixed earlier, plus an estimate
nobody rechecked.

### 8. Investigations with no known bottom

None of these blocks anything else. Take them one at a time when someone has the
appetite.

| | |
| --- | --- |
| #51 | `ObjectInteractionMenu.ocs` fails 14 of 328, all in the item-transfer path. **Settled since**: GCC, clang and MSVC report the identical 14, so it is one bug everywhere and can be diagnosed on whichever machine is convenient |
| #42 | `C4Config` violates the ODR under `WITH_AUTOMATIC_UPDATE`. Real UB; which translation unit disagrees is not established |
| #43 | Four memory-safety warnings from GCC 16, untriaged. The `C4ObjectAction` one is uninitialised reads in game logic, i.e. a desync source |
| #37 | Intermittent segfault drawing the first-run player dialog, roughly one in six, not reproduced. CI now attempts it five times per push and reports without gating; 0 of 5 so far |
| #38 | UPnP has never been exercised on any platform. Needs a router with IGD, not a build machine |

### 9. Lists this work produced

Not investigations — each is a set of known, located defects with a pinned
guard already watching it, so none of them can grow while it waits.

| | |
| --- | --- |
| #56 | Twelve undefined-behaviour sites the sanitizer job pins. Five are the script engine's integer semantics, two the landscape's polygon rasterisation. Suggested order is in the issue: `StdBuf.h` first, script engine and landscape last and separately, since both are desync-relevant |
| #55 | Seven scenarios that do not link cleanly. `ColorfulLights.ocs` has the clearest cause — nine globals declared in both `Script.c` and the generated `Objects.c` |
| #47 | `MakeTempFilename` hands out a name it does not claim. Two disabled tests state the contract a fix has to meet |
| #57 | The engine writes its whole configuration back over any `--config` file, and persists failure states it never revisits — the `Sound=0` trap is the instance that has cost time |
| #58 | The first-run player dialog logs nothing, so the test hunting #37 cannot show it tested the right path. One log line fixes it |

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

## The tracker, as of 2026-08-17

Checked against a working `gh` after the login was restored — the caveat this
section used to carry is gone, and every number on this page matches the
tracker. Nothing had drifted.

Closed since this file was written: **#41**, **#33**, **#35** (auto-closed by
the push), **#44**, and **#34**, which had been done in `a81cd4e97` and never
closed. **#53** is new — the C4Group sort-list defect found while writing the
tests for #33.

**#47** stays open with the two disabled tests attached to it. **#51** stays
open and keeps its title: 5 of 9 tests fail there, which is the same state the
CI pin describes as 14 of 328 assertions. Both counts are right; they count
different things.
