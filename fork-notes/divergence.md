# Divergence from upstream

Every change this fork carries on top of `openclonk/openclonk` master
(`36de79954`, 2026-04-28), with the reasoning behind it and what it actually
changes at runtime.

Most are macOS or Apple-Silicon specific, but not all: the c4group exit code
(11), the ctest registration (12), the MSVC guards (14) and zlib's include path
(15) are platform-independent or Windows-side. None touches game logic, the
script engine, or anything that could affect network synchronisation — the
91,026 lines of C4Script under `planet/` linked without a single warning on the
very first build.

Several announced themselves properly, as compile or configure errors. Many did
not. `c4group` reported success while writing zero-byte archives, the server sat
in an event loop without printing anything, the mouse discarded every event,
`ctest` passed while skipping its largest suite, and the audio path reported a
working device while the engine had sound switched off in its config. That is
the class of defect a dead CI cannot catch and only running the thing reveals.

A pattern runs through the build-system entries: every one of them is an option
that was never exercised on the platform where it matters. `HEADLESS_ONLY` had
never been used on macOS (1, 10), `C4GROUP_TOOL_ONLY` never on Windows (14, 15).

For how these group into pull requests, see [pr-plan.md](pr-plan.md).

---

## 1. `c02c12e38` — HEADLESS_ONLY could not configure on macOS

**Files:** `CMakeLists.txt`

### Motivation

The `if (APPLE)` block and the game-data packing loop referenced the `openclonk`
target unconditionally, but that target only exists when neither
`HEADLESS_ONLY` nor `C4GROUP_TOOL_ONLY` is set. Configuring failed outright:

```
CMake Error at CMakeLists.txt:1432 (target_link_libraries):
  Cannot specify link libraries for target "openclonk" which is not built
  by this project.
```

The option's own help text says it is "only tested with make/gcc/linux", so
this had presumably never been attempted on macOS.

A second defect hid behind the first: on Apple the packing loop attaches the
group files to `openclonk` as a POST_BUILD step, and the portable `groups`
target lives inside an `if(NOT APPLE)` branch. With no `openclonk` target there
was no way to pack game data at all.

### Technical effect

Guards the openclonk-specific statements with `TARGET openclonk`, and falls
back to the portable `OUTPUT`-based custom command plus the `groups` target
when that target is absent. A headless macOS build now configures, builds, and
can pack all 13 groups.

### Risk

None for existing configurations — every guarded statement is reached exactly
as before when `openclonk` exists.

---

## 2. `7d38493cf` — a system blake2.h shadowed the bundled one

**Files:** `CMakeLists.txt`

### Motivation

`thirdparty/blake2` propagates its include directory through the `blake2`
target's `INTERFACE_INCLUDE_DIRECTORIES`, which CMake appends *after* the
directory-scoped `include_directories()` entries. Any dependency contributing a
system include prefix therefore shadows it. With Homebrew, `/opt/homebrew/include`
lands first and libb2's `blake2.h` wins:

```c
ref/blake2.h  (bundled)  int blake2b(void *out, size_t outlen, const void *in, ...)
libb2         (system)   int blake2b(uint8_t *out, const void *in, const void *key, ...)
```

The argument orders are incompatible, so `C4ScriptLibraries.cpp` fails to
compile.

That failure is the lucky outcome. A system header that differed only in
semantics rather than in signature would have compiled cleanly and produced
wrong hashes — silently, in the function C4Script exposes for hashing.

### Technical effect

Prepends the bundled path for `libc4script` via
`target_include_directories(... BEFORE PRIVATE ...)`, resolved from the blake2
target's interface so it works for both the `ref` and `sse` variants the
subdirectory may have selected. On arm64 the `sse` variant is not built
(`HAVE_X86_64` fails), so `ref` is what gets used here.

### Risk

None. It only reorders include paths for one static library.

---

## 3. `a0d7e62cf` — gzio.c no longer compiled

**Files:** `src/zlib/gzio.c`

### Motivation

`gzio.c` is a vendored fork of zlib 1.2.3's gzip layer, patched for Clonk's
group format (zlib removed `gzio.c` in 1.2.4, so it cannot simply be updated).
It opens with:

```c
#define _POSIX_C_SOURCE 1 /* for fdopen() */
#define _BSD_SOURCE       /* for vsnprintf */
```

`_POSIX_C_SOURCE 1` selects POSIX.1-**1990**, older than C99 and therefore
older than `vsnprintf()`. On Darwin that hides the declaration in `<stdio.h>`,
and since clang stopped accepting implicit function declarations by default:

```
gzio.c:638:11: error: call to undeclared library function 'vsnprintf' with
type 'int (char *restrict, unsigned long, const char *restrict,
__builtin_va_list)'; ISO C99 and later do not support implicit function
declarations
```

`_BSD_SOURCE` is a glibc feature macro and does nothing on Darwin, but its
comment shows the declaration was always meant to be visible.

### Technical effect

Raises the request to `_POSIX_C_SOURCE 200809L`, which exposes both
`vsnprintf()` and the `fdopen()` the original comment cares about.

### Risk

Widening a feature-test macro can expose additional declarations, not hide
them. No behaviour change.

---

## 4. `d16b2b3d0` — c4group silently produced empty archives

**Files:** `src/zlib/zutil.h`

### Motivation

The most consequential of these, and the hardest to see. `zutil.h` carries a
workaround from zlib 1.2.3:

```c
#if defined(MACOS) || defined(TARGET_OS_MAC)
#  if defined(__MWERKS__) && ...
#    include <unix.h>
#  else
#    ifndef fdopen
#      define fdopen(fd,mode) NULL /* No fdopen() */
#    endif
#  endif
#endif
```

This targets **Classic Mac OS 9**, which genuinely had no `fdopen()`. Darwin
does — but Apple's SDK defines `TARGET_OS_MAC` to 1, and current SDKs pull
`TargetConditionals.h` in transitively from `<stdio.h>`, which older ones did
not. Verified on this machine: a three-line program including only `<stdio.h>`
reports `TARGET_OS_MAC = 1`.

So on current macOS every `fdopen()` in `gzio.c` becomes a literal `NULL`.

The failure is total and completely silent:

- `gz_open()` returns NULL with `errno` untouched, because no syscall was made
- `CStdFile::Create()` fails for every compressed file
- `C4Group::Save()` fails, and `Close()` calls `Clear()`, which resets the error
  string — so even `GetError()` returns empty
- `c4group` prints "Pack failed" and **exits 0**
- `cmake --build . --target groups` therefore reports success while writing
  zero-byte group files

This also explains why the timing changed: it worked on older SDKs and broke
without any change to OpenClonk.

### Technical effect

Restricts the fallback to non-Apple platforms (`#elif !defined(__APPLE__)`).
`c4group` packs correctly again; verified by a pack/unpack round-trip and by
producing all 13 game groups (~115 MB, no empty files).

### Risk

Classic Mac OS is not a supported target and has no `__APPLE__`-defining
compiler in use. The guard cannot affect any live platform.

### Not fixed here

`c4group` exiting 0 after "Pack failed" is a separate defect and still present.
It is why the build reported success. Worth a patch of its own.

---

## 5. `60f3fe708` — openclonk-server hung at startup on macOS

**Files:** `CMakeLists.txt`, `src/platform/C4AppDelegate.mm`

### Motivation

