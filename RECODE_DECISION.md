# Prop pickup regression recode decision

Date: 2026-11-23
Status: **b296 event-trigger-only build passed; user runtime verification pending.**

## Decision

Stop patching the b329 line. Reconstruct the regression by starting from a known-working public release and reintroducing the modifications in logical groups, testing client pickup after each group. There will be no b330 based on b329.

The new isolated checkout is this directory, on branch `recode/b301-prop-pickup-bisect`, created from the public tag `v0.9.0n-b292-dev` at commit `ca8537cb902bc4ed4fc5f276cb4a4b693b69909b`. The parent project remains untouched by recode implementation work.

## Evidence at the start

- **User runtime observation:** the public upstream build works for client pickup.
- **User runtime observation:** modified builds b301 through b329 fail; swapping host/client roles makes the failure follow the client.
- **User runtime observation:** b329 did not fix the issue. It is a failed candidate, not a baseline for further implementation.
- **Artifact limit:** the preserved b301 package is at `../multivoid/Multivoid-main/Multivoid-main/local/checkpoint-package/Pelmentor-Multivoid-0.9.301.zip`. The existing b301 audit reports that the package contains a stripped DLL and no source/symbols. Build logs and historical reports are retained in the parent project. We have not yet established an available b301 source snapshot or a byte-accurate b292-to-b301 source diff.
- **Source fact:** the current b329 line has changes after b301 as well as attempted repairs. Its source cannot stand in for b301 when assigning the original regression to a group.

The parent project’s [handoff](../multivoid/Multivoid-main/Multivoid-main/handoff.md), [b301/b328 comparison](../multivoid/Multivoid-main/Multivoid-main/local/analysis/round25/b301-b328-client-prop-input-comparison.md), and [b329 comparison/build report](../multivoid/Multivoid-main/Multivoid-main/local/analysis/round26/b329-native-pickup-upstream-comparison.md) are preserved as historical records, not as current instructions or a proven b301 source diff.

## Documentation policy

The upstream documentation in this checkout is retained as the starting documentation. The parent project’s docs, audits, build reports, logs, and failed test results are also retained in place so they continue to teach what was tried and what evidence exists. Recode-specific instructions and results go in this folder. Do not copy stale build status into upstream `README.md`, `START_HERE.md`, or feature pages. Any historical claim reused here must name its build and whether it came from user runtime, source, artifact inspection, or a test fixture.

## Planned incremental groups

These are investigation buckets, not claims that the b301 diff has already been reconstructed. Exact files and commits for each bucket must come from the source comparison before implementation.

1. **Prop physics:** simulation enable/disable, parking/unparking, collision state, sleep/freeze/static reconciliation, and physics handoff.
2. **Mirror creation:** host prop identity, client mirror adoption/spawn, key/EID binding, and spawn-time convergence.
3. **Input interception:** E press/release, `IsSimulatingPhysics` query routing, pickup dispatch, contextual action selection, and other hooks on the native input path.
4. **Synchronization:** held pose/release, host admission/arbitration, PropDrive ownership transfer, and throw/drop continuation.

For each group, keep earlier groups fixed at their last user-tested state, introduce only that group, build both peers from the same source, and wait for the user’s runtime result before proceeding. If the result changes, revert only that group and repeat the same test to confirm the boundary.

## Test sequence

- **Baseline:** use the existing user-verified public b292 behavior as the reference. If a fresh run is practical, confirm one ordinary client tap pickup and one intentional context action before code changes.
- **After each group:** use identical candidate builds on host and client. The client performs repeated E taps on ordinary movable props; separately exercise an intentional hold-E context action. The host observes the same object and action. Keep host-initiated pickup as a control. Test R-held luggage separately from E pickup.
- **Acceptance:** record the exact candidate commit/group, prop/action, initiating role, observer role, and both results. No logs are required unless the user later asks for a diagnostic step.
- **First failing group:** stop, revert that group, confirm pickup returns, then subdivide the group. Do not continue stacking changes on a failing candidate.

At the time this bisect decision was recorded, no candidate build was designated or delivered from this branch. Any candidate must state explicitly that it is based on b292 plus the named group(s), never imply it was built on b329, and wait for user runtime verification before the next prop-synchronization group.

