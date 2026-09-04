#include "MultiHittingBalance.h"

#include <cmath>
#include <unordered_map>

#include "Settings.h"

namespace MultiHittingBalance {

    namespace {

        std::unordered_map<RE::Actor*, SwingState> swingStates;

        constexpr float kFirstSwingMultiplier = 1.0f;

        SwingState& GetState(RE::Actor* actor) { return swingStates[actor]; }

        float GetMultiplier(std::uint32_t swingCount) {
            if (swingCount <= 1) {
                return kFirstSwingMultiplier;
            }

            return std::pow(Settings::multiHitting.scalingMultiplier, static_cast<float>(swingCount - 1));
        }

    }

    void Reset(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        swingStates[actor] = {};
    }

    void RegisterRightSwing(RE::Actor* actor) {
        if (!actor || !Settings::multiHitEnabled) {
            return;
        }

        auto& state = GetState(actor);

        ++state.rightHandSwings;
        state.lastSwingLeft = false;

        if (Settings::debugLogging) {
            logger::info("[MultiHit] {} RIGHT swing #{} | multiplier={:.4f}", actor->GetName(), state.rightHandSwings,
                         GetMultiplier(state.rightHandSwings));
        }
    }

    void RegisterLeftSwing(RE::Actor* actor) {
        if (!actor || !Settings::multiHitEnabled) {
            return;
        }

        auto& state = GetState(actor);

        ++state.leftHandSwings;
        state.lastSwingLeft = true;

        if (Settings::debugLogging) {
            logger::info("[MultiHit] {} LEFT swing #{} | multiplier={:.4f}", actor->GetName(), state.leftHandSwings,
                         GetMultiplier(state.leftHandSwings));
        }
    }

    std::uint32_t GetRightSwingCount(RE::Actor* actor) {
        if (!actor) {
            return 0;
        }

        return GetState(actor).rightHandSwings;
    }

    std::uint32_t GetLeftSwingCount(RE::Actor* actor) {
        if (!actor) {
            return 0;
        }

        return GetState(actor).leftHandSwings;
    }

    float GetStaminaCostMultiplier(RE::Actor* actor, bool leftHand) {
        if (!actor || !Settings::multiHitEnabled) {
            return 1.0f;
        }

        const auto& state = GetState(actor);

        const auto swingCount = leftHand ? state.leftHandSwings : state.rightHandSwings;

        const auto multiplier = GetMultiplier(swingCount);

        if (Settings::debugLogging) {
            logger::info("[MultiHit] {} {} stamina | swing #{} | multiplier={:.4f}", actor->GetName(),
                         leftHand ? "LEFT" : "RIGHT", swingCount, multiplier);
        }

        return multiplier;
    }

    float GetDamageMultiplier(RE::Actor* actor, bool leftHand) {
        if (!actor || !Settings::multiHitEnabled) {
            return 1.0f;
        }

        const auto& state = GetState(actor);

        const auto swingCount = leftHand ? state.leftHandSwings : state.rightHandSwings;

        return GetMultiplier(swingCount);
    }

    float GetCurrentDamageMultiplier(RE::Actor* actor) {
        if (!actor || !Settings::multiHitEnabled) {
            return 1.0f;
        }

        const auto& state = GetState(actor);

        const auto swingCount = state.lastSwingLeft ? state.leftHandSwings : state.rightHandSwings;

        return GetMultiplier(swingCount);
    }

    float ApplyDamageMultiplier(RE::Actor* actor, float damage, bool leftHand) {
        if (!actor || !Settings::multiHitEnabled) {
            return damage;
        }

        return damage * GetDamageMultiplier(actor, leftHand);
    }

}