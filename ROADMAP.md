# Roadmap

What to do next, in order, and why that order. Written 2026-08-19 at `4d2000487`,
replacing the roadmap that ran from `16ca9057d` to the Qt6 port. That one had a
destination — get an editor onto macOS — and reached it. This one does not:
nothing below blocks anything else, so the order is by cost and by what unblocks
what, not by a goal.

**Fork-local. Never include this file in a PR branch** — same rule as `CLAUDE.md`
and everything under `fork-notes/`.

A plan, not a record. What each change does is in
[`divergence.md`](fork-notes/divergence.md), why it has that shape in
[`decisions.md`](fork-notes/decisions.md), what is verified on which machine in
[`platforms.md`](fork-notes/platforms.md), the CI shape in
[`ci.md`](fork-notes/ci.md), and how this work has gone wrong before in
[`lessons.md`](fork-notes/lessons.md).

## Where things stand

Every step of the previous roadmap is done. All three platforms build, run and
are green, and no cell in the platform matrix is "derived" any more.

| | |
| --- | --- |
| CI jobs | 9, all green |
| Platforms building `HEADLESS_ONLY` | all three, in CI |
| Unit tests | 101 over four binaries, plus 2 disabled that state a known defect (#47) |
| Scenario assertions | 912, on all three platforms, agreeing assertion for assertion including which 14 fail |
| Scenarios script-linted | 99 of 99; 92 clean, 7 pinned |
| Undefined behaviour | 12 sites pinned, ASan clean everywhere |
| The client | built against Qt5 **and** Qt6, and launched under Xvfb |

What CI still does **not** do: build a client on Windows, launch the macOS
bundle, or start the editor anywhere. The 69 real game scenarios are loaded and
linted, but nothing asserts what they *do* — they carry no `doTest()`.

## The order

### 1. #58 — one log line, so the #37 hunt stops being decorative

Cheapest thing on the list with something depending on it. The CI step that
hunts #37 starts the engine five times against an empty user directory, and
neither branch of the condition it depends on logs anything — so it cannot show
it took the path it exists to take. A `LogSilentF` in each branch of
`C4StartupMainDlg::OnShown` makes the fixture assertable, which lets the
with-dialog case gate on *having tested the right thing* while still only
warning about the crash.

Half an hour, and it converts a step that might be theatre into one that is not.

### 2. #56 — the twelve undefined-behaviour sites, in the issue's order

Located, pinned, and none of them can grow while they wait. Take them in the
order the issue gives, which is by how much argument each needs:

1. **`StdBuf.h:168`** — `memcmp` with null pointers on an empty compare. Two
   lines, no behaviour change to discuss.
2. **`Standard.cpp`** — `Distance()` checking `if (d2 < 0)` for an overflow it
   just risked, and `StrToI32` accumulating without a range check.
3. **The script engine and the landscape, separately and last.** Both are
   desync-relevant: `C4AulExec`'s integer semantics are *asserted* by
   `aul_test`, and `fill_edge_structure()` shifts negative coordinates for every
   map. Each wants the scenario suite run on all three platforms either side of
   the change, which the CI matrix now gives for free.

### 3. #59 — nine globals declared twice

A `good first issue` on purpose: small, self-contained, and it teaches the pin
convention, since the fix has to lower `tests/scenario-lint-expected.txt` in the
same commit. Leave it for someone else if anyone shows up; do it in ten minutes
if nobody does.

### 4. #57 — the engine persists failures it never revisits

`Sound=0` written permanently after one run without a device is the instance
that has cost time here, and it is documented in `CLAUDE.md` as folk knowledge
rather than tracked as a defect. The valuable part is (1) in that issue: do not
persist what nobody chose. A read-only config mode is what CI wanted and is
worth less.

### 5. #46 — build the editor on macOS

The only item that needs a specific machine. The Qt6 support exists now and
Homebrew has Qt6, so `-DHEADLESS_ONLY=OFF` should configure with
`Using Qt6 for the editor` and compile all 17 sources.

**Expect the interesting part to start after it compiles.** The Cocoa window and
event plumbing has never been exercised, which is exactly the half #8 could not
test when it left the editor branch of the mouse handler alone. A successful
link proves nothing about whether the viewport draws or mouse events arrive.

### 6. Investigations with no known bottom

One at a time, when someone has the appetite. None blocks anything.

| | |
| --- | --- |
| #51 | `ObjectInteractionMenu.ocs` fails 14 of 328. **Narrowed**: GCC, clang and MSVC report the identical failures, so it is one bug everywhere and can be diagnosed on whichever machine is convenient |
| #43 | Four memory-safety warnings from GCC 16, untriaged. The `C4ObjectAction` one is uninitialised reads in game logic, i.e. a desync source |
| #42 | `C4Config` violates the ODR under `WITH_AUTOMATIC_UPDATE`. Real UB; which translation unit disagrees is not established |
| #37 | Intermittent segfault in the first-run player dialog. CI attempts it five times per push and reports without gating; 0 of 5 so far. See #58 first |
| #38 | UPnP has never been exercised anywhere. Needs a router with IGD, not a build machine — the one thing here no amount of CI can reach |

### 7. #55 — the six scenarios with no diagnosis

Two warnings each, cause unknown, pinned so they cannot grow. The rule the issue
records: **split one out once it has a cause, leave it on the list while it is
only a count.** #59 is what that looks like.

### Not a work item

**#48** — the vendored dependency inventory. A list so that the next toolchain
surprise is a known cost rather than an archaeology session. Its own suggested
order is in the issue; `src/zlib/gzio.c` is the one with real risk and deserves
its own issue when someone is ready.

## Picking this up cold

1. Read [`platforms.md`](fork-notes/platforms.md) for the machine you are on.
   The build commands differ on all three, and two of them do not have `cmake`
   on `PATH`.
2. Read [`lessons.md`](fork-notes/lessons.md). It is short, and every entry in
   it is a mistake made here that looked like diligence at the time.
3. **Verify by running the thing.** Nothing here is validated by anyone else and
   upstream's own CI is gone. "It builds" is not a result — see the whole of
   lesson 4.
