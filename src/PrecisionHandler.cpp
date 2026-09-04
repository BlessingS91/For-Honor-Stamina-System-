#include "PrecisionHandler.h"

#include <SKSE/SKSE.h>

#include "MultiHittingBalance.h"
#include "Settings.h"
#include "StaminaDamage.h"

namespace PrecisionHandler {

    namespace {

        PRECISION_API::IVPrecision4* precisionAPI = nullptr;

    }

    PRECISION_API::PreHitCallbackReturn ProcessPreHit(const PRECISION_API::PrecisionHitData& hitData) {
        PRECISION_API::PreHitCallbackReturn result;

        auto* attacker = hitData.attacker;
        auto* target = hitData.target ? hitData.target->As<RE::Actor>() : nullptr;

        if (!attacker || !target) {
            return result;
        }

        // Existing stamina-based damage scaling.
        const float staminaMultiplier = StaminaDamage::GetStaminaMultiplier(attacker);

        // Multi-hit scaling.
        // Uses whichever hand registered the most recent swing.
        const float multiHitMultiplier = MultiHittingBalance::GetCurrentDamageMultiplier(attacker);

        const float damageMultiplier = staminaMultiplier * multiHitMultiplier;

        if (Settings::debugLogging) {
            logger::info("[Precision] {} | stamina={:.4f} | multiHit={:.4f} | final={:.4f}", attacker->GetName(),
                         staminaMultiplier, multiHitMultiplier, damageMultiplier);
        }

        if (damageMultiplier == 1.0f) {
            return result;
        }

        result.modifiers.push_back({PRECISION_API::PreHitModifier::ModifierType::Damage,
                                    PRECISION_API::PreHitModifier::ModifierOperation::Multiplicative,
                                    damageMultiplier});

        if (Settings::debugLogging) {
            logger::info("[Precision] {} -> {} | Damage Multiplier = {:.3f}", attacker->GetName(), target->GetName(),
                         damageMultiplier);
        }

        return result;
    }

    void Install() {
        if (precisionAPI) {
            logger::warn("[Precision] API already installed.");
            return;
        }

        void* api = PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V4);

        if (!api) {
            logger::error(
                "[Precision] Failed to acquire Precision API. "
                "Is Precision.dll loaded?");
            return;
        }

        precisionAPI = static_cast<PRECISION_API::IVPrecision4*>(api);

        const auto result = precisionAPI->AddPreHitCallback(SKSE::GetPluginHandle(), ProcessPreHit);

        if (result != PRECISION_API::APIResult::OK) {
            logger::error("[Precision] Failed to register PreHit callback. Result={}", static_cast<int>(result));

            precisionAPI = nullptr;
            return;
        }

        logger::info("[Precision] PreHit damage callback installed.");
    }

}