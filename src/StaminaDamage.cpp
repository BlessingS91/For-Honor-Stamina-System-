#include "StaminaDamage.h"

#include "PoiseAPI.h"
#include "Settings.h"

namespace StaminaDamage {

    float ApplyPoiseDamageMultiplier(RE::Actor* attacker, [[maybe_unused]] RE::Actor* target, float damage) {
        if (!attacker || damage <= 0.0f) {
            return damage;
        }

        return damage * GetStaminaMultiplier(attacker);
    }

    namespace {

        float GetStaminaPercent(RE::Actor* actor) {
            if (!actor) {
                return 0.0f;
            }

            auto* actorValueOwner = actor->AsActorValueOwner();
            if (!actorValueOwner) {
                return 0.0f;
            }

            const float maxStamina = actorValueOwner->GetPermanentActorValue(RE::ActorValue::kStamina);

            if (maxStamina <= 0.0f) {
                return 0.0f;
            }

            const float stamina = actorValueOwner->GetActorValue(RE::ActorValue::kStamina);

            return std::clamp(stamina / maxStamina, 0.0f, 1.0f);
        }

        float CalculateStaminaMultiplier(RE::Actor* actor) {
            const float staminaPercent = GetStaminaPercent(actor);

            return Settings::staminaCosts.minStaminaDamage +
                   (staminaPercent *
                    (Settings::staminaCosts.maxStaminaDamage - Settings::staminaCosts.minStaminaDamage));
        }
    }

    extern "C" __declspec(dllexport) float GetStaminaMultiplier(RE::Actor* actor) {
        if (!actor) {
            return 1.0f;
        }

        return CalculateStaminaMultiplier(actor);
    }

}