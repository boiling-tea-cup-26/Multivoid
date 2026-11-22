// coop/world/anti_gravity_light_sync.cpp -- mirror the temporary camera PointLight only.

#include "coop/world/anti_gravity_light_sync.h"

#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"

#include "ue_wrap/core/asset_load.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/facts/function_row.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <utility>

namespace coop::anti_gravity_light_sync {
namespace {

namespace P = ue_wrap::profile;
namespace R = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace UH = ue_wrap::ufunction_hook;

constexpr uint32_t kCueAgravLightOn = 108;
constexpr uint32_t kCueAgravLightOff = 109;
constexpr int32_t kAgravLightOnPc = 705;
constexpr int32_t kAgravLightOffPc = 1918;
constexpr size_t kOccurrenceCapacity = 8;
constexpr uint8_t kGuestApplyAttempts = 8;
constexpr auto kGuestApplyInterval = std::chrono::milliseconds(100);
constexpr auto kGuestApplyMaxAge = std::chrono::milliseconds(1200);
constexpr auto kGuestSafetyLifetime = std::chrono::seconds(60);

using Clock = std::chrono::steady_clock;

UE_FACT_FN(kRowSetVisibility, "anti_gravity_light_sync", L"SceneComponent", L"SetVisibility", Declared);
UE_FACT_FN(kRowDestroyComponent, "anti_gravity_light_sync", L"ActorComponent", L"K2_DestroyComponent", Declared);

struct HostOccurrence {
    void* component = nullptr;
    void* owner = nullptr;
    int32_t objectIndex = -1;
    int32_t objectSerial = 0;
    uint32_t worldEpoch = 0;
    uint32_t sequence = 0;
};

struct GuestOccurrence {
    bool used = false;
    bool active = false;
    coop::net::Session* session = nullptr;
    coop::net::EventCuePayload payload{};
    void* component = nullptr;
    int32_t objectIndex = -1;
    uint32_t localWorld = 0;
    uint8_t attempts = 0;
    Clock::time_point created{};
    Clock::time_point nextAttempt{};
    Clock::time_point expires{};
};

coop::net::Session* g_session = nullptr;
std::array<HostOccurrence, kOccurrenceCapacity> g_hostOccurrences{};
std::array<GuestOccurrence, kOccurrenceCapacity> g_guestOccurrences{};
uint32_t g_hostWorld = 0;
uint32_t g_hostSequence = 0;
uint32_t g_guestSeenWorld = 0;
uint32_t g_guestSeenSequence = 0;
bool g_guestHasSeen = false;
uint32_t g_guestLocalWorld = 0;

void* g_setVisibilityFn = nullptr;
void* g_destroyComponentFn = nullptr;
bool g_setVisibilityHookInstalled = false;
bool g_destroyHookInstalled = false;
bool g_setVisibilityHookWarned = false;
bool g_destroyHookWarned = false;

void* g_gameplayStaticsCdo = nullptr;
void* g_getPlayerCameraManagerFn = nullptr;
void* g_agravClass = nullptr;
void* g_agravPointLightTemplate = nullptr;
void* g_addComponentFn = nullptr;
void* g_finishAddComponentFn = nullptr;
void* g_attachSceneFn = nullptr;
void* g_setCastShadowsFn = nullptr;
void* g_setInverseFalloffFn = nullptr;

bool IsAgravCaller(const UH::CallerFrame& frame, void* owner, int32_t expectedOffset) {
    return owner && frame.function && frame.codeOffset == expectedOffset &&
           R::NameEquals(R::NameOf(frame.function), L"ExecuteUbergraph_trigger_agrav") &&
           R::NameEquals(R::NameOf(R::ClassOf(owner)), L"trigger_agrav_C");
}

bool IsAgravLightComponent(void* component, void* caller) {
    if (!component || !R::IsLive(component) ||
        !R::NameEquals(R::NameOf(R::ClassOf(component)), L"PointLightComponent") ||
        !R::NameEquals(R::NameOf(component), L"PointLight_GEN_VARIABLE")) return false;
    void* owner = R::OuterOf(component);
    if (!owner || owner != caller || !R::IsLive(owner) ||
        !R::NameEquals(R::NameOf(R::ClassOf(owner)), L"trigger_agrav_C")) return false;
    int32_t offset = R::FindPropertyOffset(R::ClassOf(owner), L"PointLight");
    if (offset < 0) offset = R::FindPropertyOffset(R::ClassOf(owner), L"PointLight_GEN_VARIABLE");
    if (offset < 0) return false;
    void* expected = nullptr;
    std::memcpy(&expected, static_cast<const uint8_t*>(owner) + offset, sizeof(expected));
    return expected == component;
}

void ClearHostOccurrences() {
    for (auto& row : g_hostOccurrences) row = HostOccurrence{};
    g_hostSequence = 0;
}

void ClearGuestOccurrence(GuestOccurrence& row) {
    if (row.component && row.objectIndex >= 0 &&
        R::IsLiveByIndex(row.component, row.objectIndex)) {
        if (!g_destroyComponentFn) {
            void* componentClass = R::FindClass(L"ActorComponent");
            if (componentClass) g_destroyComponentFn = R::FindFunction(componentClass, L"K2_DestroyComponent");
        }
        if (g_destroyComponentFn) {
            ue_wrap::ParamFrame destroy(g_destroyComponentFn);
            if (destroy.valid()) ue_wrap::Call(row.component, destroy);
        } else {
            UE_LOGW("anti_gravity_light_sync: cannot resolve K2_DestroyComponent for guest light cleanup");
        }
    }
    row = GuestOccurrence{};
}

void ClearGuestOccurrences() {
    for (auto& row : g_guestOccurrences) ClearGuestOccurrence(row);
}

void ResetState() {
    ClearHostOccurrences();
    ClearGuestOccurrences();
    g_hostWorld = 0;
    g_guestSeenWorld = 0;
    g_guestSeenSequence = 0;
    g_guestHasSeen = false;
    g_guestLocalWorld = 0;
}

bool SendEdge(coop::net::Session& session, uint32_t cueId, uint32_t worldEpoch,
              uint32_t sequence, int peerSlot) {
    if (!session.connected() || session.role() != coop::net::Role::Host ||
        !worldEpoch || !sequence) return false;
    coop::net::EventCuePayload payload{};
    payload.cueId = cueId;
    payload.hostWorldEpoch = worldEpoch;
    payload.occurrenceSeq = sequence;
    if (peerSlot >= 1) {
        if (!session.IsSlotConnected(peerSlot) || !session.IsSlotWorldReady(peerSlot)) return false;
        return session.SendReliableToSlot(peerSlot, coop::net::ReliableKind::EventCue,
                                          &payload, sizeof(payload));
    }
    bool admitted = true;
    bool attempted = false;
    for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers); ++slot) {
        if (!session.IsSlotConnected(slot) || !session.IsSlotWorldReady(slot)) continue;
        attempted = true;
        if (!session.SendReliableToSlot(slot, coop::net::ReliableKind::EventCue,
                                       &payload, sizeof(payload))) {
            UE_LOGW("anti_gravity_light_sync: cue=%u world=%u seq=%u admission failed slot=%d",
                    cueId, worldEpoch, sequence, slot);
            admitted = false;
        }
    }
    return attempted && admitted;
}

