# Prop pickup regression recode decision

Date: 2026-11-25
Status: **b302 failed in-game: moving Kerfur pickup remains intermittent, the client reports a snappier/laggier feel, and client R-drop then E-pickup still fails. b303 adds a separate native-pickup handoff for K-5 adopted Kerfur mirrors; runtime verification is pending.**

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

## b297 native pickup handoff hypothesis

Source inspection of the b292-based PropDrive path found that an incoming active pose parks a
client mirror by disabling its physics (`prop_drive_stream::ApplyPose` -> `prop_park::Park`). The
cooked `mainPlayer_C::useAction` checks `IsSimulatingPhysics` before choosing `pickupObject`; a
kinematic hit follows its contextual-use branch. This is a concrete source-level explanation for
the reported case where a host-thrown, still-driven prop is visible but a client E press takes the
wrong path. It is not runtime proof, and it does not explain the separate Omega motion report.

The b297 hypothesis changes only the client E-input edge for an actor currently parked by an
active PropDrive row. Immediately before the existing native input event runs, it gives that
mirror physics back. The original game `useAction`, `pickupObject`, PhysicsHandle, and grab
timeline still run; there is no synthetic pickup or new network message. If pickup succeeds, the
existing PropDrive tick recognizes the local hand and yields its generation, while the existing
held-prop stream and host arbitration handle the cross-peer handoff. If pickup does not happen,
the active drive can re-latch physics on its next tick. Static/frozen targets, non-driven props,
context-only props, and host-side input are not modified.

This is an intentional causal test of the one source-supported failure path, not a fix guarantee.
The Windows x64 Release DLL was built locally from source commit
`10246304ed350fff7082466cc446c3c38d81955f`, using the retained
Linux clang-cl toolchain and cached dependencies. The isolated build staging copy required
compiler-only CMake/assembler adaptations (per-file Opus SSE4.1 flags, language-scoped compiler
flags, and equivalent MASM label syntax); no gameplay C++ sources were changed in that staging
copy. ABI gate, package drill, and ZIP integrity checks passed. The package is in
`~/Desktop/readytotest/recode/b297-prop-pickup/`; see the test ledger for hashes. Gameplay remains
unverified until the user tests it. Omega smoothing, NPC collision, target caching, and
interpolation were not changed because this pass did not establish a causal link from those
systems to this pickup failure.

## b298 hit-component pickup attempt

The user's b297 test showed that a moving ordinary prop still did not enter the hand. The user
then confirmed the same moving-only pickup failure for Kerfur while active/walking and while
deactivated/falling, with pickup possible after it settles. The exact Kerfur form in the active test
was not specified. Source distinguishes the regular Kerfus (a prop, claimed into PropDrive while
active by `coop/creatures/kerfus_state.cpp`) from the Omega active form (an NPC character); the
powered-off Omega is a prop (`docs/npcs-and-kerfur.md`). Incoming PropDrive pose application parks a
client prop's physics. Thus the normal active Kerfus and deactivated prop cases share the prop path;
an active Omega test would use the separate NPC pose path and is not covered by this b298 patch.

b298 is a narrowly scoped follow-up attempt, not a claimed fix. The native `useAction` decision
tests `IsSimulatingPhysics` on the raycast's hit component. b297 restored simulation through the
prop's resolved static mesh without checking that this was the component the native graph tested.
b298 now passes the hit component to `PrepareNativePickup`, and permits the handoff only when that
component is the driven prop's root or mesh and is a primitive component. If the hit is on another
component, it leaves the prop and native input untouched. It does not add a client pickup mechanic,
network message, or interpolation change. Ordinary static/non-driven props retain their original
contextual-action path; R-held luggage is untouched.

This component mismatch is a source-grounded explanation for why b297's unpark attempt could
change the branch without completing pickup, but runtime confirmation is pending. Test b298 with:
(1) a host-thrown ordinary prop while moving, (2) the active regular Kerfus while moving, (3) the
deactivated/falling prop form, (4) each again after settling, (5) a stationary contextual-action prop
with a deliberate hold-E, and (6) host pickup as control. If the active test is the Omega character,
record that separately because this b298 patch does not target it. Confirm a successful pickup uses
the normal hand state and can rotate; a sound or changed animation alone is not success. Preserve
b297 as the previous test artifact. The b298 Windows x64 Release DLL was built locally from source commit
`e3c2124c127be798ace7d5ae75b3083d997f6b92`. The ABI gate and negative-control drill passed;
manual ZIP structure, manifest, DLL identity and CRC verification passed. The repository PowerShell
package drill was unavailable because `pwsh` is not installed. Package and hashes are recorded in
`RECODE_TEST_LOG.md`. Do not advance to another patch until this result is recorded.

