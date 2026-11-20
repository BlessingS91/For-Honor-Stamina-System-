#include "Hooks.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../Settings.h"
#include "TrueHud.h"

namespace Stamina::Hooks {

    namespace {

        constexpr std::size_t kUpdateIndex = 0xAD;
        constexpr RE::FormID kOutOfStaminaEffect = 0x080C;
        constexpr RE::FormID kStaminaRecoverySpell = 0x0A57;
        constexpr auto kPluginName = "For Honor Stamina System.esp";
        constexpr std::uint32_t kPruneEveryPlayerUpdates = 600;

        // Engine-owned forms are stable for the lifetime of the loaded game data.
        RE::EffectSetting* outOfStaminaEffect = nullptr;
        RE::SpellItem* staminaRecoverySpell = nullptr;

        // Only actors that actually entered exhaustion get tracked. Handles
        // prevent stale RE::Actor* keys from outliving unloaded references.
        std::unordered_map<std::uint32_t, RE::ActorHandle> exhaustedActors;
        std::mutex exhaustedActorsMutex;
        std::atomic<bool> anyExhaustedActors{false};
        std::uint32_t playerUpdateCounter = 0;

        void CacheForms() {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                logger::error("[Stamina] Cannot cache exhaustion forms: TESDataHandler unavailable.");
                return;
            }

            outOfStaminaEffect = dataHandler->LookupForm<RE::EffectSetting>(kOutOfStaminaEffect, kPluginName);
            staminaRecoverySpell = dataHandler->LookupForm<RE::SpellItem>(kStaminaRecoverySpell, kPluginName);

            if (!outOfStaminaEffect) {
                logger::warn("[Stamina] Out of Stamina effect not found; exhaustion tracking unavailable.");
            }
            if (!staminaRecoverySpell) {
                logger::warn("[Stamina] Recovery spell not found; recovery casts unavailable.");
            }
        }

        bool HasOutOfStaminaEffect(RE::Actor* actor) {
            if (!actor || !outOfStaminaEffect) {
                return false;
            }

            auto* magicTarget = actor->AsMagicTarget();
            if (!magicTarget) {
                return false;
            }

            auto* effects = magicTarget->GetActiveEffectList();
            if (!effects) {
                return false;
            }

            // Keep the original active-effect test (rather than changing
            // which effect states are considered exhausted).
            for (auto* activeEffect : *effects) {
                if (activeEffect && activeEffect->effect &&
                    activeEffect->effect->baseEffect == outOfStaminaEffect) {
                    return true;
                }
            }

            return false;
        }

        void UpdateActorExhaustion(RE::Actor* actor) {
            if (!actor || !outOfStaminaEffect) {
                return;
            }

            const bool exhausted = HasOutOfStaminaEffect(actor);

            // Almost all actors have no relevant state. Avoid even obtaining
            // a handle or touching the map in the common non-exhausted case.
            if (!exhausted && !anyExhaustedActors.load(std::memory_order_relaxed)) {
                return;
            }

            const auto handle = actor->GetHandle();
            if (!handle) {
                return;
            }
            const auto key = handle.native_handle();

            if (exhausted) {
                bool started = false;
                {
                    std::lock_guard lock(exhaustedActorsMutex);
                    started = exhaustedActors.try_emplace(key, handle).second;
                    anyExhaustedActors.store(true, std::memory_order_relaxed);
                }

                if (started && Settings::debugLogging) {
                    logger::info("[Stamina] OUT OF STAMINA START: actor={:08X}", actor->GetFormID());
                }
                return;
            }

            bool ended = false;
            {
                std::lock_guard lock(exhaustedActorsMutex);
                ended = exhaustedActors.erase(key) > 0;
                anyExhaustedActors.store(!exhaustedActors.empty(), std::memory_order_relaxed);
            }
            if (!ended) {
                return;
            }

            const float recoveryAmount = Settings::exhaustionRecoveryPercent;
            if (Settings::debugLogging) {
                logger::info("[Stamina] OUT OF STAMINA END: actor={:08X}, recovery={:.1f}", actor->GetFormID(),
                             recoveryAmount);
            }

            if (!staminaRecoverySpell) {
                return;
            }

            auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
            if (caster) {
                caster->CastSpellImmediate(reinterpret_cast<RE::MagicItem*>(staminaRecoverySpell), true, actor, 1.0f,
                                           false, recoveryAmount, actor);
                if (Settings::debugLogging) {
                    logger::info("[Stamina] STAMINA RECOVERY: actor={:08X}, amount={:.1f}", actor->GetFormID(),
                                 recoveryAmount);
                }
            } else if (Settings::debugLogging) {
                logger::error("[Stamina] Failed to obtain instant MagicCaster for actor {:08X}.", actor->GetFormID());
            }
        }

