# Upstream PR plan

**This fork is the primary line of development, not a staging area.** Upstream
submission is opportunistic: worth doing when a cluster is ready and someone
feels like it, never a blocker for anything here.

What this file is for is keeping that option *cheap*. Commits stay separable,
each cluster stays independently defensible, and nothing gets bundled together
for convenience. Then a PR is twenty minutes of cherry-picking whenever it
suits, rather than an archaeology project.

Update it whenever you add a commit that could plausibly go upstream.

**This file is fork-local and must never appear in a PR branch.** So are
`CLAUDE.md`, `ROADMAP.md` and everything else under `fork-notes/`.

## The rule that keeps it separable

PRs are branch-based, not commit-based. Never branch a PR off this `master` —
it carries fork-local commits and everything else you have accumulated. Build
each PR branch on the upstream tip and cherry-pick:

```sh
git fetch upstream
git checkout -b <pr-branch> upstream/master
git cherry-pick <commits...>
git push -u origin <pr-branch>
```

Guard before opening the PR:

```sh
git diff --name-only upstream/master..HEAD \
  | grep -E '^(CLAUDE\.md|ROADMAP\.md|fork-notes/)' \
  && echo "STOP: fork-local files in this branch"
```

Because the branch is cut fresh from `upstream/master` each time, cherry-picked
commits are *copies*. Reshape them freely for review — split them, rewrite the
messages, squash — without touching what is already pushed here.

## Clusters

Ordered by how easy they are to argue. Every cluster is independent of the
others unless stated, so they can be opened in any order or in parallel.

### PR 1 — zlib: build and gzip output on modern macOS

| | |
| --- | --- |
| Commits | `a0d7e62cf`, `d16b2b3d0` |
| Files | `src/zlib/gzio.c`, `src/zlib/zutil.h` |
| Effect | Engine compiles again with current clang; `c4group` can pack at all |

**Keep these two together and in this order.** `a0d7e62cf` fixes a hard compile
error in `gzio.c`; without it the translation unit does not build on a current
toolchain, so a PR carrying only `d16b2b3d0` would not compile on the platform
it is about.

Strongest cluster to lead with: both are small, both fix reproducible
breakage, and neither touches engine logic.

### PR 2 — macOS: mouse input in fullscreen

| | |
| --- | --- |
| Commits | `72ce70252` |
| Files | `src/graphics/C4DrawGLMac.mm` |
| Effect | Mouse control works outside the editor |

Self-contained, and the argument is verifiable from the tree alone: the
condition being removed depends on a state only editor consoles ever set. The
Windows and SDL backends already do it the way this changes it to, which makes
the review a comparison rather than a judgement call.

### PR 3 — macOS: openclonk-server entry point

| | |
| --- | --- |
| Commits | `60f3fe708` |
| Files | `CMakeLists.txt`, `src/platform/C4AppDelegate.mm` |
| Effect | The headless server starts instead of hanging in an AppKit loop |

Independent of PR 5 despite also touching `CMakeLists.txt` — different hunks,
no textual overlap. `openclonk-server` is built regardless of `HEADLESS_ONLY`,
so this does not depend on that cluster either.

### PR 4 — CMake: configure correctly on macOS

| | |
| --- | --- |
| Commits | `c02c12e38`, `7d38493cf` |
| Files | `CMakeLists.txt` |
| Effect | `HEADLESS_ONLY=ON` configures on macOS; the bundled blake2 header wins over a system one |

Two unrelated defects that happen to share a file. Splitting them into separate
PRs is fine and arguably cleaner; keeping them together is defensible because
both are build-system corrections with no runtime effect on a working build.

`c02c12e38` also restores the `groups` target on macOS, which is what makes the
game data packable in a headless build.

### PR 4b — macOS: report the actual architecture

| | |
| --- | --- |
| Commits | `05014153a` |
| Files | `src/platform/PlatformAbstraction.h` |
| Effect | An arm64 build stops announcing itself as `mac-x86` |

Self-contained and independent of everything else. Small enough to fold into
PR 4 if you would rather not open a separate one, though it is a source change
rather than a build-system one.

Worth stating in the PR text that the existing `mac-x86` value is deliberately
untouched: it is a lookup key on the league and update servers, and Intel Macs
have always reported it despite being x86_64.

### PR 4c — macOS: HEADLESS_ONLY without a GL loader

| | |
| --- | --- |
| Commits | `1105b7e98` |
| Files | `src/platform/C4AppMac.mm` |
| Effect | `HEADLESS_ONLY` stops requiring libepoxy on macOS |

