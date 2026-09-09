#pragma once

#include "RE/Skyrim.h"

namespace Stamina::Hooks {

    void Install();

    bool IsActorOutOfStamina(RE::Actor* actor);

}