        void PruneExpiredActorHandles() {
            if (!anyExhaustedActors.load(std::memory_order_relaxed)) {
                return;
            }

            // Copy the small active set under lock, then resolve engine
            // handles outside the mutex to avoid calling engine code locked.
            std::vector<std::pair<std::uint32_t, RE::ActorHandle>> snapshot;
            {
                std::lock_guard lock(exhaustedActorsMutex);
                snapshot.reserve(exhaustedActors.size());
                for (const auto& entry : exhaustedActors) {
                    snapshot.push_back(entry);
                }
            }

            for (const auto& [key, handle] : snapshot) {
                if (handle.get()) {
                    continue;
                }

                std::lock_guard lock(exhaustedActorsMutex);
                const auto it = exhaustedActors.find(key);
                if (it != exhaustedActors.end() && it->second == handle) {
                    exhaustedActors.erase(it);
                    anyExhaustedActors.store(!exhaustedActors.empty(), std::memory_order_relaxed);
                }
            }
        }

        struct ActorUpdateHook {
            using func_t = void (*)(RE::Actor*, float);

            static void ActorThunk(RE::Actor* a_this, float a_delta) {
                if (!a_this) {
                    return;
                }

                actorFunc(a_this, a_delta);
                UpdateActorExhaustion(a_this);
            }

            static void PlayerThunk(RE::Actor* a_this, float a_delta) {
                if (!a_this) {
                    return;
                }

                playerFunc(a_this, a_delta);
                UpdateActorExhaustion(a_this);

                if (a_this == RE::PlayerCharacter::GetSingleton()) {
                    TrueHUD::Update(RE::PlayerCharacter::GetSingleton(), a_delta);

                    // Amortized cleanup: never dereference a stored raw Actor*.
                    if (++playerUpdateCounter >= kPruneEveryPlayerUpdates) {
                        playerUpdateCounter = 0;
                        PruneExpiredActorHandles();
                    }
                }
            }

            static inline func_t actorFunc = nullptr;
            static inline func_t playerFunc = nullptr;
        };

    }

    void ResetRuntimeState() {
        std::lock_guard lock(exhaustedActorsMutex);
        exhaustedActors.clear();
        anyExhaustedActors.store(false, std::memory_order_relaxed);
        playerUpdateCounter = 0;
    }

    void Install() {
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player) {
            logger::error("[Stamina] Actor::Update hook FAILED: PlayerCharacter is null.");
            return;
        }

        CacheForms();

        REL::Relocation<std::uintptr_t> actorVTable{RE::VTABLE_Actor[0]};

        auto actorUpdateOriginal =
            actorVTable.write_vfunc(kUpdateIndex, reinterpret_cast<std::uintptr_t>(ActorUpdateHook::ActorThunk));

        if (!actorUpdateOriginal) {
            logger::error("[Stamina] Failed to install Actor vtable Update hook.");
            return;
        }

        ActorUpdateHook::actorFunc = reinterpret_cast<ActorUpdateHook::func_t>(actorUpdateOriginal);

        logger::info("[Stamina] Actor::Update hook installed at Actor vtable slot 0x{:X}.", kUpdateIndex);

        const auto actualVTable = *reinterpret_cast<std::uintptr_t**>(player);

        if (!actualVTable) {
            logger::error("[Stamina] Player actual vtable is null.");
            return;
        }

        logger::info("[Stamina] Player actual vtable = {:016X}", reinterpret_cast<std::uintptr_t>(actualVTable));

        REL::Relocation<std::uintptr_t> playerVTable{reinterpret_cast<std::uintptr_t>(actualVTable)};

        auto playerUpdateOriginal =
            playerVTable.write_vfunc(kUpdateIndex, reinterpret_cast<std::uintptr_t>(ActorUpdateHook::PlayerThunk));

        if (!playerUpdateOriginal) {
            logger::error("[Stamina] Failed to install Player Actor::Update hook.");
            return;
        }

        ActorUpdateHook::playerFunc = reinterpret_cast<ActorUpdateHook::func_t>(playerUpdateOriginal);

        logger::info("[Stamina] Player Actor::Update hook installed at slot 0x{:X}.", kUpdateIndex);
    }

    void Update(float delta) {}

}
