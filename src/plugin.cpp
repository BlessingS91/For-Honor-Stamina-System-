#include "Events.h"
#include "Menu.h"
#include "MovementSpeed.h"
#include "Settings.h"
#include "Stamina.h"

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    SetupLog();

    Stamina::Events::GetSingleton()->AddEventSink();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            SKSE::AllocTrampoline(14);
            Stamina::Initialize();
            Menu::Install();
            MovementSpeed::Install();
        }
    });

    logger::info("For Honor Stamina System initialized.");

    return true;
}