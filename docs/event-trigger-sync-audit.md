# Event trigger synchronization audit

**Branch:** `recode/b301-prop-pickup-bisect`, based on upstream `v0.9.0n-b292-dev`

**Source state:** protocol 294, event request and replay changes pass Windows Release build, ABI gate, and package verification; two-instance runtime verification is pending
**Evidence:** source paths and existing lane contracts. This is not a claim that these events have been verified in a two-instance game session.

## Authority and trigger flow

### Scheduled event

1. `event_fire_sync::Install` observes `daynightCycle_C::ReceiveTick`. On a connected client,
   `OnCycleTickPre` uses `time_sync::HoldsCycle` and sets the live save slot's `allEvents.Num`
   to zero before its cycle tick. That disables the client's copy of the scheduled walk; the
   allocated array and capacity are retained. Disconnect restores the saved count if the same
   slot remains live and empty.
2. The host's native `saveSlot::settime` calls `trigger_eventer_C::runEvent`. The eventer's
   post-watch broadcasts the call after the event body returns. A nested `runSpecialEvent` is
   separately observed after the host's prank roll chooses its case.
3. The host sends each connected peer separately. If a peer's reliable queue explicitly refuses
   admission, the host keeps a bounded retry tied to that peer generation, session, and world;
   retries stop after 12 attempts at 100 ms spacing. Already-admitted packets are left to the
   reliable transport.
4. The client decodes `EventFire`, checks the row policy, and either replays the same native verb
   or leaves the output to its dedicated state/actor lane. A replay waits for the gameplay world
   and eventer if necessary and retries once a second. Completed rows in the client's save and
   duplicate scheduled rows are skipped. In-flight event snapshots use the same policy.

### Manual event from a client