On Apple, `OC_SYSTEM_SOURCES` substitutes `C4AppDelegate.mm` for
`ClonkMain.cpp`, and `openclonk-server` is built from that same list. Its
`main()` was not guarded by `USE_CONSOLE`, so the headless server got the GUI
entry point and called `NSApplicationMain()`, parking itself in an AppKit event
loop before doing anything at all:

```
NSApplicationMain → [NSApplication run] → _DPSNextEvent → mach_msg2_trap
```

Even `openclonk-server --version` hung forever. The headless server has
therefore never worked on macOS. Consistent with CI history: the Travis macOS
job built and ran the unit tests but never started a scenario.

### Technical effect

Compiles the Cocoa `main()` out of `USE_CONSOLE` builds and adds
`ClonkMain.cpp` to the server target on Apple, matching every other platform.
The server now runs scenarios:

```
C4AulScriptEngine linked - 91026 lines, 0 warnings, 0 errors
Game started.  /  Game cleared.  /  Engine shut down.
```

### Risk

`USE_CONSOLE` is only defined for `openclonk-server` and `c4script`; the GUI
binary keeps the Cocoa entry point unchanged.

---

## 6. `6333d5498` — Apple's deprecated OpenAL was linked

**Files:** `CMakeLists.txt`, `cmake/FindAudio.cmake`

### Motivation

`FindAudio` never sets `OpenAL_LIBRARIES` on Apple — that happens only in the
MSVC branch — so what actually got linked was the hardcoded `-framework OpenAL`
in the `USE_COCOA` block. That framework is OpenAL 1.1 from 2007, has no EFX,
and has been deprecated since macOS 10.15, while the headers came from whatever
real implementation was installed. The engine reported the mismatch on every
start:

```
ALExt: No efx extensions available. Sound modifications disabled.
```

### Technical effect

Looks for a real implementation and prefers it, keeping the framework as a
fallback so machines without one still link. Frameworks are searched first on
Apple, so `CMAKE_FIND_FRAMEWORK` has to be flipped for the lookup or
`find_library` simply returns `OpenAL.framework` again — which it did on the
first attempt.

With openal-soft installed:

```
OpenAL extensions loaded. Available: AL_EFFECT_REVERB, AL_EFFECT_ECHO,
AL_EFFECT_EQUALIZER. Unavailable: (none).
```

### Important: this was not the cause of the silence

It was found while chasing "no sound", but the actual cause was `Sound=0` and
`Music=0` persisted in `~/Library/Preferences/org.openclonk.openclonk.config`
by earlier runs that had no audio support — the headless server, and the first
GUI build made before freealut was installed. The engine writes that setting
out permanently and never revisits it.

Volumes were fine the whole time, which is why nothing in the log pointed at
it. Both music and effects being silent should have pointed at a shared switch
in front of them rather than at the libraries.

Whether Apple's framework produces sound at all on current macOS was never
established — it was never tested with `Sound=1`.

### Related, not committed

Homebrew's `libalut` links Apple's OpenAL framework itself, so ALUT-created
buffers (`alutCreateBufferFromFileImage`, used for every WAV) would land in a
different OpenAL instance than the sources the engine plays. That was worked
around locally by rebinding a bundled copy of `libalut`; whether it is
audibly necessary was never established, so it is not in a commit.

---

## 7. `605abb61b` — bundles were unusable on Apple Silicon

**Files:** `tools/osx_bundle_libs`

### Motivation

Two problems, both fatal.

**Signatures.** `install_name_tool` invalidates an image's code signature. On
x86_64 that was cosmetic; on arm64 every image must carry a valid one, so the
kernel refused to map the binary and killed it during dyld's dependency walk,
before `main()`:

```
exception: EXC_BAD_ACCESS, SIGKILL (Code Signature Invalid)
Termination Reason: Invalid Page
```

The script calls `install_name_tool` in five places and never signed anything.

**rpaths.** `LC_RPATH` entries relative to `@loader_path` were resolved against
the library's original directory and point nowhere once it has been copied into
`Frameworks/`. Libraries that `dlopen()` their real backend depend on those
entries, and `otool` cannot see such a dependency, so the tool never bundles
the target either.

Homebrew ships no real SDL2 any more — it ships sdl2-compat, a shim over SDL3
which locates SDL3 solely through such an rpath:

```
@loader_path/../../../../opt/sdl3/lib
```

Out of the bundle it resolves nowhere, and sdl2-compat's constructor puts up a
modal `NSAlert` and `abort()`s. The symptom is a process at 0 % CPU that looks
hung; the stack shows `dllinit → error_dialog → [NSAlert runModal]`.

### Technical effect

Re-signs ad-hoc after every edit, nested images before the executable that
loads them, and re-anchors each relative rpath to where it used to resolve
before adding `@loader_path` so bundled siblings win. Resolution goes through
`pwd -P` and therefore follows Homebrew's symlinks — a purely lexical
resolution yields `/opt/opt/sdl3/lib` instead of the correct Cellar path.

Verified by deleting the bundle, rebuilding, and launching with no manual
intervention.

### Caveats

- **The bundle is no longer relocatable.** Absolute rpaths point at this
  machine. Making it self-contained would require bundling `dlopen()`ed
  backends, which this tool cannot discover. It was not startable at all
  before, so this is still a net gain.
- **The resource seal is stale.** Game data is packed into
  `Contents/Resources` by a later POST_BUILD step, so `codesign -v` reports
  "a sealed resource is missing or invalid". It does not stop the app from
  running — the kernel validates the Mach-O, not the resource seal — but it
  must be addressed before anything can be notarised. The fix is to move
  signing after all POST_BUILD steps, which is a CMake change and deliberately
  not mixed in here.

---

## 8. `72ce70252` — every mouse event was discarded in fullscreen

**Files:** `src/graphics/C4DrawGLMac.mm`

### Motivation

The Cocoa handler forwarded events to the GUI only when the edit cursor was in
`C4CNS_ModePlay`:

```objc
if (::MouseControl.IsViewport(viewport) &&
    Console.EditCursor.GetMode() == C4CNS_ModePlay)
        ::C4GUI::MouseMove(...);
```

`C4EditCursor::Default()` leaves `Mode` at `C4CNS_ModeEdit`, and the only two
callers of `SetMode(C4CNS_ModePlay)` in the entire tree are
`C4ConsoleQtState.cpp` and `C4ConsoleWin32.cpp` — both editor consoles, neither
present in a fullscreen macOS build. The mode never changes, the condition is
never true, and every mouse event is dropped. Mouse control has never worked on
macOS outside the editor.

The other backends keep the two cases apart; only Cocoa conflates them, because
it has one handler where they have two:

| Backend | Fullscreen | Editor viewport |
| --- | --- | --- |
| `C4WindowWin32.cpp` | `MouseMove(..., nullptr)`, unconditional (l. 187–201) | `IsViewport && ModePlay` (l. 417) |
| `C4AppSDL.cpp` | `MouseMove(..., nullptr)`, unconditional (l. 290–303) | — |
| `C4DrawGLMac.mm` | one handler, editor condition applied to both | |

### Technical effect

Restores the split: dispatch straight to the GUI when not running as an editor,
keep the edit-cursor condition for editor viewports. The existing
`Application.isEditor ? viewport : NULL` argument below already anticipated the
fullscreen case — it was simply unreachable.

### Risk

Editor behaviour is unchanged: when `Application.isEditor` is true the original
condition still decides. Editor mouse handling was not tested here, since Qt5
is no longer available from Homebrew and the editor is not built.

