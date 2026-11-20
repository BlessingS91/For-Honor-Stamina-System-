#include "Events.h"
#include "Hooks.h"
#include "Menu.h"
#include "MovementSpeed.h"
#include "MultiHittingBalance.h"
#include "Settings.h"
#include "Stamina.h"
#include "TrueHud.h"

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    SetupLog();

    Stamina::Events::GetSingleton()->AddEventSink();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (!message) {
            return;
        }

        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            SKSE::AllocTrampoline(16);
            Stamina::Initialize();
            Menu::Install();
            MovementSpeed::Install();
            TrueHUD::Initialize();
        } else if (message->type == SKSE::MessagingInterface::kPreLoadGame ||
                   message->type == SKSE::MessagingInterface::kNewGame) {
            Stamina::Hooks::ResetRuntimeState();
            MultiHittingBalance::ResetAll();
            TrueHUD::ResetRuntimeState();
        }
    });

    logger::info("For Honor Stamina System initialized.");
    return true;
}