There are two sources: a direct native `runEvent` / `runSpecialEvent` call (caught by the
eventer's client pre-watch), and a selection from the developer event menu. Both cancel local
execution and send `EventFireRequest`; no client event body is allowed to create its own shared
world output. The host accepts only a seated, connected, world-ready peer; validates the exact
dispatch/name pair against `event_trigger::Events()`; and applies a 250 ms per-peer request
spacing. The host then executes the native eventer verb once. Only the host's post-watch publishes
the committed event. The request is not relayed as a result packet.

The event menu's random-prank entry is validated as the exact pair
`runEvent("arirInteraction_0", "ariralPrank")`; special-event and ambient entries are validated
against their own dispatch. Unknown names, malformed fields, and unsupported dispatch values are
dropped. Ambient entries invoke the registered weather function on the host; their result still
depends on a state lane (see below).

### Manual event from the host

The host's developer menu, game menu, and other host-native eventer calls pass through the same
post-watch. A manual bit in the high bit of `EventFirePayload.dispatch` distinguishes these
direct fires from `settime`'s scheduled fire. On replay-safe rows it prevents a client's saved
`passEvents` history or scheduled one-shot dedupe from swallowing a deliberate manual trigger.
The payload remains 64 bytes. The low seven dispatch bits retain the existing run/special values.

This bit does **not** make an unsafe row safe to replay. Rows marked no-replay still rely on their
dedicated lane or remain incomplete. The host is authoritative for shared world events; this
change does not create client-owned world actors or run host physics on the client.

## Current menu coverage

The menu contains 104 entries: 65 `runEvent` rows, 30 named `runSpecialEvent` cases, one random
prank entry, and eight ambient weather actions. The policy explicitly covers the 95 eventer rows:

### Replay through the native eventer (39 rows)

These rows have no output lane that would otherwise deliver their level/save/cosmetic flip,
scare-box arm, or sound emitters. They replay on a client:

`treehouse_0..5`, `break_RomeoSierra`, `break_Victor`, `break_Victor2`, `looker_0-1..4-1`,
`arirSignal`, `arirSpk`, `picSignal`, `peace`, `arirSat_0..2`, `piramid_sig`, `solar`, `call0`,
`toeStab`, `falseEnter`, `mann`, `vent`, `crys`, `fakeGrays`, `susArir`, `alienSounds`, and
`arirGraff_0..6`.

Manual direct fires on these rows bypass the client's prior scheduled-history check and do not
poison the scheduled replay dedupe. Scheduled fires retain one-shot dedupe.

### Do not replay: a separate lane owns the result (32 rows)

- `starRain`: event cue.
- `arirFollower`, `wisps`, `ventCrawler`, `tentacleBalls`, `graysforest`, `eggvasion`: NPC/event
  mirror paths; `wisps`, `tentacleBalls`, and `eggvasion` also depend on the documented EX-spawn
  catches.
- `piramid`: world-actor pose plus the pyramid brain/gather lane.
- `vehtp`: ATV state.
- `picnic`, `destroyPicnic`, `enasus`, `enacros`, `cookier`, `paperGray`, `arirEgg`: prop state.
- `console`, `lightswitch`, `keypadGuess`: their device lanes.
- `food`, `drive`, `atvFuel`, `atvFix`, `poisonFood`, `expDrive`, `cookiebox`, `trashPiles`,
  `vaccine`, `oil`, `begos`, `gascans`, `bombBox`: prank props/results on prop or device lanes.

These verdicts prevent duplicate local spawns or double-applied state. Their lane coverage should
still be judged by the lane's own test; a no-replay row alone is not proof that every visual or
side effect is present on clients.

### Partially synced or intentionally local

- `atvExplode`: the ATV lane carries the `trap` flag; the event's `zapped` side effect has no
  corresponding lane.
- `agrav`: the event and physics remain host-local by design. The b293 camera-light cue is
  separate cosmetic coverage; it does not synchronize anti-gravity physics.
- `treehouseSleep`: treehouse kidnapping/sleep and teleport target a particular player. Replaying
  it on every machine would target the wrong local player and duplicate host-side behavior.

## Known incomplete event outputs

The event request/replay work does not add actor-specific lanes. These rows remain host-triggered,
but a client may not see all of their output because the current replay policy deliberately skips
the native event body and the existing source-catch/lane does not cover its output:

| Event rows | Missing or partial output | Why it was not blindly replayed |
|---|---|---|
| `arirShip` | Armed overlap and ship/alarm scene output | Replaying the arm on each client can create a client-local ship when its overlap fires. Although `arirShip_C` is on the world-actor allowlist, the eventer EX-spawn catch does not list it; the exact spawn source is not proven by this checkout, so the allowlist alone is not evidence that this output reaches clients. |
| `obelisk` | Obelisk props and punch | Its event graph spawns props and targets `getMainPlayer`; replay can duplicate props and affect each machine's local player. |
| `earthTp` | New sky/black-hole scene, sound and world flip | Emails have an append lane, but `newsky_C.tp` and its scene state have no event-specific apply lane. |
| `bedEvent` | Bed/event actor state and wake/teleport transform | Sleep state is not the bed-event transform/teleport lane. Replaying on clients may target the wrong player. |
| `ventKnocker`, `morningGay`, `borgRozital` | `kocker_C`, `morningUfo_C`, and `rozitBorg_C` output | Classes are allowlisted for world actors, but the eventer EX-spawn catch does not enroll them. The mothership may also create child NPCs. |
| `graystank`, `arirBuster`, `boarwar` | Tank/pig, buster, and invasion output | The eventer EX-spawn output filter does not enroll these spawners; their child/BeginPlay behavior is not proven safe for a transform-only mirror. |
| `soltoClean` | `soltomiaCleaning_C` and related gameplay | Its BeginPlay writes the local ATV and spawns a sponge. Mirroring that BeginPlay would execute gameplay on the client, so the class is explicitly excluded from the eventer catch. |
| `salt`, `rozitalHole`, `dreambase` | Save-actor/build output | No actor/state lane in this recode tracks the full output. Replaying can create an unsaved or duplicate local scene. |
| `fallbody_0`, `fallbody_1`, `fallcar_0` | UFO dropper actors and their delayed drops | The eventer EX-spawn catch excludes the droppers; a transform-only parent mirror would not guarantee the delayed child/drop output. |
| `rockThrow`, `hillRoller`, `alienJump`, `trashBase` | Special-event spawner actors and their later behavior | Their resulting actors are not covered by an event-specific spawn/behavior lane. |

This is the smallest source-supported stopping point for this candidate. Adding one of these rows to
the native replay list without first proving its spawn source, target-player behavior, and mirror
BeginPlay safety can create duplicate actors or client-only gameplay. The next fixes should be
small per-family catches and state lanes, with the specific actor's source proven before enrollment.

## Ambient weather actions

All eight menu actions are now requestable from a client and execute on the host:
`spawnFog`, `rain ON`, `rain OFF`, `spawnRedSky`, `spawnBlackFog`, `badSun`, `flowerSpawner`, and
`trySpawnInsomniac`.

- `spawnFog`, rain, red sky, and Bad Sun have dedicated fog/weather/red-sky/Bad-Sun state paths.
- `spawnBlackFog` is explicitly client-birth-suppressed, and its comment says `blackFog_C` has no
  apply lane. The request runs on the host, but the black-fog/ghost visual is not fully shared.
- `flowerSpawner` has no output lane identified in this event audit; its flower births are not
  proven to reach clients.
- `trySpawnInsomniac` is internally gated and may do nothing; a successful spawn's full output is
  not established by the current source inventory.

## Conditions that can still drop a trigger

- A client request before it is connected or world-ready fails locally or is rejected by the host.
- Eventer name/watch resolution failure leaves no reliable host broadcast and, if the client watch
  is dead, can also leave native client cancellation unavailable. `event_fire_sync` logs this
  condition; the host must not claim successful coverage from the event table alone.
- The client's reliable send can fail admission; direct native calls are still canceled locally to
  preserve host authority.
- A host request can be rejected as malformed, unregistered, unready, or within the 250 ms rate
  interval. There is no request acknowledgment packet, so host logs distinguish acceptance from
  rejection; the client-side `ClientRequest` success means reliable-queue admission only.
- A host event's reliable queue can refuse admission. The host retries that peer up to 12 times at
  100 ms spacing; the debt is dropped if the peer generation, session, or world changes. A full
  debt queue or 12 refusals leaves the output missing. There is no application-level receipt ack,
  so a packet the transport admitted but the client later rejects is not retried here.
- A row can be known but not fully visible because of the incomplete outputs above, an unready
  target world, a missing event actor class, or a lane-specific failure.

## Source files and runtime verification

- Scheduled/client suppression, request validation, host fire and replay: `src/votv-coop/src/coop/world/event_fire_sync.cpp`.
- Replay versus no-replay decisions: `src/votv-coop/src/coop/world/event_fire_policy.cpp`.
- Client menu list and dispatch: `src/votv-coop/src/coop/dev/event_trigger.cpp`.
- Event request kind, payload, version: `src/votv-coop/include/coop/net/protocol.h`.
- Host receive routing and client-authorship boundary: `src/votv-coop/src/coop/dispatch/event_dispatch_world.cpp`, `src/votv-coop/src/coop/net/client_authorship.h`.
- Actor source allowlists: `src/votv-coop/src/coop/creatures/npc_world_enum.cpp`,
  `src/votv-coop/include/ue_wrap/core/sdk_profile_names.h`.

The candidate passed Windows Release build, ABI gate, and package verification in
[CI run 45038505058](https://github.com/boiling-tea-cup-26/Multivoid/actions/runs/45038505058).
Runtime verification still requires matching protocol-294 clients. Check a safe replayable event
(`solar`) from the client event menu and then the host event menu; both instances should show one
trigger per click. Check `rain ON` then `rain OFF` from the client and confirm the host's weather
state follows. Also let a scheduled safe event cross its time on the host and confirm the client
does not independently fire it. Do not use destructive story rows for the first smoke test. These
checks verify trigger direction and the named lanes only; they do not prove the incomplete rows
above are fixed.

Useful bounded log search terms: `event_fire: client requested`, `event_fire: accepted`,
`event_fire: refused`, `event_fire: host fire`, `event_fire: broadcast`,
`event_fire: client REPLAY`, `event_fire: ... NOT replayed`, and `event_fire: ... DROPPED`.