void OnSetVisibilityPost(void* context, void* sourceObject, void*) {
    if (!GT::IsGameThread()) return;
    auto* session = g_session;
    if (!session || !session->connected() || session->role() != coop::net::Role::Host) return;
    const auto frame = UH::CurrentCallerFrame();
    // For a post-native hook, context is the component SetVisibility ran on. sourceObject is the
    // trigger_agrav actor whose Blueprint called it. PC 705 is the cooked SetVisibility(true) call.
    if (!IsAgravCaller(frame, sourceObject, kAgravLightOnPc) ||
        !IsAgravLightComponent(context, sourceObject)) return;
    const int32_t objectIndex = R::InternalIndexOf(context);
    if (objectIndex < 0) return;
    const int32_t objectSerial = R::AllocateSlotSerial(objectIndex);
    const uint32_t world = ue_wrap::world_identity::Generation();
    if (!objectSerial || !world) return;
    if (g_hostWorld != world) {
        ClearHostOccurrences();
        g_hostWorld = world;
    }
    for (auto& row : g_hostOccurrences) {
        if (row.component == context && row.owner == sourceObject &&
            row.objectIndex == objectIndex && row.objectSerial == objectSerial) return;
    }
    HostOccurrence* freeRow = nullptr;
    for (auto& row : g_hostOccurrences) {
        if (row.component && !R::IsLiveByIndex(row.component, row.objectIndex)) row = HostOccurrence{};
        if (!row.component && !freeRow) freeRow = &row;
    }
    if (!freeRow) {
        UE_LOGW("anti_gravity_light_sync: host occurrence table full (cap=%zu)", g_hostOccurrences.size());
        return;
    }
    ++g_hostSequence;
    if (!g_hostSequence) ++g_hostSequence;
    *freeRow = HostOccurrence{context, sourceObject, objectIndex, objectSerial, world, g_hostSequence};
    if (!SendEdge(*session, kCueAgravLightOn, world, g_hostSequence, -1))
        UE_LOGW("anti_gravity_light_sync: host light start world=%u seq=%u had no ready-peer admission",
                world, g_hostSequence);
    else
        UE_LOGI("anti_gravity_light_sync: host light start world=%u seq=%u", world, g_hostSequence);
}

