#include "Stamina.h"

#include "MultiHittingBalance.h"
#include "PoiseAPI.h"
#include "PrecisionHandler.h"
#include "RE/B/BGSEntryPoint.h"
#include "Settings.h"
#include "StaminaDamage.h"
#include "hooks/Hooks.h"
namespace Stamina {
    namespace {
        void CacheSpellForms();
    }

    void Initialize() {
        Settings::Load();
        CacheSpellForms();

        Hooks::Install();
        PrecisionHandler::Install();

        if (InitializePoiseAPI()) {
            if (Poise_RegisterDamageCallback(StaminaDamage::ApplyPoiseDamageMultiplier)) {
                logger::info("Chocolate Poise API detected. Stamina poise scaling enabled.");
            } else {
                logger::warn("Chocolate Poise API detected, but callback registration failed.");
            }
        } else {
            logger::info("Chocolate Poise API not detected. Stamina poise scaling disabled.");
        }

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

        constexpr RE::FormID kOutOfStaminaMoveSpeedNerf = 0xA5B;
        constexpr auto kOutOfStaminaMoveSpeedNerfPlugin = "For Honor Stamina System.esp";

        constexpr RE::FormID kOutOfStaminaAttackSpeedNerf = 0xA5C;
        constexpr auto kOutOfStaminaAttackSpeedNerfPlugin = "For Honor Stamina System.esp";

        constexpr RE::FormID kStaminaAttackSuppressionSpell = 0xA60;
        constexpr auto kStaminaAttackSuppressionPlugin = "For Honor Stamina System.esp";

        struct SpellForms {
            RE::SpellItem* staminaCost = nullptr;
            RE::SpellItem* exhaustionRecovery = nullptr;
            RE::EffectSetting* exhaustionRecoveryEffect = nullptr;
            RE::SpellItem* exhausted = nullptr;
            RE::EffectSetting* outOfStaminaEffect = nullptr;
            RE::SpellItem* moveSpeedNerf = nullptr;
            RE::SpellItem* attackSpeedNerf = nullptr;
            RE::SpellItem* attackSuppression = nullptr;
        };

        SpellForms spellForms{};

        // Called once at kDataLoaded before the stamina hooks are installed.
        // Forms are owned by the game's data handler; never delete them.
        void CacheSpellForms() {
            auto* handler = RE::TESDataHandler::GetSingleton();
            if (!handler) {
                logger::error("[Stamina] Cannot resolve spell forms: TESDataHandler unavailable.");
                return;
            }

            spellForms.staminaCost = handler->LookupForm<RE::SpellItem>(kStaminaCostSpell, kStaminaCostPlugin);
            spellForms.exhaustionRecovery =
                handler->LookupForm<RE::SpellItem>(kExhaustionRecoverySpell, kExhaustionRecoveryPlugin);
            spellForms.exhaustionRecoveryEffect =
                handler->LookupForm<RE::EffectSetting>(kExhaustionRecoveryEffect, kExhaustionRecoveryEffectPlugin);
            spellForms.exhausted = handler->LookupForm<RE::SpellItem>(kExhaustedSpell, kExhaustedPlugin);
            spellForms.outOfStaminaEffect =
                handler->LookupForm<RE::EffectSetting>(kOutOfStaminaEffect, kOutOfStaminaEffectPlugin);
            spellForms.moveSpeedNerf =
                handler->LookupForm<RE::SpellItem>(kOutOfStaminaMoveSpeedNerf, kOutOfStaminaMoveSpeedNerfPlugin);
            spellForms.attackSpeedNerf =
                handler->LookupForm<RE::SpellItem>(kOutOfStaminaAttackSpeedNerf, kOutOfStaminaAttackSpeedNerfPlugin);
            spellForms.attackSuppression =
                handler->LookupForm<RE::SpellItem>(kStaminaAttackSuppressionSpell, kStaminaAttackSuppressionPlugin);
        }
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

                for (int index = weapon->numKeywords - 1; index >= 0; --index) {
                    auto* keyword = weapon->keywords[index];

                    if (!keyword) {
                        continue;
                    }

                    const std::string_view editorID = keyword->formEditorID.c_str();

                    if (editorID == "WeapTypeDagger") {
                        weaponType = "Dagger";
                        break;
                    }

                    if (editorID == "WeapTypeSword") {
                        weaponType = "Sword";
                        break;
                    }

                    if (editorID == "WeapTypeGreatsword") {
                        weaponType = "Greatsword";
                        break;
                    }

                    if (editorID == "WeapTypeWarAxe") {
                        weaponType = "WarAxe";
                        break;
                    }

                    if (editorID == "WeapTypeBattleaxe") {
                        weaponType = "Battleaxe";
                        break;
                    }

                    if (editorID == "WeapTypeMace") {
                        weaponType = "Mace";
                        break;
                    }

                    if (editorID == "WeapTypeWarhammer") {
                        weaponType = "Warhammer";
                        break;
                    }
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
        // Apply the configured power attack multiplier.
        if (powerAttack) {
            cost *= Settings::staminaCosts.powerAttackMultiplier;
        }

        // Apply Skyrim's native stamina-cost perk modifiers to both attack types.
        const float beforePerks = cost;

        RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModPowerAttackStamina, actor, attackObject,
                                            &cost);

        if (Settings::debugLogging) {
            logger::info("[Stamina Perks] Actor={}, Attack={}, Before={:.2f}, After={:.2f}", actor->GetName(),
                         powerAttack ? "Power" : "Light", beforePerks, cost);
        }

        // Preserve existing multi-hit scaling.
        cost *= MultiHittingBalance::GetStaminaCostMultiplier(actor, leftSwing);

        return std::isfinite(cost) ? std::max(0.0f, cost) : 0.0f;
    }

