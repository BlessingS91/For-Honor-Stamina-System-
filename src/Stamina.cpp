#include "Stamina.h"

#include "Settings.h"
#include "hooks/Hooks.h"

namespace Stamina {

    void Initialize() {
        Settings::Load();

        Hooks::Install();

        logger::info("Stamina hooks initialized.");
    }

    namespace {

        constexpr RE::FormID kStaminaCostSpell = 0xA56;
        constexpr auto kStaminaCostPlugin = "For Honor Stamina System.esp";

        constexpr RE::FormID kExhaustionRecoverySpell = 0xA57;
        constexpr auto kExhaustionRecoveryPlugin = "For Honor Stamina System.esp";

        constexpr RE::FormID kExhaustionRecoveryEffect = 0xA58;
        constexpr auto kExhaustionRecoveryEffectPlugin = "For Honor Stamina System.esp";

        constexpr RE::FormID kExhaustedSpell = 0x800;
        constexpr auto kExhaustedPlugin = "For Honor Stamina System.esp";

        constexpr RE::FormID kOutOfStaminaEffect = 0x80C;
        constexpr auto kOutOfStaminaEffectPlugin = "For Honor Stamina System.esp";
    }

    float GetAttackCost(RE::Actor* actor, RE::TESForm* attackObject, bool powerAttack, bool leftSwing) {
        if (!actor) {
            return 0.0f;
        }

        float shieldMultiplier = 1.0f;
        std::string weaponType = "HandToHandMelee";

        // The attackObject is the object in the hand performing the attack.
        if (attackObject) {
            // Check for a shield.
            if (attackObject->formType == RE::FormType::Armor) {
                auto* armor = static_cast<RE::TESObjectARMO*>(attackObject);
                for (int index = armor->numKeywords - 1; index >= 0; --index) {
                    auto* keyword = armor->keywords[index];
                    if (!keyword) {
                        continue;
                    }

                    const std::string_view editorID = keyword->formEditorID.c_str();

                    if (editorID == "ArmorTypeShield") {
                        weaponType = "Shield";
                    }
                    if (editorID == "ArmorSmallShield") {
                        shieldMultiplier = 1.0f;
                    }
                    if (editorID == "ArmorLight") {
                        shieldMultiplier = 1.1f;
                    }
                    if (editorID == "ArmorHeavy") {
                        shieldMultiplier = 1.2f;
                    }
                    if (editorID == "ArmorLargeShield") {
                        shieldMultiplier = 1.4f;
                    }
                }
            } else if (attackObject->formType == RE::FormType::Weapon) {
                auto* weapon = static_cast<RE::TESObjectWEAP*>(attackObject);
                // Find the weapon type keyword.
                for (int index = weapon->numKeywords - 1; index >= 0; --index) {
                    auto* keyword = weapon->keywords[index];
                    if (!keyword) {
                        continue;
                    }

                    const std::string_view editorID = keyword->formEditorID.c_str();
                    constexpr std::string_view prefix = "WeapType";

                    if (editorID.rfind(prefix, 0) != 0) {
                        continue;
                    }

                    weaponType = std::string(editorID.substr(prefix.length()));
                    break;
                }
            }
        }

        float cost = Settings::staminaCosts.unarmed;
        if (weaponType == "HandToHandMelee") {
            if (auto* gauntlets = actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kHands)) {
                for (int index = gauntlets->numKeywords - 1; index >= 0; --index) {
                    auto* keyword = gauntlets->keywords[index];
                    if (!keyword) {
                        continue;
                    }

                    const std::string_view editorID = keyword->formEditorID.c_str();

                    if (editorID == "ArmorHeavy") {
                        cost *= 1.20f;
                        break;
                    }

                    if (editorID == "ArmorLight") {
                        cost *= 1.10f;
                        break;
                    }
                }
            }
        }

        if (weaponType == "Dagger") {
            cost = Settings::staminaCosts.dagger;
        } else if (weaponType == "Sword") {
            cost = Settings::staminaCosts.sword;
        } else if (weaponType == "Greatsword") {
            cost = Settings::staminaCosts.greatsword;
        } else if (weaponType == "WarAxe") {
            cost = Settings::staminaCosts.warAxe;
        } else if (weaponType == "Battleaxe") {
            cost = Settings::staminaCosts.battleaxe;
        } else if (weaponType == "Mace") {
            cost = Settings::staminaCosts.mace;
        } else if (weaponType == "Warhammer") {
            cost = Settings::staminaCosts.warhammer;
        } else if (weaponType == "Shield") {
            cost = Settings::staminaCosts.shield;
            cost *= shieldMultiplier;
        }

