#include "Events.h"
#include "Menu.h"
#include "MovementSpeed.h"
#include "Settings.h"
#include "Stamina.h"
#include "TrueHud.h"
#include "hooks/Hooks.h"

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    SetupLog();

    Stamina::Events::GetSingleton()->AddEventSink();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            SKSE::AllocTrampoline(16);

            Stamina::Initialize();

            Settings::Load();
            // Settings::UpdateAttackPreventionGlobals();

            Menu::Install();
            MovementSpeed::Install();
            TrueHUD::Initialize();
        }
    });

    logger::info("For Honor Stamina System initialized.");
    return true;
}