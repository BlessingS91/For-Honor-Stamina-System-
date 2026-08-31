#pragma once

#include <RE/Skyrim.h>

namespace Stamina {
    class Events {
    public:
        static Events* GetSingleton() {
            static Events singleton;
            return std::addressof(singleton);
        }

        void AddEventSink();

    private:
        static RE::BSEventNotifyControl ProcessEvent(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this,
                                                     RE::BSAnimationGraphEvent* a_event,
                                                     RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source);

        static RE::BSEventNotifyControl PCProcessEvent(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this,
                                                       RE::BSAnimationGraphEvent* a_event,
                                                       RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source);

        static RE::BSEventNotifyControl NPCProcessEvent(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this,
                                                        RE::BSAnimationGraphEvent* a_event,
                                                        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source);

        inline static REL::Relocation<decltype(&PCProcessEvent)> _PCProcessEvent;

        inline static REL::Relocation<decltype(&NPCProcessEvent)> _NPCProcessEvent;
    };
}