void OnDestroyComponentPost(void* context, void* sourceObject, void*) {
    if (!GT::IsGameThread()) return;
    const auto frame = UH::CurrentCallerFrame();
    // As above, the destroyed component is context; sourceObject is the calling Blueprint actor.
    if (!IsAgravCaller(frame, sourceObject, kAgravLightOffPc)) return;
    for (auto& row : g_hostOccurrences) {
        if (row.component != context || row.owner != sourceObject) continue;
        const uint32_t world = row.worldEpoch;
        const uint32_t sequence = row.sequence;
        row = HostOccurrence{};
        auto* session = g_session;
        if (!session || world != ue_wrap::world_identity::Generation()) return;
        if (!SendEdge(*session, kCueAgravLightOff, world, sequence, -1))
            UE_LOGW("anti_gravity_light_sync: host light stop world=%u seq=%u had no ready-peer admission",
                    world, sequence);
        else
            UE_LOGI("anti_gravity_light_sync: host light stop world=%u seq=%u", world, sequence);
        return;
    }
}

bool CopyProperty(void* source, void* target, const wchar_t* name, size_t size) {
    void* cls = R::ClassOf(source);
    const int32_t offset = cls ? R::FindPropertyOffset(cls, name) : -1;
    if (offset < 0) return false;
    std::memcpy(static_cast<uint8_t*>(target) + offset,
                static_cast<const uint8_t*>(source) + offset, size);
    return true;
}

bool CopyBoolProperty(void* source, void* target, const wchar_t* name) {
    int32_t offset = -1;
    uint8_t mask = 0;
    void* cls = R::ClassOf(source);
    if (!cls || !R::FindBoolProperty(cls, name, offset, mask) || offset < 0 || !mask) return false;
    auto* sourceByte = static_cast<const uint8_t*>(source) + offset;
    auto* targetByte = static_cast<uint8_t*>(target) + offset;
    *targetByte = static_cast<uint8_t>((*targetByte & ~mask) | (*sourceByte & mask));
    return true;
}

bool ResolveLightTemplate() {
    if (!g_agravClass) g_agravClass = R::FindClass(L"trigger_agrav_C");
    if (!g_agravClass)
        g_agravClass = ue_wrap::asset_load::LoadObjectByPath(
            L"/Game/objects/trigger_agrav.trigger_agrav_C");
    if (!g_agravClass) return false;
    if (!g_agravPointLightTemplate) {
        void* cdo = R::FindClassDefaultObject(L"trigger_agrav_C");
        int32_t offset = R::FindPropertyOffset(g_agravClass, L"PointLight");
        if (offset < 0) offset = R::FindPropertyOffset(g_agravClass, L"PointLight_GEN_VARIABLE");
        if (cdo && offset >= 0)
            std::memcpy(&g_agravPointLightTemplate,
                        static_cast<const uint8_t*>(cdo) + offset, sizeof(g_agravPointLightTemplate));
    }
    return g_agravPointLightTemplate && R::IsLive(g_agravPointLightTemplate) &&
           R::NameEquals(R::NameOf(R::ClassOf(g_agravPointLightTemplate)), L"PointLightComponent");
}