---

## 9. `05014153a` — arm64 builds announced themselves as mac-x86

**Files:** `src/platform/PlatformAbstraction.h`

### Motivation

`C4_OS` had one value for all of Apple, so an Apple Silicon build reported
`mac-x86`. Windows and Linux distinguish their architectures
(`win-x86_64`/`win-x86`, `linux-x86_64`/`linux-x86`); Apple did not.

The string is not merely cosmetic. It appears in the startup log and the CTCP
VERSION reply, but also as the `platform=` query parameter in:

| Call site | Compiled here? |
| --- | --- |
| `C4StartupNetDlg.cpp:875` — league server | **yes** |
| `C4StartupNetDlg.cpp:1274` — update server | no, inside `#ifdef WITH_AUTOMATIC_UPDATE` |
| `C4UpdateDlg.cpp:321` — update server | no, whole file is behind that option |

### Technical effect

Adds an `__aarch64__` case yielding `mac-arm64`, and nothing else.

### Why the existing value was left alone

Intel Macs have always reported `mac-x86` even when built for x86_64.
"Correcting" that to `mac-x86_64` would change what every existing Intel build
sends to servers that may key on it, potentially breaking their update lookup.
For arm64 the question does not arise — no such build has ever existed, so
nothing is registered under either name.

### Compatibility

None at risk. `C4_OS` is never compared anywhere in the tree — no `SEqual`, no
`strcmp`, no `==` — and is not part of the network protocol, any handshake, or
the sync check. Cross-platform play is unaffected.

### Not changed

The startup line reads `Version: 9.0-alpha mac mac-arm64`. The first `mac`
comes from `cmake/Version.cmake:43` appending it to `C4VERSION`, which
`C4Application.cpp:98` then logs next to `C4_OS`. Upstream looks the same on
every platform (`win win-x86_64`), so the redundancy was left as it is.

---

## 10. `1105b7e98` — HEADLESS_ONLY required a GL loader on macOS

**Files:** `src/platform/C4AppMac.mm`

### Motivation

The option promises to skip the graphics dependencies, but a headless build
failed on any machine without libepoxy:

    C4AppMac.mm:21:10: fatal error: 'epoxy/gl.h' file not found

`#include <epoxy/gl.h>` sat at the top of the file, outside the `#ifndef
USE_CONSOLE` guard that already excludes lines 30-191. Only that guarded region
— the windowing half — needs OpenGL.

### Technical effect

Moves the include inside the guard. The file stays in the console build,
because below the guard sit four functions the server genuinely links it for:
`IsGermanSystem`, `OpenURL`, `EraseItemSafe` and
`C4AbstractApp::GetGameDataPath`, all plain Cocoa.

It was the only translation unit in `openclonk-server` pulling in a GL loader,
so the option now lives up to its description. Verified from the compiler's own
dependency output rather than by trusting the build to fail:

    build/CMakeFiles/openclonk-server.dir/src/platform/C4AppMac.mm.o.d
      epoxy: 0 hits          (was the only .o.d referencing it)
      Cocoa: 1 hit           (control: the file is still compiled)

CI asserts the same property on every run, and its macOS headless job no longer
installs libepoxy at all.

### Risk

None for the GUI build: `USE_CONSOLE` is defined only for `openclonk-server`
and `c4script`, so `openclonk` still gets the include.

---

## 11. `c1a26fc7b` — c4group reported success after failing

**Files:** `src/c4group/C4GroupMain.cpp`

### Motivation

`iResult` was declared at file scope, returned from `main()`, and **never
assigned anywhere**. The tool therefore exited 0 no matter what happened.

This is what let the `fdopen` defect (see 4 above) go unnoticed for so long:
c4group printed "Pack failed" to stderr, returned success, and
`cmake --build . --target groups` reported a completed build while writing
zero-byte archives.

### Technical effect

Routes the 26 genuine failure reports through an `ErrorOut()` helper that
prints and sets the flag. Verified across success and failure cases:

| Invocation | Before | After |
| --- | --- | --- |
| successful pack / unpack | 0 | 0 |
| `-t` with no destination | 0 | 1 |
| missing group file | 0 | 1 |
| unknown command | 0 | 1 |
| all 13 game data groups | 0 | 0 |

### The one site left alone

The `"Status: %s"` line after a command runs is guarded by
`GetError() != "No Error"`, which looks like an error check but is not: a
completed operation leaves the error string as `""`, so it fires on success
too — a successful pack prints `Status: ` with an empty message. Marking it
would make every successful pack exit non-zero. It carries a comment saying
so, because it looks exactly like a site that was missed.

### A surprise worth knowing

Naming a group that does not exist is **not** an error: `ProcessGroup()` calls
`hGroup.Open(szFilename, true)`, and that second argument is `do_create`, so
c4group creates the file and legitimately exits 0.

This cost a CI round trip. The local check for "a missing group file must fail"
passed on macOS only because the path used was under a directory that does not
exist, so creation failed for an unrelated reason; on Windows the same test used
a writable directory, the file was created, and the assertion was simply wrong.
Negative tests here should use a usage error, which has no filesystem semantics
to get wrong.

### Risk

"Unknown option" and "Error forking" previously warned and carried on; both now
fail. They mean the tool did not do what it was asked, but they are the two
most likely to affect an existing workflow.

---

## 12. `9af77e8a3` — ctest silently skipped the largest test suite

**Files:** `tests/CMakeLists.txt`

### Motivation

`add_test()` is only called from the `create_test()` helper, and the `tests`
target is declared with a bare `add_executable`. ctest therefore knew only
`StdMeshMath` and `aul_test`, and a green run said nothing about 15 of the 72
tests — C4NetIO, C4Value, StdFile, the string table, unicode handling and
DirectExec.

### Technical effect

Registers the target by hand. It cannot go through `create_test()`:
`AUX_SOURCE_DIRECTORY` already picks up `main.cpp` and `TestLog.cpp`, which the
helper adds explicitly, and the target additionally needs `C4SCRIPT_SOURCES`
and `rt`/`winmm`.

    Test #1: StdMeshMath
    Test #2: tests
    Test #3: aul_test
    100% tests passed out of 3

Note the manifest lives in `build/tests`, not `build`, because
`enable_testing()` is called from `tests/CMakeLists.txt` rather than the top
level. `ctest --test-dir build` still finds nothing.

### Risk

None. Adding a test to the manifest cannot affect a build.

---

## 13. `35aa52f40` — the dead CI configuration

**Files:** `.travis.yml`, `appveyor.yml`, `tools/ci/`, `tools/generate_license_headers.cpp`

### Motivation

`.travis.yml` targets travis-ci.org, shut down in 2021. `appveyor.yml` pins
Visual Studio 2017. Neither has run in years and neither would run if
triggered. `tools/ci/` holds four PowerShell scripts referenced from
`appveyor.yml` and nowhere else.

`generate_license_headers.cpp` additionally shelled out to the `appveyor` CLI
to surface an error, because "Appveyor/msvc won't show stderr messages". Inert
outside that service, and there is no service.

### Technical effect

All removed; `.github/workflows/build.yml` replaces them. Verified with a full
rebuild plus a `groups` run, since the license tool is a build-time code
generator and a mistake there would break every binary.

### Risk

Deleting a CI config cannot affect a build. The only code change is in a
build-time tool, and `bail()` still prints to stderr and exits non-zero.

---

## 14. `b709b8c1b` — the MSVC block touched targets that may not exist

