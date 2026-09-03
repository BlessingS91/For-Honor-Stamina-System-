#include "MovementSpeed.h"

#include <REL/Relocation.h>
#include <SKSE/SKSE.h>

#include <cstdint>

#include "RE/Skyrim.h"
#include "Settings.h"

namespace MovementSpeed {

    namespace {

        using MoveSpeedScale_t = float (*)(RE::TESObjectREFR*);
        MoveSpeedScale_t MoveSpeedScale_Original = nullptr;

        bool IsWeaponMagicOut(RE::Actor* actor) {
            if (!actor) {
                return false;
            }

            RE::TESCondition condition;

            auto* item = new RE::TESConditionItem();
            item->next = nullptr;

            item->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kIsWeaponMagicOut;

            item->data.flags.opCode = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;

            item->data.object = RE::CONDITIONITEMOBJECT::kSelf;

            item->data.comparisonValue.f = 1.0f;

            condition.head = item;

            return condition.IsTrue(actor, actor);
        }

        bool IsSprinting(RE::Actor* actor) {
            if (!actor) {
                return false;
            }

            RE::TESCondition condition;

            auto* item = new RE::TESConditionItem();
            item->next = nullptr;

            item->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kIsSprinting;

            item->data.flags.opCode = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;

            item->data.object = RE::CONDITIONITEMOBJECT::kSelf;

            item->data.comparisonValue.f = 1.0f;

            condition.head = item;

            return condition.IsTrue(actor, actor);
        }

    }

    float MoveSpeedHook::Call(RE::TESObjectREFR* a_ref) {
        if (!MoveSpeedScale_Original) {
            return 1.0f;
        }

        const float original = MoveSpeedScale_Original(a_ref);

        if (!Settings::movementSpeedEnabled) {
            return original;
        }

        if (!a_ref) {
            return original;
        }

        auto* actor = a_ref->As<RE::Actor>();
        if (!actor) {
            return original;
        }

        if (!actor->IsPlayerRef() && !actor->HasKeywordString("ActorTypeNPC")) {
            return original;
        }

        const bool sprinting = IsSprinting(actor);

        // No movement penalties while sprinting.
        if (sprinting) {
            return original;
        }

        const bool weaponDrawn = IsWeaponMagicOut(actor);
        const bool inCombat = actor->IsInCombat();

        float multiplier = 1.0f;

        if (weaponDrawn) {
            multiplier *= Settings::weaponDrawnMultiplier;
        }

        if (inCombat) {
            multiplier *= Settings::combatMultiplier;
        }

        const float result = original * multiplier;

        return result;
    }

    void Install() {
        logger::info("[MovementSpeed] Installing...");

        auto& trampoline = SKSE::GetTrampoline();

        REL::Relocation<std::uintptr_t> moveSpeedTarget{RELOCATION_ID(37013, 37943)};

        const std::uintptr_t moveSpeedCallAddress =
            moveSpeedTarget.address() + REL::Relocate<std::uintptr_t>(0x1A, 0x51);

        MoveSpeedScale_Original = reinterpret_cast<MoveSpeedScale_t>(
            trampoline.write_call<5>(moveSpeedCallAddress, reinterpret_cast<std::uintptr_t>(&MoveSpeedHook::Call)));

        if (!MoveSpeedScale_Original) {
            logger::error("[MovementSpeed] Failed to install MoveSpeed hook.");
            return;
        }

        logger::info("[MovementSpeed] MoveSpeed hook installed successfully.");
    }

}