        // Power attacks use the configurable multiplier.
        if (powerAttack) {
            cost *= Settings::staminaCosts.powerAttackMultiplier;
        }

        return cost;
    }

    void ProcessAttack(RE::Actor* actor, RE::TESForm* attackObject, bool powerAttack, bool leftSwing) {
        if (!actor) {
            return;
        }

        const float cost = GetAttackCost(actor, attackObject, powerAttack, leftSwing);

        if (cost <= 0.0f) {
            return;
        }

        auto* spell =
            RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(kStaminaCostSpell, kStaminaCostPlugin);

        if (!spell) {
            logger::error("Failed to find Stamina Costs spell.");
            return;
        }

        auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);

        if (!caster) {
            logger::error("Failed to get instant magic caster.");
            return;
        }

        const char* attackType = powerAttack ? "Power" : "Light";

        logger::info("{} attack: Cost={}, LeftSwing={}, Object={}", attackType, cost, leftSwing,
                     attackObject ? attackObject->GetName() : "Unarmed");

        caster->CastSpellImmediate(spell, true, actor, 1.0f, false, cost, actor);
    }
    void ProcessExhausted(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        auto* spell = RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(kExhaustedSpell, kExhaustedPlugin);

        if (!spell) {
            logger::error("Failed to find Exhausted spell.");
            return;
        }

        auto* magicTarget = actor->AsMagicTarget();
        if (!magicTarget) {
            return;
        }

        // Do not cast Exhausted if Out of Stamina is already active.
        auto* outOfStaminaEffect = RE::TESDataHandler::GetSingleton()->LookupForm<RE::EffectSetting>(
            kOutOfStaminaEffect, kOutOfStaminaEffectPlugin);

        if (!outOfStaminaEffect) {
            logger::error("Failed to find Out of Stamina magic effect.");
            return;
        }

        if (magicTarget->HasMagicEffect(outOfStaminaEffect)) {
            logger::info("Already in OOS State; skipping Exhausted.");
            return;
        }

        // Do not cast Exhausted while Exhaustion Recovery is active.
        auto* exhaustionRecoveryEffect = RE::TESDataHandler::GetSingleton()->LookupForm<RE::EffectSetting>(
            kExhaustionRecoveryEffect, kExhaustionRecoveryEffectPlugin);

        if (!exhaustionRecoveryEffect) {
            logger::error("Failed to find Exhaustion Recovery magic effect.");
            return;
        }

        if (magicTarget->HasMagicEffect(exhaustionRecoveryEffect)) {
            logger::info("Exhaustion Recovery is active; skipping Exhausted.");
            return;
        }

        auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);

        if (!caster) {
            logger::error("Failed to get instant magic caster.");
            return;
        }

        auto* actorValueOwner = actor->AsActorValueOwner();
        if (!actorValueOwner) {
            return;
        }

        const float oldVariable10 = actorValueOwner->GetActorValue(RE::ActorValue::kVariable10);

        const float durationValue = CalculateExhaustionValue(actor);

        actorValueOwner->SetActorValue(RE::ActorValue::kVariable10, durationValue);

        logger::info("Actor exhausted: {}, Variable10={:.1f}, Variable10 old={:.1f}", actor->GetName(), durationValue,
                     oldVariable10);

        caster->CastSpellImmediate(spell, true, actor, 1.0f, false, 0.0f, actor);

        actorValueOwner->SetActorValue(RE::ActorValue::kVariable10, oldVariable10);
    }

    void ProcessExhaustionRecovery(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        auto* spell = RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(kExhaustionRecoverySpell,
                                                                                    kExhaustionRecoveryPlugin);

        if (!spell) {
            logger::error("Failed to find Exhaustion Recovery spell.");
            return;
        }

        auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);

        if (!caster) {
            logger::error("Failed to get instant magic caster.");
            return;
        }

        auto* actorValueOwner = actor->AsActorValueOwner();

        if (!actorValueOwner) {
            return;
        }

        const float maxStamina = actorValueOwner->GetPermanentActorValue(RE::ActorValue::kStamina);
        const float recoveryAmount = maxStamina * Settings::exhaustionRecoveryPercent;

        if (recoveryAmount <= 0.0f) {
            return;
        }

        logger::info("Exhaustion recovery: Actor={}, MaxStamina={:.1f}, Recovery={:.1f}", actor->GetName(), maxStamina,
                     recoveryAmount);

        caster->CastSpellImmediate(spell, true, actor, 1.0f, false, recoveryAmount, actor);
    }

    void GetRelevantSkills(RE::Actor* actor, float& weaponSkill, float& armorSkill) {
        weaponSkill = 0.0f;
        armorSkill = 0.0f;

        if (!actor) {
            return;
        }

        auto* actorValueOwner = actor->AsActorValueOwner();
        if (!actorValueOwner) {
            return;
        }

        const char* weaponSkillType = "None";
        const char* armorSkillType = "Alteration";
        // Right-hand weapon.
        if (auto* attackObject = actor->GetEquippedObject(false)) {
            if (attackObject->formType == RE::FormType::Weapon) {
                auto* weapon = static_cast<RE::TESObjectWEAP*>(attackObject);

                const auto weaponType = static_cast<int>(weapon->GetWeaponType());

                if (weaponType == static_cast<int>(RE::WEAPON_TYPE::kOneHandSword) ||
                    weaponType == static_cast<int>(RE::WEAPON_TYPE::kOneHandDagger) ||
                    weaponType == static_cast<int>(RE::WEAPON_TYPE::kOneHandAxe) ||
                    weaponType == static_cast<int>(RE::WEAPON_TYPE::kOneHandMace)) {
                    weaponSkillType = "OneHanded";
                    weaponSkill = actorValueOwner->GetActorValue(RE::ActorValue::kOneHanded);

                } else if (weaponType == static_cast<int>(RE::WEAPON_TYPE::kTwoHandSword) ||
                           weaponType == static_cast<int>(RE::WEAPON_TYPE::kTwoHandAxe)) {
                    weaponSkillType = "TwoHanded";
                    weaponSkill = actorValueOwner->GetActorValue(RE::ActorValue::kTwoHanded);

                } else {
                    weaponSkillType = "Unarmed";
                    weaponSkill = actorValueOwner->GetActorValue(Settings::unarmedSkillActorValue);
                }

            } else {
                weaponSkillType = "Unarmed";
                weaponSkill = actorValueOwner->GetActorValue(Settings::unarmedSkillActorValue);
            }

        } else {
            weaponSkillType = "Unarmed";
            weaponSkill = actorValueOwner->GetActorValue(Settings::unarmedSkillActorValue);
        }

        // Chest armor.
        if (auto* chestArmor = actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kBody)) {
            bool isLight = false;
            bool isHeavy = false;

            for (int index = chestArmor->numKeywords - 1; index >= 0; --index) {
                auto* keyword = chestArmor->keywords[index];

                if (!keyword) {
                    continue;
                }

                const std::string_view editorID = keyword->formEditorID.c_str();

                if (editorID == "ArmorLight") {
                    isLight = true;
                    break;
                }

                if (editorID == "ArmorHeavy") {
                    isHeavy = true;
                    break;
                }
            }

            if (isLight) {
                armorSkillType = "LightArmor";
                armorSkill = actorValueOwner->GetActorValue(RE::ActorValue::kLightArmor);
            } else if (isHeavy) {
                armorSkillType = "HeavyArmor";
                armorSkill = actorValueOwner->GetActorValue(RE::ActorValue::kHeavyArmor);
            } else {
                armorSkillType = "Alteration";
                armorSkill = actorValueOwner->GetActorValue(RE::ActorValue::kAlteration);
            }
        } else {
            // No chest armor = Alteration.
            armorSkillType = "Alteration";
            armorSkill = actorValueOwner->GetActorValue(RE::ActorValue::kAlteration);
        }

        logger::info(
            "Relevant Skills: Actor={}, "
            "Weapon=[{}: {:.1f}], "
            "Armor=[{}: {:.1f}]",
            actor->GetName(), weaponSkillType, weaponSkill, armorSkillType, armorSkill);
    }

    float CalculateExhaustionValue(RE::Actor* actor) {
        if (!actor) {
            return 0.0f;
        }

        auto* actorValueOwner = actor->AsActorValueOwner();
        if (!actorValueOwner) {
            return 0.0f;
        }

        float weaponSkill = 0.0f;
        float armorSkill = 0.0f;

        GetRelevantSkills(actor, weaponSkill, armorSkill);

        const float weaponPercent = std::clamp(weaponSkill / 100.0f, 0.0f, 1.0f);

        const float armorPercent = std::clamp(armorSkill / 100.0f, 0.0f, 1.0f);

        const float combinedSkill = (weaponPercent + armorPercent) * 50.0f;

        logger::info(
            "Exhaustion Scaling: Actor={}, "
            "WeaponSkill={:.1f}, ArmorSkill={:.1f}, Variable10={:.1f}",
            *actor->GetName(), weaponSkill, armorSkill, combinedSkill);

        return combinedSkill;
    }
}