#include "Hooks.h"

#include "../Stamina.h"

namespace Stamina::Hooks {

    namespace {

        using Update_t = void (*)(RE::Actor*, float);
        REL::Relocation<Update_t> _Update;

        constexpr std::size_t kUpdateIndex = 0xAD;

        bool staminaRateMultCached = false;
        float cachedStaminaRateMult = 0.0f;

        constexpr RE::FormID kStaminaRecoverySpell = 0xA57;
        constexpr auto kStaminaRecoveryPlugin = "For Honor Stamina System.esp";

        constexpr float kStaminaRecoveryPercent = 0.33f;

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

            auto* effectSetting = dataHandler->LookupForm<RE::EffectSetting>(0x080E, "For Honor Stamina System.esp");

            if (!effectSetting) {
                logger::warn(
                    "Could not find out-of-stamina effect: "
                    "For Honor Stamina System.esp|080E");

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

        void Update(RE::Actor* actor, float delta) {
            if (_Update.address()) {
                _Update(actor, delta);
            } else {
                logger::critical("PLAYER UPDATE: original function is NULL!");
                return;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || actor != player) {
                return;
            }

            auto* actorValueOwner = player->AsActorValueOwner();
            if (!actorValueOwner) {
                return;
            }

            const bool outOfStamina = HasOutOfStaminaEffect(player);
            const float staminaRateMult = actorValueOwner->GetActorValue(RE::ActorValue::kStaminaRateMult);

            if (outOfStamina) {
                if (!staminaRateMultCached) {
                    cachedStaminaRateMult = staminaRateMult;
                    staminaRateMultCached = true;

                    logger::info("Out of stamina: disabling stamina regeneration.");
                }

                if (staminaRateMult != 0.0f) {
                    actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, 0.0f);
                }

            } else if (staminaRateMultCached) {
                actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, cachedStaminaRateMult);

                const float maxStamina = actorValueOwner->GetPermanentActorValue(RE::ActorValue::kStamina);
                const float recoveryAmount = maxStamina * kStaminaRecoveryPercent;

                auto* spell = RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(kStaminaRecoverySpell,
                                                                                            kStaminaRecoveryPlugin);

                if (spell) {
                    if (auto* caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
                        caster->CastSpellImmediate(spell, true, player, 1.0f, false, recoveryAmount, player);
                    }
                } else {
                    logger::error("Failed to find Stamina Recovery spell.");
                }

                logger::info("Out of stamina cleared: restoring regen and recovering {:.1f} stamina.", recoveryAmount);

                staminaRateMultCached = false;
                cachedStaminaRateMult = 0.0f;
            }
        }
    }

    void Install() {
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player) {
            logger::critical("PlayerCharacter::GetSingleton() returned NULL!");
            return;
        }

        // Get the vtable pointer directly from the actual player object.
        const auto actualVTable = *reinterpret_cast<std::uintptr_t*>(player);

        if (!actualVTable) {
            logger::critical("Actual PlayerCharacter vtable is NULL!");
            return;
        }

        const auto updateSlot = actualVTable + kUpdateIndex * sizeof(std::uintptr_t);

        const auto original = *reinterpret_cast<std::uintptr_t*>(updateSlot);

        if (!original) {
            logger::critical("Player Update slot is NULL!");
            return;
        }

        REL::Relocation<std::uintptr_t> vtable{actualVTable};

        _Update = vtable.write_vfunc(kUpdateIndex, reinterpret_cast<std::uintptr_t>(&Update));

        if (!_Update.address()) {
            logger::critical("Failed to install PlayerCharacter::Update hook.");
            return;
        }

        const auto hookAddress = reinterpret_cast<std::uintptr_t>(&Update);

        const auto after = *reinterpret_cast<std::uintptr_t*>(updateSlot);

        if (after != hookAddress) {
            logger::critical("PlayerCharacter::Update hook verification failed.");
            return;
        }

        logger::info("PlayerCharacter::Update hook installed.");
    }
}