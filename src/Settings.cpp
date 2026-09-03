#include "Settings.h"

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <string>

namespace Settings {

    StaminaCosts staminaCosts;
    MultiHitting multiHitting;

    RE::ActorValue unarmedSkillActorValue = RE::ActorValue::kLockpicking;

    bool multiHitEnabled = true;
    bool movementSpeedEnabled = true;
    float exhaustionRecoveryPercent = 0.33f;
    float weaponDrawnMultiplier = 0.95f;
    float combatMultiplier = 0.80f;

    namespace {

        constexpr const char* kSection = "Stamina";
        constexpr const char* kConfigPath = "Data/SKSE/Plugins/ForHonorStamina.ini";

        float GetFloat(const char* key, float defaultValue) {
            char buffer[64]{};

            GetPrivateProfileStringA(kSection, key, "", buffer, sizeof(buffer), kConfigPath);

            if (buffer[0] == '\0') {
                return defaultValue;
            }

            try {
                return std::stof(buffer);
            } catch (...) {
                return defaultValue;
            }
        }

        void WriteFloat(const char* key, float value) {
            const std::string valueString = std::to_string(value);

            WritePrivateProfileStringA(kSection, key, valueString.c_str(), kConfigPath);
        }

        std::string GetString(const char* key, const char* defaultValue) {
            char buffer[64]{};

            GetPrivateProfileStringA(kSection, key, defaultValue, buffer, sizeof(buffer), kConfigPath);

            return buffer;
        }

        void WriteString(const char* key, const std::string& value) {
            WritePrivateProfileStringA(kSection, key, value.c_str(), kConfigPath);
        }

    }

    StaminaCosts& Mutable() { return staminaCosts; }

    void ResetToDefaults() {
        staminaCosts = StaminaCosts{};
        multiHitting = MultiHitting{};
        unarmedSkillActorValue = RE::ActorValue::kLockpicking;
        movementSpeedEnabled = true;
        multiHitEnabled = true;
        exhaustionRecoveryPercent = 0.33f;
        weaponDrawnMultiplier = 0.95f;
        combatMultiplier = 0.80f;
    }

    void Load() {
        staminaCosts.unarmed = GetFloat("Unarmed", staminaCosts.unarmed);

        movementSpeedEnabled = GetFloat("MovementSpeedEnabled", movementSpeedEnabled ? 1.0f : 0.0f) != 0.0f;

        multiHitEnabled = GetFloat("MultiHitEnabled", multiHitEnabled ? 1.0f : 0.0f) != 0.0f;

        exhaustionRecoveryPercent = GetFloat("ExhaustionRecoveryPercent", exhaustionRecoveryPercent);

        // MultiHitting Settings
        multiHitting.scalingMultiplier = GetFloat("MultiHitScalingMultiplier", multiHitting.scalingMultiplier);
        // MoveSpeed Settings
        weaponDrawnMultiplier = GetFloat("WeaponDrawnMultiplier", weaponDrawnMultiplier);
        combatMultiplier = GetFloat("CombatMultiplier", combatMultiplier);

        const auto unarmedSkill = GetString("UnarmedSkill", "Lockpicking");

        if (unarmedSkill == "Lockpicking") {
            unarmedSkillActorValue = RE::ActorValue::kLockpicking;
        } else if (unarmedSkill == "OneHanded") {
            unarmedSkillActorValue = RE::ActorValue::kOneHanded;
        } else if (unarmedSkill == "TwoHanded") {
            unarmedSkillActorValue = RE::ActorValue::kTwoHanded;
        } else if (unarmedSkill == "LightArmor") {
            unarmedSkillActorValue = RE::ActorValue::kLightArmor;
        } else if (unarmedSkill == "HeavyArmor") {
            unarmedSkillActorValue = RE::ActorValue::kHeavyArmor;
        } else if (unarmedSkill == "Alteration") {
            unarmedSkillActorValue = RE::ActorValue::kAlteration;
        } else if (unarmedSkill == "Sneak") {
            unarmedSkillActorValue = RE::ActorValue::kSneak;
        } else {
            unarmedSkillActorValue = RE::ActorValue::kLockpicking;
        }

        staminaCosts.dagger = GetFloat("Dagger", staminaCosts.dagger);

        staminaCosts.sword = GetFloat("Sword", staminaCosts.sword);

        staminaCosts.warAxe = GetFloat("WarAxe", staminaCosts.warAxe);

        staminaCosts.mace = GetFloat("Mace", staminaCosts.mace);

        staminaCosts.greatsword = GetFloat("Greatsword", staminaCosts.greatsword);

        staminaCosts.battleaxe = GetFloat("Battleaxe", staminaCosts.battleaxe);

        staminaCosts.warhammer = GetFloat("Warhammer", staminaCosts.warhammer);

        staminaCosts.shield = GetFloat("Shield", staminaCosts.shield);

        staminaCosts.minStaminaDamage = GetFloat("MinStaminaDamage", staminaCosts.minStaminaDamage);

        staminaCosts.maxStaminaDamage = GetFloat("MaxStaminaDamage", staminaCosts.maxStaminaDamage);

        staminaCosts.powerAttackMultiplier = GetFloat("PowerAttackMultiplier", staminaCosts.powerAttackMultiplier);
    }