## b298 user result and b299 native-query handoff

The user tested b298 and reported that moving props still do not pick up. Holding E on Kerfur now deactivates it without allowing the intended pickup/action; b298 therefore failed and introduced or exposed an additional hold-E regression. This is runtime evidence against the b298 pre-input mutation.

The cooked `mainPlayer_C::useAction` graph provides a more precise interception point than the `InpActEvt_use` input event. At bytecode offset 1572 it calls `IsSimulatingPhysics` on the raycast's `hitcomponent`; a false result skips `pickupObject(hitResult)` and enters the `issec`/recursive context-use branch. When pickup is reached, the native game path assigns `grabbing_component`, checks its physics at offset 94202, and initializes `PhysicsHandleComponent.GrabComponentAtLocationWithRotation` at offset 94761. Thus both the route decision and pickup initialization depend on the physics state of the hit component itself.

b299 rolls back the b298 broad input-edge behavior. Instead it hooks the native `IsSimulatingPhysics` implementation and only changes a false answer when the native caller frame is exactly `mainPlayer_C::useAction`, the queried component belongs to an active client PropDrive, and that mirror's parked physics is successfully restored. It then returns true to the original Blueprint, which continues through the game's own pickup and PhysicsHandle/timeline flow. Queries from other callers are untouched; if `useAction` handles a contextual action before reaching this physics gate, that action is untouched. No wire payload or interpolation behavior changes. This is a source-grounded alternative hypothesis, not a verified fix; it still needs two-instance testing.

The b299 Windows x64 build was compiled from b292-based recode source commit `a1ae63c0dec00f9bc1be9f9fb5dd364f6558b52a`, with only the targeted grab observer change and protocol/build identifier 299. Build, ABI gate and package integrity checks passed. The b299 package is in `~/Desktop/readytotest/recode/b299-useaction-physics-gate/`. Gameplay remains unverified. Keep b297 and b298 packages intact as comparison artifacts.

## b299 user result and resolver defect

The user tested b299: the issue remains. Client E on moving props still follows the wrong interaction; R-held bin dropped while it was moving triggers the interaction until it is still/parked. Active Kerfur deactivates immediately on client E. Contextual actions continue to work. b299 is a failed candidate.

A source audit found why the b299 intervention could be inert. The new facts row named `IsSimulatingPhysics` as declared on `PrimitiveComponent`, but `engine_physics.cpp` explicitly documents that UFunction as declared on `SceneComponent` and found from `PrimitiveComponent` only through superclass dispatch lookup. `facts::Resolve` for a `Declared` row calls `reflection::FindFunction`, which walks only the owning class's own `Children` list and does not walk superclasses. Therefore the b299 row can resolve empty and skip installation of the native hook. This is a source-confirmed defect in b299's registration; it explains why the runtime result could be unchanged, though no b299 log was collected to prove what its instance logged at startup.

b300 corrects the facts row to `SceneComponent` and resolves it against the already-found declaring class with `ResolveOn`. The caller filter and interaction behavior are otherwise unchanged. This repairs the registration defect, but it does not prove that the hook fires on every eligible runtime query or that there are no additional gates. The user has now tested client Kerfur pickup and client R-dropped container pickup; those results are recorded below. Host control was not newly reported.

The b300 Windows x64 Release package was built from source commit `8028934276b642b4e4941e39b0058677d8155fae` and placed in `~/Desktop/readytotest/recode/b300-scenecomponent-hook/`. The ABI gate and negative-control drill passed; ZIP integrity and embedded DLL identity passed. Hashes are in `RECODE_TEST_LOG.md`. The user's runtime result is recorded below.

## b300 user result: partial improvement, failures track motion

