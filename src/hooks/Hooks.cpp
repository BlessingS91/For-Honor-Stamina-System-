#include "Hooks.h"

#include "../Settings.h"
#include "../Stamina.h"
#include "TrueHud.h"

namespace Stamina::Hooks {

    namespace {

        using Update_t = void (*)(RE::Actor*, float);
        REL::Relocation<Update_t> _Update;
        constexpr std::size_t kUpdateIndex = 0xAD;

        bool staminaRateMultCached = false;
        float cachedStaminaRateMult = 0.0f;

        constexpr RE::FormID kStaminaRecoverySpell = 0xA57;
        constexpr auto kStaminaRecoveryPlugin = "For Honor Stamina System.esp";

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

            auto* effectSetting = dataHandler->LookupForm<RE::EffectSetting>(0x080C, kStaminaRecoveryPlugin);

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

        void SetExhaustionGlobals(bool /*exhausted*/) {
            // Implement global state updates here if needed
        }

        void Update(RE::Actor* actor, float delta) {
            if (!_Update.address()) {
                return;
            }

            // Always let the game perform its normal update first.
            _Update(actor, delta);

            if (!actor) {
                return;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || actor != player) {
                return;
            }

            TrueHUD::Update(player, delta);

            auto* actorValueOwner = player->AsActorValueOwner();
            if (!actorValueOwner) {
                return;
            }

            if (staminaRateMultCached) {
                if (HasOutOfStaminaEffect(player)) {
                    // Keep regeneration disabled.
                    if (actorValueOwner->GetActorValue(RE::ActorValue::kStaminaRateMult) != 0.0f) {
                        actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, 0.0f);
                    }

                    return;
                }

                actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, cachedStaminaRateMult);

                const float recoveryAmount = Settings::exhaustionRecoveryPercent;

                if (Settings::debugLogging) {
                    logger::info("Out of stamina END: restoring regen={:.3f}, recovery={:.1f}", cachedStaminaRateMult,
                                 recoveryAmount);
                }

                auto* dataHandler = RE::TESDataHandler::GetSingleton();

                if (dataHandler) {
                    auto* spell = dataHandler->LookupForm<RE::SpellItem>(kStaminaRecoverySpell, kStaminaRecoveryPlugin);

                    if (spell) {
                        auto* caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);

                        if (caster) {
                            caster->CastSpellImmediate(reinterpret_cast<RE::MagicItem*>(spell), true, player, 1.0f,
                                                       false, recoveryAmount, player);

                            if (Settings::debugLogging) {
                                logger::info("Stamina recovery spell cast: amount={:.1f}", recoveryAmount);
                            }
                        } else if (Settings::debugLogging) {
                            logger::error("Failed to obtain instant MagicCaster.");
                        }
                    } else if (Settings::debugLogging) {
                        logger::error("Failed to find Stamina Recovery spell.");
                    }
                }

                staminaRateMultCached = false;
                cachedStaminaRateMult = 0.0f;
                return;
            }

            if (!HasOutOfStaminaEffect(player)) {
                return;
            }

            const float staminaRateMult = actorValueOwner->GetActorValue(RE::ActorValue::kStaminaRateMult);

            SetExhaustionGlobals(true);

            cachedStaminaRateMult = staminaRateMult;
            staminaRateMultCached = true;

            if (Settings::debugLogging) {
                logger::info("Out of stamina START: cached staminaRateMult={:.3f}", cachedStaminaRateMult);
            }

            if (staminaRateMult != 0.0f) {
                actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, 0.0f);
            }
        }

    }

    void Install() {
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player) {
            logger::critical("PlayerCharacter singleton is NULL.");
            return;
        }

        const auto actualVTable = *reinterpret_cast<std::uintptr_t**>(player);

        if (!actualVTable) {
            logger::critical("Player actual vtable is NULL.");
            return;
        }

        logger::info("Player actual vtable = {:X}", reinterpret_cast<std::uintptr_t>(actualVTable));

        //
        // Actor::Update (VTable Hook)
        //

        const auto updateSlotAddress =
            reinterpret_cast<std::uintptr_t>(actualVTable) + kUpdateIndex * sizeof(std::uintptr_t);

        const auto originalUpdateAddress = *reinterpret_cast<std::uintptr_t*>(updateSlotAddress);

        logger::info("Player Update slot {:X}: address={:X}", kUpdateIndex, originalUpdateAddress);

        if (!originalUpdateAddress) {
            logger::critical("Player Actor::Update slot is NULL.");
            return;
        }

        REL::Relocation<std::uintptr_t> playerVTable{reinterpret_cast<std::uintptr_t>(actualVTable)};

        _Update = playerVTable.write_vfunc(kUpdateIndex, reinterpret_cast<std::uintptr_t>(&Update));

        if (!_Update.address()) {
            logger::critical("Failed to install Player Actor::Update hook.");
            return;
        }

        const auto updateAfter = *reinterpret_cast<std::uintptr_t*>(updateSlotAddress);

        logger::info("Player Update slot after patch = {:X}, hook = {:X}", updateAfter,
                     reinterpret_cast<std::uintptr_t>(&Update));

        if (updateAfter != reinterpret_cast<std::uintptr_t>(&Update)) {
            logger::critical("Player Actor::Update hook verification FAILED.");
            return;
        }

        logger::info("Player Actor::Update hook installed successfully.");
    }

    std::unordered_map<RE::Actor*, float> attackStaminaRateMultCache;

    void UpdateAttackStaminaRegen(RE::Actor* actor) {
        if (!actor || !Settings::disableStaminaRegenWhileAttacking) {
            return;
        }

        auto* actorValueOwner = actor->AsActorValueOwner();
        if (!actorValueOwner) {
            return;
        }

        const bool attacking = actor->IsAttacking();

        if (attacking) {
            if (!attackStaminaRateMultCache.contains(actor)) {
                attackStaminaRateMultCache.emplace(actor,
                                                   actorValueOwner->GetActorValue(RE::ActorValue::kStaminaRateMult));
            }

            if (actorValueOwner->GetActorValue(RE::ActorValue::kStaminaRateMult) != 0.0f) {
                actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, 0.0f);
            }

            return;
        }

        const auto it = attackStaminaRateMultCache.find(actor);
        if (it == attackStaminaRateMultCache.end()) {
            return;
        }

        actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, it->second);

        attackStaminaRateMultCache.erase(it);
    }
}
