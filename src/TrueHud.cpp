#include "TrueHUD.h"

namespace TrueHUD {
    namespace {
        TRUEHUD_API::IVTrueHUD2* g_trueHUD = nullptr;
        bool g_wasOutOfStamina = false;
        float g_flashTimer = 0.0f;
        RE::EffectSetting* g_outOfStaminaEffect = nullptr;

        constexpr RE::FormID kOutOfStaminaEffect = 0x80C;
        constexpr auto kOutOfStaminaPlugin = "For Honor Stamina System.esp";

        constexpr std::uint32_t kGrey = 0xFF808080;
        constexpr float kFlashInterval = 0.75f;
    }

    void Initialize() {
        g_trueHUD =
            static_cast<TRUEHUD_API::IVTrueHUD2*>(TRUEHUD_API::RequestPluginAPI(TRUEHUD_API::InterfaceVersion::V2));

        if (!g_trueHUD) {
            logger::info("TrueHUD not detected.");
            return;
        }

        auto* handler = RE::TESDataHandler::GetSingleton();
        if (handler) {
            g_outOfStaminaEffect = handler->LookupForm<RE::EffectSetting>(kOutOfStaminaEffect, kOutOfStaminaPlugin);
        }

        if (!g_outOfStaminaEffect) {
            logger::warn("[TrueHUD] Out of Stamina effect unavailable; bar override disabled.");
        }

        logger::info("TrueHUD API initialized.");
    }

    void ResetRuntimeState() {
        // Undo a previously applied override before changing characters/saves.
        if (g_trueHUD && g_wasOutOfStamina) {
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                g_trueHUD->RevertBarColor(player->GetHandle(), RE::ActorValue::kStamina,
                                          TRUEHUD_API::BarColorType::BarColor);
            }
        }
        g_wasOutOfStamina = false;
        g_flashTimer = 0.0f;
    }

    void Update(RE::Actor* a_actor, float a_deltaTime) {
        if (!g_trueHUD || !a_actor || !a_actor->IsPlayerRef()) {
            return;
        }

        auto* magicTarget = a_actor->AsMagicTarget();
        if (!magicTarget) {
            return;
        }

        if (!g_outOfStaminaEffect) {
            return;
        }

        const bool outOfStamina = magicTarget->HasMagicEffect(g_outOfStaminaEffect);

        const auto actorHandle = a_actor->GetHandle();
        constexpr auto actorValue = RE::ActorValue::kStamina;
        constexpr auto colorType = TRUEHUD_API::BarColorType::BarColor;

        if (outOfStamina) {
            if (!g_wasOutOfStamina) {
                g_trueHUD->OverrideBarColor(actorHandle, actorValue, colorType, kGrey);

                g_trueHUD->FlashActorValue(actorHandle, actorValue, true);

                g_flashTimer = 0.0f;
                g_wasOutOfStamina = true;
            }

            g_flashTimer += a_deltaTime;

            if (g_flashTimer >= kFlashInterval) {
                g_trueHUD->FlashActorValue(actorHandle, actorValue, true);

                g_flashTimer = 0.0f;
            }
        } else if (g_wasOutOfStamina) {
            g_trueHUD->RevertBarColor(actorHandle, actorValue, colorType);

            g_flashTimer = 0.0f;
            g_wasOutOfStamina = false;
        }
    }
}