The user tested b300 and reports that client E pickup on Kerfur succeeds in roughly 90% of attempts. In the remaining attempts the client triggers Kerfur's context action/deactivation; the context menu generally opens and its options can be scrolled. For ordinary containers picked up and dropped through the client's R-held/inventory path, an E tap while the item is still moving can trigger its contextual action instead of pickup. After the object has settled, the normal E pickup behavior returns. The user describes the remaining issue as movement-related. These are user runtime observations, not source-derived conclusions. b300 is therefore the new partial-improvement baseline, not a complete fix.

### What the b300 source establishes

- The cooked `mainPlayer_C::useAction` graph queries `hitcomponent.IsSimulatingPhysics()` at offset 1572. A false result skips its first native `pickupObject(hitResult)` call and continues into the contextual/secondary-use path. There is also a later root-component physics query at offset 3259, so the exact path depends on the rest of the graph's conditions.
- b300's native query post-hook only attempts an override when the caller is `useAction`, the original result is false, and `PrepareNativePickup` finds the hit component on a currently active client `g_drives` entry whose physics the PropDrive receiver parked. It further requires the queried body to be that actor's mesh or root primitive. The hook then restores physics and lets the original Blueprint route continue.
- `simulateDrop` is a separate event graph from `useAction`; the cooked asset shows it begins at `ExecuteUbergraph_mainPlayer(119438)`. R hotkey handling calls `simulateDrop` on its own branches. Thus b300's E-query hook is not itself an R-drop synchronization implementation.
- The client PropDrive receiver parks props on incoming pose application, advances their transform on its tick, and un-parks them on the matching end path. The b300 pickup override's availability consequently depends on the live receiver row and its `physicsParked` state at the time of the E query.

### What remains unproven

The source makes the moving-versus-settled correlation compatible with a PropDrive park/eligibility window, but does not prove it is the cause of the reported misses. In particular, we do not yet know for a failed Kerfur E press whether the query hit a nested component rather than root/mesh, whether the client had an active parked drive row, whether the hook ran, or which `useAction` branch actually executed. Nor does the separate `simulateDrop` graph establish whether the user's R-dropped instance is still under host PropDrive when the E press occurs. Do not treat “movement is the root cause” as established solely from temporal correlation.

### Minimal falsification check for the next investigation

Use the same client prop and repeat E taps in two phases: while visibly moving under host control, then after it settles. Include one ordinary prop and one moving Kerfur; note for each attempt whether E picked up, ran a context action, or failed, and whether the context prompt was already visible before the tap. The decisive source/runtime comparison is whether a failed moving tap coincides with the client not taking the b300 `PrepareNativePickup` path (no eligible active parked drive or non-root/mesh hit) while a successful moving tap does. Test host R-drop and client R-drop separately because their ownership/drive histories may differ. No parameter or protocol change is justified until that boundary is identified.

## b301 hit-component eligibility test

The user asked for a patch testing the residual-motion theory. b300's native `useAction` query tests the exact raycast hit component, but `PrepareNativePickup` previously accepted that component only when it was the active host-driven prop's main mesh or root. If the ray hit a different primitive component on the same prop, b300 left it kinematic and let the native graph take its contextual-use branch. This gate is an explicit source fact; whether the user's intermittent Kerfur or bin failures hit that gate was not observed at runtime.

b301 keeps the active PropDrive actor and parked-physics requirements. It broadens eligibility only to a primitive ray-hit component on that same actor. If the native pickup does not transfer the actor into the local hand by the next drive tick, b301 turns that temporary component simulation back off before continuing the host-driven pose path. Frozen/static props, non-driven props, other callers, contextual actions reached before the native physics query, R input routing, wire payloads, and interpolation settings are unchanged. This is a causal test for a miss caused by a non-mesh/root primitive hit, not a general fix for moving items; if the actor is not under active PropDrive, b301 does not take the new path.

The b301 Windows x64 Release DLL was built from source commit `69b1233b6d2010b405b9e3d331de8208904049a3`. Full CMake build, the ABI gate, and its must-fire negative control passed. Because PowerShell is unavailable in this Linux environment, packaging reused the verified b300 archive tree and replaced its version metadata, changelog, README, and DLL; an independent ZIP CRC check and embedded-DLL hash comparison passed. Package and SHA-256 values are in `RECODE_TEST_LOG.md`; it is at `~/Desktop/readytotest/recode/b301-moving-hitbody/`. The user has since tested b301 and reported no improvement; this package is a failed candidate, not a new baseline.

