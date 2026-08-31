#include "MultiHittingBalance.h"

#include <unordered_map>

#include "Includes/PrecisionHandler.h"

namespace MultiHittingBalance {

    namespace {

        std::unordered_map<RE::Actor*, SwingState> swingStates;

        constexpr float kMultiHitMultiplier = 0.5f;

        SwingState& GetState(RE::Actor* actor) { return swingStates[actor]; }

        float GetMultiplier(std::uint32_t swingCount) {
            if (!enabled || swingCount <= 1) {
                return 1.0f;
            }

            return kMultiHitMultiplier;
        }

    }

    bool enabled = false;

    void Reset(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        swingStates[actor] = {};
    }

    bool IsCurrentHitLeftHand() {
        if (auto hittingNode = PrecisionHandler::cachedAttackData.GetHittingNode()) {
            if (hittingNode->parent) {
                if (hittingNode->parent->name == "SHIELD"sv) {
                    return true;
                }

                if (hittingNode->parent->name == "WEAPON"sv) {
                    return false;
                }
            }
        }

        return false;
    }

    void RegisterRightSwing(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        ++GetState(actor).rightHandSwings;
    }

    void RegisterLeftSwing(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        ++GetState(actor).leftHandSwings;
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
        if (!actor) {
            return 1.0f;
        }

        const auto swingCount = leftHand ? GetState(actor).leftHandSwings : GetState(actor).rightHandSwings;

        return GetMultiplier(swingCount);
    }

    float GetDamageMultiplier(RE::Actor* actor, bool leftHand) {
        if (!actor) {
            return 1.0f;
        }

        const auto swingCount = leftHand ? GetState(actor).leftHandSwings : GetState(actor).rightHandSwings;

        return GetMultiplier(swingCount);
    }

    float ApplyDamageMultiplier(RE::Actor* actor, float damage, bool leftHand) {
        if (!actor) {
            return damage;
        }

        return damage * GetDamageMultiplier(actor, leftHand);
    }

}