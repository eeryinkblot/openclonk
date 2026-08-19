# Continuous integration

`.github/workflows/build.yml`. Upstream's CI had been dead for years —
`.travis.yml` targeted travis-ci.org, which no longer exists, and `appveyor.yml`
pinned Visual Studio 2017 — so nothing had verified a build since roughly 2020.
Both are removed in this fork; this workflow replaces them.

Triggers: push to `master`, any pull request, and manual dispatch. Runs on the
same ref cancel each other (`concurrency` with `cancel-in-progress`), and every
job has a 45-minute cap so a stuck build fails instead of running to the
six-hour default.

## How the jobs are split

Worth stating, because "same product, different shape" is a fair thing to ask of
this workflow:

| Kind of check | Runs |
| --- | --- |
| **Platform** — build, unit tests, packing, the scenario suite | once per platform |
| **Dependency version** — Qt5 vs Qt6 for the editor | once per supported version |
| **Content** — the C4Script lint over `planet/` | once, anywhere |
| **Configuration** — `C4GROUP_TOOL_ONLY` | where it can actually fail |
| **Instrumentation** — ASan and UBSan | once, on the platform with the best support |

The content lint is one job because the script engine is platform-independent,
and the three platforms agreeing on it is exactly what the scenario suite
already establishes; a second runner would reprint the same 99 lines.
`C4GROUP_TOOL_ONLY` is Windows-only because that is where its defect is visible:
the default target fails there (#19), while on Linux the same configuration
succeeds by accident, since PNG and JPEG headers sit in `/usr/include` whether
the configuration looked for them or not. A Linux job for it would be green and
prove nothing.

**ubuntu and macOS share a matrix; Windows does not.** That is a fact about the
steps, not about the platforms. Of the 14 steps in `headless`, **12 are
byte-identical** across the two and only the dependency install differs. Between
`headless` and `windows-headless`, eight steps share a name and **not one shares
its body** — different shell, `/MP` instead of `--parallel`, `ctest -C`,
hardlinks instead of symlinks, a different test-binary directory, and no stdin
EOF to stop the engine with. Folding Windows into the matrix would put an `if:`
on 12 of 14 steps and make both harder to read than two honest jobs.

What must not differ is what the platforms *assert*. The scenario pins live in
`tests/scenario-suite-expected.txt` and every job that runs the suite reads
them. They used to be a `case` statement in one job and a literal in the other,
which is how Windows ended up checking 6 assertions where Linux checked 912.

## Jobs

### `headless` — `ubuntu-latest` and `macos-latest`

Builds with `HEADLESS_ONLY=ON`, then:

| Step | Guards against |
| --- | --- |
| Configure, asserting gtest was found | CMake silently omitting the test targets when the sources are missing |
| Build | — |
| Build `tests aul_test StdMeshMath determinism` | — |
| Assert no GL loader in the server | `HEADLESS_ONLY` quietly acquiring a graphics dependency |
| Run `ctest`, asserting all four binaries are registered | A target added without `add_test()` dropping out of the suite unnoticed |
| Pack game data | — |
| Reject empty group files | `c4group` exiting 0 after a failed pack |
| Stage packed groups next to the binary | — |
| Run all five scenarios that hold assertions | The engine quitting with exit 0 without running anything, and 899 written assertions never executing |

The scenario step runs **all five** scenarios that contain assertions — the
other 25 under `Tests.ocf` hold no `doTest()` at all, so that is the whole
suite, 912 executed assertions against the 7 this used to cover — 290 Stackable,
168 Producers, 120 LiquidContainer, 328 ObjectInteractionMenu, 6 Movement,
counted from a run rather than summed from memory; the 906 recorded here before
left Movement's six out. It polls for
the harness's completion line rather than sleeping a fixed 90 seconds: the
scenarios finish in 1 to 42 seconds of engine time, so the whole suite now costs
less than the single scenario did.

**Do not parse the summary line.** Four different wordings exist across the
five, and the `awk '/tests total/'` this workflow used matches only
`Movement.ocs` — the other four would have scored as "the harness ran but
reported 0 tests" and tripped the guard. `[Pass]` and `[Fail]` come from the
shared `doTest()` helper and are uniform, so the step counts those.

Every scenario has a pinned expected failure count — 0 for four of them and 14
for `ObjectInteractionMenu` (#51) — and any deviation fails the job. `Movement`
was the fifth, pinned at 1, until `013d76873` corrected the expectation its
test 3 had carried since 2019 (#35) and brought the pin down with it.

Pinning rather than warning is the whole point. A scenario allowed to fail
freely does not merely catch nothing, it **masks**: take
`ObjectInteractionMenu` from 14 failures to 40 and a warning-only step still
goes green, with only the number in the message moving. That is the shape of
blind spot a C++17 bump (#49) or a dependency change (#48) would hide in. The
pin converts two unfixed bugs into tripwires without anyone having to fix them
first.

**A count that drops fails too, deliberately.** It means someone fixed the
underlying bug, and the pin has to come down with it or it starts protecting
nothing — which is precisely how a guard like this rots. The error message says
what to do. Both directions were tested by hand before this went in:

    ::error::Movement: 1 of 6 assertions failed, expected 0
    ::error::Stackable: 0 failures, but 5 are pinned. If this is a fix, lower
             the pin in this workflow and close the issue it refers to.

### `tests/start_all_scenarios.rs` is not redundant with this

Written in 2018 by Julius Michaelis, one commit, untouched since. It is easy to
mistake for a worse version of the scenario suite above, and it is not — it does
a different job:

| | CI scenario suite | `start_all_scenarios.rs` |
| --- | --- | --- |
| Scenarios | the 5 under `Tests.ocf` with assertions | **all 99** `.ocs` under `planet/` |
| Checks | `[Pass]`/`[Fail]` from `doTest()` | `C4AulScriptEngine linked - N lines, N warnings, N errors` |
| Is | an assertion runner | a **content lint** |

So it covers the 69 real game scenarios — Missions, Worlds, Arena, Parkour,
Defense, Tutorials — that nothing else loads at all. A script error introduced in
a mission is invisible to everything in this workflow.

What it did *not* do, until `ed28194a1`, is gate. It `println!`ed the counts it
captured and never inspected them; the only `panic!` was for failing to spawn
the engine, so it exited 0 whatever it found, and a human had to read 99 lines
of output and notice. It still passes a bare `Test.ocp`, so the assertions
inside `Tests.ocf` do not run under it — which is where the "reports success for
scenarios whose assertions never ran" note comes from, and is a fair criticism
of it as an *assertion* runner, not as a lint. The suite above is what runs
those.

Note the irony recorded here rather than quietly fixed: the
`variable_out_of_scope` warning in `LiquidContainer.ocs` (95a1d8094) is exactly
what this tool was built to surface. It would have found it in 2018 if anyone
had run it.

**It gates now**, as the `scenario-lint` job. `bc55e8f42` gave it a
`tests/Cargo.toml`, because the cargo-script it was written for is unmaintained
and will not install — that alone is much of why nothing started it for eight
years. `ed28194a1` added `--expect`, and `592b7b754` the job.

Run over the whole tree it covers **99 of 99** scenarios in 1m23s, of which
**92 are clean**. Seven are not, and none of the seven has been diagnosed:

| Scenario | | |
| --- | --- | --- |
| `Tests.ocf/ScriptError1.ocs` | 2 errors | on purpose; it exists to produce them |
| `Tests.ocf/ColorfulLights.ocs` | 10 warnings | nine globals declared in both `Script.c:10-11` and the generated `Objects.c:3`, plus a deliberate assignment inside an `if` |
| `Tests.ocf/SkeletonAppend.ocs` | 2 warnings | one a `FindObject` call still using the Clonk 4 signature |
| `Tests.ocf/CableCars.ocs` | 2 warnings | |
| `Tests.ocf/LiquidSystem.ocs` | 2 warnings | |
| `Experimental.ocf/CableLorrys.ocs` | 2 warnings | |
| `Tests.ocf/Benchmarks.ocs` | 1 warning | |

**Two numbers recorded here before were wrong**, and both were mine. "97 of 99"
was a consequence of running against a packed tree: `Experimental.ocd`,
`Experimental.ocf`, `Tests.ocf` and `Issues.ocf` are deliberately not in
`OC_C4GROUPS`, so `CableCars.ocs` and `LiquidSystem.ocs` could not start —
`CableCars` needs a definition nested inside *another scenario* in
`Experimental.ocf`, so symlinking one group is not enough. The lint therefore
runs against the unpacked tree, which costs nothing: the 97 that run either way
report identical counts, so packing does not affect script linking.

"93 are clean" was a filter bug: `grep -v "0 warnings, 0 errors"` also drops
`10 warnings, 0 errors`, which is why `ColorfulLights.ocs` went unmentioned. Any
count ending in a zero would have vanished the same way. Worth remembering the
next time a substring stands in for a match.

`ScriptError1.ocs` is why "fail on any error" would have been the wrong rule,
and the pins are per scenario for the same reason the suite above pins failure
counts — deviation in **either** direction fails, so a fix has to lower its pin
in the same commit.

**All three runners agree assertion for assertion, failures included.** Since
`0875c9348` the Windows job runs the same five scenarios from the same pin file,
and GCC, clang and MSVC report the identical 290 / 168 / 120 / 314+14 / 6 —
**912 assertions**, down to *which* 14 fail in `ObjectInteractionMenu`. That is
C4Real determinism across three compilers and three operating systems, checked
on every push rather than by hand, against the 7 assertions this workflow
started with. The `determinism` unit tests say the same about the primitives:
sequences pinned from pcg32 on GCC pass unchanged on clang and MSVC.

It also says something about #51: 14 identical failures on three compilers is
not a platform defect, it is the same bug everywhere.

### `scenario-lint` — `ubuntu-latest`

The job that runs the content lint described above over all 99 scenarios. Six
minutes of wall clock in parallel with everything else, 1m23s of which is the
lint.

| Step | Note |
| --- | --- |
| Build `openclonk-server` | `HEADLESS_ONLY=ON`, no gtest, no packing |
| `cargo build --manifest-path tests/Cargo.toml` | 3.8s; the runner image ships a Rust toolchain |
| Stage the unpacked planet tree | a copy of the engine in `lintroot/` with `planet` symlinked beside it |
| Run the lint with `--expect` | fails on any deviation from the pinned counts |

**Why its own job.** The suite in `headless` needs the packed groups staged at
`build/planet`; this needs the tree unpacked, and that path can only be one of
them. Layering an unpacked staging onto the packed one also risks writing
symlinks *into* `planet/` — `mkdir -p` succeeds on an existing symlink to a
directory, and the next `ln -sf` lands in the source tree. A separate directory
with its own copy of the binary avoids the question. It skips packing entirely,
so it is cheaper than the job beside it despite building the engine again.

### `sanitizers` — `ubuntu-latest`

The check for what tests structurally cannot catch. #41 read a stack buffer
after its scope had ended; the five `C4Group` tests written for that area pass
either way, because a dead stack slot keeps returning the right bytes until
something claims it. That is a sanitizer's job, and nothing in this project had
ever been built with `-fsanitize=` — the one prior use is upstream's own
`2f837fc1a`, by hand.

| | |
| --- | --- |
| **ASan** | gates outright. Nothing reported across four test binaries, packing twelve groups, and a scenario, so anything at all is new. |
| **UBSan** | twelve sites pinned in `tests/ubsan-expected.txt`, failing in both directions. |

Only `file:line:column` is compared. The messages carry operand values that vary
with the input — `2147483647 + 1`, `left shift of negative value -5` — so
pinning the text would fail on a different input rather than on a different
defect.

Three things about the job that are easy to remove by accident:

- **It packs with the sanitized `c4group`.** `AddEntryOnDisk` is where #41 was,
  and packing a directory is the path that read the dead buffer, so this is as
  close to a regression test as that defect can have. The packing output goes
  into the same log set that is scanned afterwards.
- **It runs a scenario.** That is the only thing here that reaches the game
  loop, and two of the twelve sites are reported by nothing else:
  `fill_edge_structure()` shifts negative coordinates left to build a
  fixed-point edge slope for landscape polygons, which is undefined before
  C++20.
- **`-fno-sanitize=vptr` is not a preference.** The vptr check emits typeinfo
  references, and `tests` links `libc4script`, whose standalone host stubs
  `C4Def` out, so the link fails with `undefined reference to 'typeinfo for
  C4Def'` before anything runs.

**The pinned sites are findings, not noise.** Five are the script engine's
integer semantics: `aul_test` asserts that `2147483647 + 1` equals `INT_MIN`,
and `C4AulExec` produces that by plain signed overflow, so a guarantee of the
language rests on undefined behaviour — in an engine whose correctness model is
bit-identical results across compilers. One more is `Distance()` in
`Standard.cpp`, which squares two 64-bit differences and then writes
`if (d2 < 0)` to catch the overflow it just risked; detecting signed overflow
after the fact is exactly what a compiler may assume cannot happen, and delete.

### `linux-client` — `ubuntu-latest`, Qt5 **and** Qt6

The first job to build a **client** rather than a server. Everything else in
this workflow builds `openclonk-server`, so `C4Window`, `C4DrawGL`, the startup
dialogs, `src/editor/` and `src/mape/` were compiled by nothing until this
existed.

Linux is the only platform where all three fit in one job: Qt5 is gone from
Homebrew, so there is no editor on macOS, and `mape` needs GTK3, so it exists on
neither macOS nor Windows.

| Step | Guards against |
| --- | --- |
| Configure with `HEADLESS_ONLY=OFF` | — |
| Assert Qt5 found, audio enabled, `Qt5Widgets_DIR` cached | A `find_package` failing quietly and removing a target, leaving a green job that compiled nothing new |
| Build | — |
| Build `mape` | The target vanishing when GTK3 or gtksourceview is absent — a no-op otherwise, since `all` already builds it |
| Assert ≥15 objects under `openclonk.dir/src/editor` and Qt5 in `ldd` | `src/editor/` being added to `OC_GUI_SOURCES`, so a successful link does not prove it was compiled |
| Assert `ldd` shows epoxy, SDL2, OpenAL, ALUT, vorbisfile | A client that builds without the graphics or audio it exists for |

**It launches the engine**, since `808f9cddd` — the only job anywhere that runs
a client rather than inspecting one:

| Step | Guards against |
| --- | --- |
| Start under `xvfb-run` with a throwaway config, assert a GL context, `Loading graphics...`, and that the process is still alive | `C4Window`, `C4DrawGL` and the SDL main loop compiling but not working |
| Five first-run launches against an empty user directory, **not gating** | nothing — it is evidence-gathering for #37 |

The gate takes the *stable* path deliberately. `C4StartupMainDlg::OnShown` opens
a modal player-creation dialog when the user directory holds no `*.ocp`, and
that is where #37 crashes, roughly one launch in six by hand. Gating on it would
make the job flaky for a defect it cannot fix, so the gate gets a player file
copied in and the hunt runs beside it with `continue-on-error`. First run on a
runner: **0 of 5 died**, `GL 4.5 (Core Profile) Mesa 25.2.8 on llvmpipe`.

**Neither branch of that `if` logs anything**, which is the weak point and is
worth knowing before trusting the hunt. From outside the process a first-run
start and an ordinary one are identical, so a user directory that quietly
stopped being empty would leave the step reporting "0 of 5 died" while starting
the ordinary menu five times — green, unchanged, and testing nothing, exactly
like the scenarios in #27. The step therefore asserts the one thing it *can*
see, that the directory holds no `*.ocp` before each launch, and fails hard when
it does. Making the path itself observable is one log line in the engine, #58,
and would let the with-dialog case gate on the fixture while still only warning
about the crash.

Costs 13 seconds for the launch and 25 for the hunt, on a job whose build alone
is 398.

Three things that had to be right, all found by running it on a development
machine first (offscreen through `SDL_VIDEODRIVER`, since that machine has no
Xvfb either):

- `planet/Test.ocp` is an unpacked player, i.e. a **directory**, so the copy
  needs `-r`.
- `--language=US` is not decoration: `Loading graphics...` is `IDS_PRC_GFXRES`
  from the string table, so the assertion only holds in that language.
- The engine **rewrites the config file it is given** on exit, with its whole
  configuration rather than the keys it was handed. Harmless, and confusing if
  you read the file afterwards looking for your three lines.

The three assertions all failed at least once on a developer machine before
being written: `FindAudio` needs OpenAL *and* ALUT *and* Ogg/Vorbis together and
says only `Not enabling audio output.` when one is missing, and an absent Qt5 or
GTK3 removes a target rather than failing a build.

### `macos-app` — `macos-latest`

Builds the full `openclonk.app` with graphics and sound, then:

| Step | Guards against |
| --- | --- |
| Configure, asserting `Using Audio toolkit: OpenAL` | Falling back to Apple's deprecated OpenAL framework |
| Build | — |
| `codesign -v` every bundled dylib | `install_name_tool` leaving invalid signatures, fatal on arm64 |
| `codesign -v` the whole bundle | The resource seal being written before the game data is packed in |
| Check every non-system dependency resolves | The bundling step rewriting a path to nothing |

### `windows-c4group` — `windows-latest`

Windows had no coverage after AppVeyor, and the old setup cannot be revived: it
pulled prebuilt dependencies from a personal server that no longer answers.
Only the epoxy and curl bundles on openclonk.org are still up.

So this starts from the smallest useful target. `C4GROUP_TOOL_ONLY` needs
nothing but zlib — PNG, JPEG, Freetype and curl are all behind that option — and
vcpkg on the runner provides it in seconds. It still compiles all of `libmisc`:
`C4Group`, `CStdFile`, `StdCompiler`, `C4NetIO` and the vendored zlib fork,
which is where this fork's platform-independent changes are.

| Step | Guards against |
| --- | --- |
| Configure and build `c4group` | MSVC drifting out of compatibility with the codebase |
| Pack, unpack, compare contents | The group format breaking on Windows |
| Assert a usage error exits non-zero | The exit code regression, on Windows too |

Only the `c4group` target is built: `C4GROUP_TOOL_ONLY` still *defines*
`libc4script` and `libopenclonk`, so the default `all` target tries to compile
sources needing PNG and JPEG that this configuration does not look for.

**MSVC 2022 compiles libmisc and c4group without a single error** — the first
successful Windows build of this codebase in about five years. Two upstream
defects had to be fixed to get there, both in configurations evidently never
used on that platform: see sections 14 and 15 of [divergence.md](divergence.md).

### `windows-headless` — `windows-latest`

The gap `windows-c4group` left, and the one that mattered most after the move to
C++17: that job builds `c4group`, which links `libmisc` and nothing else, so
MSVC had compiled the archive layer and never seen `libc4script`,
`libopenclonk` or the engine under the new standard.

This one mirrors the Unix `headless` job — configure `HEADLESS_ONLY`, build
everything, run all four test binaries through ctest, pack the game data,
reject empty archives, stage the groups and run the scenario suite against the
same pinned counts, from the same file. Green on its first run, when it still
ran `Movement.ocs` alone; the rock landed at `[372, 157]`, one of the two values
Linux produces. It has run the whole suite since `0875c9348`, and reports the
same 912 assertions as the Unix jobs.

**It costs 7 minutes, not the 38 this was budgeted at.** That figure came from
the development machine, where all twelve ports build from source on four
cores; the hosted runner has these four available and installs them in two
minutes *on a cache miss*. Measured breakdown of the first run:

| Step | |
| --- | --- |
| `vcpkg install zlib libpng libjpeg-turbo curl` | 120s (cache miss) |
| Build the engine | 158s |
| Build the four test targets | 60s |
| ctest | 1s |
| Pack twelve groups | 13s |
| Run `Movement.ocs` | 11s |

So the reason this waited was never the cost. It was #29 — the `groups` target
could not invoke `c4group` under MSBuild — and the assumption that it would be
expensive, which nobody had checked.

Four things differ from the Unix jobs, each paid for by hand on the Windows
machine first:

- **No `--parallel`.** `/MP` is already on `CMAKE_CXX_FLAGS` in the MSVC branch,
  so MSBuild is parallel per project already.
- **No stdin trick, and none available.** `STDSCHEDULER_USE_EVENTS` is defined
  here, so `C4AbstractApp` never registers `C4StdInProc` and the engine cannot
  be stopped by closing stdin (`C4AppT.cpp:34` says so). The scenario step polls
  a log and kills the process, and reads either the redirected stdout or
  `%APPDATA%\OpenClonk\OpenClonk.log`, whichever carries the harness output.
- **Hardlinks, not symlinks.** A symlink needs a privilege the runner lacks.
- **Git's `tar` shadows the system one** and treats a drive letter as a remote
  host, so googletest arrives as a zip through `Expand-Archive`.

Kept separate from `windows-c4group` rather than folded into it: they are
different configurations — `C4GROUP_TOOL_ONLY` is the one that guards #19 — and
two jobs run in parallel rather than in sequence.

What it still does not cover: the full GUI build (openal-soft, freealut,
ogg/vorbis, epoxy and freetype on top) and launching it, which is the same limit
`macos-app` has.

Typical runtimes: Linux headless ≈ 5 min, Linux client ≈ 7 min, macOS headless
≈ 6 min, macOS app ≈ 4.5 min, Windows c4group ≈ 1.5 min, Windows headless ≈ 7
min. The five-scenario suite is
about 70 s of that, less than the single scenario used to cost. The client job costs
no more than the headless one despite building the editor and mape on top —
the runner has more cores than the build has serial work.

## Why the checks are shaped like this

Every assertion exists because the corresponding failure was **silent**. The
engine and its tooling report success in several situations where nothing
happened:

- `c4group` prints "Pack failed" and exits **0**, so `--target groups` succeeds
  while writing zero-byte archives.
- `openclonk-server` exits **0** when it quits before initialising, with
  "Game cleared" in the log making it look like a completed run.
- `ctest` used to pass while never running the largest of the three test
  binaries; fixed now, but the workflow still checks the manifest rather than
  assuming it is complete.

Checking exit codes alone would therefore have produced a green pipeline for a
completely broken build. Each check inspects output instead.

## Environment constraints discovered while getting it green

Four things had to be handled that are not obvious from the source tree. Each
took a failed run to find. Three are still worked around in the workflow; the
fourth was fixed in the engine. The reasoning is in
[decisions.md](decisions.md).

**Build parallelism must be bounded.** `cmake --build build -j` passes a bare
`-j` to GNU Make, which means *unlimited* parallelism. Around 500 translation
units spawned as many compilers, the runner ran out of memory and was shut
down mid-build after 46 minutes with its logs lost. The workflow now passes
`--parallel "$(getconf _NPROCESSORS_ONLN)"`.

**`HEADLESS_ONLY` used to need a GL loader on macOS.** `C4AppMac.mm` included
`epoxy/gl.h` outside its `USE_CONSOLE` guard. Fixed in the engine rather than
worked around here, so the macOS headless job installs no libepoxy and a step
asserts that no translation unit in `openclonk-server` references a GL loader —
checking the property directly, since a runner that happens to have libepoxy
would otherwise make the check pass for the wrong reason. See
[ADR-014](decisions.md#adr-014--move-the-epoxy-include-behind-use_console-rather-than-dropping-the-file).

**The engine has to find its data.** `C4Reloc::Init()` prefers a `planet`
folder next to the executable and otherwise falls back to `OC_SYSTEM_DATA_DIR`,
the install prefix. Upstream never hit that fallback because Travis configured
in-source, leaving the binaries in the repository root next to `planet/`. An
out-of-source build has neither. See
[ADR-011](decisions.md#adr-011--stage-packed-groups-next-to-the-binary-instead-of-installing-to-a-prefix).

**stdin must stay open.** `C4StdInProc::Execute()` calls `Application.Quit()`
when reading stdin fails — the intended shutdown path, which
`tests/start_all_scenarios.rs` also uses. A `run:` step gets stdin from
`/dev/null`, so the first read returns 0 and the engine quits before its state
machine ever ticks. See [ADR-009](decisions.md#adr-009--hold-stdin-open-in-ci-instead-of-letting-the-engine-self-terminate).

## What it does not cover

- **The other 94 scenarios, as gameplay.** The suite covers the five under
  `Tests.ocf` that hold assertions. The `scenario-lint` job loads all 99 and
  checks what the script engine says about each, which catches a script error
  anywhere in Missions, Worlds, Arena, Parkour, Defense or Tutorials — but only
  that. Nothing asserts what those scenarios *do*, because they carry no
  `doTest()` to assert with.

- **The open scenario bug, as a bug.** `ObjectInteractionMenu.ocs` (#51) still
  fails 14 of 328; what CI guarantees is only that it fails *exactly as much as
  before*. See the pinned counts below. `Movement.ocs` was the other one and is
  green since `013d76873`.

- **Reproducibility.** A scenario run is not repeatable: `RandomSeed` is
  `time(nullptr)` for a local game, and the seed reaches gameplay. Two runs of
  `Movement.ocs` differ by a pixel or two in every reported position. A pinned
  *count* survives that, since no assertion sits close enough to a boundary to
  flip — but it is the reason the counts are pinned rather than the positions,
  and it is worth knowing before a new assertion is written. See
  [platforms.md](platforms.md#scenario-run--verified-here).

- **The rest of Windows.** Only `c4group` is built there. `HEADLESS_ONLY`, the
  full GUI build, the unit tests and a launched engine have since all been
  verified by hand on a Windows machine — see
  [platforms.md](platforms.md#windows-x64) — and needed no source change, so
  extending this job is now a matter of adding vcpkg packages and build time
  rather than an open question. Until that happens it is one machine, once.
- **The Qt editor on macOS.** Covered on Linux by `linux-client` now. macOS has
  no Qt5 from Homebrew, so `src/editor/` is still compiled by nothing there, and
  anything platform-specific in it is unguarded — including the editor branch of
  the mouse handler #8 deliberately left alone. #46.
- **Running the GUI anywhere but Linux.** `linux-client` launches the engine
  under Xvfb now. `macos-app` still builds and inspects the bundle without
  starting it, and Windows builds no client at all.

  **"No display on the runner" was never the obstacle it was treated as**, and
  is now settled rather than argued: `xvfb-run` is one apt package, one second
  to install, and the runner reports `GL 4.5 (Core Profile) Mesa 25.2.8 on
  llvmpipe`. The cost that was real — a `HEADLESS_ONLY=OFF` build — was already
  being paid by `linux-client`. #45.
- **Most of the fixes themselves.** The unit tests cover `libmisc` and
  `libc4script`. What guards the rest is the scenario run, the bundle checks and
  the Windows round-trip.

## This workflow will not pass on upstream master

It depends on this fork's fixes — without them `c4group` writes empty archives
on macOS, `openclonk-server` hangs in an AppKit loop, and `HEADLESS_ONLY` does
not even configure. If the workflow is ever submitted upstream it has to come
**after** the fix PRs, or in the same one. Noted in
[pr-plan.md](pr-plan.md).

## After editing the workflow, check the job list

Not just that the YAML parses, and not just the steps of the job being worked
on:

```sh
ruby -ryaml -e 'puts YAML.load_file(".github/workflows/build.yml")["jobs"].keys'
```

A whole job was once deleted by an edit that took a slice from a marker comment
to the end of the file — the Windows job happened to sit after that marker. The
file stayed valid YAML, the steps that were inspected looked correct, and the
run went green with three jobs instead of four. Windows coverage was gone for
three commits before anyone noticed, and a report in between claimed "all four
jobs green" while the run had listed three.

A missing job cannot fail. It is the one kind of regression this workflow
cannot catch about itself.

## Debugging a failed run

```sh
gh run list --repo eeryinkblot/openclonk --limit 5
gh run view <id> --repo eeryinkblot/openclonk --json jobs \
  --jq '.jobs[] | "\(.conclusion)  \(.name)", (.steps[] | select(.conclusion=="failure") | "    \(.name)")'
```

`gh run view --log` returns nothing useful for these jobs. Fetch the raw log
through the API instead, and strip the escape sequences:

```sh
gh api --allow-escape-sequences repos/eeryinkblot/openclonk/actions/jobs/<jobId>/logs \
  | sed 's/\x1b\[[0-9;]*m//g; s/^[0-9T:.-]*Z //'
```

If a runner is killed mid-step the log blob may be missing entirely
(`BlobNotFound`) — that itself is a signal, usually resource exhaustion.