**Files:** `CMakeLists.txt`

### Motivation

Same defect as the Apple block (see 1 above), mirrored on Windows. The MSVC
section sets properties on `openclonk`, `openclonk-server` and `c4script`
unconditionally, but `HEADLESS_ONLY` drops the first and `C4GROUP_TOOL_ONLY`
drops all but `c4group`. Configuring with either fails:

    CMake Error at CMakeLists.txt:1402 (set_target_properties):
      set_target_properties Can not find target to add properties to

plus the same for the `/MANIFEST:NO` property and the precompiled header
block's `get_property()` on openclonk's sources.

### Technical effect

`oc_set_target_names()` returns early for a target that does not exist, which
covers all four of its call sites; the other two references are guarded
individually.

`C4GROUP_TOOL_ONLY` had evidently never been used on Windows, exactly as
`HEADLESS_ONLY` had never been used on macOS. Found by adding a Windows CI job.

### Risk

None for a full build, where all four targets exist and the guards never fire.

---

## 15. `0f2d43509` — zlib's include path was missing in one configuration

**Files:** `CMakeLists.txt`

### Motivation

`find_package(ZLIB REQUIRED)` runs unconditionally, but its include directory
was only added inside `if (NOT C4GROUP_TOOL_ONLY)`, bundled with JPEG's and
PNG's. zlib was therefore found and then never made reachable for the one
configuration that needs nothing else:

    src\lib\StdBuf.h(21,10): error C1083: Cannot open include file:
    'zlib.h': No such file or directory

`libmisc` includes `<zlib.h>` through `StdBuf.h`, so this affects every
configuration. It went unnoticed because `zlib.h` sits in a default include
path on Linux and macOS — only where a package manager puts it, as with vcpkg
on Windows, does the missing `-I` actually bite.

### Technical effect

The include directory moves next to the `find_package` call, outside the guard.

### Risk

None: it only adds an include path that every other configuration already had.

---

## 16. `4e9f0c3ec` — the app bundle's resource seal never matched

**Files:** `CMakeLists.txt`

### Motivation

Signing a bundle does two separate things: it signs the executable's code
pages, and it *seals the resources* — `Contents/_CodeSignature/CodeResources`
records a hash of every non-code file in the bundle.

`tools/osx_bundle_libs` signs as its own POST_BUILD step, and the game data is
packed into `Contents/Resources` by POST_BUILD steps registered later in
`CMakeLists.txt`. POST_BUILD commands run in registration order, so ~115 MB of
`.ocg`/`.ocd`/`.ocf` arrived after the seal was written:

    openclonk.app: a sealed resource is missing or invalid

The app runs anyway, which is why this survived: the kernel validates the
executable's code signature, not the resource seal. The seal matters to
Gatekeeper, to `codesign -v` and to notarisation.

### Technical effect

Adds a final POST_BUILD step after the packing loop that re-seals the bundle.
The fix is entirely one of ordering.

    codesign -v build-gui/openclonk.app   → exit 0
    CodeResources                         → 48 files, all 13 groups included

The app still starts with graphics, audio and music. CI now runs `codesign -v`
on the bundle instead of merely checking that a signature exists, so a
regression in the ordering fails the build.

### Risk

Signing is guarded by `find_program(codesign)`, so a cross-compiling host
without it is unaffected.

---

## 17. `1736155a4` — the version string repeated the platform

**Files:** `cmake/Version.cmake`

### Motivation

    Version: 9.0-alpha mac mac-arm64 (…)

`cmake/Version.cmake` appends a platform word to `C4VERSION`, and
`C4Application.cpp:98` logs that next to `C4_OS`, which already carries platform
*and* architecture.

### Technical effect

Drops the word. Verified first that the string is never compared anywhere:
network compatibility goes through `C4XVER1`/`C4XVER2`, checked in
`C4Network2.cpp:1246` as `C4XVER1*100 + C4XVER2`, so nothing can break.

What changes is display only — the log, the startup dialog, the c4group banner,
the crash message, and the HTTP User-Agent, which becomes `OpenClonk/9.0-alpha`
without a platform suffix. Where a server wants the platform it already receives
`C4_OS` as a separate query parameter.

### Risk

A deliberate divergence in presentation rather than a defect fix: upstream shows
the same redundancy on every platform. Recorded as such.

---

## 18. `b0ce04384` — the `groups` target could not run `c4group` under MSBuild

**Files:** `CMakeLists.txt`

### Motivation

`cmake --build build --target groups` fails on Windows before packing a single
group:

    Generating Arena.ocf
    Der Befehl "c4group.exe" ist entweder falsch geschrieben oder
    konnte nicht gefunden werden.
    Microsoft.CppCommon.targets(254,5): error MSB8066: ... Code 9009

Two things have to line up for this. CMake relativises the *command* of a custom
build step against the directory holding the generated project file, and
`oc_set_target_names()` — divergence 14's neighbourhood — puts the binaries in
exactly that directory, so the path collapses to a bare `c4group.exe`. MSBuild
then runs custom build steps with `NoDefaultCurrentDirectoryInExePath` set,
which is precisely the switch that stops `cmd.exe` from resolving a bare name
against the working directory. The executable sits right there and is still not
found.

Confirmed directly rather than inferred:

```powershell
cmd /c "cd /d build && c4group.exe"                                   # found
cmd /c "cd /d build && set NoDefaultCurrentDirectoryInExePath=1 && c4group.exe"
# Der Befehl "c4group.exe" ist entweder falsch geschrieben oder ...
```

Makefiles and Ninja substitute an absolute path, which is why this never showed
on macOS or Linux. Note that the `APPLE` branch a few lines above already writes
`$<TARGET_FILE:...>` — but on its own that is *not* enough here: the generator
relativises the resulting absolute path too, and it collapses to the same bare
filename. That was the first attempt and it failed identically.

### Technical effect

The pack step goes through `${CMAKE_COMMAND} -E env`. Only the command is
relativised, not its arguments, and `cmake.exe` lives outside the build tree, so
it keeps an absolute path and the `$<TARGET_FILE:c4group>` handed to it survives
intact:

    COMMAND "${CMAKE_COMMAND}" ARGS -E env "$<TARGET_FILE:${native_c4group}>" ...

### Risk

Low, but it is one extra process per group on every platform. The alternative of
leaving the binaries in `build/<Config>/` — where CMake would emit a path with a
directory component — would undo a deliberate fork behaviour and change where
every other step looks for the executables, including the CI job. Cross-compiled
builds keep working: `native_c4group` is then an imported target, and
`$<TARGET_FILE:...>` resolves imported targets the same way.

---

## 19. `5cec4133b` — installing an unrelated package broke the audio link

**Files:** `cmake/FindAudio.cmake`

### Motivation

Installing `qt5-base` to build the editor made a previously working GUI build
fail to link, in a target with nothing to do with Qt:

    LINK : fatal error LNK1104: Datei "OpenAL32.lib" kann nicht geöffnet werden.

The library was installed and had been found. Comparing the generated project
against the working build shows the difference:

    build-gui   C:\...\vcpkg\installed\x64-windows\lib\OpenAL32.lib
    build-qt    OpenAL32.lib

`FindAudio.cmake` picks its strategy on whether pkg-config exists:

```cmake
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND AND NOT(APPLE))
    pkg_check_modules(OpenAL "openal>=1.13")
```

