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
