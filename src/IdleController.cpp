#include "IdleController.h"

#include "../Settings.h"

namespace Stamina::Hooks {

    // Instantiate the global storage container here
    std::unordered_map<RE::Actor*, ActorStaminaState> actorStaminaStates;

    bool IdleController::CanUseIdleHook::thunk(const RE::Actor* a_this, RE::TESIdleForm* a_idle) {
        if (a_this) {
            auto it = actorStaminaStates.find(const_cast<RE::Actor*>(a_this));
            if (it != actorStaminaStates.end() && it->second.wasOutOfStamina) {
                if (Settings::debugLogging) {
                    logger::info("[Stamina] Blocked idle animation for exhausted actor={:08X}", a_this->GetFormID());
                }
                return false;
            }

            auto* avOwner = a_this->AsActorValueOwner();
            if (avOwner && avOwner->GetActorValue(RE::ActorValue::kStamina) <= 0.0f) {
                return false;
            }
        }

        return func(a_this, a_idle);
    }

    void IdleController::Install() {
        SKSE::AllocTrampoline(14);
        auto& trampoline = SKSE::GetTrampoline();

        REL::Relocation<uintptr_t> target{RELOCATION_ID(36224, 37205)};

        CanUseIdleHook::func = trampoline.write_branch<5>(target.address(), &CanUseIdleHook::thunk);

        logger::info("[Stamina] Actor::CanUseIdle hook installed successfully.");
    }
}