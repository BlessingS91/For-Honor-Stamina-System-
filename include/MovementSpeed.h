#pragma once

#include "RE/Skyrim.h"

namespace MovementSpeed {

    class MoveSpeedHook {
    public:
        static float Call(RE::TESObjectREFR* a_ref);
    };

    void Install();

}