bool ResolveComponentCalls() {
    if (!g_gameplayStaticsCdo)
        g_gameplayStaticsCdo = R::FindClassDefaultObject(P::name::GameplayStaticsClass);
    if (g_gameplayStaticsCdo && !g_getPlayerCameraManagerFn) {
        if (void* cls = R::ClassOf(g_gameplayStaticsCdo))
            g_getPlayerCameraManagerFn = R::FindFunction(cls, L"GetPlayerCameraManager");
    }
    if (!g_addComponentFn || !g_finishAddComponentFn) {
        void* actorClass = R::FindClass(P::name::ActorClassName);
        if (actorClass) {
            if (!g_addComponentFn) g_addComponentFn = R::FindFunction(actorClass, P::name::AddComponentByClassFn);
            if (!g_finishAddComponentFn) g_finishAddComponentFn = R::FindFunction(actorClass, P::name::FinishAddComponentFn);
        }
    }
    if (!g_attachSceneFn) {
        void* sceneClass = R::FindClass(L"SceneComponent");
        if (sceneClass) g_attachSceneFn = R::FindFunction(sceneClass, L"K2_AttachToComponent");
    }
    if (!g_setCastShadowsFn) {
        void* lightClass = R::FindClass(L"LightComponentBase");
        if (lightClass) g_setCastShadowsFn = R::FindFunction(lightClass, L"SetCastShadows");
    }
    if (!g_setInverseFalloffFn) {
        void* pointClass = R::FindClass(L"PointLightComponent");
        if (pointClass) g_setInverseFalloffFn = R::FindFunction(pointClass, L"SetUseInverseSquaredFalloff");
    }
    return ResolveLightTemplate() && g_gameplayStaticsCdo && g_getPlayerCameraManagerFn &&
           g_addComponentFn && g_finishAddComponentFn && g_attachSceneFn &&
           g_setCastShadowsFn && g_setInverseFalloffFn && g_setVisibilityFn;
}