## b301 user result and follow-up investigation

The user tested b301 and reports **no improvement to the previously described issues**. This rejects b301 as a useful candidate and gives no support to the specific claim that the missed pickup was caused by a ray hit landing on a non-mesh primitive component. It does not establish which branch ran during a failed attempt, and the user did not itemize fresh results for each separate control (settled pickup, hold-E, host control, or R-held luggage). No logs or runtime component identities were collected. Keep b300 as the last partial-improvement baseline; do not treat b301 as a fix.

### Additional source trace: two distinct physics gates

The upstream cooked `mainPlayer_C::useAction` graph has two routes relevant to pickup:

- At bytecode offset 1085 it tests whether the hit actor implements `int_player`. The ray-hit `hitcomponent.IsSimulatingPhysics()` check at offset 1572 is only reached when that interface test is false (`1123` branches to `1572`).
- For interface actors, the graph first evaluates prop/context handling and then, in its later fallback route, checks `hitactor.RootComponent.IsSimulatingPhysics()` at offset 3259. A true result routes to `pickupObject(hitResult)` at `2620`; false continues to the `player_use` contextual route at `3472`.
- The native Blueprint `pickupObject(hitResult)` has a further independent check: it extracts the original `HitComponent` and calls `IsSimulatingPhysics()` at offset 103372 before it assigns the grab actor and reaches the hand setup. This is after the root-component decision in `useAction`.
- b301's hook receives only the component whose `IsSimulatingPhysics` native is currently running. On the interface fallback route, that is the actor root component, not necessarily the original ray-hit component stored in `hitResult`. `PrepareNativePickup` therefore cannot infer or restore the hit component from that call. Its b301 expansion to any primitive applies only to the component it was given; it does not establish that the second `pickupObject` gate will see physics enabled on the original hit component.

These are source facts. They identify a real coverage hole in the b301 approach: it models the physics query as one gate, while the native flow has a root-component route decision followed by a second hit-component gate inside pickup. They do **not** yet prove that a particular failed client attempt had different root and hit components, or that this is the only remaining cause. The same source shows other early interface/context branches can be taken before either physics check, so a failed attempt could also bypass the hook entirely.

### Current leading explanation and what would falsify it

The leading source-grounded explanation is that b300/b301 modify the physics result for a component at the route-selection stage but do not guarantee the exact hit body passes the later native pickup gate. This is especially plausible for interface props with a root component different from the ray-hit body. b301's lack of improvement is consistent with that gap, but is not runtime confirmation. The earlier idea that the remaining misses were simply a non-mesh primitive hit is weakened: changing that eligibility alone had no reported effect.

The decisive minimal runtime comparison, if a later test is authorized, is one moving interface prop that fails and the same prop after it settles: identify whether its root component and ray-hit component are the same, and whether the native pickup gate sees the hit body simulating when the fallback root query passes. A simple prop whose root is its mesh is a useful control. This can be answered with one narrowly scoped trace at the two existing query sites; do not add broad per-tick logging. If root and hit are the same and both queries return true during a failed pickup, this explanation is falsified and investigation should move to `pickupObject`'s later conditions/hand initialization. If the root query is false, the route never reaches native pickup; investigate why that actor lacks an eligible active client PropDrive entry or whether an earlier contextual branch consumed the input. If the root query is true but the hit-component query is false, the component-handoff gap is confirmed.

### R-held drop remains a separate ownership question

The cooked graph routes R through `hotkeyAction`/`simulateDrop`, separately from the E `useAction` graph. Source also distinguishes a client's outbound `prop_syncer_client` row from the client's inbound `prop_drive_stream::g_drives` receiver rows. b301's pickup preparation searches only the inbound PropDrive receiver table. The source reviewed so far does not establish which ownership state is active on the client at the instant after its own R-drop, nor whether its root and hit component remain simulating. Do not combine the R-drop symptom with host-thrown PropDrive cases until that ownership/physics state is established.

No gameplay source, network protocol, or interpolation value was changed in this follow-up. No new build is warranted from these findings alone.

## b302 dual-gate native pickup candidate

