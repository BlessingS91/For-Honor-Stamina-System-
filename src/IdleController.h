#pragma once

#include <unordered_map>

#include "RE/Skyrim.h"

namespace Stamina::Hooks {

    struct ActorStaminaState {
        bool staminaRegenCached = false;
        float cachedStaminaRateMult = 0.0f;
        bool wasOutOfStamina = false;
    };

    extern std::unordered_map<RE::Actor*, ActorStaminaState> actorStaminaStates;

    class IdleController {
    public:
        static void Install();

    private:
        struct CanUseIdleHook {
            using func_t = bool (*)(const RE::Actor*, RE::TESIdleForm*);

            static bool thunk(const RE::Actor* a_this, RE::TESIdleForm* a_idle);

            static inline REL::Relocation<func_t> func;
        };
    };
}