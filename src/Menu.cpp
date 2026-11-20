#include "Menu.h"

#include <SKSE/SKSE.h>
#include <SKSEMenuFramework.h>

#include <cmath>
#include <iterator>

#include "Settings.h"

namespace {
    namespace ui = ImGuiMCP;

    constexpr auto kSection = "For Honor Stamina System";

    constexpr auto kStaminaPage = "Stamina Settings";
    constexpr auto kOutOfStaminaPage = "Out of Stamina Settings";
    constexpr auto kMultiHitPage = "Multi-Hit Settings";
    constexpr auto kMovementPage = "Movement Settings";
    constexpr auto kDebugPage = "Debug Settings";

    void StaminaCostSlider(const char* label, float* value) {
        ui::SliderFloat(label, value, 1.0f, 50.0f, "%.0f");
        if (ui::IsItemDeactivatedAfterEdit()) {
            *value = std::round(*value);
        }
    }

    void RenderUnarmedSkillSetting() {
        constexpr const char* names[] = {
            "Lockpicking", "One Handed", "Two Handed", "Light Armor",
            "Heavy Armor", "Alteration", "Sneak"
        };
        constexpr RE::ActorValue values[] = {
            RE::ActorValue::kLockpicking, RE::ActorValue::kOneHanded,
            RE::ActorValue::kTwoHanded, RE::ActorValue::kLightArmor,
            RE::ActorValue::kHeavyArmor, RE::ActorValue::kAlteration,
            RE::ActorValue::kSneak
        };

        int selected = 0;
        for (int i = 0; i < static_cast<int>(std::size(values)); ++i) {
            if (Settings::unarmedSkillActorValue == values[i]) {
                selected = i;
                break;
            }
        }

        if (ui::Combo("Unarmed Skill", &selected, names, static_cast<int>(std::size(names)))) {
            Settings::unarmedSkillActorValue = values[selected];
        }
    }

    void RenderSaveButtons() {
        if (ui::Button("Save Settings")) {
            Settings::Save();
        }

        ui::SameLine();
        if (ui::Button("Reset to Defaults")) {
            Settings::ResetToDefaults();
        }
    }

    void RenderStaminaSettings() {
        auto& costs = Settings::staminaCosts;

        ui::Text("For Honor Stamina System");
        ui::Separator();
        ui::Text("Attack Stamina Costs");

        StaminaCostSlider("Unarmed", &costs.unarmed);
        StaminaCostSlider("Dagger", &costs.dagger);
        StaminaCostSlider("Sword", &costs.sword);
        StaminaCostSlider("War Axe", &costs.warAxe);
        StaminaCostSlider("Mace", &costs.mace);
        StaminaCostSlider("Greatsword", &costs.greatsword);
        StaminaCostSlider("Battleaxe", &costs.battleaxe);
        StaminaCostSlider("Warhammer", &costs.warhammer);
        StaminaCostSlider("Shield", &costs.shield);

        ui::Separator();
        RenderUnarmedSkillSetting();
        ui::Separator();

        ui::Text("Stamina Damage Scaling");
        ui::SliderFloat("Minimum Stamina Damage Multiplier", &costs.minStaminaDamage, 0.0f, 2.0f, "%.2f");
        ui::Text("0%% Stamina = %.2fx", costs.minStaminaDamage);
        ui::SliderFloat("Max Stamina Damage Multiplier", &costs.maxStaminaDamage, 0.0f, 2.0f, "%.2f");
        ui::Text("100%% Stamina = %.2fx", costs.maxStaminaDamage);

        ui::Separator();
        ui::Text("Power Attack");
        ui::SliderFloat("Power Attack Stamina Multiplier", &costs.powerAttackMultiplier, 1.0f, 5.0f, "%.2f");
        ui::Text("Power Attack Cost = %.2fx Light Attack Cost", costs.powerAttackMultiplier);

        ui::Separator();
        ui::Text("Stamina Regeneration Suppression");
        ui::Checkbox("Disable Stamina Regen While Attacking", &Settings::disableStaminaRegenWhileAttacking);

        // Stamina.cpp now sets the active spell's remaining duration directly;
        // the old Variable08-based 0.5-2.0 second limit no longer applies.
        float& regenDelay = Settings::attackStaminaRegenDelay;
        ui::SliderFloat("Attack Stamina Regen Suppression Duration", &regenDelay, 0.1f, 60.0f, "%.1f seconds");
        if (ui::IsItemDeactivatedAfterEdit()) {
            regenDelay = std::round(regenDelay * 10.0f) / 10.0f;
        }
        ui::Text("Stamina regeneration resumes %.1f seconds after the last attack.", regenDelay);

        ui::Separator();
        RenderSaveButtons();
    }

