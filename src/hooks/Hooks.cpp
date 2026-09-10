#include "Hooks.h"

#include "../Settings.h"
#include "../Stamina.h"
#include "TrueHud.h"

namespace Stamina::Hooks {

    namespace {

        constexpr std::size_t kUpdateIndex = 0xAD;

        constexpr RE::FormID kOutOfStaminaEffect = 0x080C;
        constexpr RE::FormID kStaminaRecoverySpell = 0x0A57;

        constexpr auto kPluginName = "For Honor Stamina System.esp";

        struct ActorStaminaState {
            bool attackRegenCached = false;
            float cachedAttackStaminaRateMult = 0.0f;
            float attackRegenDelayTimer = 0.0f;
            bool exhaustionRegenCached = false;
            float cachedExhaustionStaminaRateMult = 0.0f;
            bool wasOutOfStamina = false;
        };

        std::unordered_map<RE::Actor*, ActorStaminaState> actorStaminaStates;

        void CleanupActorStaminaStates() {
            for (auto it = actorStaminaStates.begin(); it != actorStaminaStates.end();) {
                const auto& [actor, state] = *it;

                if (!actor || actor->IsDead() ||
                    (!state.attackRegenCached && state.cachedAttackStaminaRateMult == 0.0f &&
                     state.attackRegenDelayTimer <= 0.0f && !state.exhaustionRegenCached &&
                     state.cachedExhaustionStaminaRateMult == 0.0f && !state.wasOutOfStamina)) {
                    it = actorStaminaStates.erase(it);
                } else {
                    ++it;
                }
            }
        }

        bool HasOutOfStaminaEffect(RE::Actor* actor) {
            if (!actor) {
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

            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                return false;
            }

            auto* effectSetting = dataHandler->LookupForm<RE::EffectSetting>(kOutOfStaminaEffect, kPluginName);

            if (!effectSetting) {
                return false;
            }

            for (auto* activeEffect : *effects) {
                if (!activeEffect || !activeEffect->effect) {
                    continue;
                }

                if (activeEffect->effect->baseEffect == effectSetting) {
                    return true;
                }
            }

            return false;
        }

        void UpdateAttackStaminaRegen(RE::Actor* actor, float delta) {
            if (!actor) {
                return;
            }

            auto* actorValueOwner = actor->AsActorValueOwner();
            if (!actorValueOwner) {
                return;
            }

            auto& state = actorStaminaStates[actor];

            // If the setting is disabled, clean up any cached state and bail out safely
            if (!Settings::disableStaminaRegenWhileAttacking) {
                if (state.attackRegenCached && !HasOutOfStaminaEffect(actor)) {
                    actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, state.cachedAttackStaminaRateMult);

                    state.attackRegenCached = false;
                    state.cachedAttackStaminaRateMult = 0.0f;
                    state.attackRegenDelayTimer = 0.0f;
                }
                return;
            }

            const bool attacking = actor->IsAttacking();

            if (attacking) {
                // Attack is active, so reset the post-attack delay.
                state.attackRegenDelayTimer = Settings::attackStaminaRegenDelay;

                if (!state.attackRegenCached) {
                    if (state.exhaustionRegenCached && state.cachedExhaustionStaminaRateMult > 0.0f) {
                        state.cachedAttackStaminaRateMult = state.cachedExhaustionStaminaRateMult;
                        state.attackRegenCached = true;
                    } else {
                        const float currentRate = actorValueOwner->GetActorValue(RE::ActorValue::kStaminaRateMult);

                        if (currentRate > 0.0f) {
                            state.cachedAttackStaminaRateMult = currentRate;
                            state.attackRegenCached = true;
                        }
                    }

                    if (Settings::debugLogging) {
                        logger::info(
                            "[Stamina] ATTACK START: actor={:08X}, cached StaminaRateMult={:.3f}, delay={:.2f}s",
                            actor->GetFormID(), state.cachedAttackStaminaRateMult, Settings::attackStaminaRegenDelay);
                    }
                }

                const float currentRate = actorValueOwner->GetActorValue(RE::ActorValue::kStaminaRateMult);

                if (currentRate != 0.0f) {
                    actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, 0.0f);

                    if (Settings::debugLogging) {
                        logger::info("[Stamina] REGEN DISABLED: actor={:08X}, {:.3f} -> 0.000", actor->GetFormID(),
                                     currentRate);
                    }
                }

