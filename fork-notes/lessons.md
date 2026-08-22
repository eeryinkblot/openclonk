# Lessons

How work on this fork has gone wrong, as method rather than as incident. Every
entry cost something once, and every one of them is the kind of mistake that
looks like diligence while it is being made.

[`divergence.md`](divergence.md) records what each change does,
[`decisions.md`](decisions.md) why it has that shape, [`platforms.md`](platforms.md)
what was measured where, [`ci.md`](ci.md) what the workflow checks. This file is
about the process that produced them.

---

## An estimate nobody rechecks becomes a blocker

**#30**, extending Windows CI beyond `c4group`, sat open as "the largest single
coverage gap and the most expensive: all twelve vcpkg ports build from source,
about 38 minutes." The gap was real. The cost was not: the job takes **7
minutes**, of which vcpkg is two — on a cache *miss* — because a hosted runner
has those ports available where the development machine builds them.

The 38 minutes was a true measurement of the wrong machine, written down once
and then quoted as a property of the task. What had actually blocked the work
was #29, fixed weeks earlier and never re-examined.

**The habit:** when something is filed as too expensive, check whether the price
was measured where it will be paid. It costs one build to find out, and that is
usually less than the deliberation it replaces.

## A grep finds what it is named after

The scope estimate for the Qt6 port (**#50**) came from searching `src/editor/`
for removed class names, and predicted six changes: one `QRegExp`, four
`setMargin()` calls, one `qt5_add_resources`, and explicitly no
`QDesktopWidget`, `QLinkedList`, `QTextCodec` or `QStringRef`.

The compiler produced **92 errors**. Every miss had one shape: **an API that
changed without its name changing.**

- `QOpenGLWidget` still exists and is still called that. It moved to a module of
  its own, the `<QtWidgets>` umbrella stopped carrying it, and the viewport's
  base class went incomplete — 90 of the 92 errors from that alone.
- The removed call is spelled `QApplication::desktop()`, not `QDesktopWidget`.
- Method removals (`QFontMetrics::width`, `QWheelEvent::delta`) and a changed
  parameter type (`enterEvent(QEnterEvent*)`) are invisible to a class-name
  search by construction.

And there were five `setMargin` calls, not four.

**The habit:** a name search bounds what you know about, not what exists. For a
port, the estimate is a build. Say which method produced a number when you write
it down — the roadmap did, which is why this was a correction and not a surprise.

## A substring is not a match

Surveying the content lint, `grep -v "0 warnings, 0 errors"` was used to list
the scenarios that were not clean. It silently keeps `10 warnings, 0 errors`,
because the pattern occurs inside it. The scenario with the **most** warnings
was hidden by a search *for* warnings, and "93 of 97 clean" went into the notes
and stayed there.

Any count ending in a zero would have gone the same way.

**The habit:** anchor the pattern (`grep -vE ", 0 warnings, 0 errors$"`) or parse
the numbers. And when a survey produces a suspiciously tidy result, check the
filter before believing it.

## Compiling is not running

This one recurs, and each time in a place that looked covered:

| | |
| --- | --- |
| `Tests.ocf` scenarios reported success having executed **zero** assertions (#27) | the harness ran, no player joined |
| The content lint printed counts for eight years and gated on nothing (#52) | it worked perfectly and checked nothing |
| The editor compiled on two platforms and was started on none | a successful link proves the symbols resolve |
| UBSan found the landscape's undefined shifts only under a **scenario** | the unit tests never reach the game loop |

**The habit:** "it builds" and "it links" are not results. The fork's own rule —
*verify by running the thing* — exists because every one of these looked verified
first.

## A test that cannot observe what it tests will report success forever

The CI step hunting #37 starts the engine five times against an empty user
directory, because the crash lives in a dialog that only opens when no player
file exists. **Neither branch of that condition logs anything**, so a directory
that quietly stopped being empty would leave the step reporting "0 of 5 died"
while starting the ordinary menu five times — green, unchanged, and testing
nothing.

Same shape as #27 and as the lint. The interim guard was to assert the
*fixture* — that the directory really is empty — which restates the input where
the question is which branch the engine took. `63c3419c7` logs the branch, and
both launches now assert on it (#58).

**The habit:** for any test whose subject is a *path* rather than a value, ask
what would happen if the path were not taken. If the answer is "it passes", the
test is decoration until the path is observable. Note how cheap the real fix
was — two log lines — next to how long it sat behind an interim guard.

## Look for the effect, not for the moment

The same hunt decided whether the engine had crashed by asking whether the
process was still alive, four seconds after startup. It is the obvious check and
it is wrong here: `backward-cpp` symbolises every frame with source context
before it re-raises, which takes **seconds** on a binary this size. The one
reproduction of #37 on this machine was still running at that moment, with a
complete stack trace already flushed to its log. The step would have recorded a
healthy engine and moved on.

A crash that is still being printed has not finished happening. Liveness is a
sample of a fast-moving state; the trace in the log is a record that stays true
afterwards.

**The habit:** detect a failure by the durable evidence it leaves, not by a
state you have to catch it in. When a check has to be timed, ask what the
slowest path through the thing you are watching costs — and prefer the artefact
that is still there a minute later.

## Pin in both directions

Every known-broken thing here is pinned to an exact count — assertion failures
per scenario, warnings per scenario, undefined-behaviour sites — and a count
that **drops** fails the job as surely as one that grows.

That half is unintuitive and is the whole point. A fix that leaves its pin
behind turns the guard into decoration, silently, and the next regression hides
under the slack. The failure message says so, in every implementation of it.

The other half: pinning is what makes it possible to *keep* a known defect
without hiding it. Fourteen failing assertions that cannot grow are a tripwire;
fourteen that are merely tolerated are a blind spot.

## A claim repeated in several places is more suspect than one stated once

Recorded in full under
[Repetition is not corroboration](platforms.md#repetition-is-not-corroboration).
The short version: four claims in these notes were wrong on the same day, each
written from reading rather than running, then copied into a second and third
file until the repetition itself looked like evidence.

The worst of them was a *measured* value — "the rock lands at [372, 157] on all
three platforms, bit-identical". Three single runs of something that produces
two different answers at random.

## Corollary: say how you know

Most entries above are failures of provenance rather than of reasoning. A number
whose method is stated — "measured on this machine", "counted from a run",
"grep, not a build" — can be checked and corrected. A number that arrives
without one gets quoted.