## First event-sync candidate: anti-gravity camera light

After testing the locally compiled upstream baseline, the user confirmed that it does not reproduce the client pickup bug and directed the next work toward event syncing. The first bounded event output is the temporary blue `PointLight` attached to the local camera by `trigger_agrav`; recent playtest notes say the Ariral ship was visible while this light was still missing. This candidate mirrors only that light's visible start and destruction. It does not replay the `agrav` controller, change prop gravity or movement, or mirror the separate cloak/ship timeline.

The b292 source has generic `EventCue` messages with a 16-byte `{cueId, x, y, z}` payload. The audited cooked `trigger_agrav` graph places the light's `SetVisibility(true)` call at bytecode offset 705 and its `K2_DestroyComponent` call at 1918. A later historical implementation's post-hook callback treated the second callback argument as the target component and the third as function parameters. The checked-in `ufunction_hook` contract passes the target UObject as `context`, the calling Blueprint actor as `sourceObject`, and the native return value as the third argument; a void function has no returned parameter frame. The new candidate uses `context` for the component and relies on the exact bytecode callsite for the `true` visibility edge.

Because b292's cue payload lacks an occurrence identity, this change appends host-world epoch and occurrence sequence fields and increments the protocol to b293. The guest creates a local light from the cooked template on its own camera manager and destroys it on the matching stop cue, with bounded readiness retries and a safety timeout. The focused source-contract check and CI build/package checks are not runtime evidence; this feature remains unverified until the user tests the b293 package in-game.

## Next event-sync group: manual trigger routing and coverage audit

After the user directed broad trigger synchronization, the next change remains on the b292-based
recode branch. Connected clients suppress their scheduled `allEvents` walk; a direct client
eventer call or client developer-menu selection is canceled locally and sent as a registered
`EventFireRequest`. The host validates the sender and event row, runs the native verb, and its
existing post-watch publishes the committed event. Host manual fires also cross that watch. A
manual bit distinguishes a direct replay-safe fire from scheduled-history dedupe without overriding
the per-row no-replay policy. The host now retries an EventFire only when that peer's reliable
queue explicitly refused admission, bounded by peer/session/world generation. The payload and
request kind require protocol 294.

The full row inventory, 21 known incomplete native event outputs, ambient weather limitations,
and the request's drop conditions are recorded in
[`docs/event-trigger-sync-audit.md`](docs/event-trigger-sync-audit.md). No row was added to the
replay allowlist solely to make it appear synchronized: several outputs can spawn duplicate or
client-active actors, target `getMainPlayer`, or have no mirror lane. The user has since confirmed
b294 is clean and safe in-game. The b296 candidate is based directly on b294 source, with only the
protocol/build identifier advanced to 296. Do not treat source checks or package verification as
runtime evidence.

## b295 interaction regression and b296 scope

The user tested b295 and reported that client E pickup still often turns into a contextual
activation, while the host remains normal. b295 added multiple things after b294, including
microwave timer/activation context-action synchronization and several event-output/cord lanes. Since
those additions were bundled, this evidence does not isolate the specific cause. The added
context-action synchronization is the leading suspect because it hooks a native interaction path
and the behavior reappeared in b295; it may be breaking the wider pickup flow. This remains a
hypothesis, not a proven causal finding.

Per the user's direction, the active branch is reset to the confirmed b294 source point
(`a253622b2c5e361c4c002895784d1596a296dbdd`) and advanced directly to b296. b296 retains b294's
event-trigger routing and b293 anti-gravity light sync, but excludes b295's microwave context
actions, microwave effects, cord-head lane, `earthTp` replay/audio changes, and added event sounds.
The b294 package is preserved; the b295 desktop package is removed. This narrows the candidate to
the user's confirmed-safe event-sync set and is not a claim that the root cause is proven. b296's
Windows Release build, ABI gate, and package drill passed in [CI run 45081585387](https://github.com/boiling-tea-cup-26/Multivoid/actions/runs/45081585387)
at source commit `894fd4cc2927e01daf0eec8311eb16d2158f20a0`. The package and test checklist are in
`~/Desktop/readytotest/recode/b296-event-triggers-only/`. The user must still test it in-game.
