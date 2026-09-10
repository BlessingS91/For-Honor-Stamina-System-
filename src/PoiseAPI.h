#pragma once

#include <Windows.h>

#include <cstdint>

#include "RE/Skyrim.h"

using PoiseDamageCallback_t = float (*)(RE::Actor* attacker, RE::Actor* target, float damage);

using PoiseRegisterDamageCallback_t = bool (*)(PoiseDamageCallback_t callback);

inline PoiseRegisterDamageCallback_t Poise_RegisterDamageCallback = nullptr;

inline bool InitializePoiseAPI() {
    HMODULE poise = GetModuleHandleW(L"ChocolatePoiseReforged.dll");

    if (!poise) {
        return false;
    }

    Poise_RegisterDamageCallback =
        reinterpret_cast<PoiseRegisterDamageCallback_t>(GetProcAddress(poise, "Poise_RegisterDamageCallback"));

    return Poise_RegisterDamageCallback != nullptr;
}