A one-line move of an include inside a guard the file already has. Independent,
though it pairs naturally with PR 4 since both make `HEADLESS_ONLY` behave as
documented on macOS.

### PR 6 — c4group: report failures in the exit code

| | |
| --- | --- |
| Commits | `c1a26fc7b` |
| Files | `src/c4group/C4GroupMain.cpp` |
| Effect | A failed pack stops being reported as success |

Platform-independent, unlike everything else here, and a good companion to
PR 1: that fixes the packing failure, this makes any future one visible.

Flag two things in the PR text — the deliberately unmarked "Status:" line, and
that "Unknown option" and "Error forking" now fail where they used to warn.

### PR 7 — tests: register the tests target with ctest

| | |
| --- | --- |
| Commits | `9af77e8a3` |
| Files | `tests/CMakeLists.txt` |
| Effect | `ctest` stops skipping 15 of the 72 tests |

One line plus a comment, platform-independent, and trivially verifiable by
running `ctest -N` before and after. The easiest of all the clusters to argue.

### PR 8 — remove the dead CI configuration

| | |
| --- | --- |
| Commits | `35aa52f40` |
| Files | `.travis.yml`, `appveyor.yml`, `tools/ci/`, `tools/generate_license_headers.cpp` |
| Effect | Deletes configuration for two services that no longer run |

**The most opinionated cluster, and the only one that deletes rather than
fixes.** A maintainer may reasonably want to keep the files as a record of how
the project used to build, or may not want them gone without a replacement
landing first. Pair it with the workflow, or drop it — nothing else depends on
it.

### PR 9 — build system: options that were never exercised

| | |
| --- | --- |
| Commits | `b709b8c1b`, `0f2d43509` |
| Files | `CMakeLists.txt` |
| Effect | `C4GROUP_TOOL_ONLY` configures and builds on Windows |

Both found by adding a Windows CI job. The MSVC block sets properties on
targets that `HEADLESS_ONLY` and `C4GROUP_TOOL_ONLY` remove; zlib's include
path was only added in the branch that excludes `C4GROUP_TOOL_ONLY`, so the one
configuration needing nothing else could not find `zlib.h`.

Pairs naturally with PR 4 — same class of defect, same file, opposite platform.

### PR 9b — CMake: the `groups` target cannot run c4group on the VS generator

| | |
| --- | --- |
| Commits | `b0ce04384` |
| Files | `CMakeLists.txt` |
| Effect | `cmake --build . --target groups` packs game data on Windows |

