#include "Menu.h"

#include <SKSE/SKSE.h>
#include <SKSEMenuFramework.h>

#include <cmath>

#include "Settings.h"

namespace {

    namespace ui = ImGuiMCP;

    constexpr auto kSection = "For Honor Stamina System";

    constexpr auto kStaminaPage = "Stamina Settings";
    constexpr auto kMultiHitPage = "Multi-Hit Settings";
    constexpr auto kMovementPage = "Movement Settings";

    void StaminaCostSlider(const char* label, float* value) {
        ui::SliderFloat(label, value, 1.0f, 50.0f, "%.0f");

        if (ui::IsItemDeactivatedAfterEdit()) {
            *value = std::round(*value);
        }
    }

    void RenderUnarmedSkillSetting() {
        constexpr const char* names[] = {"Lockpicking", "One Handed", "Two Handed", "Light Armor",
                                         "Heavy Armor", "Alteration", "Sneak"};

        constexpr RE::ActorValue values[] = {RE::ActorValue::kLockpicking, RE::ActorValue::kOneHanded,
                                             RE::ActorValue::kTwoHanded,   RE::ActorValue::kLightArmor,
                                             RE::ActorValue::kHeavyArmor,  RE::ActorValue::kAlteration,
                                             RE::ActorValue::kSneak};

        int selected = 0;

        for (int i = 0; i < static_cast<int>(std::size(values)); ++i) {
            if (Settings::unarmedSkillActorValue == values[i]) {
                selected = i;
                break;
            }
        }

        if (ui::Combo("Unarmed Skill Actor Value. Adamant Hand to Hand uses Lockpicking.", &selected, names,
                      static_cast<int>(std::size(names)))) {
            Settings::unarmedSkillActorValue = values[selected];
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

        ui::SliderFloat("Minimum Damage Multiplier", &costs.minStaminaDamage, 0.0f, 2.0f, "%.2f");

        ui::SliderFloat("Maximum Damage Multiplier", &costs.maxStaminaDamage, 0.0f, 2.0f, "%.2f");

        ui::Text("0%% Stamina = %.2fx", costs.minStaminaDamage);

        ui::Text("100%% Stamina = %.2fx", costs.maxStaminaDamage);

        ui::Separator();

        ui::Text("Target Stagger");

        ui::Text("0%% Stamina = %.2fx", costs.minStaminaDamage);

        ui::Text("100%% Stamina = %.2fx", costs.maxStaminaDamage);

        ui::Separator();

        ui::Text("Power Attack");

        ui::SliderFloat("Power Attack Stamina Multiplier", &costs.powerAttackMultiplier, 1.0f, 5.0f, "%.2f");

        ui::Text("Power Attack Cost = %.2fx Light Attack Cost", costs.powerAttackMultiplier);

        ui::Separator();

        ui::Text("Exhaustion Recovery");

        ui::SliderFloat("Recovery Percentage", &Settings::exhaustionRecoveryPercent, 0.0f, 1.0f, "%.2f");

        ui::Text("Recovery = %.0f%% of Maximum Stamina", Settings::exhaustionRecoveryPercent * 100.0f);

        ui::Separator();

        if (ui::Button("Save Settings")) {
            Settings::Save();
        }

        ui::SameLine();

        if (ui::Button("Reset to Defaults")) {
            Settings::ResetToDefaults();
        }
    }

    void RenderMultiHitSettings() {
        ui::Text("Multi-Hitting");
        ui::Separator();

        ui::Checkbox("Enable Multi-Hit Scaling", &Settings::multiHitEnabled);

        ui::SliderFloat("Multi-Hit Scaling Multiplier", &Settings::multiHitting.scalingMultiplier, 0.0f, 2.0f, "%.2f");

        ui::Text("Multi-Hit Scaling = %.2fx", Settings::multiHitting.scalingMultiplier);

        ui::Separator();

        if (ui::Button("Save Settings")) {
            Settings::Save();
        }

        ui::SameLine();

        if (ui::Button("Reset to Defaults")) {
            Settings::ResetToDefaults();
        }
    }

    void RenderMovementSettings() {
        ui::Text("Movement Speed");
        ui::Separator();

        ui::Checkbox("Enable Movement Speed Modifiers", &Settings::movementSpeedEnabled);

        ui::SliderFloat("Weapon Drawn Speed Multiplier", &Settings::weaponDrawnMultiplier, 0.0f, 1.0f, "%.2f");

        ui::Text("Weapon Drawn Speed = %.0f%%", Settings::weaponDrawnMultiplier * 100.0f);

        ui::SliderFloat("Combat Speed Multiplier", &Settings::combatMultiplier, 0.0f, 1.0f, "%.2f");

        ui::Text("Combat Speed = %.0f%%", Settings::combatMultiplier * 100.0f);

        ui::Separator();

        if (ui::Button("Save Settings")) {
            Settings::Save();
        }

        ui::SameLine();

        if (ui::Button("Reset to Defaults")) {
            Settings::ResetToDefaults();
        }
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

        SKSEMenuFramework::AddSectionItem(kMultiHitPage, RenderMultiHitSettings);

        SKSEMenuFramework::AddSectionItem(kMovementPage, RenderMovementSettings);

        SKSE::log::info("[menu] registered {}/{}", kSection, kStaminaPage);

        SKSE::log::info("[menu] registered {}/{}", kSection, kMultiHitPage);

        SKSE::log::info("[menu] registered {}/{}", kSection, kMovementPage);
    }

}