# Continuous integration

`.github/workflows/build.yml`. Upstream's CI had been dead for years —
`.travis.yml` targeted travis-ci.org, which no longer exists, and `appveyor.yml`
pinned Visual Studio 2017 — so nothing had verified a build since roughly 2020.
Both are removed in this fork; this workflow replaces them.

Triggers: push to `master`, any pull request, and manual dispatch. Runs on the
same ref cancel each other (`concurrency` with `cancel-in-progress`), and every
job has a 45-minute cap so a stuck build fails instead of running to the
six-hour default.

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

What it does *not* do is gate. It `println!`s the counts it captures and never
inspects them; the only `panic!` is for failing to spawn the engine, so it exits
0 whatever it finds. A human has to read the output. It also passes a bare
`Test.ocp`, so the assertions inside `Tests.ocf` never run under it — which is
where the "reports success for scenarios whose assertions never ran" note comes
from, and is a fair criticism of it as an *assertion* runner, not as a lint.

Note the irony recorded here rather than quietly fixed: the
`variable_out_of_scope` warning in `LiquidContainer.ocs` (95a1d8094) is exactly
what this tool was built to surface. It would have found it in 2018 if anyone
had run it.

**It still works.** Built against Rust 1.93 with its 2018 dependencies
unchanged, in 2.6 seconds, with one deprecation warning — and run over the whole
tree it reported 97 of 99 scenarios. 93 are clean; `ScriptError1.ocs` has two
errors on purpose, and `SkeletonAppend.ocs`, `CableLorrys.ocs` and
`Benchmarks.ocs` have warnings nobody has looked at. One of them is a
`FindObject` call still using the Clonk 4 signature.

The two that report nothing, `CableCars.ocs` and `LiquidSystem.ocs`, die with
`Required object file Experimental.ocd not available` — `Experimental.ocf`,
`Tests.ocf` and `Issues.ocf` are not in `OC_C4GROUPS`, so they are never packed
and two test scenarios cannot run against a packed tree at all.

So this is not a revival project, it is a tool nobody starts. What it needs is a
decision about gating — and `ScriptError1.ocs` means a naive "fail on any error"
would be wrong, so it wants pinned per-scenario counts exactly like the suite
above. #52.

The two runners agree assertion for assertion, failures included — 290/168/120
passing and 14 failing in `ObjectInteractionMenu` on both. That
is C4Real determinism across clang and GCC, over 912 assertions rather than the
7 it used to be, and it is now checked on every push rather than by hand. The
`determinism` unit tests say the same thing about the primitives: sequences
pinned from pcg32 on GCC pass unchanged on clang.

### `linux-client` — `ubuntu-latest`

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

It **does not launch anything**. That needs `xvfb` and is #45 — the engine runs
on llvmpipe, so the display was never the obstacle it was taken for.

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

Typical runtimes: Linux headless ≈ 5 min, Linux client ≈ 7 min, macOS headless
≈ 6 min, macOS app ≈ 4.5 min, Windows ≈ 1.5 min. The five-scenario suite is
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

- **The other 69 scenarios.** The suite covers the five under `Tests.ocf` that
  hold assertions. `planet/` contains **99** `.ocs` in total — Missions, Worlds,
  Arena, Parkour, Defense, Tutorials — and no automation loads any of them, so a
  script error in real game content is caught by nobody. That is what
  `tests/start_all_scenarios.rs` was written for; see the note below.

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
- **Actually running the GUI.** `macos-app` builds and inspects the bundle but
  never launches it, and no job builds a client on Linux at all.

  **"No display on the runner" is not the obstacle it has been treated as.**
  `xvfb-run` is one apt package, and the engine does not need a GPU: it runs on
  llvmpipe at `GL 4.6 (Core Profile)` and renders the full main menu, verified
  on the Linux machine with `LIBGL_ALWAYS_SOFTWARE=1`. The real cost is a
  `HEADLESS_ONLY=OFF` build in CI, which nothing does yet. #45.
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
