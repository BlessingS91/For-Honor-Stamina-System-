#pragma once

#include "RE/Skyrim.h"

namespace StaminaDamage {

    void Update(RE::Actor* actor);

    float GetAttackDamageMultiplier(RE::Actor* actor);

    float GetTargetStaggerMultiplier(RE::Actor* actor);

}