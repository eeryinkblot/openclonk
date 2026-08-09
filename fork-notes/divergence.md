# Divergence from upstream

Every change this fork carries on top of `openclonk/openclonk` master
(`36de79954`, 2026-04-28), with the reasoning behind it and what it actually
changes at runtime.

All of them are macOS or Apple-Silicon specific. None touches game logic, the
script engine, or anything that could affect network synchronisation — the
91,026 lines of C4Script under `planet/` linked without a single warning on the
very first build.

Four of the ten announced themselves properly, as compile or configure errors
(1, 2, 3, 10). The rest did not. `c4group` reported success while writing
zero-byte archives, the server sat in an event loop without printing anything,
the mouse discarded every event, and the audio path reported a working device
while the engine had sound switched off in its config. That is the class of
defect a dead CI cannot catch and only running the thing reveals.

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

### Risk

"Unknown option" and "Error forking" previously warned and carried on; both now
fail. They mean the tool did not do what it was asked, but they are the two
most likely to affect an existing workflow.

---

## Not addressed

Known, deliberately left alone:

| Issue | Where |
| --- | --- |
| Redundant `mac mac-arm64` in the version line | `cmake/Version.cmake`, upstream behaviour on all platforms |
| Bundle resource seal stale after data packing | POST_BUILD ordering in `CMakeLists.txt` |
| `ctest` covers only 2 of the 3 test binaries | `add_test()` is only called from `create_test()`, which `tests` does not use |
| CI is dead (Travis, AppVeyor with VS 2017) | `.travis.yml`, `appveyor.yml` |

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

Version choice is constrained from both sides: the tests use the arity-based
`MOCK_METHOD1`/`MOCK_METHOD2` macros that later googletest releases dropped,
and `CMAKE_CXX_STANDARD 14` with `STANDARD_REQUIRED ON` rules out 1.15+.
1.10.0 sits in the remaining window.

This means the engine-side changes in this fork are now covered by whatever
those tests cover — which is `libmisc` and `libc4script`, not the macOS
platform layer where all the fixes live. None of the nine changes is exercised
by them.