void* CreateCameraLight(void* localPlayer) {
    if (!localPlayer || !R::IsLive(localPlayer) || !ResolveComponentCalls()) return nullptr;
    ue_wrap::ParamFrame camera(g_getPlayerCameraManagerFn);
    if (!camera.valid() || !camera.Set<void*>(L"WorldContextObject", localPlayer) ||
        !camera.Set<int32_t>(L"PlayerIndex", 0) || !ue_wrap::Call(g_gameplayStaticsCdo, camera)) return nullptr;
    void* cameraManager = camera.Get<void*>(L"ReturnValue");
    void* cameraRoot = cameraManager && R::IsLive(cameraManager)
        ? ue_wrap::engine::RootComponentOf(cameraManager) : nullptr;
    if (!cameraRoot) return nullptr;

    void* lightClass = R::ClassOf(g_agravPointLightTemplate);
    if (!lightClass) return nullptr;
    ue_wrap::FVector relativeLocation{};
    ue_wrap::FRotator relativeRotation{};
    ue_wrap::FVector relativeScale{1.f, 1.f, 1.f};
    void* templateClass = R::ClassOf(g_agravPointLightTemplate);
    const int32_t locationOffset = R::FindPropertyOffset(templateClass, L"RelativeLocation");
    const int32_t rotationOffset = R::FindPropertyOffset(templateClass, L"RelativeRotation");
    const int32_t scaleOffset = R::FindPropertyOffset(templateClass, L"RelativeScale3D");
    if (locationOffset < 0 || rotationOffset < 0 || scaleOffset < 0) return nullptr;
    std::memcpy(&relativeLocation, static_cast<const uint8_t*>(g_agravPointLightTemplate) + locationOffset,
                sizeof(relativeLocation));
    std::memcpy(&relativeRotation, static_cast<const uint8_t*>(g_agravPointLightTemplate) + rotationOffset,
                sizeof(relativeRotation));
    std::memcpy(&relativeScale, static_cast<const uint8_t*>(g_agravPointLightTemplate) + scaleOffset,
                sizeof(relativeScale));
    ue_wrap::FTransform transform{};
    ue_wrap::engine::RotatorToQuat(relativeRotation.Pitch, relativeRotation.Yaw, relativeRotation.Roll,
                                   transform.RotX, transform.RotY, transform.RotZ, transform.RotW);
    transform.TX = relativeLocation.X; transform.TY = relativeLocation.Y; transform.TZ = relativeLocation.Z;
    transform.SX = relativeScale.X; transform.SY = relativeScale.Y; transform.SZ = relativeScale.Z;

    ue_wrap::ParamFrame add(g_addComponentFn);
    if (!add.valid() || !add.Set<void*>(L"Class", lightClass) ||
        !add.Set<bool>(L"bManualAttachment", true) ||
        !add.SetRaw(L"RelativeTransform", &transform, sizeof(transform)) ||
        !add.Set<bool>(L"bDeferredFinish", true) || !ue_wrap::Call(cameraManager, add)) return nullptr;
    void* component = add.Get<void*>(L"ReturnValue");
    if (!component || !R::IsLive(component)) return nullptr;
    auto discard = [&]() -> void* {
        if (!g_destroyComponentFn) {
            void* componentClass = R::FindClass(L"ActorComponent");
            if (componentClass) g_destroyComponentFn = R::FindFunction(componentClass, L"K2_DestroyComponent");
        }
        if (g_destroyComponentFn) {
            ue_wrap::ParamFrame destroy(g_destroyComponentFn);
            if (destroy.valid()) ue_wrap::Call(component, destroy);
        }
        return nullptr;
    };

    for (const auto& property : {
             std::pair<const wchar_t*, size_t>{L"Intensity", sizeof(float)},
             {L"AttenuationRadius", sizeof(float)}, {L"SourceRadius", sizeof(float)},
             {L"LightFalloffExponent", sizeof(float)}, {L"LightColor", sizeof(ue_wrap::FColor)}}) {
        if (!CopyProperty(g_agravPointLightTemplate, component, property.first, property.second)) {
            UE_LOGW("anti_gravity_light_sync: template property missing: %ls", property.first);
            return discard();
        }
    }
    if (!CopyBoolProperty(g_agravPointLightTemplate, component, L"bUseInverseSquaredFalloff")) return discard();

    ue_wrap::ParamFrame finish(g_finishAddComponentFn);
    if (!finish.valid() || !finish.Set<void*>(L"Component", component) ||
        !finish.Set<bool>(L"bManualAttachment", true) ||
        !finish.SetRaw(L"RelativeTransform", &transform, sizeof(transform)) ||
        !ue_wrap::Call(cameraManager, finish)) return discard();
    ue_wrap::ParamFrame attach(g_attachSceneFn);
    const R::FName none{};
    if (!attach.valid() || !attach.Set<void*>(L"Parent", cameraRoot) ||
        !attach.SetRaw(L"SocketName", &none, sizeof(none)) ||
        !attach.Set<uint8_t>(L"LocationRule", 0) || !attach.Set<uint8_t>(L"RotationRule", 0) ||
        !attach.Set<uint8_t>(L"ScaleRule", 0) || !attach.Set<bool>(L"bWeldSimulatedBodies", false) ||
        !ue_wrap::Call(component, attach)) return discard();
    ue_wrap::ParamFrame shadows(g_setCastShadowsFn);
    if (!shadows.valid() || !shadows.Set<bool>(L"bNewValue", false) ||
        !ue_wrap::Call(component, shadows)) return discard();
    ue_wrap::ParamFrame falloff(g_setInverseFalloffFn);
    if (!falloff.valid() || !falloff.Set<bool>(L"bNewValue", false) ||
        !ue_wrap::Call(component, falloff)) return discard();
    ue_wrap::ParamFrame visible(g_setVisibilityFn);
    if (!visible.valid() || !visible.Set<bool>(L"bNewVisibility", true) ||
        !visible.Set<bool>(L"bPropagateToChildren", false) ||
        !ue_wrap::Call(component, visible)) return discard();
    return component;
}

bool IsNewer(uint32_t candidate, uint32_t previous) {
    return static_cast<int32_t>(candidate - previous) > 0;
}

bool ValidCue(const coop::net::EventCuePayload& payload) {
    return (payload.cueId == kCueAgravLightOn || payload.cueId == kCueAgravLightOff) &&
           payload.x == 0.f && payload.y == 0.f && payload.z == 0.f &&
           payload.hostWorldEpoch != 0 && payload.occurrenceSeq != 0;
}