    void RenderOutOfStaminaSettings() {
        ui::Text("Out of Stamina Settings");
        ui::Separator();

        ui::Checkbox("Disable Out of Stamina State", &Settings::ignoreOutOfStamina);
        ui::Separator();
        ui::Checkbox("Allow Exhaustion Out of Combat", &Settings::allowExhaustionOutOfCombat);
        ui::Separator();

        ui::Text("Exhaustion Recovery");
        ui::SliderFloat("Stamina Recovery", &Settings::exhaustionRecoveryPercent, 0.0f, 100.0f, "%.0f");
        if (ui::IsItemDeactivatedAfterEdit()) {
            Settings::exhaustionRecoveryPercent = std::round(Settings::exhaustionRecoveryPercent);
        }
        ui::Text("Stamina Recovery = %.0f", Settings::exhaustionRecoveryPercent);

        ui::Separator();
        ui::Text("Out of Stamina State (Single Spell)");
        ui::Text("All five magic effects use the same calculated duration.");
        ui::SliderFloat("Base Out of Stamina Duration", &Settings::outOfStaminaBaseDuration,
                        5.0f, 20.0f, "%.1f seconds");
        ui::Text("Base Duration = %.1f seconds (reduced by weapon and armor skills)",
                 Settings::outOfStaminaBaseDuration);

        ui::Separator();
        ui::Text("Out of Stamina Penalties");

        // Applied to FHS_OutofStaminaMoveSpeed (0x805) before the single
        // OutofStaminaState spell is cast, rather than casting a second spell.
        // Movement-speed magnitude uses the 1-100 scale directly.
        // Display it as 1%-100% without converting the stored value.
        ui::SliderFloat("Movement Speed Reduction", &Settings::outOfStaminaMoveSpeedNerf,
                        1.0f, 100.0f, "%.0f%%");
        if (ui::IsItemDeactivatedAfterEdit()) {
            Settings::outOfStaminaMoveSpeedNerf = std::round(Settings::outOfStaminaMoveSpeedNerf);
        }
        ui::Text("Movement Speed Reduction = %.0f%%", Settings::outOfStaminaMoveSpeedNerf);

        // Weapon-speed magnitude uses the 0.0-1.0 scale; show 0%-100%.
        // Example: menu 20% is stored as 0.20 and written to the MGEF.
        float attackSpeedReductionPercent = Settings::outOfStaminaAttackSpeedNerf * 100.0f;
        if (ui::SliderFloat("Attack Speed Reduction", &attackSpeedReductionPercent,
                            0.0f, 100.0f, "%.0f%%")) {
            Settings::outOfStaminaAttackSpeedNerf = attackSpeedReductionPercent / 100.0f;
        }
        ui::Text("Out of Stamina Attack Speed = %.0f%%", 100.0f - attackSpeedReductionPercent);
        ui::Text("Penalty changes take effect the next time exhaustion is applied.");

        ui::Separator();
        RenderSaveButtons();
    }

    void RenderMultiHitSettings() {
        ui::Text("Multi-Hitting");
        ui::Separator();
        ui::Checkbox("Enable Multi-Hit Scaling", &Settings::multiHitEnabled);
        ui::SliderFloat("Multi-Hit Scaling Multiplier", &Settings::multiHitting.scalingMultiplier,
                        0.0f, 2.0f, "%.2f");
        ui::Text("Multi-Hit Scaling = %.2fx", Settings::multiHitting.scalingMultiplier);
        ui::Separator();
        RenderSaveButtons();
    }

    void RenderMovementSettings() {
        ui::Text("Movement Speed");
        ui::Separator();
        ui::Checkbox("Enable Movement", &Settings::movementSpeedEnabled);
        ui::SliderFloat("Weapon Drawn Speed", &Settings::weaponDrawnMultiplier, 0.0f, 1.0f, "%.2f");
        ui::Text("Weapon Drawn Speed = %.0f%%", Settings::weaponDrawnMultiplier * 100.0f);
        ui::SliderFloat("Combat Speed", &Settings::combatMultiplier, 0.0f, 1.0f, "%.2f");
        ui::Text("Combat Speed = %.0f%%", Settings::combatMultiplier * 100.0f);
        ui::Separator();
        RenderSaveButtons();
    }

    void RenderDebugSettings() {
        ui::Text("Debug");
        ui::Separator();
        ui::Checkbox("Enable Debug Logging", &Settings::debugLogging);
        ui::Separator();
        RenderSaveButtons();
    }
}

namespace Menu {
    void Install() {
        if (!SKSEMenuFramework::IsInstalled()) {
            SKSE::log::info("[menu] SKSE Menu Framework not loaded, settings page unavailable");
            return;
        }

        SKSEMenuFramework::SetSection(kSection);
        SKSEMenuFramework::AddSectionItem(kStaminaPage, RenderStaminaSettings);
        SKSEMenuFramework::AddSectionItem(kOutOfStaminaPage, RenderOutOfStaminaSettings);
        SKSEMenuFramework::AddSectionItem(kMultiHitPage, RenderMultiHitSettings);
        SKSEMenuFramework::AddSectionItem(kMovementPage, RenderMovementSettings);
        SKSEMenuFramework::AddSectionItem(kDebugPage, RenderDebugSettings);

        SKSE::log::info("[menu] registered {}/{}", kSection, kStaminaPage);
        SKSE::log::info("[menu] registered {}/{}", kSection, kOutOfStaminaPage);
        SKSE::log::info("[menu] registered {}/{}", kSection, kMultiHitPage);
        SKSE::log::info("[menu] registered {}/{}", kSection, kMovementPage);
        SKSE::log::info("[menu] registered {}/{}", kSection, kDebugPage);
    }
}