b302 starts from the b300 source commit `8028934276b642b4e4941e39b0058677d8155fae`, not b301. b301's primitive-hit eligibility change is excluded. The source branch retains the b301 failure report as documentation evidence, then adds only the b302 implementation and build identifier.

The native call path now has a paired, actor-scoped handoff:

1. When the client's actively host-driven prop is parked, the `useAction` physics query releases the prop's main mesh and records a pending native pickup for that actor. This route query may be on the actor's root component for `int_player` props.
2. At the separate `pickupObject` physics query, the hook receives the original `HitComponent`. It enables that primitive hit body if necessary and lets the native Blueprint continue only when the exact body reports simulating physics. The game's `pickupObject`, PhysicsHandle, grab timeline, and held-prop stream still perform the pickup; no synthetic grab or new wire payload is introduced.
3. If the hand did not take the prop by the next PropDrive tick, the temporary hit body is restored and the driven mesh is re-parked. If the game's live `grabbing_actor` already names the prop, the drive yields and leaves the native hand's body enabled.

The live-hand read addresses a second source-confirmed timing gap: `net_pump.cpp` calls `subsystems::TickGameplay` before `local_streams::Tick`, while `prop_drive_host::Pump` advances `prop_drive_stream::TickApplyAndDrive` from that earlier chain. `prop_drive_host::InAHand` uses `local_streams::LastHeldActor()`, which is refreshed later in the same pump. The b302 receiver therefore checks `engine::IsMainPlayerGrabbing` directly before re-parking, in addition to the existing cached hand and remote-held-drive checks. This closes the one-pump stale-cache window for the local native hand.

This scope is the client copy currently parked by an inbound host PropDrive. It does not alter interpolation or add a generic R-input override. Client R-drop/luggage is tracked as a separate test because it may be locally authored and need not enter this receiver table. Do not infer that it is fixed by the E pickup test.

The b302 Windows x64 Release build used the retained Linux clang-cl staging tree and cached dependencies. The full CMake build passed; the ABI gate passed and its synthesized violating-DLL control fired; the resource version string is `0.9.0n b302`. Packaging reused the verified b301 archive tree because PowerShell is unavailable, replacing the DLL and the b302 README/changelog/manifest. ZIP CRC, manifest version `0.9.302`, and embedded/loose DLL identity passed. Source commit: `689c7ce22c8b070eee855ddea4027c7e99eb574e`. Package: `~/Desktop/readytotest/recode/b302-native-pickup-gates/Pelmentor-Multivoid-0.9.302.zip`; ZIP SHA-256 `35c53462a389acd5be1d31d4177c09da590a3504601be4c24e9d425d3ba3f92b`; DLL SHA-256 `3fe30d25d57070c1a49f548c6016a822e6522906e1d570d3a571983c3f3717f9`.

The package README has the b302 test checklist. The user has since reported b302 failed; the b302 test result and the next source investigation are recorded below.

## b302 user result and b303 K-5 scope

The user reports that b302 is not fixed. A client can sometimes pick up moving Kerfur, but it still fails most attempts; Kerfur feels snappier and laggier to the client. The client R-hold/drop/E sequence still fails consistently. These are runtime observations. The R-drop lane was explicitly outside b302's change, so its continued failure does not test that patch. The changed feel is correlated with b302, but the observation does not identify which operation caused it.

### Source distinction found after b302

The b302 helper only searches the incoming `prop_drive_stream::g_drives` table. Regular Kerfur mirrors are created through a distinct path: `kerfur_prop_adoption::BindAsMirror` calls `SetActorSimulatePhysics(actor, false)` and registers the actor with `kerfur_entity::NotifyKerfurPropMirrorBound` for the K-5 held-pose stream. That path does not create an entry in `prop_drive_stream::g_drives`. Therefore b302's two-gate prep does not normally cover an adopted regular Kerfur mirror. This is a source-confirmed coverage gap; it does not, by itself, prove the runtime Kerfur instance was adopted through this path or explain every intermittent miss.

The client copy's K-5 mapping is also used by `local_streams::ResolveHeldPropEid`, so if the native pickup succeeds, the existing held-prop stream has an identity for that Kerfur and can continue the normal host-side arbitration. No new network message or synthetic grab was added.

### b303 change and limits

