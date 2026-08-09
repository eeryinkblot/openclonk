# Upstream PR plan

How the commits on this fork's `master` group into pull requests against
`openclonk/openclonk`. Update this file whenever you add a commit that is meant
to go upstream eventually.

**This file is fork-local and must never appear in a PR branch.** So is
`CLAUDE.md` and everything else under `fork-notes/`.

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
git diff --name-only upstream/master..HEAD | grep -E '^(CLAUDE\.md|fork-notes/)' \
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
- The bundle's resource seal is stale afterwards (`codesign -v` complains)
  because game data is packed by a later POST_BUILD step. Not fixed here.
- `6333d5498` was found while chasing silent audio but was **not** its cause.
  Do not claim it fixes "no sound" — see `divergence.md`.

Consider splitting `6333d5498` off into its own PR; it is clean, while
`605abb61b` carries the caveats.

## Never upstream

| Commits | Files |
| --- | --- |
| `4d1e0ba08` | `CLAUDE.md` |
| — | `fork-notes/` |

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

## Status

Nothing has been submitted yet. The project has been near-dormant since 2021
(single-digit commits per year, CI dead since Travis shut down), so expect PRs
to sit. That is an argument for keeping this fork's `master` usable on its own
rather than waiting on upstream.