                return;
            }

            // If exhaustion is active, exhaustion owns the suppression.
            if (HasOutOfStaminaEffect(actor)) {
                return;
            }

            // Nothing was cached by the attack system.
            if (!state.attackRegenCached) {
                return;
            }

            // Count down the post-attack delay.
            if (state.attackRegenDelayTimer > 0.0f) {
                state.attackRegenDelayTimer -= delta;

                if (state.attackRegenDelayTimer > 0.0f) {
                    return;
                }

                state.attackRegenDelayTimer = 0.0f;
            }

            const float restoredRate = state.cachedAttackStaminaRateMult;

            actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, restoredRate);

            state.attackRegenCached = false;
            state.cachedAttackStaminaRateMult = 0.0f;

            if (Settings::debugLogging) {
                logger::info("[Stamina] ATTACK END: actor={:08X}, restored StaminaRateMult={:.3f}", actor->GetFormID(),
                             restoredRate);
            }
        }

        void UpdateActorExhaustion(RE::Actor* actor) {
            if (!actor) {
                return;
            }

            auto* actorValueOwner = actor->AsActorValueOwner();
            if (!actorValueOwner) {
                return;
            }

            auto& state = actorStaminaStates[actor];

            const bool exhausted = HasOutOfStaminaEffect(actor);

            // ---------------------------------------------------------
            // OOS active
            // ---------------------------------------------------------

            if (exhausted) {
                if (!state.wasOutOfStamina) {
                    state.wasOutOfStamina = true;

                    if (Settings::debugLogging) {
                        logger::info("[Stamina] OUT OF STAMINA START: actor={:08X}", actor->GetFormID());
                    }
                }

                if (!state.exhaustionRegenCached) {
                    if (state.attackRegenCached && state.cachedAttackStaminaRateMult > 0.0f) {
                        state.cachedExhaustionStaminaRateMult = state.cachedAttackStaminaRateMult;

                        Settings::savedExhaustionStaminaRateMult = state.cachedAttackStaminaRateMult;

                    } else {
                        const float currentRate = actorValueOwner->GetActorValue(RE::ActorValue::kStaminaRateMult);

                        if (currentRate > 0.0f) {
                            state.cachedExhaustionStaminaRateMult = currentRate;
                            Settings::savedExhaustionStaminaRateMult = currentRate;
                        } else {
                            state.cachedExhaustionStaminaRateMult = Settings::savedExhaustionStaminaRateMult;
                        }
                    }

                    state.exhaustionRegenCached = true;
                }

                const float currentRate = actorValueOwner->GetActorValue(RE::ActorValue::kStaminaRateMult);

                if (currentRate != 0.0f) {
                    actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, 0.0f);
                }

                return;
            }

            // ---------------------------------------------------------
            // OOS has ended.
            //
            // Do not restore regeneration while an attack is active
            // OR while the attack system still owns its post-attack
            // suppression/delay.
            // ---------------------------------------------------------

            if (!state.exhaustionRegenCached) {
                state.wasOutOfStamina = false;
                return;
            }

            if (state.wasOutOfStamina && (actor->IsAttacking() || state.attackRegenCached)) {
                return;
            }

            const float restoredRate = state.cachedExhaustionStaminaRateMult;

            actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, restoredRate);

            // ---------------------------------------------------------
            // OOS -> normal transition.
            // Recovery happens exactly once.
            // ---------------------------------------------------------

            if (state.wasOutOfStamina) {
                const float recoveryAmount = Settings::exhaustionRecoveryPercent;

                if (Settings::debugLogging) {
                    logger::info("[Stamina] OUT OF STAMINA END: actor={:08X}, restoring regen={:.3f}, recovery={:.1f}",
                                 actor->GetFormID(), restoredRate, recoveryAmount);
                }

                auto* dataHandler = RE::TESDataHandler::GetSingleton();

                if (dataHandler) {
                    auto* spell = dataHandler->LookupForm<RE::SpellItem>(kStaminaRecoverySpell, kPluginName);

                    if (spell) {
                        auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);

                        if (caster) {
                            caster->CastSpellImmediate(reinterpret_cast<RE::MagicItem*>(spell), true, actor, 1.0f,
                                                       false, recoveryAmount, actor);

                            if (Settings::debugLogging) {
                                logger::info("[Stamina] STAMINA RECOVERY: actor={:08X}, amount={:.1f}",
                                             actor->GetFormID(), recoveryAmount);
                            }

                        } else if (Settings::debugLogging) {
                            logger::error("[Stamina] Failed to obtain instant MagicCaster for actor {:08X}.",
                                          actor->GetFormID());
                        }

                    } else if (Settings::debugLogging) {
                        logger::error("[Stamina] Failed to find Stamina Recovery spell.");
                    }
                }
            }

            // Clear ONLY the exhaustion state.
            state.wasOutOfStamina = false;
            state.exhaustionRegenCached = false;
            state.cachedExhaustionStaminaRateMult = 0.0f;
        }

        void UpdateActorStamina(RE::Actor* actor, float delta) {
            if (!actor) {
                return;
            }
            UpdateAttackStaminaRegen(actor, delta);
            UpdateActorExhaustion(actor);
        }

        struct ActorUpdateHook {
            using func_t = void (*)(RE::Actor*, float);

            static void ActorThunk(RE::Actor* a_this, float a_delta) {
                if (!a_this) {
                    return;
                }
                actorFunc(a_this, a_delta);
                UpdateActorStamina(a_this, a_delta);
            }

            static void PlayerThunk(RE::Actor* a_this, float a_delta) {
                if (!a_this) {
                    return;
                }

                playerFunc(a_this, a_delta);
                UpdateActorStamina(a_this, a_delta);

                if (a_this == RE::PlayerCharacter::GetSingleton()) {
                    TrueHUD::Update(RE::PlayerCharacter::GetSingleton(), a_delta);
                }
            }

            static inline func_t actorFunc = nullptr;
            static inline func_t playerFunc = nullptr;
        };
    }

    void Install() {
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player) {
            logger::error("[Stamina] Actor::Update hook FAILED: PlayerCharacter is null.");
            return;
        }

        // ---------------------------------------------------------
        // Hook the normal Actor vtable.
        //
        // This handles NPCs.
        // ---------------------------------------------------------

        auto actorVTable = REL::Relocation<std::uintptr_t>(RE::VTABLE_Actor[0]);

        auto actorUpdateOriginal = actorVTable.write_vfunc(kUpdateIndex, ActorUpdateHook::ActorThunk);

        if (!actorUpdateOriginal) {
            logger::error("[Stamina] Failed to install Actor vtable Update hook.");
            return;
        }

        ActorUpdateHook::actorFunc = reinterpret_cast<ActorUpdateHook::func_t>(actorUpdateOriginal);

        logger::info("[Stamina] Actor::Update hook installed at Actor vtable slot 0x{:X}.", kUpdateIndex);

        // ---------------------------------------------------------
        // Hook the player's actual vtable.
        //
        // The player does not dispatch through the Actor vtable
        // path we originally tested, so patch its actual vtable.
        // ---------------------------------------------------------

        const auto actualVTable = *reinterpret_cast<std::uintptr_t**>(player);

        if (!actualVTable) {
            logger::error("[Stamina] Player actual vtable is null.");
            return;
        }

        logger::info("[Stamina] Player actual vtable = {:016X}", reinterpret_cast<std::uintptr_t>(actualVTable));

        REL::Relocation<std::uintptr_t> playerVTable{reinterpret_cast<std::uintptr_t>(actualVTable)};

        auto playerUpdateOriginal = playerVTable.write_vfunc(kUpdateIndex, ActorUpdateHook::PlayerThunk);

        if (!playerUpdateOriginal) {
            logger::error("[Stamina] Failed to install Player Actor::Update hook.");
            return;
        }

        ActorUpdateHook::playerFunc = reinterpret_cast<ActorUpdateHook::func_t>(playerUpdateOriginal);

        logger::info("[Stamina] Player Actor::Update hook installed at slot 0x{:X}.", kUpdateIndex);
    }

    void Update(float delta) {
        static float cleanupTimer = 0.0f;

        cleanupTimer += delta;

        if (cleanupTimer >= 10.0f) {
            CleanupActorStaminaStates();
            cleanupTimer -= 10.0f;
        }
    }
}