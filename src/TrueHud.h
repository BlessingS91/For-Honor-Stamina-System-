#pragma once

#include <RE/Skyrim.h>

#include "Includes/TrueHUDAPI.h"

namespace TrueHUD {
    void Initialize();
    void Update(RE::Actor* a_actor, float a_deltaTime);
}