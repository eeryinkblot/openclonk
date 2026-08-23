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
| Unit tests | 106 over four binaries, plus 2 disabled that state a known defect (#47) |
| Scenario assertions | 912, on all three platforms, agreeing assertion for assertion including which 14 fail |
| Scenarios script-linted | 99 of 99; 92 clean, 7 pinned |
| Undefined behaviour | **none reported**: all 12 UBSan sites fixed (#56), ASan clean everywhere |
| The client | built against Qt5 **and** Qt6, and launched under Xvfb |
| Downloads | every run uploads its build outputs; a `v*` tag publishes them as a release (#23) |

What CI still does **not** do: build a client on Windows, launch the macOS
bundle, or start the editor anywhere. The 69 real game scenarios are loaded and
linted, but nothing asserts what they *do* — they carry no `doTest()`.

## The order

### 1. #59 — nine globals declared twice

A `good first issue` on purpose: small, self-contained, and it teaches the pin
convention, since the fix has to lower `tests/scenario-lint-expected.txt` in the
same commit. Leave it for someone else if anyone shows up; do it in ten minutes
if nobody does.

### 2. #57 — the engine persists failures it never revisits

`Sound=0` written permanently after one run without a device is the instance
that has cost time here, and it is documented in `CLAUDE.md` as folk knowledge
rather than tracked as a defect. The valuable part is (1) in that issue: do not
persist what nobody chose. A read-only config mode is what CI wanted and is
worth less.

### 3. #46 — build the editor on macOS

The only item that needs a specific machine. The Qt6 support exists now and
Homebrew has Qt6, so `-DHEADLESS_ONLY=OFF` should configure with
`Using Qt6 for the editor` and compile all 17 sources.

**Expect the interesting part to start after it compiles.** The Cocoa window and
event plumbing has never been exercised, which is exactly the half #8 could not
test when it left the editor branch of the mouse handler alone. A successful
link proves nothing about whether the viewport draws or mouse events arrive.

### 4. Investigations with no known bottom

One at a time, when someone has the appetite. None blocks anything. #37 came
off this list on 2026-08-23: it was never intermittent, and the fix is
`3ba4f6fea` — see the lesson it left in
[`lessons.md`](fork-notes/lessons.md), which is about how it was hunted rather
than about the engine.

| | |
| --- | --- |
| #51 | `ObjectInteractionMenu.ocs` fails 14 of 328. **Narrowed**: GCC, clang and MSVC report the identical failures, so it is one bug everywhere and can be diagnosed on whichever machine is convenient |
| #43 | Four memory-safety warnings from GCC 16, untriaged. The `C4ObjectAction` one is uninitialised reads in game logic, i.e. a desync source |
| #42 | `C4Config` violates the ODR under `WITH_AUTOMATIC_UPDATE`. Real UB; which translation unit disagrees is not established |
| #61 | C4Script's shift operators are undefined for negative values and counts past the word size. Split out of #56 while fixing it: unlike the arithmetic there is no specified answer to preserve, so it is a decision (mask, saturate, or throw like division by zero already does) rather than a repair |
| #38 | UPnP has never been exercised anywhere. Needs a router with IGD, not a build machine — the one thing here no amount of CI can reach |

### 5. #55 — the six scenarios with no diagnosis

Two warnings each, cause unknown, pinned so they cannot grow. The rule the issue
records: **split one out once it has a cause, leave it on the list while it is
only a count.** #59 is what that looks like.

### 6. #24 — what a release still cannot carry

Half of it is done: a tag now publishes the tools for all three platforms, the
Linux client, the game data and the macOS bundle, and
[`ci.md`](fork-notes/ci.md#what-a-run-leaves-behind) says what each is. Two gaps
remain, and both are real work rather than packaging:

- **No Windows client is built anywhere.** `windows-headless` stops at the
  engine. Adding the GUI means openal-soft, freealut, ogg/vorbis, epoxy,
  freetype and Qt5 through vcpkg, which is build time and a cache key, not an
  open question — it has been built by hand on the Windows machine already.
- **The macOS bundle is published for inspection, not for playing.**
  `tools/osx_bundle_libs` re-anchors rpaths to absolute paths on the build
  machine and the signature is ad-hoc, so Gatekeeper refuses it anywhere else.
  Relocatability is the fixable half; a Developer ID and notarisation are a
  purchase, not a patch.

Neither blocks anything. The release exists and is honest about both.

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
