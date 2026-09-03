#pragma once

#include <RE/Skyrim.h>

namespace StaminaDamage {

    void Update(RE::Actor* actor);

    extern "C" __declspec(dllexport) float GetStaminaMultiplier(RE::Actor* actor);

}