    void Save() {
        WriteFloat("Unarmed", staminaCosts.unarmed);

        WriteFloat("MovementSpeedEnabled", movementSpeedEnabled ? 1.0f : 0.0f);

        WriteFloat("MultiHitEnabled", multiHitEnabled ? 1.0f : 0.0f);

        WriteFloat("ExhaustionRecoveryPercent", exhaustionRecoveryPercent);

        WriteFloat("MultiHitScalingMultiplier", multiHitting.scalingMultiplier);

        WriteFloat("WeaponDrawnMultiplier", weaponDrawnMultiplier);
        WriteFloat("CombatMultiplier", combatMultiplier);

        if (unarmedSkillActorValue == RE::ActorValue::kLockpicking) {
            WriteString("UnarmedSkill", "Lockpicking");

        } else if (unarmedSkillActorValue == RE::ActorValue::kOneHanded) {
            WriteString("UnarmedSkill", "OneHanded");

        } else if (unarmedSkillActorValue == RE::ActorValue::kTwoHanded) {
            WriteString("UnarmedSkill", "TwoHanded");

        } else if (unarmedSkillActorValue == RE::ActorValue::kLightArmor) {
            WriteString("UnarmedSkill", "LightArmor");

        } else if (unarmedSkillActorValue == RE::ActorValue::kHeavyArmor) {
            WriteString("UnarmedSkill", "HeavyArmor");

        } else if (unarmedSkillActorValue == RE::ActorValue::kAlteration) {
            WriteString("UnarmedSkill", "Alteration");

        } else if (unarmedSkillActorValue == RE::ActorValue::kSneak) {
            WriteString("UnarmedSkill", "Sneak");

        } else {
            WriteString("UnarmedSkill", "Lockpicking");
        }

        WriteFloat("Dagger", staminaCosts.dagger);
        WriteFloat("Sword", staminaCosts.sword);
        WriteFloat("Greatsword", staminaCosts.greatsword);
        WriteFloat("WarAxe", staminaCosts.warAxe);
        WriteFloat("Battleaxe", staminaCosts.battleaxe);
        WriteFloat("Mace", staminaCosts.mace);
        WriteFloat("Warhammer", staminaCosts.warhammer);
        WriteFloat("Shield", staminaCosts.shield);

        WriteFloat("MinStaminaDamage", staminaCosts.minStaminaDamage);

        WriteFloat("MaxStaminaDamage", staminaCosts.maxStaminaDamage);

        WriteFloat("PowerAttackMultiplier", staminaCosts.powerAttackMultiplier);
    }

    float GetWeaponCost(const std::string& weaponType) {
        if (weaponType == "Unarmed") return staminaCosts.unarmed;

        if (weaponType == "Dagger") return staminaCosts.dagger;

        if (weaponType == "Sword") return staminaCosts.sword;

        if (weaponType == "WarAxe") return staminaCosts.warAxe;

        if (weaponType == "Mace") return staminaCosts.mace;

        if (weaponType == "Greatsword") return staminaCosts.greatsword;

        if (weaponType == "Battleaxe") return staminaCosts.battleaxe;

        if (weaponType == "Warhammer") return staminaCosts.warhammer;

        if (weaponType == "Shield") return staminaCosts.shield;

        return staminaCosts.unarmed;
    }

}