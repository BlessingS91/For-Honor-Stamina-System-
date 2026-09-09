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
        float greatsword = 26.0f;
        float battleaxe = 27.0f;
        float warhammer = 28.0f;
        float shield = 16.0f;

        float minStaminaDamage = 0.85f;
        float maxStaminaDamage = 1.15f;
        float powerAttackMultiplier = 2.0f;
    };

    struct MultiHitting {
        float scalingMultiplier = 0.6f;
    };

    extern StaminaCosts staminaCosts;
    extern bool disableStaminaRegenWhileAttacking;

    extern bool ignoreOutOfStamina;
    extern float outOfStaminaBaseDuration;
    extern float attackStaminaRegenDelay;

    extern bool lightAttackPrevention;
    extern bool heavyAttackPrevention;

    extern MultiHitting multiHitting;

    extern RE::ActorValue unarmedSkillActorValue;

    extern float weaponDrawnMultiplier;
    extern float combatMultiplier;

    extern bool movementSpeedEnabled;
    extern bool multiHitEnabled;

    extern float exhaustionRecoveryPercent;
    extern bool allowExhaustionOutOfCombat;
    extern float outOfStaminaMoveSpeedNerf;
    extern float outOfStaminaAttackSpeedNerf;

    extern float savedExhaustionStaminaRateMult;

    extern bool debugLogging;

    void Load();
    void Save();
    void ResetToDefaults();
    void UpdateAttackPreventionGlobals();

    float GetWeaponCost(const std::string& weaponType);

    StaminaCosts& Mutable();

}