#include "Settings.h"

#include <SKSE/SKSE.h>
#include <SKSEMenuFramework.h>

#include "Menu.h"

namespace {

    namespace ui = ImGuiMCP;

    constexpr auto kSection = "For Honor Blocking";
    constexpr auto kBlockingPage = "Blocking Settings";

    void RenderBlockingSettings() {
        ui::Text("For Honor Blocking");
        ui::Separator();

        ui::Text("Hit Detection");

        const bool previousValue = Settings::usePrecisionPreHit;

        ui::Checkbox("Use Precision PreHit", &Settings::usePrecisionPreHit);

        if (Settings::usePrecisionPreHit != previousValue) {
            Settings::Save();
        }

        if (Settings::usePrecisionPreHit) {
            ui::Text("Blocking uses Precision PreHit callbacks.");
        } else {
            ui::Text("Blocking uses TESHitEvent.");
        }

        ui::Separator();

        if (ui::Button("Save Settings")) {
            Settings::Save();
        }

        ui::SameLine();

        if (ui::Button("Reset to Defaults")) {
            Settings::ResetToDefaults();
            Settings::Save();
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
        SKSEMenuFramework::AddSectionItem(kBlockingPage, RenderBlockingSettings);

        SKSE::log::info("[menu] registered {}/{}", kSection, kBlockingPage);
    }

}