#include "StaminaDamage.h"

namespace StaminaDamage {

    namespace {

        struct StaminaModifiers {
            float attackDamage = 1.0f;
            float targetStagger = 1.0f;
            float lastStaminaPercent = -1.0f;
        };

        std::unordered_map<RE::Actor*, StaminaModifiers> modifiers;

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

        float GetStaminaMultiplier(RE::Actor* actor) {
            const float staminaPercent = GetStaminaPercent(actor);

            // 0%   = 0.85
            // 50%  = 1.00
            // 100% = 1.15
            return 0.85f + (staminaPercent * 0.30f);
        }

    }

    void Update(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        const float staminaPercent = GetStaminaPercent(actor);
        const float staminaMultiplier = 0.85f + (staminaPercent * 0.30f);

        auto& data = modifiers[actor];

        // Only log when stamina changes enough to avoid flooding the log
        // every frame.
        if (data.lastStaminaPercent < 0.0f || std::abs(staminaPercent - data.lastStaminaPercent) >= 0.01f) {
            data.lastStaminaPercent = staminaPercent;
        }

        data.attackDamage = staminaMultiplier;
        data.targetStagger = staminaMultiplier;
    }

    float GetAttackDamageMultiplier(RE::Actor* actor) {
        if (!actor) {
            return 1.0f;
        }

        auto it = modifiers.find(actor);

        if (it == modifiers.end()) {
            const float multiplier = GetStaminaMultiplier(actor);

            return multiplier;
        }

        return it->second.attackDamage;
    }

    float GetTargetStaggerMultiplier(RE::Actor* actor) {
        if (!actor) {
            return 1.0f;
        }

        auto it = modifiers.find(actor);

        if (it == modifiers.end()) {
            const float multiplier = GetStaminaMultiplier(actor);

            return multiplier;
        }

        return it->second.targetStagger;
    }

}