`qt5-base` pulls in `pkgconf`, so `find_package(PkgConfig)` starts succeeding
and the module switches from the `else()` branch — written for `MSVC OR APPLE`,
resolving absolute paths through `find_library` — to `pkg_check_modules`, which
reports bare library names and puts their location in `OpenAL_LIBRARY_DIRS`.
That variable is never exported: the module sets only `Audio_LIBRARIES` and
`Audio_INCLUDE_DIRS`, and nothing calls `link_directories()` with the dirs. On a
Unix linker the bare names would still resolve from default paths; MSVC has no
such fallback.

Worth dwelling on the failure mode: a working configuration silently flips
because an unrelated package appeared in the dependency tree, and the error
surfaces at link time in another subsystem. Nothing about audio changed.

### Technical effect

`AND NOT(MSVC)` in the branch condition, so MSVC uses the branch already written
for it.

### Risk

Low, and deliberately narrow. It only changes behaviour where the alternative is
a hard link error, and it routes MSVC to a code path this fork has already
exercised end to end — the `build-gui` engine plays sound with full EFX
(`Available: AL_EFFECT_REVERB, AL_EFFECT_ECHO, AL_EFFECT_EQUALIZER.
Unavailable: (none).`), which comes from exactly that branch.

Not fixed the more general way — resolving the pkg-config names to absolute
paths via `find_library(... HINTS ${OpenAL_LIBRARY_DIRS})` — because that would
rewrite the path Linux uses and currently works, to fix a platform that has a
working branch already. See [ADR-018](decisions.md).

---

## 20. `83b23b4b6` — mape's crash handler had no signal number

**Files:** `src/mape/mape.c`

### Motivation

`mape` does not compile on GCC 15 or later:

    src/mape/mape.c:135:25: error: passing argument 2 of 'signal' from
        incompatible pointer type [-Wincompatible-pointer-types]
      135 |         signal(SIGSEGV, segv_handler);
          |                         ^~~~~~~~~~~~
          |                         void (*)(void)
    /usr/include/signal.h:88:57: note: expected '__sighandler_t'
        {aka 'void (*)(int)'} but argument is of type 'void (*)(void)'

`segv_handler` is declared `static void segv_handler()`. Up to C17 an empty
parameter list meant "unspecified", and the conversion to `__sighandler_t` went
through with at most a warning. C23 redefined it as `(void)`, and GCC 15 made
C23 the default. Nothing about the handler changed; the language under it did.

The project sets no `CMAKE_C_STANDARD`, so the C sources take whatever the
compiler defaults to — which is exactly how this arrives unannounced.

### Technical effect

The handler takes its `int`. Unused, as before, but the type now matches.

### Risk

None worth naming. `mape` is Linux-only (it needs GTK3), which is why neither
the macOS nor the Windows machine could have found this.

