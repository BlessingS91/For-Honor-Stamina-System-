#pragma once

#include "RE/Skyrim.h"

namespace MultiHittingBalance {

    struct SwingState {
        std::uint32_t rightHandSwings{0};
        std::uint32_t leftHandSwings{0};
    };

    extern bool enabled;

    void Reset(RE::Actor* actor);

    void RegisterRightSwing(RE::Actor* actor);
    void RegisterLeftSwing(RE::Actor* actor);

    std::uint32_t GetRightSwingCount(RE::Actor* actor);
    std::uint32_t GetLeftSwingCount(RE::Actor* actor);

    float GetStaminaCostMultiplier(RE::Actor* actor, bool leftHand);
    float GetDamageMultiplier(RE::Actor* actor, bool leftHand);

    float ApplyDamageMultiplier(RE::Actor* actor, float damage, bool leftHand);

    bool IsCurrentHitLeftHand();

}