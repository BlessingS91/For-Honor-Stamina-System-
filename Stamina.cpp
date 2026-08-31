#include "Stamina.h"

#include "hooks/Hooks.h"

namespace Stamina {

    void Initialize() {
        Hooks::Install();
        logger::info("Stamina hooks initialized.");
    }

    namespace {

        constexpr RE::FormID kStaminaCostSpell = 0xA56;
        constexpr auto kStaminaCostPlugin = "For Honor Stamina System.esp";

        constexpr RE::FormID kExhaustionRecoverySpell = 0xA57;
        constexpr auto kExhaustionRecoveryPlugin = "For Honor Stamina System.esp";

        constexpr RE::FormID kExhaustedSpell = 0x800;
        constexpr auto kExhaustedPlugin = "For Honor Stamina System.esp";

        constexpr RE::FormID kOutOfStaminaEffect = 0x80C;
        constexpr auto kOutOfStaminaEffectPlugin = "For Honor Stamina System.esp";

        constexpr float kExhaustionRecoveryPercent = 0.33f;
    }

    float GetAttackCost(RE::Actor* actor, RE::TESForm* attackObject, bool powerAttack, bool leftSwing) {
        if (!actor) {
            return 0.0f;
        }

        std::string weaponType = "HandToHandMelee";

        // The attackObject is the object in the hand performing the attack.
        if (attackObject) {
            // Check for a shield.
            if (auto* armor = attackObject->As<RE::TESObjectARMO>()) {
                for (int index = armor->numKeywords - 1; index >= 0; --index) {
                    auto* keyword = armor->keywords[index];

                    if (!keyword) {
                        continue;
                    }

                    const std::string editorID = keyword->formEditorID.c_str();

                    if (editorID == "ArmorTypeShield") {
                        weaponType = "Shield";
                        break;
                    }
                }

            } else if (auto* weapon = attackObject->As<RE::TESObjectWEAP>()) {
                // Find the weapon type keyword.
                for (int index = weapon->numKeywords - 1; index >= 0; --index) {
                    auto* keyword = weapon->keywords[index];

                    if (!keyword) {
                        continue;
                    }

                    const std::string editorID = keyword->formEditorID.c_str();
                    constexpr std::string_view prefix = "WeapType";

                    if (editorID.rfind(prefix, 0) != 0) {
                        continue;
                    }

                    weaponType = editorID.substr(prefix.length());
                    break;
                }
            }
        }

        // Unarmed/default.
        float cost = 10.0f;

        if (weaponType == "Dagger") {
            cost = 12.0f;
        } else if (weaponType == "Sword") {
            cost = 14.0f;
        } else if (weaponType == "Greatsword") {
            cost = 28.0f;
        } else if (weaponType == "WarAxe") {
            cost = 15.0f;
        } else if (weaponType == "Battleaxe") {
            cost = 30.0f;
        } else if (weaponType == "Mace") {
            cost = 16.0f;
        } else if (weaponType == "Warhammer") {
            cost = 32.0f;
        } else if (weaponType == "Claw") {
            cost = 10.0f;
        } else if (weaponType == "Katana") {
            cost = 16.0f;
        } else if (weaponType == "Pike") {
            cost = 28.0f;
        } else if (weaponType == "Spear") {
            cost = 26.0f;
        } else if (weaponType == "QtrStaff") {
            cost = 24.0f;
        } else if (weaponType == "Rapier") {
            cost = 13.0f;
        } else if (weaponType == "Javelin") {
            cost = 15.0f;
        } else if (weaponType == "Lance") {
            cost = 30.0f;
        } else if (weaponType == "LightGreatSword") {
            cost = 26.0f;
        } else if (weaponType == "Claymore") {
            cost = 30.0f;
        } else if (weaponType == "Whip") {
            cost = 10.0f;
        } else if (weaponType == "Shield") {
            cost = 12.0f;
        }

        // Power attacks cost twice as much.
        if (powerAttack) {
            cost *= 2.0f;
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

        // Do not cast Exhausted again while Out of Stamina Effect is active.
        auto* outOfStaminaEffect = RE::TESDataHandler::GetSingleton()->LookupForm<RE::EffectSetting>(
            kOutOfStaminaEffect, kOutOfStaminaEffectPlugin);

        if (!outOfStaminaEffect) {
            logger::error("Failed to find Out of Stamina magic effect.");
            return;
        }

        if (magicTarget->HasMagicEffect(outOfStaminaEffect)) {
            logger::info("Already in OOS State.");
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
        const float recoveryAmount = maxStamina * kExhaustionRecoveryPercent;

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

        // Right-hand weapon.
        if (auto* attackObject = actor->GetEquippedObject(false)) {
            if (auto* weapon = attackObject->As<RE::TESObjectWEAP>()) {
                switch (weapon->GetWeaponType()) {
                    case RE::WEAPON_TYPE::kOneHandSword:
                    case RE::WEAPON_TYPE::kOneHandDagger:
                    case RE::WEAPON_TYPE::kOneHandAxe:
                    case RE::WEAPON_TYPE::kOneHandMace:
                        weaponSkill = actorValueOwner->GetActorValue(RE::ActorValue::kOneHanded);
                        break;

                    case RE::WEAPON_TYPE::kTwoHandSword:
                    case RE::WEAPON_TYPE::kTwoHandAxe:
                        weaponSkill = actorValueOwner->GetActorValue(RE::ActorValue::kTwoHanded);
                        break;

                    default:
                        break;
                }
            }
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
                armorSkill = actorValueOwner->GetActorValue(RE::ActorValue::kLightArmor);
            } else if (isHeavy) {
                armorSkill = actorValueOwner->GetActorValue(RE::ActorValue::kHeavyArmor);
            } else {
                armorSkill = actorValueOwner->GetActorValue(RE::ActorValue::kAlteration);
            }
        } else {
            armorSkill = actorValueOwner->GetActorValue(RE::ActorValue::kAlteration);
        }
    }

    float CalculateExhaustionValue(RE::Actor* actor) {
        if (!actor) {
            return 0.0f;
        }

        float weaponSkill = 0.0f;
        float armorSkill = 0.0f;

        GetRelevantSkills(actor, weaponSkill, armorSkill);

        const float weaponPercent = std::clamp(weaponSkill / 100.0f, 0.0f, 1.0f);

        const float armorPercent = std::clamp(armorSkill / 100.0f, 0.0f, 1.0f);

        const float combinedSkill = (weaponPercent + armorPercent) * 50.0f;

        logger::info(
            "Exhaustion Scaling: Actor={}, WeaponSkill={:.1f}, "
            "ArmorSkill={:.1f}, Variable10={:.1f}",
            actor->GetName(), weaponSkill, armorSkill, combinedSkill);

        return combinedSkill;
    }
}