b303 extends the existing scoped `IsSimulatingPhysics` handoff only to an actor that `GetKerfurMirrorEidForActor` confirms is a live K-5 host mirror. It temporarily enables the adopted mirror's mesh for the route/hit-body checks; if the local hand has not taken it by the next PropDrive pump, the attempt restores the mesh and any separately enabled hit body to non-simulating. On a successful native handoff it leaves simulation on, allowing the existing local held-pose/K-5 path to carry the prop. Static/frozen actors, non-Kerfur props, host-side input, and unrelated contextual interactions are outside this extra branch. This does not alter interpolation settings.

This is a narrow fix for the confirmed K-5 eligibility gap, not a complete fix claim. Client R-held/drop then E remains unresolved: the held-pose receiver on the host and the client's post-release ownership/physics state are distinct from both K-5 mirror adoption and an incoming host PropDrive row. The available observations do not identify which owner or physics gate is active at the failed E press, so b303 does not change that path. Test b303's moving/deactivated regular Kerfur pickup and hold-E context actions; report client R-drop separately. The user has not yet tested b303.

The user reports the b303 active-Kerfur pickup succeeds. The remaining reproducible case is after deactivation: an immediate E tap runs the activation action, and a moving/falling body becomes grabbable only after settling. R-drop/E is unchanged. This narrows the new work to the K-5 native physics gate; it does not implicate the separate R path.

## b304 deactivated-Kerfur physics gate

The b303 K-5 helper requires a K-5 mirror identity and a prop body that is neither static nor frozen (`PhysicsStaysOff`). It returns without enabling physics otherwise, so the native `useAction` route can fall through to the game's contextual activation branch. The user's timing report—failure during the moving/falling period, then pickup after settling—matches this explicit gate. The new b304 candidate keeps the mirror identity requirement and the static-prop exclusion, but stops treating the Kerfur `Frozen` flag as a reason to block a temporary native pickup attempt. The helper already records original mesh/body physics and restores it on a failed attempt; a successful native PhysicsHandle pickup keeps physics enabled and the existing host-authoritative held-pose route carries the prop.

This is a source-grounded hypothesis tested by a narrow candidate, not yet runtime confirmation: the report does not directly expose the `Frozen` flag at the failed tap. It changes only K-5 mirror pickup eligibility; it does not change ordinary props, contextual actions outside the intercepted native physics query, host simulation authority, protocol payloads, interpolation, or R-held luggage. If b304 still reactivates Kerfur on immediate E, the next evidence needed is a direct read of `IsFrozen`, `IsStatic`, the root/hit component simulation results, and whether the physics-query hook fires for the deactivated prop at that exact tap.

b304 source commit: `0c0579dfc9221d4fa58d6b5ffdecb6fcbeb08fa2`. Windows x64 Release CMake build completed from the retained Linux clang-cl staging environment. The staging-only build adaptation disabled optional Opus SSE4.1/AVX2 paths and removed MSVC-only compile switches from the MASM invocation; gameplay source and repository build files were not altered for these adaptations. ABI gate passed, including its synthesized violating-DLL drill. VERSIONINFO contains `0.9.0n b304`. The b303 archive tree was repackaged with the b304 DLL and metadata; ZIP integrity, manifest version, and embedded/loose DLL identity passed. Package: `~/Desktop/readytotest/recode/b304-deactivated-kerfur-pickup/Pelmentor-Multivoid-0.9.304.zip`; ZIP SHA-256 `f22af14d214b120e54135828068e6a74bac2b7afb70e46286de959b716d4b6e9`; DLL SHA-256 `1876785cc050a976b2bf06c475757bed6d923bf926bdae047d9a0d6873d68ad6`. Runtime verification is pending.

### Audit correction before b304 testing

A read-only review found that `PrepareNativePickup` applies the shared `PhysicsStaysOff` rejection before it calls the Kerfur-specific helper. Therefore b304's removal of the frozen check inside that helper is unreachable for frozen mirrors. Do not use b304 to judge the intended frozen-Kerfur hypothesis. b305 reorders only this gate: mapped Kerfur mirrors reach their scoped helper first (still excluding static actors), while non-Kerfur mirrors retain the generic frozen/static rejection before the PropDrive path. No protocol payload or interpolation behavior changes.