    void ProcessAttack(RE::Actor* actor, RE::TESForm* attackObject, bool powerAttack, bool leftSwing) {
        ProcessAttack(actor, attackObject, powerAttack, leftSwing,
                      GetAttackCost(actor, attackObject, powerAttack, leftSwing));
    }

    void ProcessAttack(RE::Actor* actor, RE::TESForm* attackObject, bool powerAttack, bool leftSwing, float cost) {
        if (!actor || cost <= 0.0f) {
            return;
        }

        auto* spell = spellForms.staminaCost;

        if (!spell) {
            if (Settings::debugLogging) {
                logger::error("Failed to find Stamina Costs spell.");
            }
            return;
        }

        auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);

        if (!caster) {
            if (Settings::debugLogging) {
                logger::error("Failed to get instant magic caster.");
            }
            return;
        }

        const char* attackType = powerAttack ? "Power" : "Light";

        if (Settings::debugLogging) {
            logger::info("{} attack: Cost={}, LeftSwing={}, Object={}", attackType, cost, leftSwing,
                         attackObject ? attackObject->GetName() : "Unarmed");
        }

        caster->CastSpellImmediate(spell, true, actor, 1.0f, false, cost, actor);
    }

    void ProcessAttackStaminaSuppression(RE::Actor* actor) {
        if (!actor || !Settings::disableStaminaRegenWhileAttacking) {
            return;
        }

        auto* spell = spellForms.attackSuppression;

        if (!spell) {
            if (Settings::debugLogging) {
                logger::error("Failed to find Stamina Attack Suppression spell.");
            }
            return;
        }

        auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        auto* actorValueOwner = actor->AsActorValueOwner();
        if (!caster || !actorValueOwner) {
            if (Settings::debugLogging) {
                logger::warn("Attack Suppression: Missing MagicCaster or ActorValueOwner for actor {:08X}",
                             actor->GetFormID());
            }
            return;
        }

        const float oldVariable08 = actorValueOwner->GetActorValue(RE::ActorValue::kVariable08);

        // Base spell duration is 0.5 seconds.
        // Variable08 scales duration by 0.015 seconds per level.
        // Variable08 = 0   -> 0.5 seconds
        // Variable08 = 100 -> 2.0 seconds
        const float durationValue = std::clamp((Settings::attackStaminaRegenDelay - 0.5f) / 0.015f, 0.0f, 100.0f);

        actorValueOwner->SetActorValue(RE::ActorValue::kVariable08, durationValue);

        if (Settings::debugLogging) {
            logger::info("Attack Suppression: Actor={}, Delay={:.2f}s, PrevVar08={:.2f}, NewVar08={:.2f}",
                         actor->GetName(), Settings::attackStaminaRegenDelay, oldVariable08, durationValue);
        }

        caster->CastSpellImmediate(reinterpret_cast<RE::MagicItem*>(spell), true, actor, 1.0f, false, 0.0f, actor);

        actorValueOwner->SetActorValue(RE::ActorValue::kVariable08, oldVariable08);
    }

    void ProcessExhausted(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        auto* spell = spellForms.exhausted;

        if (!spell) {
            if (Settings::debugLogging) {
                logger::error("Failed to find Exhausted spell.");
            }
            return;
        }

        auto* magicTarget = actor->AsMagicTarget();
        if (!magicTarget) {
            return;
        }

        // Do not cast Exhausted if Out of Stamina is already active.
        auto* outOfStaminaEffect = spellForms.outOfStaminaEffect;

        if (!outOfStaminaEffect) {
            if (Settings::debugLogging) {
                logger::error("Failed to find Out of Stamina magic effect.");
            }
            return;
        }

        if (magicTarget->HasMagicEffect(outOfStaminaEffect)) {
            if (Settings::debugLogging) {
                logger::info("Already in OOS State; skipping Exhausted.");
            }
            return;
        }

        // Do not cast Exhausted while Exhaustion Recovery is active.
        auto* exhaustionRecoveryEffect = spellForms.exhaustionRecoveryEffect;

        if (!exhaustionRecoveryEffect) {
            if (Settings::debugLogging) {
                logger::error("Failed to find Exhaustion Recovery magic effect.");
            }
            return;
        }

        if (magicTarget->HasMagicEffect(exhaustionRecoveryEffect)) {
            if (Settings::debugLogging) {
                logger::info("Exhaustion Recovery is active; skipping Exhausted.");
            }
            return;
        }

        auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);

        if (!caster) {
            if (Settings::debugLogging) {
                logger::error("Failed to get instant magic caster.");
            }
            return;
        }

        auto* actorValueOwner = actor->AsActorValueOwner();
        if (!actorValueOwner) {
            return;
        }

        const float oldVariable10 = actorValueOwner->GetActorValue(RE::ActorValue::kVariable10);

        const float exhaustionValue = CalculateExhaustionValue(actor);
        const float baseDuration = Settings::outOfStaminaBaseDuration;

        // The ESP spell has a 10-second base duration.
        // Variable10 scales that duration:
        // 0   = 10 seconds
        // 100 = 5 seconds
        //
        // First calculate the desired duration using the selected base,
        // then convert that duration back into the Variable10 value.

        const float durationMultiplier = 1.0f - (exhaustionValue / 200.0f);

        const float desiredDuration = baseDuration * durationMultiplier;

        const float durationValue = 200.0f * (1.0f - (desiredDuration / 10.0f));

        actorValueOwner->SetActorValue(RE::ActorValue::kVariable10, durationValue);

        if (Settings::debugLogging) {
            logger::info(
                "OOS: Actor={}, PrevVar10={:.2f}, ExhaustionValue={:.2f}, BaseDuration={:.2f}s, "
                "DurationMultiplier={:.4f}, DesiredDuration={:.2f}s, NewVar10={:.2f}, ExpectedDuration={:.2f}s",
                actor->GetName(), oldVariable10, exhaustionValue, baseDuration, durationMultiplier, desiredDuration,
                durationValue, 10.0f * (1.0f - (durationValue / 200.0f)));
        }

        auto* moveSpeedNerf = spellForms.moveSpeedNerf;
        auto* attackSpeedNerf = spellForms.attackSpeedNerf;

        if (moveSpeedNerf) {
            caster->CastSpellImmediate(reinterpret_cast<RE::MagicItem*>(moveSpeedNerf), true, actor, 1.0f, false,
                                       Settings::outOfStaminaMoveSpeedNerf, actor);
        }

        if (attackSpeedNerf) {
            caster->CastSpellImmediate(reinterpret_cast<RE::MagicItem*>(attackSpeedNerf), true, actor, 1.0f, false,
                                       Settings::outOfStaminaAttackSpeedNerf, actor);
        }

        caster->CastSpellImmediate(reinterpret_cast<RE::MagicItem*>(spell), true, actor, 1.0f, false, 0.0f, actor);

        actorValueOwner->SetActorValue(RE::ActorValue::kVariable10, oldVariable10);
    }

    void ProcessExhaustionRecovery(RE::Actor* actor) {
        if (!actor) {
            return;
        }

        auto* spell = spellForms.exhaustionRecovery;

        if (!spell) {
            if (Settings::debugLogging) {
                logger::error("Failed to find Exhaustion Recovery spell.");
            }
            return;
        }

        auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);

        if (!caster) {
            if (Settings::debugLogging) {
                logger::error("Failed to get instant magic caster.");
            }
            return;
        }

        auto* actorValueOwner = actor->AsActorValueOwner();

        if (!actorValueOwner) {
            return;
        }

        const float recoveryAmount = Settings::exhaustionRecoveryPercent;

        if (recoveryAmount <= 0.0f) {
            return;
        }

        if (Settings::debugLogging) {
            logger::info("Exhaustion recovery: Actor={}, Recovery={:.1f}", actor->GetName(), recoveryAmount);
        }

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

        if (Settings::debugLogging) {
            logger::info(
                "Relevant Skills: Actor={}, "
                "Weapon=[{}: {:.1f}], "
                "Armor=[{}: {:.1f}]",
                actor->GetName(), weaponSkillType, weaponSkill, armorSkillType, armorSkill);
        }
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

        if (Settings::debugLogging) {
            logger::info(
                "Exhaustion Scaling: Actor={}, "
                "WeaponSkill={:.1f}, ArmorSkill={:.1f}, Variable10={:.1f}",
                actor->GetName() ? actor->GetName() : "<unnamed>", weaponSkill, armorSkill, combinedSkill);
        }

        return combinedSkill;
    }
}