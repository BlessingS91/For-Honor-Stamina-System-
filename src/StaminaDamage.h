#pragma once

#include <RE/Skyrim.h>

namespace StaminaDamage {

    extern "C" __declspec(dllexport) float GetStaminaMultiplier(RE::Actor* actor);

    float ApplyPoiseDamageMultiplier(RE::Actor* attacker, RE::Actor* target, float damage);
}