void DestroyGuestLight(GuestOccurrence& row) {
    if (row.component && row.objectIndex >= 0 &&
        R::IsLiveByIndex(row.component, row.objectIndex)) {
        if (!g_destroyComponentFn) {
            void* componentClass = R::FindClass(L"ActorComponent");
            if (componentClass) g_destroyComponentFn = R::FindFunction(componentClass, L"K2_DestroyComponent");
        }
        if (g_destroyComponentFn) {
            ue_wrap::ParamFrame destroy(g_destroyComponentFn);
            if (destroy.valid()) ue_wrap::Call(row.component, destroy);
        } else {
            UE_LOGW("anti_gravity_light_sync: cannot resolve K2_DestroyComponent for guest light cleanup");
        }
    }
    row = GuestOccurrence{};
}

void QueueGuestStart(const coop::net::EventCuePayload& payload) {
    if (g_guestHasSeen) {
        if (payload.hostWorldEpoch == g_guestSeenWorld) {
            if (!IsNewer(payload.occurrenceSeq, g_guestSeenSequence)) return;
        } else if (!IsNewer(payload.hostWorldEpoch, g_guestSeenWorld)) {
            return;
        }
    }
    GuestOccurrence* freeRow = nullptr;
    for (auto& row : g_guestOccurrences) {
        if (row.used && row.payload.hostWorldEpoch == payload.hostWorldEpoch &&
            row.payload.occurrenceSeq == payload.occurrenceSeq) return;
        if (!row.used && !freeRow) freeRow = &row;
    }
    if (!freeRow) {
        UE_LOGW("anti_gravity_light_sync: guest occurrence table full (cap=%zu)", g_guestOccurrences.size());
        return;
    }
    const auto now = Clock::now();
    freeRow->used = true;
    freeRow->session = g_session;
    freeRow->payload = payload;
    freeRow->localWorld = ue_wrap::world_identity::Generation();
    freeRow->created = now;
    freeRow->nextAttempt = now;
    g_guestHasSeen = true;
    g_guestSeenWorld = payload.hostWorldEpoch;
    g_guestSeenSequence = payload.occurrenceSeq;
}

void QueueGuestStop(const coop::net::EventCuePayload& payload) {
    for (auto& row : g_guestOccurrences) {
        if (row.used && row.payload.hostWorldEpoch == payload.hostWorldEpoch &&
            row.payload.occurrenceSeq == payload.occurrenceSeq) {
            DestroyGuestLight(row);
            return;
        }
    }
}

void TickHost(coop::net::Session& session) {
    const uint32_t world = ue_wrap::world_identity::Generation();
    if (!world) return;
    if (world != g_hostWorld) {
        ClearHostOccurrences();
        g_hostWorld = world;
    }
    for (auto& row : g_hostOccurrences) {
        if (row.component && !R::IsLiveByIndex(row.component, row.objectIndex)) row = HostOccurrence{};
    }
    (void)session;
}

void TickGuest(coop::net::Session& session) {
    const uint32_t world = ue_wrap::world_identity::Generation();
    if (g_guestLocalWorld && world && g_guestLocalWorld != world) ClearGuestOccurrences();
    if (world) g_guestLocalWorld = world;
    const auto now = Clock::now();
    for (auto& row : g_guestOccurrences) {
        if (!row.used) continue;
        if (row.session != &session || (row.localWorld && world && row.localWorld != world)) {
            DestroyGuestLight(row);
            continue;
        }
        if (row.active) {
            if (now >= row.expires) {
                UE_LOGW("anti_gravity_light_sync: guest light safety timeout world=%u seq=%u",
                        row.payload.hostWorldEpoch, row.payload.occurrenceSeq);
                DestroyGuestLight(row);
            }
            continue;
        }
        if (now - row.created >= kGuestApplyMaxAge || row.attempts >= kGuestApplyAttempts) {
            UE_LOGW("anti_gravity_light_sync: guest start expired world=%u seq=%u attempts=%u",
                    row.payload.hostWorldEpoch, row.payload.occurrenceSeq,
                    static_cast<unsigned>(row.attempts));
            DestroyGuestLight(row);
            continue;
        }
        if (!world || now < row.nextAttempt) continue;
        if (!row.localWorld) row.localWorld = world;
        void* localPlayer = coop::players::Registry::Get().Local();
        if (!localPlayer) {
            row.nextAttempt = now + kGuestApplyInterval;
            continue;
        }
        ++row.attempts;
        row.component = CreateCameraLight(localPlayer);
        if (row.component) {
            row.objectIndex = R::InternalIndexOf(row.component);
            row.active = row.objectIndex >= 0;
            row.expires = now + kGuestSafetyLifetime;
            if (!row.active) DestroyGuestLight(row);
            else UE_LOGI("anti_gravity_light_sync: guest light active world=%u seq=%u",
                         row.payload.hostWorldEpoch, row.payload.occurrenceSeq);
        } else {
            row.nextAttempt = now + kGuestApplyInterval;
        }
    }
}

