# Decision records

Why each change in this fork looks the way it does, and — more usefully — what
was considered and rejected. [divergence.md](divergence.md) describes *what*
each change does; this file records *why that shape and not another*.

Reference material, not required reading. Consult a record when you are about
to change the same area, or when an alternative rejected here starts looking
attractive again.

| | Decision | Status |
| --- | --- | --- |
| [ADR-001](#adr-001--keep-a-usable-master-and-cherry-pick-for-prs) | Keep a usable `master`, cherry-pick for PRs | accepted |
| [ADR-002](#adr-002--fix-the-vendored-zlib-in-place-rather-than-replacing-it) | Fix the vendored zlib in place | accepted |
| [ADR-003](#adr-003--raise-_posix_c_source-rather-than-declaring-vsnprintf) | Raise `_POSIX_C_SOURCE` | accepted |
| [ADR-004](#adr-004--prepend-the-bundled-blake2-include-path) | Prepend the bundled blake2 include path | accepted |
| [ADR-005](#adr-005--compile-the-cocoa-main-out-of-console-builds) | Compile the Cocoa `main()` out of console builds | accepted |
| [ADR-006](#adr-006--prefer-a-real-openal-keep-the-framework-as-fallback) | Prefer a real OpenAL, framework as fallback | accepted |
| [ADR-007](#adr-007--re-anchor-rpaths-to-absolute-paths-and-give-up-relocatability) | Re-anchor rpaths, give up relocatability | accepted, with cost |
| [ADR-008](#adr-008--mirror-the-windowssdl-split-for-mouse-dispatch) | Mirror the Windows/SDL split for mouse input | accepted |
| [ADR-009](#adr-009--hold-stdin-open-in-ci-instead-of-letting-the-engine-self-terminate) | Hold stdin open in CI | accepted |
| [ADR-010](#adr-010--pin-googletest-to-1100-and-fetch-the-sources-in-ci) | Pin googletest to 1.10.0 | accepted |
| [ADR-011](#adr-011--stage-packed-groups-next-to-the-binary-instead-of-installing-to-a-prefix) | Stage packed groups next to the binary | accepted |
| [ADR-012](#adr-012--install-libepoxy-in-ci-rather-than-making-headless_only-live-up-to-its-name) | Install libepoxy in CI | **superseded by ADR-014** |
| [ADR-013](#adr-013--add-mac-arm64-without-touching-mac-x86) | Add `mac-arm64` without touching `mac-x86` | accepted |
| [ADR-014](#adr-014--move-the-epoxy-include-behind-use_console-rather-than-dropping-the-file) | Move the epoxy include behind `USE_CONSOLE` | accepted |
| [ADR-015](#adr-015--route-c4group-failures-through-a-helper-and-leave-one-site-alone) | Route c4group failures through a helper | accepted |
| [ADR-016](#adr-016--register-tests-by-hand-and-keep-checking-the-ctest-manifest) | Register `tests` by hand, keep checking the manifest | accepted |
| [ADR-017](#adr-017--run-c4group-through-cmake--e-env-rather-than-moving-the-binaries) | Run c4group through `cmake -E env` | accepted |
| [ADR-018](#adr-018--exclude-msvc-from-the-pkg-config-branch-instead-of-fixing-the-branch) | Exclude MSVC from the pkg-config branch | accepted |
| [ADR-019](#adr-019--serialise-the-groups-target-rather-than-repair-maketempfilename) | Serialise `groups` rather than repair `MakeTempFilename` | accepted, defect live (#47) |
| [ADR-020](#adr-020--widen-the-mock-macros-instead-of-pinning-googletest-harder) | Widen the mock macros, move to googletest 1.14.0 | accepted; ceiling lifted by ADR-023 |
| [ADR-021](#adr-021--guard-the-resort-at-the-call-site-not-in-sortbylist) | Guard the resort at the call site | accepted |
| [ADR-022](#adr-022--put-the-cmake-floor-at-310-not-at-the-version-each-policy-needs) | CMake floor at 3.10 | accepted |
| [ADR-023](#adr-023--c17-now-and-not-c20) | C++17 now, not C++20 | accepted |
| [ADR-024](#adr-024--support-qt5-and-qt6-together-rather-than-replacing-one) | Support Qt5 and Qt6 together | accepted |

The table stopped being maintained after ADR-016 and was caught up on
2026-08-17. If you add a record, add its row.

---

## ADR-001 — Keep a usable `master` and cherry-pick for PRs

**Context.** The fork needs to be playable on this machine, and the changes
should eventually reach upstream. The conventional fork layout keeps `master` a
mirror of upstream and all work on topic branches, which makes PRs trivial but
means no single branch is actually usable.

**Decision.** `master` carries everything, including fork-local files. PR
branches are cut fresh from `upstream/master` and built by cherry-picking.

**Alternatives.**

- *Keep `master` as an upstream mirror.* Cleanest for PRs, but then playing the
  game means checking out an integration branch, and every new fix has to be
  merged into it by hand.
- *One long-lived branch per PR, master untouched.* Same problem, plus the
  branches drift apart and each needs its own rebases.

**Consequences.** Cherry-picking is a manual step at PR time, and the copies
must not be confused with the originals on `master` — reshaping a cherry-picked
commit is free, reshaping one already pushed to `master` needs a force-push.
Upstream has been near-dormant since 2021, so optimising for a usable local
`master` over PR ergonomics is the better trade. Mechanics in
[pr-plan.md](pr-plan.md).

---

## ADR-002 — Fix the vendored zlib in place rather than replacing it

**Context.** `src/zlib/zutil.h` carries a Classic Mac OS workaround that
defines `fdopen()` away to `NULL`. It fires on modern macOS because Apple's SDK
sets `TARGET_OS_MAC=1` and current SDKs pull `TargetConditionals.h` in through
`<stdio.h>`. Every compressed write then fails silently.

**Decision.** Guard the fallback with `!defined(__APPLE__)` and leave the rest
of the vendored copy alone.

**Alternatives.**

- *`#undef fdopen` in `gzio.c` after including `zutil.h`.* Smaller blast radius,
  but treats the symptom at one call site and leaves the trap armed for any
  other file that includes `zutil.h`.
- *Update the vendored zlib.* Impossible as such: `gzio.c` was **removed** from
  zlib in 1.2.4. This copy is a fork patched for Clonk's group format (new magic
  bytes, no transparent files), so there is nothing to update it to.
- *Use the system zlib's gzip layer.* Would mean reimplementing the group format
  changes on top of the modern `gz*` API — a real project, and a risky one for a
  format that has to stay byte-compatible with existing `.oc*` files.

**Consequences.** The fork keeps carrying a 2005 zlib snapshot. The guard cannot
affect any live platform, since Classic Mac OS has no `__APPLE__`-defining
compiler in use.

---

## ADR-003 — Raise `_POSIX_C_SOURCE` rather than declaring `vsnprintf`

**Context.** `gzio.c` requests `_POSIX_C_SOURCE 1` — POSIX.1-1990, older than
C99 — which hides `vsnprintf()` on Darwin. Current clang rejects implicit
function declarations, so the file no longer compiles.

**Decision.** Request `_POSIX_C_SOURCE 200809L`.

**Alternatives.**

- *Declare `vsnprintf` manually.* The file already has that pattern for `__MVS__`.
  It would work, but re-declaring a standard function to work around a
  self-inflicted feature-test level is backwards.
- *Delete the define entirely.* The comment says it is there for `fdopen()`.
  Removing it might work on Darwin — where the default exposes everything — but
  would change behaviour on stricter platforms for no gain.

**Consequences.** None expected: widening a feature-test macro exposes more
declarations, never fewer.

---

## ADR-004 — Prepend the bundled blake2 include path

**Context.** CMake appends interface include directories from linked targets
*after* directory-scoped ones, so `/opt/homebrew/include` shadows
`thirdparty/blake2`. Homebrew's libb2 declares `blake2b()` with a different
argument order, and `C4ScriptLibraries.cpp` fails to compile.

**Decision.** Prepend the bundled path for `libc4script` with
`target_include_directories(... BEFORE PRIVATE ...)`, taking the directory from
the blake2 target's own interface so it works for both the `ref` and `sse`
variants.

**Alternatives.**

- *Change the `#include` to a path like `blake2/ref/blake2.h`.* Would pin the
  source to one variant; the subdirectory picks `ref` or `sse` at configure
  time based on `HAVE_X86_64`.
- *Drop the vendored copy and use the system libb2.* Different API, so every
  call site would change, and it adds a dependency where a self-contained
  implementation already exists.
- *Stop putting `/opt/homebrew/include` on the global include path.* The right
  long-term fix, but it means auditing every `include_directories()` call in a
  1600-line `CMakeLists.txt`.

**Consequences.** Only affects include ordering for one static library. The
underlying fragility — a global include path that can shadow vendored headers —
remains for other targets.

---

## ADR-005 — Compile the Cocoa `main()` out of console builds

**Context.** On Apple, `OC_SYSTEM_SOURCES` substitutes `C4AppDelegate.mm` for
`ClonkMain.cpp`, and `openclonk-server` is built from that same list. Its
`main()` calls `NSApplicationMain()`, so the headless server parked itself in an
AppKit event loop before doing anything.

**Decision.** Wrap the Cocoa `main()` in `#ifndef USE_CONSOLE` and add
`ClonkMain.cpp` to the server target on Apple.

**Alternatives.**

- *Give the server its own source list.* Cleaner in principle, but
  `OC_SYSTEM_SOURCES` is shared by three targets and splitting it risks
  silently dropping platform files from one of them.
- *Detect console mode at runtime and skip `NSApplicationMain()`.* Would leave
  the whole Cocoa app delegate linked into a headless binary for no reason, and
  the decision is known at compile time.

**Consequences.** `C4AppDelegate.mm` still compiles into the server, just
without its entry point; most of its body is already behind `USE_COCOA`.

---

## ADR-006 — Prefer a real OpenAL, keep the framework as fallback

**Context.** `FindAudio` never sets `OpenAL_LIBRARIES` on Apple, so what got
linked was a hardcoded `-framework OpenAL` — OpenAL 1.1 from 2007, no EFX,
deprecated since 10.15 — while the headers came from whatever real
implementation was installed.

**Decision.** Look for a real implementation and prefer it; fall back to the
framework when none is found. `CMAKE_FIND_FRAMEWORK` has to be flipped for the
lookup, or `find_library` returns `OpenAL.framework` again.

**Alternatives.**

- *Require a real OpenAL and drop the framework.* Would break configuring for
  anyone without openal-soft installed, on a platform where the framework has
  been the default for fifteen years.
- *Leave it and just document the limitation.* The mismatch between openal-soft
  headers and Apple's library is exactly the kind of thing that works until it
  doesn't.

**Consequences.** Builds pick up openal-soft when present, which is why EFX is
now available. Whether Apple's framework produces sound at all on current macOS
was never established — see the note in divergence.md, this change was **not**
what fixed the silence.

---

## ADR-007 — Re-anchor rpaths to absolute paths and give up relocatability

**Context.** `tools/osx_bundle_libs` copies dylibs into the bundle and rewrites
their install names, but leaves `LC_RPATH` entries untouched. Entries relative
to `@loader_path` then point nowhere. Homebrew's SDL2 is sdl2-compat, which
finds SDL3 solely through such an rpath and `abort()`s with a modal dialog when
it cannot.

**Decision.** Resolve each relative rpath against the library's original
directory and write the result back as an absolute path, then add
`@loader_path` so bundled siblings win.

**Alternatives.**

- *Copy the `dlopen()`ed backend into the bundle too.* Tried; did not work. And
  it cannot be made general: the dependency is invisible to `otool`, so the tool
  has no way to discover what to copy.
- *Leave rpaths alone and document that bundles do not run.* The bundle was
  completely unusable, which is worse than not being relocatable.
- *Drop the bundling step for local builds.* What was done by hand before this
  fix, but it is a per-build manual step and leaves the tool broken.

**Consequences.** **The bundle is no longer relocatable** — absolute paths point
at the build machine's Homebrew. Acceptable because it did not start at all
before, but it means the tool no longer does what its name suggests for
distribution. Anything shipped would need the missing backends bundled first.

---

## ADR-008 — Mirror the Windows/SDL split for mouse dispatch

**Context.** The Cocoa handler forwarded mouse events only when the edit cursor
was in `C4CNS_ModePlay`. That mode is set exclusively by editor consoles, none
of which exist in a fullscreen build, so every event was dropped.

**Decision.** Dispatch straight to the GUI when not running as an editor, and
keep the edit-cursor condition for editor viewports — the structure Windows and
SDL already use.

**Alternatives.**

- *Call `SetMode(C4CNS_ModePlay)` at startup in non-editor builds.* One line,
  but it makes a fullscreen build carry editor state that means nothing there,
  and anything else reading `GetMode()` would silently change behaviour.
- *Drop the condition entirely.* Would change editor behaviour, which is
  untested here since Qt5 is unavailable and the editor is not built.

**Consequences.** Editor paths are untouched: when `Application.isEditor` is
true the original condition still decides. Editor mouse handling remains
unverified in this fork.

---

## ADR-009 — Hold stdin open in CI instead of letting the engine self-terminate

**Context.** `C4StdInProc::Execute()` calls `Application.Quit()` when reading
stdin fails. A `run:` step gets stdin from `/dev/null`, so the engine quit
before its state machine ticked once — silently, with exit code 0.

**Decision.** `tail -f /dev/null | openclonk-server …`, then kill the engine
after the run window.

**Alternatives.**

- *`sleep 90 | openclonk-server …`* — elegant, because closing stdin is the
  engine's *intended* shutdown path, and it is what
  `tests/start_all_scenarios.rs` does. Rejected because macOS does not honour it:
  locally the engine ignores stdin EOF and keeps running, so the step would hang
  until the job timeout.
- *A FIFO with a writer held open.* Equivalent, more moving parts, and the
  open-for-write blocks until a reader appears, which makes ordering matter.
- *`< /dev/zero`.* Never reaches EOF, but feeds an endless stream of NUL bytes
  into the engine's command parser.

**Consequences.** Teardown has to be forced, and getting it wrong hangs the job
rather than failing it. Two things bite:

- Without an stdin EOF to act on, the macOS build does not react to `SIGTERM`
  here, so the kill escalates to `SIGKILL` after a grace period.
- `tail` inherits the step's stdout and the runner waits for that pipe to close,
  so it must be killed explicitly.

The engine is killed rather than shut down cleanly, so the run no longer
exercises the shutdown path — which is the ironic cost of holding stdin open,
since that path *is* the stdin EOF.

---

## ADR-010 — Pin googletest to 1.10.0 and fetch the sources in CI

**Context.** `tests/CMakeLists.txt` compiles `gtest-all.cc` and `gmock-all.cc`
into the project, so it needs sources, not a packaged library. The version is
constrained from both ends: the tests use the arity-based `MOCK_METHOD1` and
`MOCK_METHOD2` macros that later releases dropped, and the project is
`CMAKE_CXX_STANDARD 14` with `STANDARD_REQUIRED ON`, which rules out 1.15+.

**Decision.** Pin 1.10.0 — it has both macro generations and is C++11 — and
download it in CI rather than vendoring it.

**Alternatives.**

- *1.7.0, as Travis used.* From 2013; unnecessary risk with a current clang when
  a later release works.
- *1.14.0, the newest that is still C++14.* Would require rewriting the mock
  declarations in `tests/` to the modern `MOCK_METHOD` form — a change to test
  code this fork otherwise does not touch.
- *Vendor it under `thirdparty/`.* Adds a few megabytes to the repository and a
  second copy to keep current, for something CI can fetch in seconds.
- *`FetchContent` in CMake.* Would work, but it means editing
  `tests/CMakeLists.txt`, which upstream would have to accept; keeping the
  choice in the workflow leaves the build files untouched.

**Consequences.** The version lives in the workflow's `GTEST_VERSION` and in
CLAUDE.md's local setup instructions — two places to update. Anyone building the
tests by hand must fetch the sources themselves.

Related: `ctest` only registers two of the three binaries, because `add_test()`
is called from the `create_test()` helper and the `tests` target does not use
it. All three are run explicitly rather than fixing that upstream quirk here.

---

## ADR-011 — Stage packed groups next to the binary instead of installing to a prefix

**Context.** `C4Reloc::Init()` prefers a `planet` folder next to the executable
and otherwise falls back to `OC_SYSTEM_DATA_DIR`, the install prefix. Upstream
never hit that fallback because Travis configured **in-source**, leaving the
binaries in the repository root beside `planet/`. An out-of-source build has
neither, and CI installs nothing.

**Decision.** Symlink the packed group files into `<build>/planet` before the
scenario run.

**Alternatives.**

- *`cmake --install` into a temporary prefix and run from there.* The most
  faithful to how a distribution deploys, and it would test the install rules
  too. Rejected as more moving parts for the same result: the prefix has to be
  fixed at configure time because it is baked into `OC_SYSTEM_DATA_DIR`.
- *Configure in-source, like Travis did.* Matches upstream exactly and needs no
  staging step, but pollutes the checkout and makes the two build directories
  used locally impossible.
- *Symlink the source `planet/` directory.* Simplest of all, but the engine
  would load the **unpacked** content and the run would no longer exercise what
  `c4group` produced — which is half the point of the step.

**Consequences.** The staging step encodes a detail of `C4Reloc`'s lookup order.
If that changes upstream the step breaks, hence the comment in the workflow. The
install rules remain untested.

---

## ADR-012 — Install libepoxy in CI rather than making `HEADLESS_ONLY` live up to its name

> **Superseded by [ADR-014](#adr-014--move-the-epoxy-include-behind-use_console-rather-than-dropping-the-file).**
> Kept because the reasoning for deferring was sound and the alternative it
> sketched turned out to be wrong.

**Context.** `HEADLESS_ONLY` claims to skip graphics dependencies, but on Apple
`OC_SYSTEM_SOURCES` pulls in `src/platform/C4AppMac.mm`, which includes
`epoxy/gl.h`. The macOS headless job failed on a clean runner.

**Decision.** Install libepoxy in CI and note the discrepancy.

**Alternatives.**

- *Exclude `C4AppMac.mm` from console builds*, the way [ADR-005](#adr-005--compile-the-cocoa-main-out-of-console-builds)
  excludes the Cocoa `main()`. Probably the right fix, and it would make the
  option honest. Deferred: the file provides more than the entry point, and
  working out what a console build still needs from it is a separate task with
  its own risk of breaking the GUI build.

**Consequences.** The option stays misleading on macOS.

---

## ADR-013 — Add `mac-arm64` without touching `mac-x86`

**Context.** `C4_OS` had one value for all of Apple, so arm64 builds announced
themselves as `mac-x86`. The string is not only cosmetic: it is the `platform=`
query parameter sent to the league and update servers.

**Decision.** Add an `__aarch64__` case and change nothing else.

**Alternatives.**

- *Also correct Intel to `mac-x86_64`, matching the Windows and Linux naming.*
  Consistent, and arguably what the value should always have been. Rejected
  because Intel Macs have always reported `mac-x86`; changing it alters what
  every existing build sends to servers that may key on it, with no way to test
  the consequences from here. For arm64 the question does not arise — no such
  build has ever existed, so nothing is registered under either name.
- *Leave it alone entirely.* The value would simply be wrong, and it is the only
  place the architecture is reported.

**Consequences.** Apple's two architectures now use different naming
conventions from each other (`mac-x86` for x86_64, `mac-arm64`). Ugly, and worth
explaining in the PR text.

---

## ADR-014 — Move the epoxy include behind `USE_CONSOLE` rather than dropping the file

**Context.** Resolves [ADR-012](#adr-012--install-libepoxy-in-ci-rather-than-making-headless_only-live-up-to-its-name).
`src/platform/C4AppMac.mm` included `<epoxy/gl.h>` unconditionally, so
`HEADLESS_ONLY` required a GL loader on macOS despite promising the opposite.

**Decision.** Move the include inside the `#ifndef USE_CONSOLE` guard the file
already has, and keep the file in the console build.

**Alternatives.**

- *Exclude `C4AppMac.mm` from console builds*, as ADR-012 proposed. **This would
  have been wrong.** Only lines 30–191 are guarded; below the guard sit four
  functions a console build genuinely links this file for — `IsGermanSystem`,
  `OpenURL`, `EraseItemSafe` and `C4AbstractApp::GetGameDataPath`, all plain
  Cocoa. Dropping the file would have produced four undefined symbols.
- *Move the four functions into a separate file.* Tidier separation, but it
  splits a platform file for the benefit of one include and makes the diff
  larger than the problem.

**Consequences.** `openclonk-server` no longer references a GL loader in any
translation unit, so the option is honest on macOS. CI asserts this directly by
grepping the compiler's own `.o.d` dependency files rather than relying on
libepoxy being absent from the runner — otherwise the check would pass for the
wrong reason on a runner that happens to have it.

The lesson worth keeping: ADR-012 guessed at the fix while deferring it, and the
guess was wrong in a way that would have broken the build. Deferring a fix is
fine; recording an untested guess as the likely solution is what misled.

---

## ADR-015 — Route c4group failures through a helper, and leave one site alone

**Context.** `iResult` in `C4GroupMain.cpp` was returned from `main()` but never
assigned, so c4group always exited 0. There are 27 places that print to stderr.

**Decision.** Add an `ErrorOut()` helper that prints and sets the flag, and use
it at the 26 sites that report a genuine failure. Leave the 27th — the
`"Status: %s"` line after a command runs — as a plain `fprintf`.

**Alternatives.**

- *Assign `iResult = 1` at each site.* Same number of edited lines, but every
  future error path has to remember the assignment. The helper makes the
  intent explicit and is the obvious thing to copy.
- *Mark all 27 sites.* What a mechanical reading of the code suggests, and it
  would have been wrong. The `"Status:"` line is guarded by
  `GetError() != "No Error"`, which reads like an error check but is not: a
  completed operation leaves the string as `""`, so it fires on success too.
  Confirmed empirically before touching it — a successful pack prints
  `Status: ` with an empty message and produces a valid 79-byte archive.
  Marking it would have made every successful pack exit non-zero and broken
  the build in a way that looks like the fix working.
- *Only mark the "Pack failed" paths*, the ones that caused the trouble.
  Narrower and safer, but it leaves the same trap armed for unpack, update
  generation and update application.

**Consequences.** Two sites are judgement calls: "Unknown option" and
"Error forking" previously warned and carried on. Both mean the tool did not do
what it was asked, so they now fail, but they are the ones most likely to
affect an existing workflow — called out in the commit message and in
[pr-plan.md](pr-plan.md) so a reviewer can object.

The excluded site now carries a comment explaining why it is not marked. Without
it, the next reader would see 26 converted calls and one `fprintf` and assume
an oversight.

---

## ADR-016 — Register `tests` by hand, and keep checking the ctest manifest

**Context.** `add_test()` is only called from the `create_test()` helper, and
the `tests` target uses a bare `add_executable`, so ctest covered two of the
three binaries. The CI worked around it by running all three explicitly.

**Decision.** Add a single `add_test(NAME tests COMMAND tests)` next to the
target, and switch CI to `ctest` — but assert first that all three binaries
appear in `ctest -N`.

**Alternatives.**

- *Make `tests` go through `create_test()`.* The obvious tidy-up, and it does
  not work as-is: the helper adds `main.cpp` and `TestLog.cpp` explicitly while
  `AUX_SOURCE_DIRECTORY` already globs them, so they would be compiled twice.
  The target also needs `C4SCRIPT_SOURCES` and `rt`/`winmm`, which the helper
  has no parameters for. Extending it for one caller is more code than the one
  line it would save.
- *Keep running the three binaries explicitly and leave CMake alone.* Works,
  but it means anyone using `ctest` — the obvious thing to reach for — keeps
  getting a false all-clear.
- *Trust ctest once registration is fixed.* Tempting, but the same defect can
  recur silently the moment a fourth binary is added. The manifest check costs
  four lines and turns a silent regression into a failed job.

**Consequences.** The assertion names the three binaries, so adding a fourth
requires editing the workflow. That is deliberate: it forces a decision about
whether the new binary belongs in the suite rather than letting it be forgotten.

Also worth knowing: `enable_testing()` is called from `tests/CMakeLists.txt`,
not the top level, so the manifest is in `build/tests` and
`ctest --test-dir build` finds nothing at all.

---

## ADR-017 — Run c4group through `cmake -E env` rather than moving the binaries

**Context.** On the Visual Studio generator the `groups` target died with
`MSB8066 … Code 9009` before packing anything: the pack command came out as a
bare `c4group.exe`. Two independent things have to be true for that. CMake
relativises the *command* of a custom build step against the directory of the
generated project file, and `oc_set_target_names()` puts the executables in
exactly that directory — so the path has nothing left to relativise. MSBuild
then runs custom build steps with `NoDefaultCurrentDirectoryInExePath` set,
which is the switch that stops `cmd.exe` resolving a bare name against the
working directory.

**Decision.** Invoke the packer as
`${CMAKE_COMMAND} -E env $<TARGET_FILE:c4group> …`. Only the command is
relativised, not its arguments; `cmake.exe` lives outside the build tree and so
keeps an absolute path, and the c4group path it is handed arrives intact.

**Alternatives.**

- *Just use `$<TARGET_FILE:...>`, the way the `APPLE` branch does.* The first
  attempt, and it fails identically — the generator relativises the resulting
  absolute path too and lands back on `c4group.exe`. Worth stating plainly
  because the `APPLE` branch four lines up makes it look like the obvious fix.
- *Stop moving the binaries into the build root.* This would make the defect
  disappear at the source: CMake would emit `RelWithDebInfo\c4group.exe`, which
  has a directory component and resolves fine. But `oc_set_target_names()` is
  deliberate fork behaviour, and every consumer of the layout — the CI job, the
  staging step, the documented paths in platforms.md — would move with it. A
  packaging convention is not worth rewriting to work around a quoting quirk.
- *Set `WORKING_DIRECTORY` on the custom command.* Might change what the path is
  relativised against, but it is unspecified which way CMake resolves the two,
  and it would silently depend on generator internals. `cmake -E env` states the
  intent in the command itself.
- *Wrap in `cmd /c`.* Windows-only, and the branch would have to be maintained
  against the three other generators that never had the problem.

**Consequences.** One extra process per group, on every platform, for a defect
that only exists on one. That is the price of not special-casing: 12 groups is
12 short-lived `cmake -E env` invocations and the packing itself dominates by
orders of magnitude. It also means Makefile and Ninja builds exercise the same
code path Windows does, so this cannot rot unnoticed the way a `if(MSVC)` branch
would.

---

## ADR-018 — Exclude MSVC from the pkg-config branch instead of fixing the branch

**Context.** `FindAudio.cmake` prefers `pkg_check_modules` whenever pkg-config
is present. That returns bare library names and reports their location in
`_LIBRARY_DIRS`, which the module never exports and no caller feeds to
`link_directories()`. MSVC cannot resolve a bare name, so the link dies with
`LNK1104` on a library that is installed and was found. It stayed hidden until
`qt5-base` dragged `pkgconf` into the vcpkg tree and flipped the branch under a
build that had been working.

**Decision.** Add `AND NOT(MSVC)` to the condition. The `else()` branch is
already written for `MSVC OR APPLE` and resolves absolute paths through
`find_library`; it simply loses a race it should never have been in.

**Alternatives.**

- *Export `Audio_LIBRARY_DIRS` and call `link_directories()`.* Fixes the real
  omission, and is wrong here anyway: `link_directories()` is directory-scoped
  and order-dependent, which is why CMake has spent a decade steering people
  away from it. It would also apply globally for one subsystem's benefit.
- *Resolve the pkg-config names to absolute paths with
  `find_library(... HINTS ${OpenAL_LIBRARY_DIRS})`.* The most correct fix, and
  it helps MinGW too. Rejected for scope: it rewrites the path Linux and macOS
  use daily, in a module nothing tests, to repair a platform that already has a
  working branch sitting unused. Worth doing if MinGW ever matters.
- *Leave it and document "do not install pkgconf".* Not viable — pkgconf arrives
  as a transitive dependency of a package you do want, and nobody will connect a
  Qt install to an OpenAL link error.

**Consequences.** MSVC now ignores pkg-config for audio even when a deliberate
pkg-config setup exists. Acceptable: on Windows the library layout comes from
vcpkg or a manual install, both of which `find_library` handles, and the branch
is the one this fork has verified — the `build-gui` engine plays sound with full
EFX through it.

The general defect stays live for any future MSVC-like toolchain that does find
pkg-config. It is recorded here rather than fixed so the next person meets a
decision instead of a mystery.

## ADR-019 — Serialise the `groups` target rather than repair `MakeTempFilename`

**Context.** Packing game data in parallel fails intermittently with
`Pack failed`, because `MakeTempFilename()` (`src/platform/StdFile.cpp:320`)
picks the lowest unused `<name>.NNN` and hands it back without claiming it. Two
`c4group` processes in the same directory choose the same temporary file. The
existing `USES_TERMINAL` guard only binds Ninja; Makefiles ignore it.

**Decision.** Chain each group's output onto the previous one, so the twelve
pack in sequence under every generator. Leave `MakeTempFilename` alone and
record why.

**Alternatives.**

- *Fix `MakeTempFilename` to claim the name.* The actual defect, and the fix
  that would make parallel packing work rather than merely stop being wrong.
  Rejected on blast radius. Two obvious repairs both fail:
  - *Create the file with `O_EXCL` after choosing it.* Wrong for several
    callers. `C4Group.cpp:434` passes the result to a directory creation, and
    others expect the path to be free, not to exist as an empty file. Making it
    right means auditing every one of the nine call sites in a function shared
    by the engine, `c4group` and the standalone script host.
  - *Put the process id in the name.* The `char*` overload could take it — the
    buffer is `_MAX_PATH_LEN`. The `StdStrBuf` overload cannot: it writes
    through `getMData()` into a buffer sized to the current string, so any name
    longer than `<base>.NNN` is a heap overflow. Two overloads, one of which
    silently corrupts memory if extended, is not a change to make in passing.
- *Drop `USES_TERMINAL` and document "do not use `--parallel` with `groups`".*
  Nobody reads that before typing the command they type for every other target,
  and the punishment is a corrupt group set that looks complete.
- *Give each group its own working directory.* Removes the collision without
  touching engine code, and costs a `WORKING_DIRECTORY` per command. Rejected
  because it leaves the underlying race intact while making it look solved —
  the next caller to run two `c4group` processes in one directory gets the
  original failure with no clue this ever happened.

**Consequences.** Packing stays serial everywhere, which costs about six
seconds in total and nothing at all on Ninja or macOS, where it already was.
The real defect stays live for any *other* concurrent `c4group` use, including
two developers' builds sharing a directory and anything the release scripts do.
Recorded rather than repaired, so the next person meets a decision instead of a
mystery — and if `MakeTempFilename` is ever fixed properly, this chaining can go
away with it.

Tracked as **#47** since this was written, so the root cause is no longer
findable only through a fork-local ADR and a closed issue. A third repair the
ADR did not cost — `mkstemp`/`GetTempFileName` per platform, with the
directory-creating caller split into its own helper — is probably the real
answer and is recorded there.

## ADR-020 — Widen the mock macros instead of pinning googletest harder

**Context.** googletest 1.10.0 stopped compiling on GCC 15 and later, which have
no transitive `<cstdint>`. The pin existed because the tests used
`MOCK_METHOD1`/`MOCK_METHOD2`, removed in 1.13.0, and because
`CMAKE_CXX_STANDARD 14` rules out 1.15+. The window had closed to nothing.

**Decision.** Rewrite the nine mocks to the variadic `MOCK_METHOD` and move CI
to 1.14.0.

**Alternatives.**

- *Patch the dependency — inject `-include cstdint` on the gtest target.* Two
  lines, no test changes, and it keeps every machine on the version it already
  has unpacked. Rejected: it makes the build system carry a workaround for a
  third-party bug fixed upstream years ago, keyed to a compiler version, and it
  would have to stay forever because nothing would ever prompt its removal.
- *Move to gtest 1.17, the version Arch packages.* Wants C++17. Raising
  `CMAKE_CXX_STANDARD` for the test targets alone is possible and is a much
  larger argument than this change deserves; raising it project-wide is a
  separate decision that should be made on its own merits.
- *Use the installed system googletest.* Not possible as the build is written —
  CMake compiles `gtest-all.cc` into the project, and a package ships headers
  and libraries only. That constraint is upstream's and is left alone.

**Superseded in part by [ADR-023](#adr-023--c17-now-and-not-c20).** The upper
bound described here was `CMAKE_CXX_STANDARD 14`, and the project moved to
C++17 on 2026-08-17, so 1.15+ is no longer excluded. Nothing needs it; the
reasoning below is kept as the record of why 1.14.0 was chosen at the time.

**Consequences.** The supported range is now 1.10.0 through 1.14.0 rather than
exactly 1.10.0, because the variadic macro predates the pin. macOS and Windows
keep working with the googletest they already have unpacked, and gain
`(override)` checking on nine mocks that had none. Only CI actually moves, and
1.14.0 has not yet run on either hosted runner.

## ADR-023 — C++17 now, and not C++20

**Context.** `CMAKE_CXX_STANDARD 14` capped googletest at 1.14.0 and ruled out
Qt6, which is the only route to an editor on current macOS. The roadmap treated
the bump as the pivot the rest of the plan was building towards, and budgeted a
build per platform to find out how bad it was.

**Decision.** 17, measured first. On Linux/GCC 16 with Qt5 it costs nothing:
headless, all four test binaries (101 passing) and the full client including the
Qt5 editor and `mape` all build, and `Movement.ocs` runs to 3 of 3 with the same
positions the C++14 build produces.

**Alternatives.**

- *Stay at 14 until something actually needs 17.* Something did: the googletest
  floor had already risen to meet the ceiling, leaving a window one release
  wide, and #50 cannot start at all without this.
- *Go to C++20 while touching the line.* This is the one that would have cost
  something. `throw()` is **deprecated** in C++17 and **removed** in C++20, and
  there are four of them in `C4DrawGL.h` and `StdMeshMaterial.h`. C++20 also
  brings much larger behaviour surface — the spaceship operator's effect on
  existing comparison operators, `char8_t`, aggregate initialisation changes.
  None of it is unmanageable and none of it is needed; taking 17 first means a
  failure in CI on the other two platforms points at one change rather than two.
- *Raise it only for the test targets, to unlock newer googletest.* Considered
  and rejected in ADR-020 as a much larger argument than that change deserved.
  It stays rejected for the opposite reason now: the project-wide bump turned
  out to be free, so the narrow version would have bought a split configuration
  for nothing.

**Consequences.** googletest has no upper bound any more; nothing needs one, so
all three machines stay on the 1.14.0 they have unpacked. #50 becomes possible.

CI went green on all six jobs at the first attempt, which extends the result to
clang across the engine, the app bundle and a client. The MSVC gap named here —
only `libmisc`, through `c4group` — closed a day later with the headless Windows
job (#30, `5198b01c6`), which builds the whole engine under C++17 on MSVC, runs
the unit tests and starts a scenario. The editor and the GUI client remain
uncompiled under C++17 anywhere but Linux.

## ADR-024 — Support Qt5 and Qt6 together rather than replacing one

**Context.** The editor asked for Qt5 by name. macOS has no Qt5 from Homebrew
any more, so `src/editor/` is compiled by nothing there (#46); Windows gets Qt5
from vcpkg and Linux packages both.

**Decision.** `find_package(QT NAMES Qt6 Qt5 ...)`, with `QT_VERSION_MAJOR`
carrying the choice through the resource macro, the link and the deploy helper.
Qt6 is preferred where both exist.

**Alternatives.**

- *Move to Qt6 and drop Qt5.* Simpler in the file, and it would break the two
  platforms that have a working editor today to fix the one that does not.
  vcpkg's Qt6 on Windows is a much larger dependency change than this port, and
  nothing forces it.
- *Keep Qt5 and wait for macOS to get it back.* Homebrew removed it; waiting is
  not a plan.
- *A `WITH_QT6` option.* An option that must be set correctly to get a working
  build is a worse version of asking CMake which one is installed.

**Consequences.** One `QT_VERSION` check survives, for
`QWidget::enterEvent`, whose signature changed rather than disappearing.
Everything else compiles unchanged against both. The Qt5 floor rises from 5.4 to
5.14 for `QWidget::screen()`, which is the replacement for the removed
`QApplication::desktop()` — 5.4 is from 2014 and both live installations are
5.15. CI builds both legs, because a supported version that nothing builds is a
version nobody knows about.

## ADR-021 — Guard the resort at the call site, not in `SortByList`

**Context.** `AppendEntry2StdFile` reads a `false` from
`SortByList(C4Group_SortList, ...)` as a write failure. That function returns
`false` for exactly one reason — no list was passed — so any embedder that
never called `C4Group_SetSortList()` cannot write a group containing a renamed
child group, and the failure arrives with an empty error string. See
[divergence 26](divergence.md#26-5a34e6eef--writing-a-group-failed-silently-without-a-sort-list).

**Decision.** Skip the whole resort block when `C4Group_SortList` is null.

**Alternatives.**

- *Make `SortByList` return `true` for a null list.* The tidier reading —
  nothing to sort is not a failure — and it fixes the caller without touching
  it. Rejected because the resort block does more than sort: it copies the
  group to a temporary file, opens it, closes it and erases it. With no sort
  list that entire round trip provably cannot change a byte. Fixing the return
  value would leave the pointless copy in place and make it look intentional.
- *Document that embedders must call `C4Group_SetSortList()` first.* This is
  what the code effectively requires today. Rejected: an unmet precondition
  that manifests as a silent write failure with a cleared error string is the
  worst available way to state a requirement, and the two callers that do it
  are both in this tree, so nobody outside would ever see the rule.
- *Refuse to open a group for writing without a sort list.* Turns a silent
  failure into a loud one, but a sort list is genuinely optional — reading and
  most writing do not need one — so this would reject correct programs.

**Consequences.** The engine and `c4group` are unaffected: both set a list, so
the guard is true wherever the branch used to run, which is why repacking the
game data produces the same twelve groups. The `Error` call stays, now covering
only a `SortByList` failure mode that does not currently exist. And the reason
this was found at all — a unit test driving C4Group as a library — is worth
keeping in mind for the rest of the class: the defect was old, cheap to fix,
and invisible to every program in the tree.

## ADR-022 — Put the CMake floor at 3.10, not at the version each policy needs

**Context.** `cmake_minimum_required (VERSION 3.5.1)` sat one patch version
above the compatibility CMake 4.0 removed outright, and CMake 4 warns on
anything below 3.10. Two blocks in `CMakeLists.txt` existed only to work around
older versions: a `try_compile` wrapper for pre-3.8 (CMP0067) and an IPO
fallback for pre-3.9 (CMP0069).

**Decision.** 3.10, and delete both workarounds plus a pre-3.6 warning in
`cmake/DeployQt.cmake` that the floor makes unreachable.

**Alternatives.**

- *3.8 or 3.9 — exactly what the policies need.* Cheapest defensible bump, and
  it still leaves the deprecation warning on every configure, which is the
  thing that prompted this. A floor nobody's toolchain is near should be picked
  by what the tooling asks for, not by the oldest thing that compiles.
- *Jump to a modern floor — 3.16 for `target_precompile_headers`, or 3.20.*
  Tempting while in the file, and this fork does want those eventually.
  Rejected for now: it is a claim about what every machine building this must
  have, made in advance of any feature needing it. Take it when something does.
- *Keep 3.5.1 and pass `-Wno-deprecated`.* Hides the one signal that will say
  when the floor stops working at all.

**Consequences.** 3.10 is from 2017 and older than the CMake on all three
development machines and all three CI runners, so nothing is cut off. It also
sets CMP0067 and CMP0069 to NEW by itself, which is what the deleted blocks did
by hand — the IPO check now runs unconditionally instead of behind
`if(POLICY CMP0069)`. Unrelated policy warnings (CMP0071 on the autogen path)
are untouched and still there.
