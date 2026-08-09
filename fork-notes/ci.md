# Continuous integration

`.github/workflows/build.yml`. Upstream's CI has been dead for years —
`.travis.yml` targets travis-ci.org, which no longer exists, and `appveyor.yml`
pins Visual Studio 2017 — so nothing had verified a build since roughly 2020.

Triggers: push to `master`, any pull request, and manual dispatch. Runs on the
same ref cancel each other (`concurrency` with `cancel-in-progress`), and both
jobs have a 45-minute cap so a stuck build fails instead of running to the
six-hour default.

## Jobs

### `headless` — `ubuntu-latest` and `macos-latest`

Builds with `HEADLESS_ONLY=ON`, then:

| Step | Guards against |
| --- | --- |
| Configure, asserting gtest was found | CMake silently omitting the test targets when the sources are missing |
| Build | — |
| Build `tests aul_test StdMeshMath` | — |
| Assert no GL loader in the server | `HEADLESS_ONLY` quietly acquiring a graphics dependency |
| Run all three binaries individually | `ctest` covering only two of them (see [ADR-010](decisions.md#adr-010--pin-googletest-to-1100-and-fetch-the-sources-in-ci)) |
| Pack game data | — |
| Reject empty group files | `c4group` exiting 0 after a failed pack |
| Stage packed groups next to the binary | — |
| Run a scenario, assert `Game started` and `0 warnings, 0 errors` | The engine quitting with exit 0 without running anything |

### `macos-app` — `macos-latest`

Builds the full `openclonk.app` with graphics and sound, then:

| Step | Guards against |
| --- | --- |
| Configure, asserting `Using Audio toolkit: OpenAL` | Falling back to Apple's deprecated OpenAL framework |
| Build | — |
| `codesign -v` every bundled dylib | `install_name_tool` leaving invalid signatures, fatal on arm64 |
| Check every non-system dependency resolves | The bundling step rewriting a path to nothing |

The executable is deliberately **not** verified with `codesign -v`: game data is
packed into `Contents/Resources` by a later POST_BUILD step, so its resource
seal is always stale. Only the presence of a signature is checked.

Typical runtimes: Linux ≈ 5.5 min, macOS headless ≈ 4.5 min, macOS app ≈ 4 min.

## Why the checks are shaped like this

Every assertion exists because the corresponding failure was **silent**. The
engine and its tooling report success in several situations where nothing
happened:

- `c4group` prints "Pack failed" and exits **0**, so `--target groups` succeeds
  while writing zero-byte archives.
- `openclonk-server` exits **0** when it quits before initialising, with
  "Game cleared" in the log making it look like a completed run.
- `ctest` passes while never running the largest of the three test binaries.

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

- **Windows.** No job; `appveyor.yml` is still there and still broken.
- **The Qt editor.** Qt5 is no longer available from Homebrew, so
  `WITH_QT_EDITOR` is off everywhere and `src/editor/` is not compiled.
- **Actually running the GUI.** `macos-app` builds and inspects the bundle but
  never launches it; there is no display on the runner.
- **The fixes themselves.** The unit tests cover `libmisc` and `libc4script`.
  None of the ten changes in this fork lives there, so none is exercised by
  them. What guards them is the scenario run and the bundle checks.

## This workflow will not pass on upstream master

It depends on this fork's fixes — without them `c4group` writes empty archives
on macOS, `openclonk-server` hangs in an AppKit loop, and `HEADLESS_ONLY` does
not even configure. If the workflow is ever submitted upstream it has to come
**after** the fix PRs, or in the same one. Noted in
[pr-plan.md](pr-plan.md).

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
