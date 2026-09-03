#include "Events.h"

#include "Stamina.h"
#include "StaminaDamage.h"

namespace Stamina {

    void Events::AddEventSink() {
        REL::Relocation<std::uintptr_t> pcVtbl{RE::VTABLE_PlayerCharacter[2]};
        REL::Relocation<std::uintptr_t> npcVtbl{RE::VTABLE_Character[2]};

        _PCProcessEvent = pcVtbl.write_vfunc(0x1, reinterpret_cast<std::uintptr_t>(&PCProcessEvent));

        _NPCProcessEvent = npcVtbl.write_vfunc(0x1, reinterpret_cast<std::uintptr_t>(&NPCProcessEvent));

        logger::info("Installed stamina animation event hooks.");
    }

    RE::BSEventNotifyControl Events::ProcessEvent(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this,
                                                  RE::BSAnimationGraphEvent* a_event,
                                                  RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source) {
        if (!a_event || !a_event->holder) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* actor = const_cast<RE::Actor*>(a_event->holder->As<RE::Actor>());

        if (!actor) {
            return RE::BSEventNotifyControl::kContinue;
        }

        const auto& tag = a_event->tag;

        // bash/stagger exhaust only when player stamina is below 1.

        if (tag == "bashStop" || tag == "staggerStart") {
            if (actor == RE::PlayerCharacter::GetSingleton()) {
                if (auto* actorValueOwner = actor->AsActorValueOwner()) {
                    const float currentStamina = actorValueOwner->GetActorValue(RE::ActorValue::kStamina);

                    if (currentStamina < 1.0f) {
                        ProcessExhausted(actor);
                        logger::info("Exhaustion applied on {}. Stamina={:.1f}.", tag, currentStamina);
                    }
                }
            }

            return RE::BSEventNotifyControl::kContinue;
        }

        // Only weapon swings reach the stamina check.
        if (tag != "weaponSwing" && tag != "weaponLeftSwing") {
            return RE::BSEventNotifyControl::kContinue;
        }

        const bool leftSwing = tag == "weaponLeftSwing";
        auto* attackObject = actor->GetEquippedObject(leftSwing);

        bool powerAttack = false;

        auto& runtimeData = actor->GetActorRuntimeData();
        auto* currentProcess = runtimeData.currentProcess;

        if (currentProcess && currentProcess->high && currentProcess->high->attackData) {
            auto* attackData = currentProcess->high->attackData.get();

            if (attackData) {
                powerAttack = attackData->data.flags.all(RE::AttackData::AttackFlag::kPowerAttack);
            }
        }

        const float attackCost = GetAttackCost(actor, attackObject, powerAttack, leftSwing);

        const float damageMultiplier = StaminaDamage::GetStaminaMultiplier(actor);
        logger::info("Attack: {} | Damage Multiplier={:.3f}", actor->GetName(), damageMultiplier);

        bool willExhaust = false;

        // Check whether this attack exceeds the player's current stamina.
        if (actor == RE::PlayerCharacter::GetSingleton()) {
            if (auto* actorValueOwner = actor->AsActorValueOwner()) {
                const float currentStamina = actorValueOwner->GetActorValue(RE::ActorValue::kStamina);

                willExhaust = currentStamina < attackCost;

                logger::info("Attack stamina check: Current={:.1f}, Cost={:.1f}, WillExhaust={}", currentStamina,
                             attackCost, willExhaust);
            }
        }

        // Always let the stamina-cost spell handle the stamina transaction.
        ProcessAttack(actor, attackObject, powerAttack, leftSwing);

        if (willExhaust) {
            ProcessExhausted(actor);

            logger::info("Attack exhausted player: exhaustion applied immediately on {}.", tag);
        }

        logger::info("Attack event: tag={}, powerAttack={}, hand={}, object={}", tag, powerAttack,
                     leftSwing ? "Left" : "Right",
                     attackObject && attackObject->GetName() ? attackObject->GetName() : "Unarmed");

        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl Events::PCProcessEvent(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this,
                                                    RE::BSAnimationGraphEvent* a_event,
                                                    RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source) {
        ProcessEvent(a_this, a_event, a_source);

        return _PCProcessEvent(a_this, a_event, a_source);
    }

    RE::BSEventNotifyControl Events::NPCProcessEvent(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this,
                                                     RE::BSAnimationGraphEvent* a_event,
                                                     RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source) {
        ProcessEvent(a_this, a_event, a_source);

        return _NPCProcessEvent(a_this, a_event, a_source);
    }

}