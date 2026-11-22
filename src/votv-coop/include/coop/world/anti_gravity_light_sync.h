// coop/world/anti_gravity_light_sync.h -- mirror only the temporary anti-gravity camera light.
#pragma once

namespace coop::net { class Session; struct EventCuePayload; }

namespace coop::anti_gravity_light_sync {

// Called by the event-cue pump on the game thread. The host observes the exact cooked
// trigger_agrav PointLight visibility/destruction edges; a client creates the corresponding light
// on its own camera manager.
void Install(net::Session* session);

// Return true when the payload is one of this module's cue ids, including malformed payloads that
// this module consumes and rejects.
bool OnCue(const net::EventCuePayload& payload);

// Host: seed the active camera light to a peer at its world-ready edge.
void OnPeerWorldReady(int peerSlot);

// Destroy transient guest components and clear per-session occurrence state.
void OnDisconnect();

}  // namespace coop::anti_gravity_light_sync
