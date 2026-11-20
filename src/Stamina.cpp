#include "Stamina.h"

#include <SKSE/SKSE.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Hooks.h"
#include "MultiHittingBalance.h"
#include "PoiseAPI.h"
#include "PrecisionHandler.h"
#include "RE/A/ActiveEffect.h"
#include "RE/B/BGSEntryPoint.h"
#include "RE/E/Effect.h"
#include "RE/M/MagicTarget.h"
#include "Settings.h"
#include "StaminaDamage.h"

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

        // These MGEFs are part of OutofStaminaState (0x800), not separate spells.
        constexpr RE::FormID kOutOfStaminaIMOD = 0x803;
        constexpr RE::FormID kOutOfStaminaMoveEffect = 0x805;
        constexpr RE::FormID kOutOfStaminaWeaponEffect = 0x80E;
        constexpr RE::FormID kOutOfStaminaRegenEffect = 0xA61;

        constexpr RE::FormID kStaminaAttackSuppressionSpell = 0xA60;

        constexpr auto kStaminaAttackSuppressionPlugin = "For Honor Stamina System.esp";

        struct SpellForms {
            RE::SpellItem* staminaCost = nullptr;

            RE::SpellItem* exhaustionRecovery = nullptr;

            RE::EffectSetting* exhaustionRecoveryEffect = nullptr;

            RE::SpellItem* exhausted = nullptr;

            RE::EffectSetting* outOfStaminaEffect = nullptr;

            RE::EffectSetting* outOfStaminaIMOD = nullptr;

            RE::EffectSetting* outOfStaminaMoveEffect = nullptr;

            RE::EffectSetting* outOfStaminaWeaponEffect = nullptr;

            RE::EffectSetting* outOfStaminaRegenEffect = nullptr;

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

            spellForms.outOfStaminaIMOD = handler->LookupForm<RE::EffectSetting>(kOutOfStaminaIMOD, kExhaustedPlugin);

            spellForms.outOfStaminaMoveEffect =
                handler->LookupForm<RE::EffectSetting>(kOutOfStaminaMoveEffect, kExhaustedPlugin);

            spellForms.outOfStaminaWeaponEffect =
                handler->LookupForm<RE::EffectSetting>(kOutOfStaminaWeaponEffect, kExhaustedPlugin);

            spellForms.outOfStaminaRegenEffect =
                handler->LookupForm<RE::EffectSetting>(kOutOfStaminaRegenEffect, kExhaustedPlugin);

            spellForms.attackSuppression =

                handler->LookupForm<RE::SpellItem>(kStaminaAttackSuppressionSpell, kStaminaAttackSuppressionPlugin);

            if (!spellForms.exhausted || !spellForms.outOfStaminaEffect || !spellForms.outOfStaminaIMOD ||
                !spellForms.outOfStaminaMoveEffect || !spellForms.outOfStaminaWeaponEffect ||
                !spellForms.outOfStaminaRegenEffect) {
                logger::error("[Stamina] Missing OutofStaminaState spell or one of its five magic effects.");
            }
        }

        // Changes spell effect definitions BEFORE casting. Skyrim copies magnitudes
        // into active effects on application; changing only ActiveEffect::magnitude
        // afterward does not reliably reapply an actor-value modifier.
        void ConfigureOutOfStaminaSpell(RE::SpellItem* spell, float duration) {
            if (!spell) {
                return;
            }

            const auto wholeSeconds = static_cast<std::uint32_t>(std::ceil(std::max(duration, 0.1f)));
            std::uint32_t found = 0;
            bool foundMove = false;
            bool foundWeapon = false;
            using EffectFlag = RE::EffectSetting::EffectSettingData::Flag;

            for (auto* effect : spell->effects) {
                if (!effect || !effect->baseEffect) {
                    continue;
                }

                // Set the same base duration on ALL parts of the single OOS spell.
                // ActiveEffect::duration is adjusted to the precise float afterward.
                effect->effectItem.duration = wholeSeconds;
                ++found;

                const bool isMove = effect->baseEffect == spellForms.outOfStaminaMoveEffect;
                const bool isWeapon = effect->baseEffect == spellForms.outOfStaminaWeaponEffect;
                if (!isMove && !isWeapon) {
                    continue;
                }

                if (isMove) {
                    foundMove = true;
                    effect->effectItem.magnitude = Settings::outOfStaminaMoveSpeedNerf;
                } else if (isWeapon) {
                    foundWeapon = true;

                    // Detrimental weapon-speed effect.
                    // 25% reduction = 0.25 magnitude.
                    effect->effectItem.magnitude = std::clamp(Settings::outOfStaminaAttackSpeedNerf, 0.0f, 1.0f);
                }

                // A No Magnitude MGEF ignores the adjustable spell magnitude.
                // Report it rather than silently mutating the loaded ESP record.
                const bool noMagnitude = effect->baseEffect->data.flags.all(EffectFlag::kNoMagnitude);
                const bool noDuration = effect->baseEffect->data.flags.all(EffectFlag::kNoDuration);
                if (noMagnitude) {
                    logger::error(
                        "[Stamina] OOS {} MGEF {:08X} has No Magnitude enabled. "
                        "Disable that flag in the ESP for the magnitude slider to work.",
                        isMove ? "movement" : "weapon-speed", effect->baseEffect->GetFormID());
                }
                if (Settings::debugLogging) {
                    logger::info(
                        "[Stamina] OOS {} template MGEF={:08X}, magnitude={:.3f}, "
                        "NoMagnitude={}, NoDuration={}, archetype={}, primaryAV={}",
                        isMove ? "movement" : "weapon-speed", effect->baseEffect->GetFormID(),
                        effect->effectItem.magnitude, noMagnitude, noDuration,
                        static_cast<int>(effect->baseEffect->data.archetype),
                        static_cast<int>(effect->baseEffect->data.primaryAV));
                }
            }

            if (!foundMove || !foundWeapon) {
                logger::error(
                    "[Stamina] OutofStaminaState 0x800 missing {}{} MGEF in its spell effects. "
                    "Check the SPEL effect list in xEdit.",
                    !foundMove ? "0x805 movement" : "",
                    !foundMove && !foundWeapon ? " and 0x80E weapon-speed"
                                               : (!foundWeapon ? "0x80E weapon-speed" : ""));
            }
            if (Settings::debugLogging && found != 5) {
                logger::warn("[Stamina] OOS spell has {} effect entries (expected 5). Check the ESP.", found);
            }
        }

        class SpellDurationVisitor final : public RE::MagicTarget::ForEachActiveEffectVisitor {
        public:
            SpellDurationVisitor(RE::SpellItem* a_spell, float a_remainingDuration)
                : spell(a_spell), remainingDuration(a_remainingDuration) {}

            RE::BSContainer::ForEachResult Accept(RE::ActiveEffect* effect) override {
                if (effect && effect->spell == spell) {
                    // Specify duration relative to current elapsed time, which also
                    // refreshes an already-active suppression effect after a new swing.
                    effect->duration = effect->elapsedSeconds + remainingDuration;
                    ++updated;

                    if (Settings::debugLogging && spell == spellForms.exhausted) {
                        auto* mgef = effect->GetBaseObject();
                        if (mgef == spellForms.outOfStaminaMoveEffect || mgef == spellForms.outOfStaminaWeaponEffect) {
                            logger::info(
                                "[Stamina] OOS active {} MGEF={:08X}, templateMagnitude={:.3f}, "
                                "activeMagnitude={:.3f}, duration={:.2f}s",
                                mgef == spellForms.outOfStaminaMoveEffect ? "movement" : "weapon-speed",
                                mgef->GetFormID(), effect->effect ? effect->effect->effectItem.magnitude : 0.0f,
                                effect->magnitude, effect->duration);
                        }
                    }
                }
                return RE::BSContainer::ForEachResult::kContinue;
            }

            std::size_t updated = 0;

        private:
            RE::SpellItem* spell;
            float remainingDuration;
        };

        std::size_t SetSpellRemainingDuration(RE::Actor* actor, RE::SpellItem* spell, float duration) {
            if (!actor || !spell || !std::isfinite(duration) || duration <= 0.0f) {
                return 0;
            }

            auto* target = actor->AsMagicTarget();
            if (!target) {
                return 0;
            }

            SpellDurationVisitor visitor(spell, duration);
            target->VisitEffects(visitor);
            return visitor.updated;
        }

        void ApplySpellDuration(RE::Actor* actor, RE::SpellItem* spell, float duration) {
            const auto found = SetSpellRemainingDuration(actor, spell, duration);

            // The engine can defer creating some active effects. Retry once on
            // the next main-thread task rather than silently leaving base lengths.
            if (found < spell->effects.size()) {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    const auto handle = actor->GetHandle();
                    tasks->AddTask([handle, spell, duration]() {
                        if (auto resolved = handle.get()) {
                            const auto lateFound = SetSpellRemainingDuration(resolved.get(), spell, duration);
                            if (Settings::debugLogging) {
                                logger::info("[Stamina] Delayed duration sync actor={:08X}, effects={}",
                                             resolved->GetFormID(), lateFound);
                            }
                        }
                    });
                }
            }

            if (Settings::debugLogging) {
                logger::info("[Stamina] Duration sync actor={:08X}, spell={:08X}, duration={:.2f}s, effects={}",
                             actor->GetFormID(), spell->GetFormID(), duration, found);
            }
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
                logger::error("[Stamina] Attack suppression spell not found.");
            }
            return;
        }

        auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        if (!caster) {
            return;
        }

        // No Variable08 changes. Set the spell's cast duration directly, then
        // refresh its active effects to the exact requested remaining time.
        const float desiredDuration = std::clamp(Settings::attackStaminaRegenDelay, 0.1f, 60.0f);
        const auto baseDuration = static_cast<std::uint32_t>(std::ceil(desiredDuration));
        for (auto* effect : spell->effects) {
            if (effect) {
                effect->effectItem.duration = baseDuration;
            }
        }

        caster->CastSpellImmediate(spell, true, actor, 1.0f, false, 0.0f, actor);
        ApplySpellDuration(actor, spell, desiredDuration);
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

        // Preserve the existing weapon/armor-skill exhaustion duration scaling.
        const float exhaustionValue = CalculateExhaustionValue(actor);
        const float baseDuration = Settings::outOfStaminaBaseDuration;
        const float durationMultiplier = 1.0f - (exhaustionValue / 200.0f);
        const float desiredDuration = std::max(0.1f, baseDuration * durationMultiplier);

        // This is the ONLY out-of-stamina cast. All five effects reside inside
        // OutofStaminaState; movement/weapon-speed magnitudes are prepared before
        // application so Skyrim initializes their actor-value effects correctly.
        ConfigureOutOfStaminaSpell(spell, desiredDuration);

        if (Settings::debugLogging) {
            logger::info(
                "[Stamina] OOS Actor={}, Skill={:.1f}, Base={:.2f}s, Duration={:.2f}s, "
                "MoveMagnitude={:.2f}, WeaponMagnitude={:.2f}",
                actor->GetName(), exhaustionValue, baseDuration, desiredDuration, Settings::outOfStaminaMoveSpeedNerf,
                Settings::outOfStaminaAttackSpeedNerf);
        }

        caster->CastSpellImmediate(spell, true, actor, 1.0f, false, 0.0f, actor);
        ApplySpellDuration(actor, spell, desiredDuration);
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

                "WeaponSkill={:.1f}, ArmorSkill={:.1f}, CombinedSkill={:.1f}",

                actor->GetName() ? actor->GetName() : "<unnamed>", weaponSkill, armorSkill, combinedSkill);
        }

        return combinedSkill;
    }

}