void TryInstallHooks() {
    if (!g_setVisibilityHookInstalled) {
        if (const auto function = ue_wrap::facts::Resolve(kRowSetVisibility)) {
            g_setVisibilityFn = function.Fn();
            g_setVisibilityHookInstalled = UH::InstallPostHook(function, &OnSetVisibilityPost);
            if (!g_setVisibilityHookInstalled && !g_setVisibilityHookWarned) {
                g_setVisibilityHookWarned = true;
                UE_LOGW("anti_gravity_light_sync: SetVisibility hook install failed");
            }
        }
    }
    if (!g_destroyHookInstalled) {
        if (const auto function = ue_wrap::facts::Resolve(kRowDestroyComponent)) {
            g_destroyComponentFn = function.Fn();
            g_destroyHookInstalled = UH::InstallPostHook(function, &OnDestroyComponentPost);
            if (!g_destroyHookInstalled && !g_destroyHookWarned) {
                g_destroyHookWarned = true;
                UE_LOGW("anti_gravity_light_sync: K2_DestroyComponent hook install failed");
            }
        }
    }
}

}  // namespace

void Install(net::Session* session) {
    if (g_session != session) {
        ResetState();
        g_session = session;
    }
    TryInstallHooks();
    if (!session || !session->connected()) return;
    if (session->role() == coop::net::Role::Host) TickHost(*session);
    else TickGuest(*session);
}

bool OnCue(const net::EventCuePayload& payload) {
    if (payload.cueId != kCueAgravLightOn && payload.cueId != kCueAgravLightOff) return false;
    if (!GT::IsGameThread()) {
        UE_LOGW("anti_gravity_light_sync: cue received off game thread; dropping");
        return true;
    }
    if (!ValidCue(payload)) {
        UE_LOGW("anti_gravity_light_sync: malformed cue=%u world=%u seq=%u",
                payload.cueId, payload.hostWorldEpoch, payload.occurrenceSeq);
        return true;
    }
    if (!g_session || !g_session->connected() || g_session->role() != coop::net::Role::Client) return true;
    if (payload.cueId == kCueAgravLightOn) QueueGuestStart(payload);
    else QueueGuestStop(payload);
    return true;
}

void OnPeerWorldReady(int peerSlot) {
    auto* session = g_session;
    if (!GT::IsGameThread() || !session || !session->connected() ||
        session->role() != coop::net::Role::Host || peerSlot < 1 ||
        peerSlot >= static_cast<int>(coop::net::kMaxPeers) ||
        !session->IsSlotConnected(peerSlot) || !session->IsSlotWorldReady(peerSlot)) return;
    const uint32_t world = ue_wrap::world_identity::Generation();
    if (!world || world != g_hostWorld) return;
    for (const auto& row : g_hostOccurrences) {
        if (!row.component || row.worldEpoch != world ||
            !R::IsLiveByIndex(row.component, row.objectIndex) ||
            R::SlotSerial(row.objectIndex) != row.objectSerial) continue;
        if (!SendEdge(*session, kCueAgravLightOn, row.worldEpoch, row.sequence, peerSlot))
            UE_LOGW("anti_gravity_light_sync: join seed refused slot=%d world=%u seq=%u",
                    peerSlot, row.worldEpoch, row.sequence);
    }
}

void OnDisconnect() {
    ResetState();
    g_session = nullptr;
}

}  // namespace coop::anti_gravity_light_sync
