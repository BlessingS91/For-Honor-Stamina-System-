
#include "Events.h"
#include "Stamina.h"

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    SetupLog();

    Stamina::Events::GetSingleton()->AddEventSink();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            Stamina::Initialize();
        }
    });

    logger::info("For Honor Stamina System initialized.");

    return true;
}