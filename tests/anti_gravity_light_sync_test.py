#!/usr/bin/env python3
"""Focused source-contract checks for the anti-gravity camera-light cue."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/votv-coop/src/coop/world/anti_gravity_light_sync.cpp").read_text()
CUE_SOURCE = (ROOT / "src/votv-coop/src/coop/world/event_cue_sync.cpp").read_text()
PROTOCOL = (ROOT / "src/votv-coop/include/coop/net/protocol.h").read_text()

checks = {
    "protocol bump and occurrence identity are explicit":
        "kProtocolVersion = 294" in PROTOCOL and
        "uint32_t hostWorldEpoch" in PROTOCOL and
        "uint32_t occurrenceSeq" in PROTOCOL and
        'sizeof(EventCuePayload) == 24' in PROTOCOL and
        "payload.hostWorldEpoch = worldEpoch" in SOURCE,
    "host watches the cooked light visibility and destroy call sites":
        "kAgravLightOnPc = 705" in SOURCE and
        "kAgravLightOffPc = 1918" in SOURCE and
        'L\"ExecuteUbergraph_trigger_agrav\"' in SOURCE and
        'L\"SetVisibility\"' in SOURCE and
        'L\"K2_DestroyComponent\"' in SOURCE,
    "post-hook uses the target component argument, not the caller actor":
        "void OnSetVisibilityPost(void* context, void* sourceObject, void*)" in SOURCE and
        "IsAgravLightComponent(context, sourceObject)" in SOURCE and
        "void OnDestroyComponentPost(void* context, void* sourceObject, void*)" in SOURCE and
        "row.component != context || row.owner != sourceObject" in SOURCE,
    "start and stop carry the same host-world and occurrence sequence":
        "SendEdge(*session, kCueAgravLightOn, world, g_hostSequence" in SOURCE and
        "SendEdge(*session, kCueAgravLightOff, world, sequence" in SOURCE and
        "row.payload.occurrenceSeq == payload.occurrenceSeq" in SOURCE,
    "guest light attaches to its own camera manager":
        'L\"GetPlayerCameraManager\"' in SOURCE and
        "Registry::Get().Local()" in SOURCE and
        "RootComponentOf(cameraManager)" in SOURCE and
        "ue_wrap::Call(component, attach)" in SOURCE,
    "cue routes before generic unknown-cue rejection":
        "anti_gravity_light_sync::OnCue(payload)" in CUE_SOURCE and
        CUE_SOURCE.index("anti_gravity_light_sync::OnCue(payload)") <
        CUE_SOURCE.index("payload.cueId >= static_cast<uint32_t>(kCueCount)"),
    "cleanup is bounded and does not replay event gameplay":
        "kGuestApplyMaxAge = std::chrono::milliseconds(1200)" in SOURCE and
        "kGuestSafetyLifetime = std::chrono::seconds(60)" in SOURCE and
        "ClearGuestOccurrences()" in SOURCE and
        "SetEnableGravity" not in SOURCE and "event_fire_sync::" not in SOURCE,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("PASS" if ok else "FAIL") + " " + name)
print(f"{len(checks) - len(failed)}/{len(checks)} source-contract checks passed")
raise SystemExit(bool(failed))
