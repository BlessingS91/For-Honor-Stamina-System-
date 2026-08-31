#pragma once

namespace Stamina {

    void Initialize();

    void ProcessAttack(RE::Actor* actor, RE::TESForm* attackObject, bool powerAttack, bool leftSwing);

    float GetAttackCost(RE::Actor* actor, RE::TESForm* attackObject, bool powerAttack, bool leftSwing);

    void ProcessExhausted(RE::Actor* actor);

    float CalculateExhaustionValue(RE::Actor* actor);

    void ProcessExhaustionRecovery(RE::Actor* actor);

    void GetRelevantSkills(RE::Actor* actor, float& weaponSkill, float& armorSkill);

    static void ApplyPerkEntryPoint(std::int32_t entry, RE::Actor* actor_a, RE::Actor* actor_b, float* out) {
        using func_t = decltype(&ApplyPerkEntryPoint);
        REL::Relocation<func_t> func{REL::RelocationID(23073, 23526)};
        return func(entry, actor_a, actor_b, out);
    }

}