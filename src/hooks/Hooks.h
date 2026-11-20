#pragma once

#include "RE/Skyrim.h"

namespace Stamina::Hooks {

    void Install();
    void ResetRuntimeState();

    bool IsActorOutOfStamina(RE::Actor* actor);

}