Same shape as [3](#3-a0d7e62cf--gzioc-did-not-compile-on-modern-clang): an old C
source that only ever compiled by the grace of a lenient default, breaking the
moment a current toolchain is pointed at it. Expect more of these, and expect
them in C rather than C++.

---

## 21. `0ce1ea716` — the `groups` target raced against itself on Makefiles

**Files:** `CMakeLists.txt`

### Motivation

`cmake --build build --target groups --parallel N` fails on a Makefile
generator:

    Status:
    Pack failed
    make[3]: *** [CMakeFiles/groups.dir/build.make:115: Objects.ocd] Error 1
    make[3]: *** Deleting file 'Objects.ocd'

Intermittently, and by preference on `Objects.ocd` — at 29 MB the largest
group, so the one still running when the others start.

The custom command already carried the property meant to prevent this:

```cmake
USES_TERMINAL # Hack: prevent parallel execution (for ninja), c4group tends to fail otherwise
```

`USES_TERMINAL` puts a Ninja job in the console pool, which takes one job at a
time. Makefiles ignore the property entirely. So the protection existed only on
the generator that upstream's authors happened to use, and the comment says as
much without drawing the conclusion.

Why concurrent `c4group` is unsafe at all: `MakeTempFilename()`
(`src/platform/StdFile.cpp:320`) scans for the lowest unused `<name>.NNN` and
returns it **without claiming it** —

```cpp
do { cnum++; osprintf(fn_ext,"%03d",cnum); }
while (FileExists(szFilename) && (cnum<999));
```

— so two processes in the same directory pick the same temporary file. A
textbook check-then-use gap, in a function that has no way to report one.

The failure mode is the interesting part. It leaves a complete-looking eleven
groups of twelve behind, and the engine reports it much later as

    FATAL ERROR: Required object file Objects.ocd not available.

which reads like a staging mistake rather than a packing one.

### Technical effect

Each group's `OUTPUT` gains a dependency on the previous group's output, so the
twelve pack in sequence under any generator. `USES_TERMINAL` stays: it is
harmless, and it keeps Ninja's output readable.

The Apple `POST_BUILD` path is untouched — commands attached to a target already
run in order.

### Risk

Low. Packing all twelve groups takes about six seconds; it was already serial on
Ninja and on macOS, so the only configuration that loses parallelism is the one
that was producing wrong results with it.

Not fixed at the source — see
[ADR-019](decisions.md#adr-019--serialise-the-groups-target-rather-than-repair-maketempfilename).

---

## 22. `3207274f2` — a system gtest include path outlived being given the sources

**Files:** `tests/CMakeLists.txt`

### Motivation

`find_path(GTEST_INCLUDE_DIR)` and its GMock counterpart ran unconditionally, so
a configure that found no googletest *sources* still cached the system headers.
That is `/usr/include` on any machine with a googletest package installed —
gtest 1.17 on Arch.

It is also the configure CLAUDE.md tells you to run first, without
`GTEST_ROOT`. `find_path` never revisits a cached value, so adding
`-DGTEST_ROOT=` to that same build directory afterwards changes the *sources*
and leaves the *headers* where they were. `gtest-all.cc` 1.10.0 then compiles
against gtest 1.17 headers:

    gtest.cc:5950: error: 'kDeathTestStyleFlag' was not declared in this scope
    gtest-port.cc:737: error: 'StrDup' is not a member of 'testing::internal::posix'
    gtest-port.cc:1289: error: 'Int32' has not been declared

Nothing in a page of those points at the include path, the configure cheerfully
reports GTest as found, and deleting the build directory is the only cure.

Latent everywhere, invisible on the two machines that met it first: Homebrew's
`googletest` was never installed on the macOS one, and vcpkg does not put
headers on a default search path.

### Technical effect

The `find_file` for the sources moves above the `find_path` for the headers, and
the header search is guarded by it. The headers can now only come from the tree
the sources came from. The source search already had `NO_DEFAULT_PATH`, so its
own result was never at risk.

### Risk

Low, and it narrows rather than widens. A layout that splits sources from
headers — Debian's old `/usr/src/gmock` with headers in `/usr/include` — still
resolves, because `GMOCK_ROOT` is found first and the system path is still
reachable from the guarded search.

---

## 23. `8722c249b`, `753b74a59` — googletest 1.10.0 stopped compiling

**Files:** `tests/TestLog.h`, `tests/aul/ErrorHandler.h`,
`.github/workflows/build.yml`

### Motivation

googletest 1.10.0 does not build on GCC 15 or later. `gtest-death-test.cc` and
`gtest-port.cc` use `uintptr_t` and `Int32` without including `<cstdint>`, and
the compiler stopped supplying it transitively:

    gtest-death-test.cc:1385:26: error: 'uintptr_t' does not name a type

Not fixable in this repository — it is the dependency's source. The way out is a
newer googletest, and the pin was load-bearing in both directions: the tests
used `MOCK_METHOD1`/`MOCK_METHOD2`, removed in 1.13.0, while
`CMAKE_CXX_STANDARD 14` with `STANDARD_REQUIRED ON` rules out 1.15+, which wants
C++17. 1.10.0 was the only version satisfying both, and it had just stopped
working.

### Technical effect

The nine mocks move to the variadic `MOCK_METHOD`, which **already exists in
1.10.0** (`gmock-function-mocker.h:42`). So this is a widening, not a bump:
every version from 1.10.0 to 1.14.0 now works, and no other machine has to move
in step. CI's `GTEST_VERSION` goes to 1.14.0, the newest release still inside
the C++14 bound.

All seven `TestLog` methods and both `C4AulErrorHandler` methods are pure
virtual in their bases, so the new form also carries `(override)` — which the
arity macros could not express, and without which a signature drifting away from
the base would quietly produce a new function instead of a failed build.

One detail the version bump needs: the tag naming changes at 1.11, from
`release-1.10.0` to `v1.14.0`, and the extracted directory follows the tag.

### Risk

Low for the mocks — the macro is older than the pin, so the change is
compatible in both directions. The CI bump is the part that moves: 1.14.0 has
never run on `ubuntu-latest` or the macOS runner here, only locally on GCC 16,
where all 72 pass.

---

## 24. `95a1d8094` — a scenario used a variable outside its declared scope

**Files:** `planet/Tests.ocf/LiquidContainer.ocs/Script.c`

### Motivation

The scenario links with two warnings:

    WARNING: variable 'test' used outside of its declared scope
        (in Global.Test5_CheckPipes, LiquidContainer.ocs/Script.c:540:17)
        [variable_out_of_scope]
    ... and the same at 541:115

`var test` was declared inside `if (pipeA != nil)` and used again inside
`if (pipeB != nil)`. C4Script variables are function-scoped, so it works — which
is precisely why the engine warns: the code reads as though it would not.

Found by running the scenario. Nothing had, so nothing had ever seen the
warning. It is also exactly what `tests/start_all_scenarios.rs` was written to
surface in 2018 (#52).

### Technical effect

The declaration moves to function scope, next to `functionA`/`functionB`, where
this function's other locals already are.

### Risk

None. The generated code is identical — function-scoped either way — so this
changes what the engine says about the file, not what it does.

Worth keeping rather than suppressing: the CI scenario suite asserts
`0 warnings, 0 errors` per scenario, and that assertion is only worth having if
the tree satisfies it.

---

## 25. `9ce177d99` — a packed directory's temp filename was read after its scope ended

**Files:** `src/c4group/C4Group.cpp`

### Motivation

`C4Group::AddEntryOnDisk` declares the buffer holding the temporary name inside
the `if (DirectoryExists(filename))` block:

```cpp
if (DirectoryExists(filename))
{
    char temp_filename[_MAX_PATH_LEN];
    ...
    filename = temp_filename;
    move = true;
}

bool fIsGroup = !!C4Group_IsGroup(filename);   // reads it
int size = ... UncompressedFileSize(filename); // reads it
is_executable = (access(filename, X_OK) == 0); // reads it
return AddEntry(..., filename, ...);           // stores it
```

`filename` is pointed at a local that ceases to exist one line later, and four
statements then read through it. That is undefined behaviour rather than a
style problem, and it is not a rare path: every directory any `groups` build
packs goes through it, which is every nested `.ocd` in the game data.

Found by reading, not by a failure. It works today because nothing else claims
that stack slot between the block's end and the reads — an accident the
compiler is free to stop granting at any optimisation level.

### Technical effect

The declaration moves one block up, with a comment saying why it is there.
Nothing else changes; the same bytes end up in the same entry.

### Risk

None. The buffer is `_MAX_PATH_LEN` on the stack either way, and its lifetime
only grows.

---

## 26. `5a34e6eef` — writing a group failed silently without a sort list

**Files:** `src/c4group/C4Group.cpp`

### Motivation

`AppendEntry2StdFile` resorts a child group whenever its entry name differs
from the file backing it on disk — which is every child group added under a
name of its own, so every `Add(directory, name)` and every nested group in a
pack. It copies the group aside, opens the copy, and calls
`SortByList(C4Group_SortList, entry->FileName)`, treating `false` as fatal.

`SortByList` returns `false` for exactly one reason: no list was passed. It has
no other failure path. So a program that never called `C4Group_SetSortList()`
cannot write a group holding a renamed child at all — `Save` aborts, `Close`
returns false, and `Clear` resets the error string on the way out, so
`GetError()` is empty afterwards. Same silent shape as the empty archives in
[4](#4-d16b2b3d0--c4group-silently-produced-empty-archives), and for the same
underlying reason: a write path that reports failure only through a return
value nobody is positioned to see.

Only `C4Application` and `C4GroupMain` set a sort list, so the engine and
`c4group` never meet this. The first thing that did was the unit test added in
`c0c6d8639` — the defect was found the first time C4Group was driven as a
library rather than as part of a program that knows to configure it.

### Technical effect

The resort is skipped when there is no sort list, which also drops a
copy-open-erase round trip that could not have changed anything. The `Error`
call stays for a `SortByList` that fails for a reason that does not exist yet.

### Risk

None for the engine or `c4group`: both set a list, so the condition is true
where it was true before and the branch is unchanged. Verified by repacking all
twelve game groups and running `Movement.ocs` against the result.

---

## 27. `443690c41` — the CMake floor was below what CMake still supports

**Files:** `CMakeLists.txt`, `cmake/DeployQt.cmake`

### Motivation

`cmake_minimum_required (VERSION 3.5.1)` sat one patch version above the
compatibility CMake 4.0 removed outright, and every configure on a current
CMake said so:

    CMake Warning (deprecated) at CMakeLists.txt:14 (cmake_minimum_required):
      Compatibility with CMake < 3.10 will be removed from a future version of
      CMake.

Two blocks existed only for versions below that floor: a `try_compile` wrapper
for pre-3.8 (CMP0067), carrying a comment asking to be deleted at exactly this
bump, and an `if(POLICY CMP0069)` guard whose else branch warned that CMake was
too old for IPO. `cmake/DeployQt.cmake` had a third, a warning about MSVC 2015
needing CMake 3.6.

### Technical effect

The floor moves to 3.10, which sets both policies to NEW by itself, and all
three workarounds go. The IPO check now runs unconditionally. Why 3.10 and not
higher is ADR-022.

### Risk

None reachable. 3.10 is from 2017 and older than the CMake on all three
development machines and all three CI runners. The unrelated CMP0071 policy
warning on the autogen path is untouched and still appears.

---

## 28. `013d76873` — `Movement.ocs` test 3 asserted a pre-2019 rock position

**Files:** `planet/Tests.ocf/Movement.ocs/Script.c`, `.github/workflows/build.yml`

### Motivation

The scenario's third test launches a rock diagonally at high speed and asserted
`GetX() > 380`. It lands at 372 or 374 and has for as long as anyone here has
run it, on all three platforms.

The history says which side is wrong. The test was added 2019-03-13
(`3fa5d8beb`); on 2019-06-22 the same author merged `0315ea6ef`, *Improved
movement code*, whose own message reads: "This also slightly changes how objects
with high velocity behave when colliding with the landscape, which may break
scenarios that rely on this specific behaviour." Test 3 is a rock thrown into
the landscape at high velocity.

Confirmed by building today's tree with only `C4Movement.cpp` reverted to the
pre-merge revision: the rock then lands at `[482, 157]` and test 3 passes —
while test 2 fails, its clonk stopping at x = 327 rather than 394. The merge
moved both tests; only test 3 was left behind.

### Technical effect

The assertion becomes `Inside(GetX(), 360, 390)`, and the CI pin for the
scenario drops from 1 expected failure to 0.

A range rather than a corrected bound, because **a scenario run is not
reproducible**. `RandomSeed = time(nullptr)` for a local game
(`src/game/C4Game.cpp:341`), and the seed reaches this result: twenty runs
produced exactly two outcomes, `[372, 157]` and `[374, 158]`, ten each. Ten
fixed seeds reproduce the same split, and a fixed seed repeats its own outcome
exactly — so the engine is deterministic and the scenario is not. `360-390`
holds across both states, stays clear of the launch position at 320 and of the
pre-2019 result at 482, and leaves margin for the two platforms sampled once
each.

That variance is a fact about the whole suite, not about this test: any
scenario assertion on a position needs a range, and any position that
reproduced twice is not thereby established.

### Risk

The scenario is content, not engine, so the blast radius is one test. The
remaining risk is the range being wrong on a platform where the two states
differ from the two seen here — which is why the margin is twelve pixels rather
than two.

---

## 29. `bc55e8f42`, `ed28194a1` — the content lint could not be started, and never gated

**Files:** `tests/Cargo.toml`, `tests/Cargo.lock`, `tests/start_all_scenarios.rs`,
`tests/scenario-lint-expected.txt`, `.gitignore`

### Motivation

`tests/start_all_scenarios.rs` runs every scenario under `planet/` and reads the
`C4AulScriptEngine linked - N lines, N warnings, N errors` line the engine
prints — a compiler pass over the game content. It is the only thing that ever
loaded the 69 real game scenarios in Missions, Worlds, Arena, Parkour, Defense
and Tutorials. Written in 2018, one commit, never touched again.

Two reasons it did nothing for eight years:

- **It could not be started.** The file carries
  `#!/usr/bin/env run-cargo-script` and a cargo-script dependency header.
  cargo-script has been unmaintained since roughly then and does not install on
  a current toolchain, so running the tool meant hand-building a crate around
  it first.
- **It could not fail.** It printed the counts it captured and never inspected
  them; the only `panic!` is for failing to spawn the engine. A human had to
  read 99 lines of output and notice. Nobody did — the `variable_out_of_scope`
  warning fixed in `95a1d8094` is precisely what it was built to surface, and it
  sat there from 2018.

### Technical effect

`tests/Cargo.toml` states the same five dependencies as an ordinary manifest,
with a committed `Cargo.lock`, so `cargo build --manifest-path tests/Cargo.toml`
works with nothing installed beyond cargo. The versions are unchanged; they are
old and they still build, and modernising them is a separate decision.

`--expect <file>` turns the tool into a check. `tests/scenario-lint-expected.txt`
pins a warning and error count per scenario, and deviation fails in **both**
directions — the same rule the CI scenario suite uses, for the same reason:

    FAIL Tests.ocf/Benchmarks.ocs: 1 warning, 0 errors; pinned at 0 and 0.
    FAIL Tests.ocf/SkeletonAppend.ocs: 2 warnings, 0 errors; pinned at 99 and 0.
         Something was fixed -- lower the pin in the same commit.
    FAIL Missions.ocf/New.ocs: not in <file>. Add it with the counts it reports.
    FAIL Tests.ocf/Gone.ocs: pinned in <file> but no such scenario was found.

All four were exercised by hand against a doctored expectations file.

The shebang and cargo-script header stay: rustc skips the first line, and they
record how the tool was meant to be run.

### Risk

None to the engine — no C++ changes. The risk this carries is a **stale pin
file**: 99 lines that have to move when content moves. That is deliberate, and
the "not in this file" failure is what keeps new content from arriving
unnoticed.

### What it found

Seven scenarios are not clean, not four, and one of the three additions was
invisible for a reason worth recording. `Tests.ocf/ColorfulLights.ocs` reports
**10** warnings — nine globals declared in both `Script.c:10-11` and the
generated `Objects.c:3`, plus a deliberate assignment inside an `if` — and the
earlier survey missed it because `grep -v "0 warnings, 0 errors"` also drops
`10 warnings, 0 errors`. Any count ending in a zero would have gone the same
way, in a survey specifically looking for warnings.

The other two, `CableCars.ocs` and `LiquidSystem.ocs`, were counted as
non-reporting rather than unclean: against a *packed* tree they cannot start,
because `Experimental.ocd`, `Experimental.ocf`, `Tests.ocf` and `Issues.ocf` are
deliberately not in `OC_C4GROUPS`, and `CableCars.ocs` needs a definition nested
inside another scenario in `Experimental.ocf`. The lint therefore runs against
the unpacked tree, which costs nothing: the 97 that run either way report
identical counts, so packing does not affect script linking.

---

## 30. `fb3ec7b9c` — the project was still C++14

**Files:** `CMakeLists.txt`, `.github/workflows/build.yml`

### Motivation

`CMAKE_CXX_STANDARD 14` had stopped being free. It capped googletest at 1.14.0
— a window one release wide, since 1.10.0 no longer compiles on GCC 15 — and it
ruled out Qt6, which is the only route to an editor on current macOS (#46, #50).
The roadmap treated this as the pivot everything else was preparing for and
budgeted a build per platform to find out how bad it was.

### Technical effect

One line, 14 to 17. Measured before it was changed, on Linux with GCC 16.2.1
and Qt5:

| | |
| --- | --- |
| headless (`openclonk-server`, `c4group`, `c4script`) | 0 errors |
| `tests aul_test StdMeshMath determinism` | built, 101 of 101 pass |
| full client, including the Qt5 editor and `mape` | builds |
| `Movement.ocs` | 3 tests, 0 failed |

The scenario is the interesting row: the rock lands at `[372, 157]` and the
clonks at `[390, 149]` and `[394, 147]`, which are exactly the values the C++14
build produces, and script linking is unchanged at 91039 lines, 0 warnings,
0 errors. So the standard change does not move gameplay arithmetic — which for
a lockstep engine is the property that actually matters.

It is free because nothing in `src/` uses what C++17 removed: no `auto_ptr`,
`bind1st`/`bind2nd`, `ptr_fun`/`mem_fun`, `random_shuffle`,
`unary_function`/`binary_function` or `std::iterator`. All 33 `register` matches
are the word inside comments. The four `throw()` specifications are deprecated
rather than removed, and do not warn.

Why not C++20 while in the file: `throw()` *is* removed there, and those four
would have to go with it. [ADR-023](decisions.md#adr-023--c17-now-and-not-c20).

### Risk

Smaller than expected after the first CI run, which was green on all six jobs:
Linux headless, the Linux client with the Qt5 editor and `mape`, the content
lint, macOS headless, the macOS app bundle and Windows `c4group`. So clang
compiles the whole engine and a client under C++17 as well.

The MSVC gap that was open when this was written closed the next day. At the
time the Windows job built only `c4group`, which links `libmisc` and nothing
else, so MSVC had compiled the archive layer under C++17 and none of the engine.
`5198b01c6` (#30) added a headless Windows job: MSVC builds `libc4script`,
`libopenclonk` and the whole engine under C++17, passes all four unit-test
binaries and runs `Movement.ocs` to 3 of 3 with the rock at `[372, 157]` — the
same value Linux produces.

What no CI job compiles under C++17 on any platform but Linux is the editor and
the GUI client. A revert is still one commit if that turns out to disagree.

---

## 31. `511e3eea4` — the editor was Qt5-only, and macOS has no Qt5

**Files:** `CMakeLists.txt`, `cmake/DeployQt.cmake`, `src/editor/*` (4 files)

### Motivation

macOS has had no editor since Qt5 left Homebrew. Nothing in this tree pinned an
old version — the build simply asked for Qt5 by name and got nothing, so
`WITH_QT_EDITOR` turned itself off and `src/editor/` was compiled by nothing on
that platform (#46). C++17 (`30`) removed the other half of the obstacle, since
Qt6 requires it.

### Technical effect

`find_package(QT NAMES Qt6 Qt5 ...)` picks whichever is installed, and
`QT_VERSION_MAJOR` carries the choice through the resource macro, the link and
the deploy helper. **Both are supported**: Windows gets Qt5 from vcpkg and Linux
ships both, so replacing 5 with 6 would have broken working builds to fix an
absence somewhere else. [ADR-024](decisions.md#adr-024--support-qt5-and-qt6-together-rather-than-replacing-one).

The Qt5 floor moves from 5.4 to 5.14, for `QWidget::screen()`. 5.4 is from 2014;
both Qt5 installations this fork builds against are 5.15.

### What the grep missed

The roadmap's estimate came from searching for removed class names: one
`QRegExp`, four `QLayout::setMargin()` calls, one `qt5_add_resources`, and
explicitly no `QDesktopWidget`, `QLinkedList`, `QTextCodec` or `QStringRef`. It
was right about what it looked for and wrong about the size: **92 compiler
errors**, from five API changes a name search cannot see.

| What | Why no grep found it |
| --- | --- |
| `QOpenGLWidget` moved out of `QtWidgets` into its own module | The class is still called that; only its module changed. The `<QtWidgets>` umbrella in `C4ConsoleQt.h` stopped carrying it, so the viewport's base class was incomplete and every method on it failed — 90 of the 92 errors |
| `QApplication::desktop()` removed, 2 sites | The searched-for name was `QDesktopWidget`; the calls say `desktop()` |
| `Qt::TextColorRole`, `Qt::BackgroundColorRole` removed | Aliases for `ForegroundRole`/`BackgroundRole` since 4.2 |
| `QFontMetrics::width`, `QWheelEvent::delta/x/y`, `QByteArray::append(QString)` | Method removals, invisible to a class-name search |
| `QWidget::enterEvent` takes `QEnterEvent*` from 6.0 | The one signature that *changed* rather than disappeared, and the only place needing a `QT_VERSION` check |

There were also five `setMargin` calls, not four.

### Risk

Real for the Qt5 path, which is why both are built in CI now: `linux-client`
runs a matrix over `qt: [5, 6]`, each leg installing only the version it tests
and asserting on `Using Qt<N> for the editor` rather than trusting the search
order.

Verified by running, not only building. Both configurations start the editor
headlessly through `QT_QPA_PLATFORM=offscreen` on `Movement.ocs`: 433
definitions loaded, `C4AulScriptEngine linked - 91039 lines, 0 warnings, 0
errors`, landscape created, `Game started.` The two logs are 30 lines each and
agree.

### Not fixed here

**The macOS half.** Getting the editor to compile there is what this makes
possible; the Cocoa window and event plumbing has still never been exercised,
and that is exactly the part #8 could not test when it left the editor branch of
the mouse handler alone. #46 stays open until someone builds and runs it on that
machine.

---

## Not addressed

Known, deliberately left alone:

| Issue | Where |
| --- | --- |
| `C4GROUP_TOOL_ONLY` still defines libc4script and libopenclonk, so the default `all` target tries to compile sources needing PNG and JPEG | `CMakeLists.txt`; the Windows CI job builds the `c4group` target explicitly to work around it |

## Unit tests

Not a divergence — no source change was needed — but worth recording since the
targets had never been built here.

With googletest 1.10.0 unpacked outside the repo and passed via `-DGTEST_ROOT=`
/ `-DGMOCK_ROOT=`, all three targets build clean and **all 72 tests pass** on
arm64 macOS:

| Binary | Tests | Suites |
| --- | --- | --- |
| `tests` | 15 | C4NetIO, C4StringTable, C4Value, DirectExec, StdFile, UnicodeHandling |
| `aul_test` | 52 | Aul math, predefined functions, death, diagnostics, syntax |
| `StdMeshMath` | 5 | vector, quaternion |

`SKIP_IPV6_TEST` is not needed on this machine; the C4NetIO tests pass with
IPv6 enabled.

The same three binaries build and pass on Windows/MSVC, where `tests` runs 17
rather than 15 — `UnicodeHandlingTest` has two registry cases that only exist
there — for **74 of 74**. Note the different layout: the test binaries land in
`build\tests\<Config>\`, since `oc_set_target_names()` only redirects the four
shipped executables.

All three also build and pass on Linux/GCC 16 — **72 of 72**, the same set as
macOS.

`c0c6d8639` added five `C4GroupTest` cases to the `tests` binary — the first
coverage the group format has ever had, which takes that binary to 20 and the
suite to **101**. C4Group.cpp is already in libmisc, so it needed no build
change. They are round trips rather than assertions about internals, because
both defects this class has had were invisible from the inside: one produced
empty archives, the other reads a dead stack buffer. Neither would fail these
tests directly — the first is fixed, and the second needs a sanitizer — but the
tests found a third one on the way in, [26](#26-5a34e6eef--writing-a-group-failed-silently-without-a-sort-list).

`0776ab86f` added two **disabled** tests to `StdFileTest`, stating what
`MakeTempFilename` does not promise: called twice on the same base name it
hands out the same name twice, which is the race behind ADR-019. gtest reports
`YOU HAVE 2 DISABLED TESTS` after every run, so the count of known-broken
things stays somewhere a person reads, and a fix has something to turn green.

A fourth target, `determinism`, was added in `a81cd4e97`: 24 tests over
`C4Real` and `C4Random`, the two units gameplay reproducibility rests on. That
brings the suite to **96**, and `ctest` to four binaries. The assertions are
exact bit patterns rather than approximations, since a result that moves by one
unit is a desync rather than a rounding difference. The pinned `pcg32` sequences
were captured on GCC and pass unchanged on clang.

Version choice is constrained from both sides, and the lower bound moved. It
used to be the arity-based `MOCK_METHOD1`/`MOCK_METHOD2` macros, dropped by
later googletest releases; since `8722c249b` it is the toolchain, because
1.10.0 itself no longer compiles on GCC 15 or later. The upper bound is
unchanged: `CMAKE_CXX_STANDARD 14` with `STANDARD_REQUIRED ON` rules out 1.15+.
**1.14.0** sits in the remaining window. See [23](#23-8722c249b-753b74a59--googletest-1100-stopped-compiling).

What this does and does not buy: the suites cover `libmisc` and `libc4script`,
not the macOS platform layer where most of the fixes live. The exception is the
c4group exit code (11), which the Windows CI job checks directly. The rest are
guarded by the scenario run and the bundle checks, not by these tests.