Not fork-specific: `oc_set_target_names()` is upstream code, so anyone
generating a Visual Studio solution and building `groups` hits
`MSB8066 … code 9009` before a single group is packed. See divergence.md 18 for
the mechanism and [ADR-017](decisions.md#adr-017--run-c4group-through-cmake--e-env-rather-than-moving-the-binaries)
for why the obvious `$<TARGET_FILE:...>` fix is not enough.

Independent of PR 9 but from the same family, and a reviewer looking at one will
find the other easy to accept. Worth stating in the message that the change is
verified on two generators — Visual Studio and Ninja produce byte-identical
groups — since it touches a path every platform uses to fix a fault only one
sees.

### PR 9c — FindAudio: the pkg-config branch cannot work on MSVC

| | |
| --- | --- |
| Commits | `5cec4133b` |
| Files | `cmake/FindAudio.cmake` |
| Effect | Audio links on MSVC when pkg-config happens to be installed |

Independent of PR 9 and 9b, same family. The argument is self-contained:
`pkg_check_modules` reports bare library names, the module never exports the
`_LIBRARY_DIRS` that say where they are, and MSVC has no default search path to
fall back on. Reviewers can see it in the file without reproducing anything.

Worth mentioning in the message that the trigger is mundane — installing
`qt5-base` pulls in `pkgconf` — because it makes the case that this is reachable
rather than theoretical. Note also what was *not* done and why (resolving the
pkg-config names through `find_library`), so a reviewer who prefers the general
fix sees it was considered; [ADR-018](decisions.md) has the reasoning.

### PR 10 — CI: a GitHub Actions workflow

| | |
| --- | --- |
| Commits | the `.github/workflows/build.yml` series, starting `91b545a07`; latest `3f0b9374d`, `2e9855c3d`, `65a61cce6`, `013d76873` (workflow hunk only), `592b7b754`, `5198b01c6` |
| Files | `.github/workflows/build.yml` |
| Effect | Replaces the dead Travis and AppVeyor configuration |

**Submit last.** The workflow cannot pass on upstream master without the fixes
above, so it has to follow them or be squashed together with them. It also
pairs with PR 8, which deletes what it replaces.

### PR 11 — cosmetic: stop repeating the platform in the version string

| | |
| --- | --- |
| Commits | `1736155a4` |
| Files | `cmake/Version.cmake` |
| Effect | `Version: 9.0-alpha mac-arm64` instead of `9.0-alpha mac mac-arm64` |

Presentation, not a defect — upstream shows the same redundancy everywhere, so a
maintainer may simply not care. Pairs with PR 4b, which added the architecture
in the first place. Mention in the PR text that the HTTP User-Agent loses its
platform suffix, and that the string is provably never compared.

### PR 12 — mape: compile under a C23 default

| | |
| --- | --- |
| Commits | `83b23b4b6` |
| Files | `src/mape/mape.c` |
| Effect | `mape` builds on GCC 15 and later |

The easiest cluster in this file. One parameter, a compiler error quoted in
full, and a target nobody can build on a current distribution without it.
Independent of everything else.

Lead the message with the error text rather than the standards history — a
reviewer who has never seen the C23 change to empty parameter lists will
recognise the diagnostic immediately.

### PR 13 — CMake: the `groups` target races against itself on Makefiles

| | |
| --- | --- |
| Commits | `0ce1ea716` |
| Files | `CMakeLists.txt` |
| Effect | `--target groups --parallel N` stops producing an incomplete group set |

Not fork-specific and not platform-specific: the `USES_TERMINAL` guard upstream
already wrote only binds Ninja, so every Makefile user packing in parallel is
exposed. Same family as PR 9b and can be sent with it or after it; they touch
the same block but different lines.

Worth putting in the PR text: the cause is `MakeTempFilename()`, this commit
does not fix it, and [ADR-019](decisions.md#adr-019--serialise-the-groups-target-rather-than-repair-maketempfilename)
says why. A reviewer who spots the real bug should find it already
acknowledged rather than argue for it.

### PR 14 — tests: build against a googletest that still compiles

| | |
| --- | --- |
| Commits | `8722c249b`, `3207274f2` |
| Files | `tests/TestLog.h`, `tests/aul/ErrorHandler.h`, `tests/CMakeLists.txt` |
| Effect | The three test targets build on GCC 15+, and on hosts with gtest installed |

**Keep both, in this order.** `8722c249b` removes the macros that forced the
1.10.0 pin; `3207274f2` stops the include path being cached from the system
before the sources are known. Either alone leaves a machine that cannot build
the tests.

Both are widenings — the variadic `MOCK_METHOD` exists in 1.10.0 too, and the
header search is narrowed, not redirected — so neither obliges a reviewer to
change their own googletest. Say that explicitly; the natural first objection is
"this bumps a dependency", and it does not.

Independent of PR 7, which registers the `tests` target with ctest, though a
maintainer may want them together as the test-infrastructure cluster.

### PR 15 — tests: cover the determinism primitives

| | |
| --- | --- |
| Commits | `a81cd4e97` |
| Files | `tests/CMakeLists.txt`, `tests/math/C4RealTest.cpp`, `tests/math/C4RandomTest.cpp` |
| Effect | 24 tests over `C4Real` and `C4Random`; `ctest` runs four binaries instead of three |

Pure addition — no engine file changes, so nothing can regress. The argument is
the one upstream already made for itself in the comments at the top of
`C4Real.h`: fixed point exists because floating point desyncs, and until now
nothing checked that the fixed-point type keeps its word.

Two things to say in the PR text. The assertions are exact bit patterns on
purpose, because `EXPECT_NEAR` would pass through precisely the change that
causes a desync. And the pinned `pcg32` sequences were captured on GCC and pass
unchanged on clang, which is a result rather than a formality.

Independent of everything else. Pairs naturally with PR 7 and PR 16 as a
testing cluster if a maintainer prefers one PR.

### PR 16 — Tests.ocf: a variable used outside its declared scope

| | |
| --- | --- |
| Commits | `95a1d8094` |
| Files | `planet/Tests.ocf/LiquidContainer.ocs/Script.c` |
| Effect | The scenario links with 0 warnings instead of 2 |

Three lines, one of them a comment. `var test` was declared inside one `if`
block and used inside the next; C4Script variables are function-scoped so it
works, which is why the engine warns rather than errors.

Worth mentioning in the PR that this is what `tests/start_all_scenarios.rs`
exists to find, and that the tool still works (#52) — it makes the change look
like the first result of a tool upstream already has rather than a drive-by.

### PR 17 — C4Group: two defects in the write path, and the first tests for it

| | |
| --- | --- |
| Commits | `9ce177d99`, `5a34e6eef`, `c0c6d8639` |
| Files | `src/c4group/C4Group.cpp`, `tests/C4GroupTest.cpp` |
| Effect | No undefined behaviour when packing a directory; a group with a renamed child can be written without a sort list; five tests where there were none |

The strongest cluster here for a dormant reviewer, because it needs no
platform knowledge and no environment: one lifetime bug visible by reading
the function, one silent failure with a reproducer, and tests that cover
both paths.

Order matters — `5a34e6eef` is what makes `c0c6d8639` pass, since the tests
are the first caller in the tree that does not set a sort list. Lead the PR
with the lifetime fix; it is the one that needs no argument.

### PR 18 — tests: state what MakeTempFilename does not promise

| | |
| --- | --- |
| Commits | `0776ab86f` |
| Files | `tests/StdFileTest.cpp` |
| Effect | Two disabled tests defining what a fix has to do |

Send only alongside or after a fix for the function itself. On its own it is
a bug report written in C++, which is a fine thing to have in this fork and
an odd thing to ask a stranger to merge.

### PR 19 — CMake: raise the floor to 3.10

| | |
| --- | --- |
| Commits | `443690c41` |
| Files | `CMakeLists.txt`, `cmake/DeployQt.cmake` |
| Effect | No deprecation warning on CMake 4; three dead workarounds removed |

Independent of everything else and cheap to review. Worth pairing in the
description with the fact that CMake 4.0 removed `< 3.5` outright, so the old
floor was already below what any current CMake accepts.

### PR 20 — Movement.ocs: an expectation left behind by a 2019 engine change

| | |
| --- | --- |
| Commits | `013d76873` (the scenario half) |
| Files | `planet/Tests.ocf/Movement.ocs/Script.c` |
| Effect | The scenario passes 3 of 3 instead of 2 of 3 |

The commit also lowers a CI pin in `.github/workflows/build.yml`, which is
fork-local — split that hunk out when cherry-picking.

This one has an unusually good argument attached: the commit that changed the
behaviour is upstream's own (`0315ea6ef`), by the same author as the test, and
says in its message that it may break scenarios relying on it. Quote that, and
mention the reverted-file experiment.

### PR 23 — Support Qt6 for the editor, keeping Qt5

| | |
| --- | --- |
| Commits | `511e3eea4` |
| Files | `CMakeLists.txt`, `cmake/DeployQt.cmake`, `src/editor/` (4 files), `.github/workflows/build.yml` (matrix hunk — fork-local, split it out) |
| Effect | The editor builds against either Qt major version |

**Depends on PR 22**: Qt6 requires C++17, so this cannot land before the
standard does.

The argument writes itself for a project whose editor is unavailable on a whole
platform: Homebrew has no Qt5 any more. Lead with that, and with the fact that
nothing is dropped — `find_package(QT NAMES Qt6 Qt5)` keeps every existing Qt5
build working, which matters for their Windows users.

Worth stating in the description that both configurations were *run*, not only
compiled, and that the port is nine small changes rather than a rewrite. The
`QOpenGLWidget` module move is the one a reviewer will not know offhand.

### PR 22 — Raise the language standard to C++17

| | |
| --- | --- |
| Commits | `fb3ec7b9c` |
| Files | `CMakeLists.txt`, `.github/workflows/build.yml` (comment only) |
| Effect | Lifts the googletest ceiling and makes a Qt6 port possible |

One line, and the PR text should lead with the measurement rather than the
motivation: nothing in `src/` uses anything C++17 removed, and on GCC 16 with
Qt5 the headless build, all four test binaries, the full client with editor and
`mape`, and a scenario run are all unaffected — same positions, same
script-linking counts.

**Send it after the platform evidence exists.** Upstream cannot check macOS or
Windows either, so the argument is only as good as the builds behind it. Worth
waiting until the Windows machine has done a full C++17 build by hand.

Independent of everything else here, but note that a Qt6 port (#50) would have
to follow it rather than accompany it.

### PR 21 — tests: make the scenario lint runnable and make it gate

| | |
| --- | --- |
| Commits | `bc55e8f42`, `ed28194a1` |
| Files | `tests/Cargo.toml`, `tests/Cargo.lock`, `tests/start_all_scenarios.rs`, `tests/scenario-lint-expected.txt`, `.gitignore` |
| Effect | A tool upstream already has stops being unrunnable, and starts failing on deviation |

An unusually good fit for a dormant project: it revives their own
contributor's work rather than proposing something new, and the argument is
one sentence — cargo-script does not install any more, so the tool cannot be
started at all.

Worth saying in the description that the pinned file records seven scenarios
that are not clean *today*, in their tree, and that one of them
(`ColorfulLights.ocs`, nine duplicate global declarations) has been sitting
there unnoticed since the editor generated `Objects.c`.

The `scenario-lint` job that runs it is **not** in this cluster — it belongs to
PR 10 with the rest of the workflow, which is submitted last because it cannot
pass on upstream master until everything above has landed. The two tests/
commits stand on their own without it: the tool becomes runnable and gains a
`--expect` mode whether or not anything automated calls it.

### PR 24 — startup: log which player-file path was taken

| | |
| --- | --- |
| Commits | `63c3419c7` |
| Files | `src/gui/C4StartupMainDlg.cpp` |
| Effect | A first start and an ordinary one stop being indistinguishable in the log |

Small and self-contained: one `LogSilentF` in each branch of the `if` in
`C4StartupMainDlg::OnShown`, plus a copy of the found filename out of the
`DirectoryIterator`, whose storage does not outlive the loop.

Lead with the debugging argument rather than the CI one — upstream does not run
this fork's workflow, but anyone reading a user's log has the same problem: the
modal player-creation dialog leaves no trace at all, and it is the one path a
startup crash has been seen in. The `LogSilent` level is the point to make
second: nothing new appears on screen for an ordinary player.

The CI half (`8585f0226`) is fork-local and belongs to PR 10.

### PR 5 — macOS: audio and app bundling

| | |
| --- | --- |
| Commits | `6333d5498`, `605abb61b` |
| Files | `CMakeLists.txt`, `cmake/FindAudio.cmake`, `tools/osx_bundle_libs` |
| Effect | Links a real OpenAL; produces a bundle that actually launches |

Weakest cluster — submit last, and be upfront in the PR text:

- `605abb61b` re-anchors rpaths to absolute paths, so the bundle stops being
  relocatable. It was not startable at all before, so this is still a net gain,
  but a maintainer will ask.
- The stale resource seal that used to come with this is fixed separately by
  `4e9f0c3ec`; include it, or `codesign -v` on the result still fails.
- `6333d5498` was found while chasing silent audio but was **not** its cause.
  Do not claim it fixes "no sound" — see `divergence.md`.

Consider splitting `6333d5498` off into its own PR; it is clean, while
`605abb61b` carries the caveats.

## Never upstream

| Commits | Files |
| --- | --- |
| `4d1e0ba08`, `18a459494`, `660a1f8f1`, `41c1b6f30`, `c2336894d`, `52be316de`, `9bd0c8776` | `CLAUDE.md` |
| — | `ROADMAP.md` |
| — | `fork-notes/` |

Both files are fork-local by policy, so the `fork-notes/` row stays a blanket
one. `CLAUDE.md` keeps explicit hashes because that is what makes the guard
usable in the other direction: when a PR branch turns out to carry one of these,
the hash names it. Add yours when you touch the file.

## Adding new commits

Before committing something you may want upstream:

1. Keep it to one defect per commit. Everything above is separable only because
   nothing was bundled together for convenience.
2. Put the evidence in the commit message — the error output, the stack, the log
   line that changed. Reviewers of a dormant project have no context and no way
   to reproduce your environment.
3. Add it to a cluster here, or open a new one. Note any commit it depends on.
4. If it is fork-local, say so in the commit message and list it under
   *Never upstream*.

## What to expect from upstream

Dormant, but not dead. As of August 2026:

| | |
| --- | --- |
| Open PRs | 7; the oldest from 2019, untouched since the day it was opened |
| Merged | #171 (Apr 2026), #166 (Jan 2025), #152 (Jan 2021) |
| Last push | 2026-04-28 |

So roughly one merged PR a year while outside contributions sit for years.
Someone still has commit rights and acts occasionally; the review pipeline
effectively does not function.

Plan accordingly: submit when it costs nothing, expect no answer, and never
hold anything here waiting for one.
