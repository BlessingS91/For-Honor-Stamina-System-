#pragma once

#include <string>

#include "RE/Skyrim.h"

namespace Settings {

    struct StaminaCosts {
        float unarmed = 10.0f;
        float dagger = 12.0f;
        float sword = 14.0f;
        float warAxe = 15.0f;
        float mace = 16.0f;
        float greatsword = 28.0f;
        float battleaxe = 30.0f;
        float warhammer = 32.0f;
        float shield = 15.0f;
        float minStaminaDamage = 0.85f;
        float maxStaminaDamage = 1.15f;
        float powerAttackMultiplier = 2.0f;
    };

    extern StaminaCosts staminaCosts;
    extern RE::ActorValue unarmedSkillActorValue;
    extern float exhaustionRecoveryPercent;

    void Load();
    void Save();
    void ResetToDefaults();
    float GetWeaponCost(const std::string& weaponType);
    StaminaCosts& Mutable();
}