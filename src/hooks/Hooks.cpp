#include "Hooks.h"

#include "../Settings.h"
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

            auto* effectSetting = dataHandler->LookupForm<RE::EffectSetting>(0x080E, kStaminaRecoveryPlugin);

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

            auto* actorValueOwner = player->AsActorValueOwner();

            if (!actorValueOwner) {
                return;
            }

            /*
             * ============================================================
             * EXHAUSTION ACTIVE
             * ============================================================
             *
             * Once we have detected exhaustion, we already know the
             * player's stamina regeneration must remain at zero.
             *
             * We only need to check whether the effect has ended.
             */
            if (staminaRateMultCached) {
                if (HasOutOfStaminaEffect(player)) {
                    // Keep regeneration disabled.
                    if (actorValueOwner->GetActorValue(RE::ActorValue::kStaminaRateMult) != 0.0f) {
                        actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, 0.0f);
                    }

                    return;
                }

                /*
                 * ========================================================
                 * EXHAUSTION ENDED
                 * ========================================================
                 */

                actorValueOwner->SetActorValue(RE::ActorValue::kStaminaRateMult, cachedStaminaRateMult);

                const float maxStamina = actorValueOwner->GetPermanentActorValue(RE::ActorValue::kStamina);

                const float recoveryAmount = maxStamina * Settings::exhaustionRecoveryPercent;

                logger::info("Out of stamina END: restoring regen={:.3f}, recovery={:.1f}", cachedStaminaRateMult,
                             recoveryAmount);

                auto* dataHandler = RE::TESDataHandler::GetSingleton();

                if (dataHandler) {
                    auto* spell = dataHandler->LookupForm<RE::SpellItem>(kStaminaRecoverySpell, kStaminaRecoveryPlugin);

                    if (spell) {
                        auto* caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);

                        if (caster) {
                            caster->CastSpellImmediate(reinterpret_cast<RE::MagicItem*>(spell), true, player, 1.0f,
                                                       false, recoveryAmount, player);

                            logger::info("Stamina recovery spell cast: amount={:.1f}", recoveryAmount);

                        } else {
                            logger::error("Failed to obtain instant MagicCaster.");
                        }

                    } else {
                        logger::error("Failed to find Stamina Recovery spell.");
                    }
                }

                // Exhaustion cycle is finished.
                staminaRateMultCached = false;
                cachedStaminaRateMult = 0.0f;

                return;
            }

            /*
             * ============================================================
             * NORMAL STATE
             * ============================================================
             *
             * Only here do we check whether exhaustion has started.
             */
            if (!HasOutOfStaminaEffect(player)) {
                return;
            }

            /*
             * ============================================================
             * EXHAUSTION START
             * ============================================================
             */

            const float staminaRateMult = actorValueOwner->GetActorValue(RE::ActorValue::kStaminaRateMult);

            cachedStaminaRateMult = staminaRateMult;
            staminaRateMultCached = true;

            logger::info("Out of stamina START: cached staminaRateMult={:.3f}", cachedStaminaRateMult);

            // Immediately stop regeneration.
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

        const auto actualVTable = *reinterpret_cast<std::uintptr_t*>(player);

        if (!actualVTable) {
            logger::critical("Player actual vtable is NULL.");
            return;
        }

        logger::info("Player actual vtable = {:X}", actualVTable);

        const auto slotAddress = actualVTable + kUpdateIndex * sizeof(std::uintptr_t);

        const auto originalAddress = *reinterpret_cast<std::uintptr_t*>(slotAddress);

        logger::info("Player Update slot {:X}: address={:X}", kUpdateIndex, originalAddress);

        if (!originalAddress) {
            logger::critical("Player Actor::Update slot is NULL.");
            return;
        }

        REL::Relocation<std::uintptr_t> playerVTable{actualVTable};

        _Update = playerVTable.write_vfunc(kUpdateIndex, reinterpret_cast<std::uintptr_t>(&Update));

        if (!_Update.address()) {
            logger::critical("Failed to install Player Actor::Update hook.");
            return;
        }

        const auto after = *reinterpret_cast<std::uintptr_t*>(slotAddress);

        logger::info("Player Update slot after patch = {:X}, hook = {:X}", after,
                     reinterpret_cast<std::uintptr_t>(&Update));

        if (after != reinterpret_cast<std::uintptr_t>(&Update)) {
            logger::critical("Player Actor::Update hook verification FAILED.");
            return;
        }

        logger::info("Player Actor::Update hook installed successfully.");
    }

}