#pragma once

#include <RE/Skyrim.h>

#include "TrueHUDAPI.h"

namespace TrueHUD {
    void Initialize();
    void ResetRuntimeState();
    void Update(RE::Actor* a_actor, float a_deltaTime);
}